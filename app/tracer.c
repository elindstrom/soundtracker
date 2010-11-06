
/*
 * The Real SoundTracker - Pseudo-mixer for sample playback parameter tracing
 *
 * Copyright (C) 2001 Michael Krause
 * Copyright (C) 2003 Yury Aliaev

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
#include <math.h>

#include "mixer.h"
#include "xm-player.h"
#include "tracer.h"
#include "main.h"
#include "audio.h"
#include "gui-settings.h"

static int num_channels, mixfreq;

// This is an artificial limit. The code can do more channels.
static tracer_channel channels[32];

void
tracer_setnumch (int n)
{
    g_assert(n >= 1 && n <= 32);

    num_channels = n;
}

static void
tracer_updatesample (st_mixer_sample_info *si)
{
    int i;
    tracer_channel *c;

    for(i = 0; i < 2 * 32; i++) {
	c = &channels[i];

	if(c->sample != si || !(c->flags & TR_FLAG_SAMPLE_RUNNING)) {
	    continue;
	}

	if(c->data != si->data
	   || c->length != si->length
	   || c->looptype != si->looptype) {
	    c->flags &= ~TR_FLAG_SAMPLE_RUNNING;
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

static void
tracer_setmixfreq (guint16 frequency)
{
    mixfreq = frequency;
}

static void
tracer_reset (void)
{
    memset(channels, 0, sizeof(channels));
}

static void
tracer_startnote (int channel,
		  st_mixer_sample_info *s)
{
    tracer_channel *c = &channels[channel];

    c->flags = 0;
    
    c->sample = s;

    // The following three for update_sample()
    c->data = s->data;
    c->length = s->length;
    c->looptype = s->looptype;

    c->positionw = 0;
    c->positionf = 0;
    c->playend = 0;
    if(s->looptype == ST_MIXER_SAMPLE_LOOPTYPE_AMIGA) {
	c->flags |= TR_FLAG_LOOP_UNIDIRECTIONAL;
    } else if(s->looptype == ST_MIXER_SAMPLE_LOOPTYPE_PINGPONG) {
	c->flags |= TR_FLAG_LOOP_BIDIRECTIONAL;
    }
    c->direction = 1;
    c->freso = 0.0;
    c->ffreq = 1.0;
    c->flags |= TR_FLAG_SAMPLE_RUNNING;
}

static void
tracer_stopnote (int channel)
{
    tracer_channel *c = &channels[channel];

    c->flags = 0; /* Just stop the note without reverances */
}

static void
tracer_setsmplpos (int channel,
		   guint32 offset)
{
    tracer_channel *c = &channels[channel];

    if(c->sample && c->flags != 0) {
	if(offset < c->sample->length) {
	    c->positionw = offset;
	    c->positionf = 0;
	    c->direction = 1;
	} else {
	    c->flags = 0;
	}
    }
}

static void
tracer_setsmplend (int channel,
		   guint32 offset)
{
    tracer_channel *c = &channels[channel];

    if(c->sample && c->flags != 0) {
	if(c->positionw != 0 || offset < c->sample->length) {
	    // only end if the selection is not the whole sample
	    c->playend = offset;
	}
    }
}

static void
tracer_setfreq (int channel,
		float frequency)
{
    tracer_channel *c = &channels[channel];

    frequency /= mixfreq;

    c->freqw = (guint32)floor(frequency);
    c->freqf = (guint32)((frequency - c->freqw) * 4294967296.0);
}

static void
tracer_setvolume (int channel,
		  float volume)
{
    tracer_channel *c = &channels[channel];

    c->volume = volume;
}

static void
tracer_setpanning (int channel,
		   float panning)
{
    tracer_channel *c = &channels[channel];

    c->panning = 0.5 * (panning + 1.0);
}

static void
tracer_setchcutoff (int channel,
		    float freq)
{
    tracer_channel *c = &channels[channel];

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
tracer_setchreso (int channel,
		  float reso)
{
    tracer_channel *c = &channels[channel];

    g_assert(0.0 <= reso);
    g_assert(reso <= 1.0);
    c->freso = reso;
}

static guint32
tracer_mix_sub (tracer_channel *ch,
		guint32 num_samples_left)
{
    const gboolean loopit = (ch->playend == 0) &&
                              (ch->flags & (TR_FLAG_LOOP_UNIDIRECTIONAL | TR_FLAG_LOOP_BIDIRECTIONAL));
    const gboolean gonnapingpong = loopit && (ch->flags & TR_FLAG_LOOP_BIDIRECTIONAL);

    const gint64 lstart64 = ((guint64)ch->sample->loopstart) << 32;
    const gint64 freq64 = (((guint64)ch->freqw) << 32) + (guint64)ch->freqf;
    gint64 pos64 = ((guint64)(ch->positionw) << 32) + (guint64)ch->positionf;
    const gint32 ende = (ch->playend != 0) ? (ch->playend) : (loopit ? ch->sample->loopend : ch->length);
    const gint64 ende64 = (guint64)ende << 32;
    
    gint64 vieweit64;
    guint32 num_samples;

    if(ch->direction == 1)
	vieweit64 = pos64 + freq64 * num_samples_left;
    else
	vieweit64 = pos64 - freq64 * num_samples_left;

    if((ch->direction == 1 && vieweit64 < ende64) || (ch->direction == -1 && vieweit64 > lstart64)) {
	/* Normal conditions; we are far from loop boundaries and sample end */
	ch->positionw = vieweit64 >> 32;
	ch->positionf = vieweit64 & 0xffffffff;
	
	return num_samples_left;
    }

    /* If we've reached this point, this means we are close to any `critical point' of the sample */
    if(!loopit) {
	/* Sample without loop just stopped; we don't care about it more */
	ch->flags = 0;
	
	return 0;
    }

    if(!gonnapingpong) {
	/* End of Amiga-type loop */
	num_samples = (ende64 - pos64) / freq64 + 1;/* plus one sample from the next loop cycle */
	pos64 += num_samples * freq64 - ende64 + lstart64;

	ch->positionw = pos64 >> 32;
	ch->positionf = pos64 & 0xffffffff;

	g_assert(num_samples <= num_samples_left);
	return num_samples;
    }

    if(ch->direction == 1) {
	/* Reflection at the end of pingpong loop */
	num_samples = (ende64 - pos64) / freq64 + 1;/* plus one sample from the next loop cycle */
	pos64 += num_samples * freq64;
	pos64 = ende64 - (pos64 - ende64);

	ch->positionw = pos64 >> 32;
	ch->positionf = pos64 & 0xffffffff;
	ch->direction = -1;

	g_assert(num_samples <= num_samples_left);
	return num_samples;
    }
    
    /* The same but at the start... */
    num_samples = (pos64 - lstart64) / freq64 + 1;/* plus one sample from the next loop cycle */
    pos64 -= num_samples * freq64;
    pos64 = lstart64 - (pos64 - lstart64);

    ch->positionw = pos64 >> 32;
    ch->positionf = pos64 & 0xffffffff;
    ch->direction = 1;

    g_assert(num_samples <= num_samples_left);
    return num_samples;
}

static void *
tracer_mix (void * dest, guint32 count, gint16 *scopebufs[], int scopebufs_offset)
{
    int chnr;
    
    for(chnr = 0; chnr < num_channels; chnr++) {
	tracer_channel *ch = channels + chnr;
	int num_samples_left = count;

	if(!((ch->flags & TR_FLAG_SAMPLE_RUNNING) && (gui_settings.permanent_channels & (1 << chnr))))
	    continue;
	
	g_assert(ch->sample->lock);
	g_mutex_lock(ch->sample->lock);

	while(num_samples_left && (ch->flags & TR_FLAG_SAMPLE_RUNNING)) {
	    int num_samples = 0;	

	    num_samples = tracer_mix_sub(ch, num_samples_left);

	    num_samples_left -= num_samples;
	}

	g_mutex_unlock(ch->sample->lock);
    }
    return NULL;
}

static void
tracer_dumpall (void)
{
}

static st_mixer mixer_tracer = {
    "tracer",
    "Pseudo-mixer for channel settings tracing",/* It will NEVER be used and hence translated */

    tracer_setnumch,
    tracer_updatesample,
    NULL,
    NULL,
    tracer_setmixfreq,
    NULL,
    NULL,
    tracer_reset,
    tracer_startnote,
    tracer_stopnote,
    tracer_setsmplpos,
    tracer_setsmplend,
    tracer_setfreq,
    tracer_setvolume,
    tracer_setpanning,
    tracer_setchcutoff,
    tracer_setchreso,
    tracer_mix,
    NULL,
    NULL,

    0x7fffffff,

    NULL
};

void 
tracer_trace (int mixfreq, int songpos, int patpos)
{
    /* Attemp to take pitchband into account */
    /* Test if tempo and BPM are traced */
    st_mixer *real_mixer = mixer;
    mixer = &mixer_tracer;

    int stopsongpos = songpos;
    int stoppatpos = patpos;

    double rest = 0, previous = 0; /* Fractional part of the samples */
    
    if((stoppatpos -= 1) < 0){
	stopsongpos -= 1;
	stoppatpos = xm->patterns[xm->pattern_order_table[stopsongpos]].length - 1;
    }

    tracer_setmixfreq(mixfreq);
    tracer_reset();

    while(1) {
	double t;
	
	double current = xmplayer_play();
	t = current - previous + rest;
	previous = current;
	
	guint32 samples = t * mixfreq;
	rest = t - (double)samples / (double)mixfreq;
	
	tracer_mix(NULL, samples, NULL, 0);
	if(player_songpos > stopsongpos ||
	    (player_songpos == stopsongpos && player_patpos > stoppatpos) ||
	    (player_songpos == stopsongpos && player_patpos == stoppatpos && curtick >= player_tempo - 1))
		break;//? maybe player_patpos - 1
    }

    mixer = real_mixer;
}

tracer_channel*
tracer_return_channel (int n)
{
    g_assert(n < num_channels);

    return &channels[n];
}
