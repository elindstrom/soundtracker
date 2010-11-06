
/*
 * The Real SoundTracker - Cubically interpolating mixing routines
 *                         with IT style filter support
 *
 * Copyright (C) 2001 Michael Krause
 * Copyright (C) 1999-2000 Tammo Hinrichs

 * Despite its name, this mixer can run on every platform.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "mixer.h"
#include "kb-x86-asm.h"
#include "i18n.h"
#include "tracer.h"

static int num_channels, mixfreq;
static int clipflag;

static float *kb_x86_tempbuf = NULL;
static int kb_x86_tempbufsize = 0;

static float kb_x86_amplification = 0.25;

float kb_x86_ct0[256];
float kb_x86_ct1[256];
float kb_x86_ct2[256];
float kb_x86_ct3[256];

typedef struct kb_x86_channel {
    st_mixer_sample_info *sample;

    void *data;                   // for updatesample() to see if sample has changed
    int looptype;
    guint32 length;

    guint32 flags;                // see below
    float volume;                 // 0.0 ... 1.0
    float panning;                // 0.0 ... 1.0
    int direction;                // +1 for forward, -1 for backward
    guint32 playend;              // for a forced premature end of the sample

    float volleft;                // left volume (1.0 = no change)
    float volright;               // rite volume (1.0 = no change)

    guint32 ramp_num_samples;     // number of mixer output samples during which ramping is active
    float rampleft;               // left volramp (delta_vol per sample)
    float rampright;              // rite volramp (delta_vol per sample)
    float rampdestleft;           // ramp destination volume left
    float rampdestright;          // ramp destination volume right

    guint32 positionw;            // current sample position (whole part of 32.32)
    guint32 positionf;            // current sample position (fractional part of 32.32)
    guint32 freqw;                // frequency (whole part of 32.32)
    guint32 freqf;                // frequency (fractional part of 32.32)

    float ffreq;                  // filter frequency (0<=x<=1)
    float freso;                  // filter resonance (0<=x<1)
    float fl1;                    // filter lp buffer
    float fb1;                    // filter bp buffer
} kb_x86_channel;

#define KB_FLAG_LOOP_UNIDIRECTIONAL       1
#define KB_FLAG_LOOP_BIDIRECTIONAL        2
#define KB_FLAG_SAMPLE_RUNNING            4
#define KB_FLAG_JUST_STARTED              8
#define KB_FLAG_UPPER_ACTIVE             16
#define KB_FLAG_STOP_AFTER_VOLRAMP       32
#define KB_FLAG_DO_SAMPLE_START_DECLICK  64

// This is an artificial limit. The code can do more channels.
static kb_x86_channel channels[2 * 32];

// Number of samples the mixer needs in advance
#define KB_X86_SAMPLE_PADDING           3

// A ramp from 32768 to 0 should take RAMP_MAX_DURATION seconds
#define RAMP_MAX_DURATION 0.001

static void
kb_x86_setnumch (int n)
{
    g_assert(n >= 1 && n <= 32);

    num_channels = n;
}

/* This is just a quick hack to implement sample-change declicking
   without coding full virtual channel support. But this will come
   sooner or later so that hack is only temporary.

   It works like this: When a sample is stopped and a new one is
   immediately started, there usually results a little click. In order
   to avoid it, we let the old sample continue, but start a quick
   volume downramp on it towards 0 (so that the click gets a linear
   ramp and it's not that disturbing). It's obvious that for the time
   of the volume ramp, we need to store the kb_x86_channel data blocks
   of both the old channel and the new channel. Because of this, our
   channels[] array is twice as large, and in the lower copy, the
   KB_FLAG_UPPER_ACTIVE flag is active when the upper copy is the
   one with the currently running main sample and the lower copy is
   only the fading out old sample.

   Alle Klarheiten beseitigt? :-) As I said before, full
   virtual-channel support will make this superfluous.
*/
static kb_x86_channel *
kb_x86_get_channel_struct (int channel)
{
    kb_x86_channel *c = &channels[channel];

    if(c->flags & KB_FLAG_UPPER_ACTIVE) {
	c = &channels[channel + 32];
    }

    return c;
}

static void
kb_x86_updatesample (st_mixer_sample_info *si)
{
    int i;
    kb_x86_channel *c;

    for(i = 0; i < 2 * 32; i++) {
	c = &channels[i];

	if(c->sample != si || !(c->flags & KB_FLAG_SAMPLE_RUNNING)) {
	    continue;
	}

	if(c->data != si->data
	   || c->length != si->length
	   || c->looptype != si->looptype) {
	    c->flags &= ~KB_FLAG_SAMPLE_RUNNING;
	    continue;
	}
	 
	/* No relevant data has changed. Don't stop the sample, but update
	   our local loop data instead. */
	c->looptype = si->looptype;
	if(c->looptype != ST_MIXER_SAMPLE_LOOPTYPE_NONE) {
	    if(c->positionw < si->loopstart) {
		c->positionw = si->loopstart;
		c->positionf = 0x7fffffff;
	    } else if(c->positionw >= si->loopend) {
		c->positionw = si->loopend - 1;
		c->positionf = 0x7fffffff;
	    }
	}
    }
}

static gboolean
kb_x86_setmixformat (int format)
{
    if(format != 16)
	return FALSE;

    return TRUE;
}

static gboolean
kb_x86_setstereo (int on)
{
    if(!on)
	return FALSE;

    return TRUE;
}

static void
kb_x86_setmixfreq (guint16 frequency)
{
    mixfreq = frequency;
}

static void
kb_x86_setampfactor (float amplification)
{
    kb_x86_amplification = 0.25 * amplification;
}

static gboolean
kb_x86_getclipflag (void)
{
    return clipflag;
}

static void
kb_x86_reset (void)
{
    int i;

    memset(channels, 0, sizeof(channels));
    clipflag = 0;

    for(i = 0; i < 256; i++) {
	float x1 = i / 256.0;
	float x2 = x1*x1;
	float x3 = x1*x1*x1;
	kb_x86_ct0[i] = -0.5*x3 + x2 - 0.5*x1;
	kb_x86_ct1[i] = 1.5*x3 - 2.5 * x2+1;
	kb_x86_ct2[i] = -1.5*x3 + 2*x2 + 0.5*x1;
	kb_x86_ct3[i] = 0.5*x3 - 0.5*x2;
    }
}

static void
kb_x86_startnote (int channel,
		  st_mixer_sample_info *s)
{
    kb_x86_channel *c = kb_x86_get_channel_struct(channel);

    c->flags &= KB_FLAG_UPPER_ACTIVE;
    
    c->sample = s;

    // The following three for update_sample()
    c->data = s->data;
    c->length = s->length;
    c->looptype = s->looptype;

    c->positionw = 0;
    c->positionf = 0;
    c->playend = 0;
    if(s->looptype == ST_MIXER_SAMPLE_LOOPTYPE_AMIGA) {
	c->flags |= KB_FLAG_LOOP_UNIDIRECTIONAL;
    } else if(s->looptype == ST_MIXER_SAMPLE_LOOPTYPE_PINGPONG) {
	c->flags |= KB_FLAG_LOOP_BIDIRECTIONAL;
    }
    c->direction = 1;
    c->ramp_num_samples = 0;
    c->freso = 0.0;
    c->ffreq = 1.0;
    c->fl1 = 0.0;
    c->fb1 = 0.0;
    c->flags |= KB_FLAG_SAMPLE_RUNNING | KB_FLAG_JUST_STARTED;
}

static void
kb_x86_stopnote (int channel)
{
    kb_x86_channel *c = &channels[channel];
    kb_x86_channel *current_used_chan = kb_x86_get_channel_struct(channel);

    if(current_used_chan->flags & KB_FLAG_SAMPLE_RUNNING) {
	if(current_used_chan != c) {
	    c->flags &= ~KB_FLAG_UPPER_ACTIVE;
	} else {
	    c->flags |= KB_FLAG_UPPER_ACTIVE;
	}

	c = current_used_chan;

	c->flags |= KB_FLAG_STOP_AFTER_VOLRAMP;

	c->ramp_num_samples = RAMP_MAX_DURATION * mixfreq;
	if(c->ramp_num_samples == 0) {
	    c->ramp_num_samples = 1;
	}
	c->rampdestleft = 0;
	c->rampdestright = 0;
	c->rampleft = (c->rampdestleft - c->volleft) / c->ramp_num_samples;
	c->rampright = (c->rampdestright - c->volright) / c->ramp_num_samples;
    }
}

static void
kb_x86_setsmplpos (int channel,
		   guint32 offset)
{
    kb_x86_channel *c = kb_x86_get_channel_struct(channel);

    if(c->sample && c->flags != 0) {
	if(offset < c->sample->length) {
	    c->positionw = offset;
	    c->positionf = 0;
	    c->direction = 1;

	    if(c->flags & KB_FLAG_JUST_STARTED && offset > 0) {
		/* User has used 9xx command - declick sample start */
		c->flags |= KB_FLAG_DO_SAMPLE_START_DECLICK;
	    }
	} else {
	    c->flags &= KB_FLAG_UPPER_ACTIVE;
	}
    }
}

static void
kb_x86_setsmplend (int channel,
		   guint32 offset)
{
    kb_x86_channel *c = kb_x86_get_channel_struct(channel);

    if(c->sample && c->flags != 0) {
	if(c->positionw != 0 || offset < c->sample->length) {
	    // only end if the selection is not the whole sample
	    c->playend = offset;
	}
    }
}

static void
kb_x86_setfreq (int channel,
		float frequency)
{
    kb_x86_channel *c = kb_x86_get_channel_struct(channel);

    frequency /= mixfreq;

    c->freqw = (guint32)floor(frequency);
    c->freqf = (guint32)((frequency - c->freqw) * 4294967296.0 /* this is pow(2,32) */ ); 
}

static void
kb_x86_redo_vol_fields (kb_x86_channel *c)
{
    c->rampdestleft = c->volume * (1.0 - c->panning);
    c->rampdestright = c->volume * c->panning;
    g_assert(c->rampdestleft >= 0.0 && c->rampdestleft <= 1.0);
    g_assert(c->rampdestright >= 0.0 && c->rampdestright <= 1.0);

    if(c->flags & KB_FLAG_JUST_STARTED) {
	c->volleft = c->rampdestleft;
	c->volright = c->rampdestright;
    } else {
	c->ramp_num_samples = RAMP_MAX_DURATION * mixfreq;
	if(c->ramp_num_samples == 0) {
	    c->ramp_num_samples = 1;
	}
	c->rampleft = (c->rampdestleft - c->volleft) / c->ramp_num_samples;
	c->rampright = (c->rampdestright - c->volright) / c->ramp_num_samples;
    }
}

static void
kb_x86_setvolume (int channel,
		  float volume)
{
    kb_x86_channel *c = kb_x86_get_channel_struct(channel);

    c->volume = volume;
    kb_x86_redo_vol_fields(c);
}

static void
kb_x86_setpanning (int channel,
		   float panning)
{
    kb_x86_channel *c = kb_x86_get_channel_struct(channel);

    c->panning = 0.5 * (panning + 1.0);
    kb_x86_redo_vol_fields(c);
}

static void
kb_x86_setchcutoff (int channel,
		    float freq)
{
    kb_x86_channel *c = kb_x86_get_channel_struct(channel);

    if(freq < 0.0) {
	c->ffreq = 1.0;
	c->freso = 0.0;
    } else {
	g_assert(0.0 <= freq);
	g_assert(freq <= 1.0);
	c->ffreq = freq;
	c->freso = 0.0;
    }
}

static void
kb_x86_setchreso (int channel,
		  float reso)
{
    kb_x86_channel *c = kb_x86_get_channel_struct(channel);

    g_assert(0.0 <= reso);
    g_assert(reso <= 1.0);
    c->freso = reso;
}

#if defined(DEBUG_BUFFER)
static void
kb_x86_debug_dump_buffer (gint16 *buffer,
			  int count)
{
    int i;

    printf("{ ");
    for(i = 0; i < count; i++) {
	if(i > 0) {
	    printf(", ");
	}
	printf("%d", buffer[i]);
    }
    printf(" }\n");
}
#endif

static void
kb_x86_call_mixer (kb_x86_channel *ch,
		   kb_x86_mixer_data *md,
		   gboolean forward)
{
    if(!forward) {
	md->flags |= KB_X86_MIXER_FLAGS_BACKWARD;
    }
    kbasm_mix(md);
    ch->volleft = md->volleft;
    ch->volright = md->volright;
}

static guint32
kb_x86_mix_sub (kb_x86_channel *ch,
		guint32 num_samples_left,
		gboolean volramping,
		float *mixbuf,
		gint16 *scopebuf)
{
    kb_x86_mixer_data md;

    const gboolean loopit = (ch->playend == 0) &&
                              (ch->flags & (KB_FLAG_LOOP_UNIDIRECTIONAL | KB_FLAG_LOOP_BIDIRECTIONAL));
    const gboolean gonnapingpong = loopit && (ch->flags & KB_FLAG_LOOP_BIDIRECTIONAL);

    const gint64 lstart64 = ((guint64)ch->sample->loopstart) << 32;
    const gint32 pos = ch->positionw;
    const gint64 freq64 = (((guint64)ch->freqw) << 32) + (guint64)ch->freqf;
    const gint64 pos64 = ((guint64)(ch->positionw) << 32) + (guint64)ch->positionf;
    const gint32 ende = (ch->playend != 0) ? (ch->playend) : (loopit ? ch->sample->loopend : ch->length);
    const gint64 ende64 = (guint64)ende << 32;

    int num_samples;

    /* Initalize the data array for the assembly subroutines. */
    md.volleft = ch->volleft;
    md.volright = ch->volright;
    if(!volramping) {
	md.volrampl = 0;
	md.volrampr = 0;
    } else {
	md.volrampl = ch->rampleft;
	md.volrampr = ch->rampright;
    }
    md.positionf = ch->positionf;
    if(ch->direction == 1) {
	md.freqi = ch->freqw;
	md.freqf = ch->freqf;
    } else {
	gint64 freq64_ = -freq64;
	md.freqi = freq64_ >> 32;
	md.freqf = freq64_ & 0xffffffff;
    }
    md.mixbuffer = mixbuf;
    md.scopebuf = scopebuf;
    md.freso = ch->freso;
    md.ffreq = ch->ffreq;
    md.fl1 = ch->fl1;
    md.fb1 = ch->fb1;
    md.flags = KB_X86_MIXER_FLAGS_FILTERED;

    if(md.ffreq == 1.0 && md.freso == 0.0) {
	md.flags &= ~KB_X86_MIXER_FLAGS_FILTERED;
    }
    if(md.scopebuf) {
	md.flags |= KB_X86_MIXER_FLAGS_SCOPES;
    }
    if(volramping) {
	md.flags |= KB_X86_MIXER_FLAGS_VOLRAMP;
    }

    if((ch->direction == 1 && pos >= ende - KB_X86_SAMPLE_PADDING)
       || (ch->direction == -1 && pos < (gint32)(ch->sample->loopstart + KB_X86_SAMPLE_PADDING))) {
	/* This is the dangerous case. We are near one of the ends of
	   a loop or sample (we might even have crossed it
	   already!). We have to take care of handling the looping and
	   perhaps we also have to prepare a mirror copy of the sample
	   values located around the loop incontinuity so that the
	   assembly routines don't use illegal values when
	   interpolating. */
	gint16 buffer[KB_X86_SAMPLE_PADDING + 1];
	unsigned int i;
	gint32 j;
	gint16 *bufferpt;

	/* First check if we're completely out of bounds, that means
	   for example: not just slightly before the end of the loop,
	   but way behind it. */
	if(gonnapingpong) {
	    /* The pingpong loop case. Care is taken to take into
	       account extremely short loops with extremly high
	       frequencies. */
	    gboolean touched = FALSE;
	    gint64 mypos64 = ((guint64)(ch->positionw) << 32) + (guint64)ch->positionf;
	    gint64 lend64 = (((guint64)ch->sample->loopend) << 32) - 1;

	    while(1) {
		if(ch->direction == 1 && mypos64 >= lend64) {
		    ch->direction = -1;
		    mypos64 -= lend64;
		    mypos64 *= -1;
		    mypos64 += lend64;
		    touched = TRUE;
		} else if(ch->direction == -1 && mypos64 < lstart64) {
		    ch->direction = +1;
		    mypos64 -= lstart64;
		    mypos64 *= -1;
		    mypos64 += lstart64;
		    touched = TRUE;
		} else {
		    break;
		}
	    }

	    if(touched) {
		/* We've been really out of bounds. Start all over. */
		ch->positionf = (guint32)(mypos64 & 0xffffffff); /* just the lower 32 bits */
		ch->positionw = (guint32)(mypos64 >> 32); /* just the upper 32 bits */
		return 0;
	    }
	} else if(loopit) {
	    /* The unidirectional ("Amiga") loop case. */
	    gboolean touched = FALSE;
	    guint32 looplen = ch->sample->loopend - ch->sample->loopstart;

	    while(ch->positionw >= ch->sample->loopend) {
		ch->positionw -= looplen;
		touched = TRUE;
	    }

	    if(touched) {
		/* We've been out of bounds. Start all over. */
		return 0;
	    }
	} else {
	    if(ch->positionw >= ende) {
		/* A sample without loop has just ended. */
		ch->flags &= KB_FLAG_UPPER_ACTIVE;
		return num_samples_left;
	    }
	}

	/* The following code is concerned with doing the fake sample
	   stuff for correct handling of the loop incontinuities. */
	if(loopit || gonnapingpong) {
	    g_assert(pos < ch->sample->loopend);
	}

	if(gonnapingpong) {
	    int dir, bufferfilldir;

	    dir = bufferfilldir = ch->direction;
	    bufferpt = buffer;
	    if(ch->direction == -1) {
		bufferpt += (sizeof(buffer) / sizeof(buffer[0])) - 1;
	    }
	    for(i = 0, j = pos; i < sizeof(buffer) / sizeof(buffer[0]); i++) {
		*bufferpt = ((gint16*)ch->sample->data)[j];
		if(dir == +1) {
		    if(++j >= ch->sample->loopend) {
			dir = -1;
			j--;
		    }
		} else {
		    if(--j < (gint32)ch->sample->loopstart) {
			j++;
			dir = +1;
		    }
		}
		bufferpt += bufferfilldir;
	    }
	} else {
	    for(i = 0, j = pos; i < sizeof(buffer) / sizeof(buffer[0]); i++) {
		buffer[i] = ((gint16*)ch->sample->data)[j];
		if(++j >= ende) {
		    if(loopit) {
			j -= (ch->sample->loopend - ch->sample->loopstart);
		    } else {
			j--;
		    }
		}
	    }
	}

	num_samples = 1;
	md.numsamples = 1;
	if(ch->direction == 1) {
	    md.positioni = buffer;
	    kb_x86_call_mixer(ch, &md, TRUE);
	    ch->positionw = md.positioni - buffer + pos;
	} else {
	    md.positioni = buffer + ((sizeof(buffer) / sizeof(buffer[0])) - 1);
	    kb_x86_call_mixer(ch, &md, FALSE);
	    ch->positionw = md.positioni - (buffer + ((sizeof(buffer) / sizeof(buffer[0])) - 1)) + pos;
	}
	
	ch->positionf = md.positionf;
	ch->volleft = md.volleft;
	ch->volright = md.volright;
	ch->fl1 = md.fl1;
	ch->fb1 = md.fb1;

	return num_samples;
    } else {
	/* That we've reached this point means that the current sample
	   position is in an area of the sample that's not near the
	   start nor the end, which in turn means that we don't have
	   to create auxiliary arrays to prevent the mixer routine
	   from accessing invalid data beyond the end of the sample.

	   Now calculate how far we can go on like this until we hit a
	   dangerous area.  */
	if(ch->direction == 1) {
	    const guint64 wieweit64 = pos64 + freq64 * num_samples_left;
	    const guint32 wieweit = wieweit64 >> 32;

	    if(wieweit >= ende - KB_X86_SAMPLE_PADDING) {
		/* oh, sorry - we need to truncate mixing length this
		   time.  calculate exactly how many samples we can
		   still render in one run until we hit a dangerous
		   area. */
		num_samples = (ende64 - ((guint64)KB_X86_SAMPLE_PADDING << 32) - pos64 + (freq64 - 1)) / freq64;
		g_assert(num_samples > 0);
		g_assert(pos64 + freq64 * num_samples >= ende64 - ((guint64)KB_X86_SAMPLE_PADDING << 32));
		g_assert(pos64 + freq64 * (num_samples - 1) < ende64 - ((guint64)KB_X86_SAMPLE_PADDING << 32));
		num_samples = MIN(num_samples_left, num_samples);
	    } else {
		/* no problem, we can render the full buffer without
		   problems in one run.  */
		num_samples = num_samples_left;
	    }

	    md.positioni = (gint16*)ch->sample->data + pos;
	    md.numsamples = num_samples;
	    kb_x86_call_mixer(ch, &md, TRUE);
	} else {
	    /* The same stuff, only for backwards direction */
	    const gint64 wieweit64 = pos64 - freq64 * num_samples_left;
	    const gint32 wieweit = wieweit64 >> 32;

	    if(wieweit < (gint32)(ch->sample->loopstart + KB_X86_SAMPLE_PADDING)) {
		num_samples = 1 + (pos64 - (((guint64)KB_X86_SAMPLE_PADDING + ch->sample->loopstart) << 32)) / freq64;
		g_assert(num_samples > 0);
		g_assert(pos64 - freq64 * (gint64)(num_samples) < ((gint64)(ch->sample->loopstart + KB_X86_SAMPLE_PADDING) << 32));
		g_assert(pos64 - freq64 * (gint64)(num_samples - 1) >= ((gint64)(ch->sample->loopstart + KB_X86_SAMPLE_PADDING) << 32));
		num_samples = MIN(num_samples_left, num_samples);
	    } else {
		num_samples = num_samples_left;
	    }

	    md.positioni = (gint16*)ch->sample->data + pos;
	    md.numsamples = num_samples;
	    kb_x86_call_mixer(ch, &md, FALSE);
	}
	
	ch->positionw = md.positioni - (gint16*)ch->sample->data;
	ch->positionf = md.positionf;
	ch->volleft = md.volleft;
	ch->volright = md.volright;
	ch->fl1 = md.fl1;
	ch->fb1 = md.fb1;

	return num_samples;
    }
}

static void *
kb_x86_mix (void *dest,
	    guint32 count,
	    gint16 *scopebufs[],
	    int scopebuf_offset)
{
    int chnr;

    if(count > kb_x86_tempbufsize) {
	free(kb_x86_tempbuf);
	kb_x86_tempbufsize = count;
	kb_x86_tempbuf = malloc(2 * sizeof(float) * kb_x86_tempbufsize);
    }

    memset(kb_x86_tempbuf, 0, 2 * sizeof(float) * count);

    for(chnr = 0; chnr < 2 * 32; chnr++) {
	kb_x86_channel *ch = channels + chnr;
	float *tempbuf = kb_x86_tempbuf;
	int num_samples_left = count;
	gint16 *scopedata = NULL;

	if((chnr & 31) >= num_channels)
	    continue;

	if(scopebufs && (chnr < 32 || (channels[chnr - 32].flags & KB_FLAG_UPPER_ACTIVE))) {
	    scopedata = scopebufs[chnr & 31] + scopebuf_offset;
	}

	if(!(ch->flags & KB_FLAG_SAMPLE_RUNNING)) {
	    if(scopedata) {
		memset(scopedata, 0, 2 * num_samples_left);
	    }
	    continue;
	}

	if(ch->flags & KB_FLAG_JUST_STARTED) {
	    if(ch->flags & KB_FLAG_DO_SAMPLE_START_DECLICK) {
		ch->ramp_num_samples = RAMP_MAX_DURATION * mixfreq;
		if(ch->ramp_num_samples == 0) {
		    ch->ramp_num_samples = 1;
		}
		ch->volleft = 0.0;
		ch->volright = 0.0;
		ch->rampleft = (ch->rampdestleft - ch->volleft) / ch->ramp_num_samples;
		ch->rampright = (ch->rampdestright - ch->volright) / ch->ramp_num_samples;
	    }

	    ch->flags &= ~KB_FLAG_JUST_STARTED;
	}

	g_assert(ch->sample->lock);
	g_mutex_lock(ch->sample->lock);

	while(num_samples_left && (ch->flags & KB_FLAG_SAMPLE_RUNNING)) {
	    int num_samples = 0;
	    gboolean vol_ramping = (ch->ramp_num_samples != 0);
	    int max_samples_this_time = vol_ramping ? MIN(ch->ramp_num_samples, num_samples_left) : num_samples_left;

	    num_samples = kb_x86_mix_sub(ch,
					 max_samples_this_time, vol_ramping,
					 tempbuf, scopedata);

	    if(vol_ramping) {
		ch->ramp_num_samples -= num_samples;
		if(ch->ramp_num_samples == 0) {
		    /* Volume ramping finished. */
		    ch->volleft = ch->rampdestleft;
		    ch->volright = ch->rampdestright;
		    if(ch->flags & KB_FLAG_STOP_AFTER_VOLRAMP) {
			/* This was only a declicking channel. Stop sample. */
			ch->flags &= KB_FLAG_UPPER_ACTIVE;
		    }
		}
	    }

	    num_samples_left -= num_samples;
	    tempbuf += (num_samples * 2);
	    if(scopedata) {
		scopedata += num_samples;
	    }
	}

	g_mutex_unlock(ch->sample->lock);
    }

    clipflag = kbasm_post_mixing(kb_x86_tempbuf, (gint16*)dest, count, kb_x86_amplification);

    return dest + count * 2 * 2;
}

void
kb_x86_dumpstatus (st_mixer_channel_status array[])
{
    int i;
    gint32 pos;

    for(i = 0; i < 32; i++) {
	kb_x86_channel *c = kb_x86_get_channel_struct(i);
	
	if(c->flags & KB_FLAG_SAMPLE_RUNNING) {
	    array[i].current_sample = c->sample;
	    pos = c->positionw;
	    if(pos < 0) {
		pos = 0;
	    } else if(pos >= c->sample->length) {
		pos = c->sample->length - 1;
	    }
	    array[i].current_position = pos;
	} else { 
	    array[i].current_sample = NULL;
	}
    }
}

static void
kb_x86_loadchsettings (int ch)
{
    tracer_channel *tch;
    kb_x86_channel *kbch;

    g_assert(ch < num_channels);
    
    tch = tracer_return_channel(ch);
    kbch = kb_x86_get_channel_struct(ch);
    
    kbch->sample = tch->sample;
    kbch->data = tch->data;
    kbch->looptype = tch->looptype;
    kbch->length = tch->length;
    kbch->volume = tch->volume;
    kbch->panning = tch->panning;
    kbch->direction = tch->direction;
    kbch->playend = tch->playend;
    kbch->positionw = tch->positionw;
    kbch->positionf = tch->positionf;
    kbch->freqw = tch->freqw;
    kbch->freqf = tch->freqf;
    kbch->ffreq = tch->ffreq;
    kbch->freso = tch->freso;
    
    kbch->flags = (kbch->flags & KB_FLAG_UPPER_ACTIVE) | KB_FLAG_JUST_STARTED |
		  ((tch->flags & TR_FLAG_LOOP_UNIDIRECTIONAL) ? KB_FLAG_LOOP_UNIDIRECTIONAL : 0) |
		  ((tch->flags & TR_FLAG_LOOP_BIDIRECTIONAL) ? KB_FLAG_LOOP_BIDIRECTIONAL : 0) |
		  ((tch->flags & TR_FLAG_SAMPLE_RUNNING) ? KB_FLAG_SAMPLE_RUNNING : 0);

    kb_x86_redo_vol_fields(kbch);
}

st_mixer mixer_kbfloat = {
    "kbfloat",
    N_("High-quality FPU mixer, cubic interpolation, IT filters, unlimited length samples"),

    kb_x86_setnumch,
    kb_x86_updatesample,
    kb_x86_setmixformat,
    kb_x86_setstereo,
    kb_x86_setmixfreq,
    kb_x86_setampfactor,
    kb_x86_getclipflag,
    kb_x86_reset,
    kb_x86_startnote,
    kb_x86_stopnote,
    kb_x86_setsmplpos,
    kb_x86_setsmplend,
    kb_x86_setfreq,
    kb_x86_setvolume,
    kb_x86_setpanning,
    kb_x86_setchcutoff,
    kb_x86_setchreso,
    kb_x86_mix,
    kb_x86_dumpstatus,
    kb_x86_loadchsettings,

    0x7fffffff,

    NULL
};
