
/*
 * The Real SoundTracker - XM support routines
 *
 * Copyright (C) 1998-2001 Michael Krause
 *
 * The XM loader is based on the "maube" 0.10.4 source code, Copyright
 * (C) 1997 by Conrad Parker (not much is left over, though :D)
 *
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/wait.h>
#include <time.h>

#include <glib.h>

#include "i18n.h"
#include "gui-settings.h"
#include "xm.h"
#include "xm-player.h"
#include "endian-conv.h"
#include "st-subs.h"
#include "recode.h"
#include "errors.h"
#include "audio.h"

#include "preferences.h"

#define LFSTAT_IS_MODULE 1

static guint16 npertab[60]={
    /* -> Tuning 0 */
    1712,1616,1524,1440,1356,1280,1208,1140,1076,1016, 960, 906,
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
    428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
    107, 101,  95,  90,  85,  80,  75,  71,  67,  63,  60,  56
};

/* This function calculates relnote and finetune settings such that
   the sample, when played back at XM note 'note' (for example, C-6)
   will be played back at sample frequency 'frequency'. */

void
xm_freq_note_to_relnote_finetune (float frequency,
				  unsigned note,
				  gint8 *relnote,
				  gint8 *finetune)
{
    int pitch = - log(frequency / 8363.0) / M_LN2 * (float)PITCH_OCTAVE;
    int nn = 128 * (49 - note) - pitch / 2;
    int rn;

    /* Now, calculate relnote and finetune such that nn = (128*relnote
       + finetune).  The XM format allows a finetune in the interval
       [-128, +127].  Because of this, we typically have two solutions
       to the above equation. We use the one with finetune in the
       interval [-64, +63]. */

    if(nn < 0) {
        rn = (nn - 63) / 128;
    } else {
        rn = (nn + 64) / 128;
    }

    *finetune = nn - (rn * 128);
    rn = CLAMP(rn, -128, +127);
    *relnote = rn;
}

static void
xm_load_xm_note (XMNote *note,
		 FILE *f)
{
    guint8 c, d[4];
   
    note->note = 0;
    note->instrument = 0;
    note->volume = 0;
    note->fxtype = 0;
    note->fxparam = 0;

    c = fgetc(f);

    if(c & 0x80) {
	if(c & 0x01)
	    note->note = fgetc(f);
	if(c & 0x02)
	    note->instrument = fgetc(f);
	if(c & 0x04)
	    note->volume = fgetc(f);
	if(c & 0x08)
	    note->fxtype = fgetc(f);
	if(c & 0x10)
	    note->fxparam = fgetc(f);
    } else {
	fread(d, 1, sizeof(d), f);
	note->note = c;
	note->instrument = d[0];
	note->volume = d[1];
	note->fxtype = d[2];
	note->fxparam = d[3];
    }
}

static int
xm_put_xm_note (XMNote *n,
		guint8 *b)
{
    int p = 0;
    guint8 x = 0x80;

    if(n->note != 0) {
	x |= 0x01;
	p++;
    }
    if(n->instrument != 0) {
	x |= 0x02;
	p++;
    }
    if(n->volume != 0) {
	x |= 0x04;
	p++;
    }
    if(n->fxtype != 0) {
	x |= 0x08;
	p++;
    }
    if(n->fxparam != 0) {
	x |= 0x10;
	p++;
    }

    if(p <= 3) { /* FastTracker compares against 4 here */
	*b++ = x;
	if(x & 0x01)
	    *b++ = n->note;
	if(x & 0x02)
	    *b++ = n->instrument;
	if(x & 0x04)
	    *b++ = n->volume;
	if(x & 0x08)
	    *b++ = n->fxtype;
	if(x & 0x10)
	    *b++ = n->fxparam;
	return p + 1;
    } else {
	*b++ = n->note;
	*b++ = n->instrument;
	*b++ = n->volume;
	*b++ = n->fxtype;
	*b++ = n->fxparam;
	return 5;
    }
}

static int
xm_load_xm_pattern (XMPattern *pat,
		    int num_channels,
		    FILE *f)
{
    guint8 ph[9];
    int i, j;
    guint16 len;
    guint32 hdr_len;
    unsigned long new_position, position;

    fread(ph, 1, sizeof(ph), f);

    len = get_le_16(ph + 5);
    if(len > 256) {
	char buf[128];
	sprintf(buf, _("Pattern length out of range: %d.\n"), len);
	error_error(buf);
	return 0;
    }

    /* skip the rest of the header, if exists */
    if ((hdr_len = get_le_32(ph)) > 9)
	fseek(f, hdr_len - 9, SEEK_CUR);
    position = ftell(f);

    if(!st_init_pattern_channels(pat,
				 len > 0 ? len : 1,
				 (num_channels + 1) & 0xfe))
	return 0;

    if(get_le_16(ph + 7) == 0)
	return 1;

    /* Read channel data */
    for(j = 0; j < len; j++) {
	for(i = 0; i < num_channels; i++) {
	    xm_load_xm_note(&pat->channels[i][j], f);
	}
    }

    /* skip the rest of patterndata, which is really waste, but can exist */
    new_position = ftell(f);
    if((new_position - position) < get_le_16(ph + 7))
	fseek(f, get_le_16(ph + 7) - new_position + position, SEEK_CUR);

    return 1;
}


static int
xm_load_patterns (XMPattern ptr[],
		  int num_patterns,
		  int num_channels,
		  FILE *f,
		  int(*loadfunc)(XMPattern*,int,FILE*))
{
    int i, e;
    
    for(i = 0; i < 256; i++) {
	if(i < num_patterns)
	    e = loadfunc(&ptr[i], num_channels, f);
	else
	    e = st_init_pattern_channels(&ptr[i], 64, (num_channels + 1) & 0xfe);

	if(!e)
	    return 0;
    }
    
    return 1;
}

static void
xm_save_xm_pattern (XMPattern *p,
		    int num_channels,
		    FILE *f)
{
    int i, j;
    guint8 sh[9];
    static guint8 buf[32 * 256 * 5];
    int bp;

    bp = 0;
    for(j = 0; j < p->length; j++) {
	for(i = 0; i < num_channels; i++) {
	    bp += xm_put_xm_note(&p->channels[i][j], buf + bp);
	}
    }

    put_le_32(sh + 0, 9);
    sh[4] = 0;
    put_le_16(sh + 5, p->length);
    put_le_16(sh + 7, bp);

    if(bp == p->length * num_channels) {
	/* pattern is empty */
	put_le_16(sh + 7, 0);
	fwrite(sh, 1, sizeof(sh), f);
    } else {
	fwrite(sh, 1, sizeof(sh), f);
	fwrite(buf, 1, bp, f);
    }
}

static void
xm_load_xm_samples (STSample samples[],
		    int num_samples,
		    FILE *f)
{
    int i, j;
    guint8 sh[40];
    STSample *s;
    guint16 p;
    gint16 *d16;
    gint8 *d8;

    g_assert(num_samples <= 16);

    for(i = 0; i < num_samples; i++) {
	s = &samples[i];
	fread(sh, 1, sizeof(sh), f);
	s->sample.length = get_le_32(sh + 0);
	s->sample.loopstart = get_le_32(sh + 4);
	s->sample.loopend = get_le_32(sh + 8); /* this is really the loop _length_ */
	s->volume = sh[12];
	s->finetune = sh[13];
	s->sample.looptype = sh[14];
	s->panning = sh[15];
	s->relnote = sh[16];
	strncpy(s->name, sh + 18, 22);
	s->name[22] = '\0';
	recode_ibmpc_to_latin1(s->name, 22);
    }

    for(i = 0; i < num_samples; i++) {
	s = &samples[i];
	if(s->sample.length == 0) {
	    s->sample.data = NULL;
	    continue;
	}
	s->treat_as_8bit = !(s->sample.looptype & 0x10);
	s->sample.looptype &= 3;

	if(!s->treat_as_8bit) {
	    /* 16 bit sample */
	    s->treat_as_8bit = FALSE;
	    s->sample.length >>= 1;
	    s->sample.loopstart >>= 1;
	    s->sample.loopend >>= 1;

	    d16 = s->sample.data = malloc(2 * s->sample.length);
	    fread(d16, 1, 2 * s->sample.length, f);
	    le_16_array_to_host_order(d16, s->sample.length);

	    for(j = s->sample.length, p = 0; j; j--) {
		p += *d16;
		*d16++ = p;
	    }
	} else {
	    s->treat_as_8bit = TRUE;

	    d16 = s->sample.data = malloc(2 * s->sample.length);
	    d8 = (gint8*)d16;
	    d8 += s->sample.length;
	    fread(d8, 1, s->sample.length, f);

	    for(j = s->sample.length, p = 0; j; j--) {
		p += *d8++;
		*d16++ = p << 8;
	    }
	}

	if(s->sample.loopend == 0) {
	    s->sample.looptype = 0;
	} else {
	    s->sample.loopend += s->sample.loopstart; /* now it is the loop _end_ */
	}

	if(s->sample.looptype == ST_MIXER_SAMPLE_LOOPTYPE_NONE) {
	    s->sample.loopstart = 0;
	    s->sample.loopend = 1;
	}
    }
}

static void
xm_save_xm_samples (STSample samples[],
		    FILE *f,
		    int num_samples)
{
    int i, k;
    guint8 sh[40];
    STSample *s;

    for(i = 0; i < num_samples; i++) {
	/* save sample header */
	s = &samples[i];
	memset(sh, 0, sizeof(sh));
	put_le_32(sh + 0, s->sample.length * (s->treat_as_8bit ? 1 : 2));
	put_le_32(sh + 4, s->sample.loopstart * (s->treat_as_8bit ? 1 : 2));
	put_le_32(sh + 8, (s->sample.loopend - s->sample.loopstart) * (s->treat_as_8bit ? 1 : 2));
	sh[12] = s->volume;
	sh[13] = s->finetune;
	sh[14] = s->sample.looptype | (s->treat_as_8bit ? 0x00 : 0x10);
	sh[15] = s->panning;
	sh[16] = s->relnote;
	sh[17] = 0;
	strncpy(sh + 18, s->name, 22);
	recode_latin1_to_ibmpc(sh + 18, 22);
	fwrite(sh, 1, sizeof(sh), f);
    }
    
    for(i = 0; i < num_samples; i++) {
	s = &samples[i];

	if(!s->treat_as_8bit) {
	    // Save as 16 bit sample
	    gint16 *packbuf, *d16, *ss;
	    gint16 p, d;

	    packbuf = malloc(s->sample.length * 2);
	    d16 = s->sample.data;
	    ss = packbuf;

	    for(k = s->sample.length, p = 0, d = 0; k; k--) {
		d = *d16 - p;
		*ss++ = d;
		p = *d16++;
	    }

	    le_16_array_to_host_order(packbuf, s->sample.length);
	    fwrite(packbuf, 1, s->sample.length * 2, f);
	    free(packbuf);
	} else {
	    // Save as 8 bit sample
	    gint16 *d16;
	    gint8 *packbuf, *ss;
	    gint8 p, d;

	    packbuf = malloc(s->sample.length);
	    d16 = s->sample.data;
	    ss = packbuf;

	    for(k = s->sample.length, p = 0, d = 0; k; k--) {
		d = (*d16 >> 8) - p;
		*ss++ = d;
		p = (*d16++ >> 8);
	    }

	    fwrite(packbuf, 1, s->sample.length, f);
	    free(packbuf);
	}
    }
}

static void
xm_check_envelope (STEnvelope *e)
{
    int i;

    if(e->num_points == 0 || e->num_points > 12)
	e->num_points = 1;

    for(i = 0; i < e->num_points; i++) {
	int h = e->points[i].val;
	if(!(h >= 0 && h <= 64))
	    e->points[i].val = 32;
    }

    e->points[0].pos = 0;
}

static int
xm_load_xm_instrument (STInstrument *instr,
		       FILE *f)
{
    guint8 a[29], b[16];
    guint16 num_samples;
    guint32 iheader_size;

    st_clean_instrument(instr, NULL);

    fread(a, 1, sizeof(a), f);
    iheader_size = get_le_32(a);
    strncpy(instr->name, a + 4, 22);
    recode_ibmpc_to_latin1(instr->name, 22);

    if(iheader_size <= 29) {
	return 1;
    }

    num_samples = get_le_16(a + 27);
    if(num_samples > 16) {
	error_error("XM Load Error: Number of samples in instrument > 16.\n");
	return 0;
    }

    if(num_samples == 0) {
	/* Skip rest of header */
	fseek(f, iheader_size - sizeof(a), SEEK_CUR);
    } else {
	fread(a, 1, 4, f);
	if(get_le_32(a) != 40) {
	    error_error("XM Load Error: Sample header size != 40.\n");
	    return 0;
	}
	fread(instr->samplemap, 1, 96, f);
	fread(instr->vol_env.points, 1, 48, f);
	le_16_array_to_host_order((gint16*)instr->vol_env.points, 24);
	fread(instr->pan_env.points, 1, 48, f);
	le_16_array_to_host_order((gint16*)instr->pan_env.points, 24);

	fread(b, 1, sizeof(b), f);
	instr->vol_env.num_points = b[0];
	instr->vol_env.sustain_point = b[2];
	instr->vol_env.loop_start = b[3];
	instr->vol_env.loop_end = b[4];
	instr->vol_env.flags = b[8];
	instr->pan_env.num_points = b[1];
	instr->pan_env.sustain_point = b[5];
	instr->pan_env.loop_start = b[6];
	instr->pan_env.loop_end = b[7];
	instr->pan_env.flags = b[9];

	xm_check_envelope(&instr->vol_env);
	xm_check_envelope(&instr->pan_env);

	instr->vibtype = b[10];
	if(instr->vibtype >= 4) {
	    char buf[128];
	    instr->vibtype = 0;
	    sprintf(buf, "XM Load Warning: Invalid vibtype %d, using Sine.\n", instr->vibtype);
	    error_warning(buf);
	}
	instr->vibrate = b[13];
	instr->vibdepth = b[12];
	instr->vibsweep = b[11];
	
	instr->volfade = get_le_16(b + 14);

	if(iheader_size > 241) {
            /* Skip remainder of header */
	    fseek(f, iheader_size - 241, SEEK_CUR);
	}

	xm_load_xm_samples(instr->samples, num_samples, f);
    }

    return 1;
}

/* xm_load_xi loads a FastTracker instrument in XI format. This format
   is not entirely identical to the instrument format found in XM
   modules. Thanks to KB for reverse-engineering the file format (see
   XI.TXT) */
gboolean
xm_load_xi (STInstrument *instr,
	    FILE *f)
{
    guint8 a[29], b[38];
    int num_samples;

    st_clean_instrument(instr, NULL);

    fread(a, 1, 21, f);
    a[21] = 0;
    if(strcmp(a, "Extended Instrument: ")) {
	error_error(_("File is no XI instrument."));
	return 0;
    }

    fread(a, 1, 22, f);
    strncpy(instr->name, a, 22);
    recode_ibmpc_to_latin1(instr->name, 22);

    fread(a, 1, 23, f);
    if(get_le_16(a + 21) != 0x0102) {
	sprintf(b, _("Unknown XI version 0x%x\n"), get_le_16(a+21));
	error_error(b);
	return 0;
    }

    fread(instr->samplemap, 1, 96, f);
    fread(instr->vol_env.points, 1, 48, f);
    le_16_array_to_host_order((gint16*)instr->vol_env.points, 24);
    fread(instr->pan_env.points, 1, 48, f);
    le_16_array_to_host_order((gint16*)instr->pan_env.points, 24);

    fread(b, 1, 16, f);
    instr->vol_env.num_points = b[0];
    instr->vol_env.sustain_point = b[2];
    instr->vol_env.loop_start = b[3];
    instr->vol_env.loop_end = b[4];
    instr->vol_env.flags = b[8];
    instr->pan_env.num_points = b[1];
    instr->pan_env.sustain_point = b[5];
    instr->pan_env.loop_start = b[6];
    instr->pan_env.loop_end = b[7];
    instr->pan_env.flags = b[9];

    xm_check_envelope(&instr->vol_env);
    xm_check_envelope(&instr->pan_env);

    instr->vibtype = b[10];
    if(instr->vibtype >= 4) {
	char buf[128];
	instr->vibtype = 0;
	sprintf(buf, _("Invalid vibtype %d, using Sine.\n"), instr->vibtype);
	error_warning(buf);
    }
    instr->vibrate = b[13];
    instr->vibdepth = b[12];
    instr->vibsweep = b[11];
	
    instr->volfade = get_le_16(b + 14);

    fread(a, 1, 24, f);
    num_samples = get_le_16(a + 22);
    xm_load_xm_samples(instr->samples, num_samples, f);

    return 1;
}

gboolean
xm_save_xi (STInstrument *instr,
	    FILE *f)
{
    guint8 a[48];
    int num_samples;

    num_samples = st_instrument_num_save_samples(instr);

    fwrite("Extended Instrument: ", 1, 21, f);

    strncpy(a, instr->name, 22);
    recode_latin1_to_ibmpc(a, 22);
    fwrite(a, 1, 22, f);

    a[0] = 0x1a;
    memcpy(a + 1, "rst's SoundTracker  ", 20);
    put_le_16(a + 21, 0x0102);
    fwrite(a, 1, 23, f);

    fwrite(instr->samplemap, 1, 96, f);
    memcpy(a, instr->vol_env.points, 48);
    le_16_array_to_host_order((gint16*)a, 24);
    fwrite(a, 1, 48, f);
    memcpy(a, instr->pan_env.points, 48);
    le_16_array_to_host_order((gint16*)a, 24);
    fwrite(a, 1, 48, f);

    a[0] = instr->vol_env.num_points;
    a[2] = instr->vol_env.sustain_point;
    a[3] = instr->vol_env.loop_start;
    a[4] = instr->vol_env.loop_end;
    a[8] = instr->vol_env.flags;
    a[1] = instr->pan_env.num_points;
    a[5] = instr->pan_env.sustain_point;
    a[6] = instr->pan_env.loop_start;
    a[7] = instr->pan_env.loop_end;
    a[9] = instr->pan_env.flags;
    a[10] = instr->vibtype;
    a[13] = instr->vibrate;
    a[12] = instr->vibdepth;
    a[11] = instr->vibsweep;
    put_le_16(a + 14, instr->volfade);
    fwrite(a, 1, 16, f);

    memset(a, 0, 24);
    put_le_16(a + 22, num_samples);
    fwrite(a, 1, 24, f);

    xm_save_xm_samples(instr->samples, f, num_samples);

    return TRUE;
}

static void
xm_save_xm_instrument (STInstrument *instr,
                       FILE *f,
                       gboolean mode)
{
    guint8 h[48];
    int num_samples;

    num_samples = st_instrument_num_save_samples(instr);

    memset(h, 0, sizeof(h));
    strncpy(h + 4, instr->name, 22);
    recode_latin1_to_ibmpc(h + 4, 22);

    if(mode==FALSE)
	num_samples = 0;

    h[27] = num_samples;

    if(num_samples == 0) {
	h[0] = 33;
	h[1] = 0;
	fwrite(h, 1, 29, f);
	put_le_32(h, 40);
	fwrite(h, 1, 4, f);
	return;
    }

    put_le_16(h + 0, 263);
    fwrite(h, 1, 29, f);
    put_le_32(h, 40);
    fwrite(h, 1, 4, f);

    fwrite(instr->samplemap, 1, 96, f);
    memcpy(h, instr->vol_env.points, 48);
    le_16_array_to_host_order((gint16*)h, 24);
    fwrite(h, 1, 48, f);
    memcpy(h, instr->pan_env.points, 48);
    le_16_array_to_host_order((gint16*)h, 24);
    fwrite(h, 1, 48, f);

    memset(&h, 0, 38);
    h[0] = instr->vol_env.num_points;
    h[2] = instr->vol_env.sustain_point;
    h[3] = instr->vol_env.loop_start;
    h[4] = instr->vol_env.loop_end;
    h[8] = instr->vol_env.flags;
    h[1] = instr->pan_env.num_points;
    h[5] = instr->pan_env.sustain_point;
    h[6] = instr->pan_env.loop_start;
    h[7] = instr->pan_env.loop_end;
    h[9] = instr->pan_env.flags;

    h[10] = instr->vibtype;
    h[13] = instr->vibrate;
    h[12] = instr->vibdepth;
    h[11] = instr->vibsweep;
	
    put_le_16(h + 14, instr->volfade);
    
    fwrite(&h, 1, 38, f);

    if (mode==TRUE) xm_save_xm_samples(instr->samples, f, num_samples);
}

static void
xm_load_mod_note (XMNote *dest,
		  FILE *f)
{
    guint8 c[4];
    int note, period;

    fread(c, 1, 4, f);

    period = ((c[0] & 0x0f) << 8) | c[1];
    note = 0;
    
    if(period) {
	for(note = 0; note < 60; note++)
	    if(period >= npertab[note])
		break;
	note++;
	if(note==61)
	    note=0;
    }

    dest->note = note ? note + 24 : 0;
    dest->instrument = (c[0] & 0xf0) | (c[2] >> 4);
    dest->volume = 0;
    dest->fxtype = c[2] & 0x0f;
    dest->fxparam = c[3];
}

static int
xm_load_mod_pattern (XMPattern *pat,
		     int num_channels,
		     FILE *f)
{
    int i, j, len;

    len = 64;

    pat->length = pat->alloc_length = len;

    if(!st_init_pattern_channels(pat, len, num_channels))
	return 0;

    /* Read channel data */
    for(j = 0; j < len; j++) {
	for(i = 0; i < num_channels; i++) {
	    xm_load_mod_note(&pat->channels[i][j], f);
	}
    }

    return 1;
}

static void
xm_init_locks (XM *xm)
{
    int i, j;

    for(i = 0; i < sizeof(xm->instruments) / sizeof(xm->instruments[0]); i++) {
	STInstrument *ins = &xm->instruments[i];
	for(j = 0; j < sizeof(ins->samples) / sizeof(ins->samples[0]); j++) {
	    ins->samples[j].sample.lock = g_mutex_new();
	}
    }
}

static XM *
xm_load_mod (FILE *f,int *status)
{
    XM *xm;
    guint8 sh[31][8];
    int i, n;
    guint8 mh[8];

    xm = calloc(1, sizeof(XM));
    if(!xm)
	goto fileerr;

    fread(xm->name, 1, 20, f);

    xm_init_locks(xm);

    for(i = 0; i < 31; i++) {
	char buf[25];
	fread(buf, 1, 22, f);
	buf[22] = 0;
	st_clean_instrument(&xm->instruments[i], buf);
	fread(sh[i], 1, 8, f);
    }

    fread(mh, 1, 2, f);
    xm->song_length = mh[0];

    fread(xm->pattern_order_table, 1, 128, f);
    fread(mh, 1, 4, f);

    *status = LFSTAT_IS_MODULE;
    if ((!memcmp("M.K.", mh, 4)) ||
        (!memcmp("M&K!", mh, 4)) ||
        (!memcmp("M!K!", mh, 4)))
    {
      // classic ProTracker 31-instrument Modules, 4 channels
      xm->num_channels = 4;
    }
    else if (!memcmp("FLT4", mh, 4))
    {
      // StarTrekker Module, 4 channels
      xm->num_channels = 4;
    }
    else if (!memcmp("CHN", (mh+1), 3))
    {
      // FastTracker 1.0 Module (Sign: xCHN), x channels
      xm->num_channels = mh[0] - 0x30;
    }
    else if (!memcmp("CH", (mh+2), 2))
    {
      // FastTracker 1.0 Module (Sign: xxCH), xx channels
      xm->num_channels = (mh[0] - 0x30) * 10 + (mh[1] - 0x30);
    }
    else
    {
        *status &= ~LFSTAT_IS_MODULE;  /* see notes in File_Load() about
                                          this status*/
	goto ende;
    }

    for(i = 0, n = 0; i < 128; i++) {
	if(xm->pattern_order_table[i] > n)
	    n = xm->pattern_order_table[i];
    }

    xm->tempo = 6;
    xm->bpm = 125;
    player_tempo = xm->tempo;
    player_bpm = xm->bpm;
    xm->flags = XM_FLAGS_IS_MOD | XM_FLAGS_AMIGA_FREQ;

    if(!xm_load_patterns(xm->patterns, n + 1, xm->num_channels, f, xm_load_mod_pattern)) {
	error_error(_("Error while loading patterns."));
	goto ende;
    }

    for(i = 0; i < 31; i++) {
	STSample *s = &xm->instruments[i].samples[0];

	s->sample.length = get_be_16(sh[i] + 0) << 1;

	if(s->sample.length > 0) {
	    s->finetune = (sh[i][2] & 0x0f) << 4;
	    s->volume = sh[i][3];
	    s->sample.loopstart = get_be_16(sh[i] + 4) << 1;
	    s->sample.loopend = s->sample.loopstart + (get_be_16(sh[i] + 6) << 1);
	    s->treat_as_8bit = TRUE;
	    s->panning = 128;
	    
	    if(get_be_16(sh[i] + 6) > 1)
		s->sample.looptype = ST_MIXER_SAMPLE_LOOPTYPE_AMIGA;
	    if(s->sample.loopend > s->sample.length)
		s->sample.loopend = s->sample.length;
	    if(s->sample.loopstart == s->sample.loopend) {
		s->sample.loopstart = 0;
		s->sample.loopend = 1;
		s->sample.looptype = 0;
	    } else if(s->sample.loopstart > s->sample.loopend) {
		char buf[128];
		sprintf(buf, "%d: Wrong loop start parameter. Don't know how to handle this. %04x %04x %04x\n", i, get_be_16(sh[i] + 0), get_be_16(sh[i] + 4), get_be_16(sh[i] + 6));
		error_warning(buf);
		s->sample.loopstart = 0;
		s->sample.loopend = 1;
		s->sample.looptype = 0;
	    }

	    s->sample.data = malloc(2 * s->sample.length);
	    if(!s->sample.data) {
		goto ende;
	    }
	    fread((char*)s->sample.data + s->sample.length, 1, s->sample.length, f);
	    st_convert_sample((char*)s->sample.data + s->sample.length,
			      s->sample.data,
			      8,
			      16,
			      s->sample.length);
	}
    }

    fclose(f);
    return xm;

  ende:
    XM_Free(xm);
  fileerr:
    fclose(f);
    return NULL;
}

XM *
XM_Load (const char *filename,int *status)
{
    XM *xm;
    FILE *f;
    guint8 xh[80];
    int i, j, num_patterns, num_instruments;

    *status = 0;
    f = fopen(filename, "rb");
    if(!f) {
        error_error(_("Can't open file"));
	return NULL;
    }

    memset(xh, 0, sizeof(xh));

    if(fread(xh + 0, 1, sizeof(xh), f) != sizeof(xh)
       || strncmp(xh + 0, "Extended Module: ", 17) != 0
       || xh[37] != 0x1a) {
	fseek(f, 0, SEEK_SET);
	return xm_load_mod(f,status);
    }

    if(get_le_32(xh + 60) != 276) {
	error_warning("XM header length != 276. Maybe a pre-0.0.12 SoundTracker module? :-)\n");
    }

    if(get_le_16(xh + 58) != 0x0104) {
	error_error("Version != 0x0104.");
	goto fileerr;
    }

    *status |= LFSTAT_IS_MODULE;  /* see notes in File_Load() about
                                     this status*/

    xm = calloc(1, sizeof(XM));
    if(!xm)
	goto fileerr;
    xm_init_locks(xm);

    strncpy(xm->name, xh + 17, 20);
    recode_ibmpc_to_latin1(xm->name, 20);
    xm->song_length = get_le_16(xh + 64);
    xm->restart_position = get_le_16(xh + 66);

    if(xm->restart_position >= xm->song_length) {
	xm->restart_position = xm->song_length - 1;
    }

    xm->num_channels = get_le_16(xh + 68);
    if(xm->num_channels > 32 || xm->num_channels < 1) {
	error_error("Invalid number of channels in XM (only 1..32 allowed).");
	goto ende;
    }

    num_patterns = get_le_16(xh + 70);
    num_instruments = get_le_16(xh + 72);
    if(get_le_16(xh + 74) != 1) {
	xm->flags |= XM_FLAGS_AMIGA_FREQ;
    }
    xm->tempo = get_le_16(xh + 76);
    xm->bpm = get_le_16(xh + 78);
    player_tempo = xm->tempo;
    player_bpm = xm->bpm;
    fread(xm->pattern_order_table, 1, 256, f);

    if(!xm_load_patterns(xm->patterns, num_patterns, xm->num_channels, f, xm_load_xm_pattern)) {
	error_error(_("Error while loading patterns."));
	goto ende;
    }
    
    for(i = 0; i < num_instruments; i++) {
	if(!xm_load_xm_instrument(&xm->instruments[i], f)) {
	    error_error(_("Error while loading instruments."));
	    goto ende;
	}
    }

    // Check if sample lengths are okay
    for(i = 0; i < num_instruments; i++) {
	STInstrument *instr = &xm->instruments[i];
	for(j = 0; j < (sizeof(instr->samples) / sizeof(instr->samples[0])); j++) {
	    if(instr->samples[j].sample.length > mixer->max_sample_length) {
		char buf[128];
		sprintf(buf, _("Module contains sample(s) that are too long for the current mixer.\nMaximum sample length is %d."), mixer->max_sample_length);
		error_warning(buf);
		goto weiter;
	    }
	}
    }

  weiter:
    if(xm->num_channels & 1) {
	/* Yes, mods like these *do* exist. */
	xm->num_channels++;
    }

    fclose(f);
    return xm;
    
  ende:
    XM_Free(xm);
  fileerr:
    fclose(f);
    return NULL;
}

int
XM_Save (XM *xm,
	 const char *filename,
	 gboolean song)
{
    FILE *f;
    int i;
    guint8 xh[80];
    int num_patterns, num_instruments;

    f = fopen(filename, "wb");
    if(!f)
	return 0;

    num_patterns = st_num_save_patterns(xm);
    num_instruments = st_num_save_instruments(xm);

    memcpy(xh + 0, "Extended Module: ", 17);
    memcpy(xh + 17, xm->name, 20);
    recode_latin1_to_ibmpc(xh + 17, 20);
    xh[37] = 0x1a;
    memcpy(xh + 38, "rst's SoundTracker  ", 20);
    put_le_16(xh + 58, 0x104);
    put_le_32(xh + 60, 276);
    put_le_16(xh + 64, xm->song_length);
    put_le_16(xh + 66, xm->restart_position);
    put_le_16(xh + 68, xm->num_channels);
    put_le_16(xh + 70, num_patterns);
    put_le_16(xh + 72, num_instruments);
    put_le_16(xh + 74, xm->flags & XM_FLAGS_AMIGA_FREQ ? 0 : 1);
    put_le_16(xh + 76, xm->tempo);
    put_le_16(xh + 78, xm->bpm);

    fwrite(&xh, 1, sizeof(xh), f);
    fwrite(xm->pattern_order_table, 1, 256, f);

    for(i = 0; i < num_patterns; i++)
	xm_save_xm_pattern(&xm->patterns[i], xm->num_channels, f);

    for(i = 0; i < num_instruments; i++)
        if(song==TRUE)
            xm_save_xm_instrument(&xm->instruments[i], f, FALSE);
        else
            xm_save_xm_instrument(&xm->instruments[i], f, TRUE);
   
    if(ferror(f)) {
	fclose(f);
	return 0;
    }
	
    fclose(f);
    return 1;
}

XM *
XM_New ()
{
    XM *xm;

    xm = calloc(1, sizeof(XM));
    if(!xm)
	goto ende;
    xm_init_locks(xm);

    xm->song_length = 1;
    xm->num_channels = 8;
    xm->tempo = 6;
    xm->bpm = 125;
    player_tempo = xm->tempo;
    player_bpm = xm->bpm;
    if(!xm_load_patterns(xm->patterns, 0, xm->num_channels, NULL, NULL))
	goto ende;

    return xm;

  ende:
    XM_Free(xm);
    return NULL;
}

void
XM_Free (XM *xm)
{
    int i, j;

    if(xm) {
	st_free_all_pattern_channels(xm);

	for(i = 0; i < sizeof(xm->instruments) / sizeof(xm->instruments[0]); i++) {
	    STInstrument *ins = &xm->instruments[i];
	    st_clean_instrument(ins, NULL);
	    for(j = 0; j < sizeof(ins->samples) / sizeof(ins->samples[0]); j++) {
		g_mutex_free(ins->samples[j].sample.lock);
	    }
	}

	free(xm);
    }
}

/**************************************************************************
* Mezzinane compression Handler/Loader
* coded by Jason Nunn <jsno@downunder.net.au>
* 7/3/2000
*
* Fixed to cope with weird filenames
* Rob Adamson <r.adamson@hotpop.com>
* 9-Dec-2000
*
* these routines handle modules that are compressed in the known formats-
* zip, gz, lha, bz2.
*
* for simple ones like gz and bz2, it just creates a temp file, and tells
* XM_Load() to load it before deleting the tmp file.
*
* For more complex formats like zip and lha, where they can store multiple
* files, it's an all-out affair- the compressed file is extracted, and
* each file is loaded by XM_Load(). The first file it comes across that
* can be successfully loaded is the one that's accepted as the mod file.
* After it has loaded a mod, it will remove the directory.
*
**************************************************************************/
char *err_msg = "Bzzzz, error extracting song, aborting operation.";


static char *st_tmpnam (char *tmp_path)
{
static char tname[1024], tfname[32];
char *tmpchr = "abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
int i;

    srand (time(NULL));

    strcpy (tname, prefs_get_prefsdir());
    strcat (tname, "/tmp/");
    sprintf (tfname, "st%d", getpid());
    strcat (tname, tfname);

    for(i=0; i < 16; i++)
        tfname[i] = tmpchr[(int) ((62.0 * rand()) / RAND_MAX)];
    tfname[i]='\0';

    strcat (tname, tfname);

	if (tmp_path != NULL)
		strcpy (tmp_path, tname);

	return tname;
}

static char *escape_filename(const char *filename)
{
    GString *escaped = g_string_new("");
    const char *s = filename;
    char *r;

    g_assert(filename != NULL);

    for (; *s; s ++) {
	if (strchr("\\\"'*?!# ()&|<>", *s)) {
	    g_string_append_c(escaped, '\\');
	}
	g_string_append_c(escaped, *s);
    }
    r = escaped->str;
    g_string_free(escaped, FALSE);

    return r;
}

/* Apparently one shouldn't use system() because it doesn't handle
   the child process getting interrupted. */
static int ExecuteAndWait(const char *command, const char **argv)
{
    int pid, status;

    g_assert(command != NULL);

    pid = fork();
    switch (pid) {
    case -1: return -1;
    case 0: {
	execv(command, (char **)argv);
	exit(127);
    }
    default:
	do {
	    if (waitpid(pid, &status, 0)==-1) {
		if (errno!=EINTR)
		    return -1;
	    } else 
		return status;
	} while (1);
    }
    return -1;
}


/*
 * this is for zip style archives, where compression format stores
 * multiple files
 */
static XM *
File_Extract_Archive (const char *extract_cmd,char *tmp_dir_path,int *status)
{
    XM *ret = NULL;
    register int r;
    DIR *dp;
    char str[256];
    const char *args[4] = {"sh", "-c", NULL, NULL};

    /*
     * extract archive, and store it in tmp directory
     */
    args[2] = extract_cmd;
    r = ExecuteAndWait ("/bin/sh", args);
    if ((r == -1) || (r == 127)) {
        g_snprintf (str, sizeof str, "%s (Err 0)",err_msg);
        error_error (str);
        return ret;
    }

    /*
     * open directory
     */
    dp = opendir (tmp_dir_path);
    if (dp == NULL) {
        g_snprintf (str, sizeof str, "%s (Err 1)",err_msg);
        error_error (str);
        return ret;
    }

    /*
     * read each file name. for each file, try to load it.
     * the first one that successfully loads- accept it.
     */
    for (;;) {
        struct dirent *ds;
        struct stat sp;

        ds = readdir (dp);
        if (ds == NULL)
            break;

        /*
         * if not a normal file, then ignore. if get a stat() error,
         * then don't make a fuss. it's no big deal.
         */
        g_snprintf (str, sizeof str, "%s/%s",tmp_dir_path,ds->d_name);
        if (stat (str,&sp) == 0)
            if (S_ISREG (sp.st_mode)) {
                ret = XM_Load (str,status);
                if (ret != NULL)
                    break;

            }
    }

    closedir(dp);

    /*
     * delete tmp directory
     */
    g_snprintf (str, sizeof str, "%s -rf %s",gui_settings.rm_path,tmp_dir_path);
    r = system (str);
    if ((r == -1) || (r == 127)) {
        g_snprintf (str, sizeof str, "%s (Err 2)",err_msg);
        error_error (str);
    }

    return ret;
}

/*
 * this is for zcat and bunzip2 style archives, where compression format
 * stores only one file
 */
static XM *
File_Extract_SingleFile (const char *extract_cmd,char *tmp_path,int *status)
{
    XM *ret = NULL;
    register int r;
    char str[256];
    const char *args[4] = {"sh", "-c", NULL, NULL};

    /*
     * extract archive- run cmd
     */
    args[2] = extract_cmd;
    r = ExecuteAndWait ("/bin/sh", args);
    if ((r == -1) || (r == 127)) {
        g_snprintf (str, sizeof str, "%s (Err 3)",err_msg);
        error_error (str);
        return ret;
    }

    ret = XM_Load (tmp_path,status);
    unlink (tmp_path); /*delete tmp file*/

    return ret;
}

/*
 * this tests a file extension. if it matches, return 1, if not return 0
 */
static int
f_extension_cmp (const char *filename,char *exten)
{
    register int a,b,fs,ks;

    fs = strlen (filename);
    ks = strlen (exten);
    if (fs < ks)
        return 0;

    for (a = fs - ks,b = 0;a < fs;a++) {
        if (exten[b++] == tolower (filename[a]))
            continue;

        return 0;
    }

    return 1;
}

XM *
File_Load (const char *filename)
{
    char tmp_path[256];
    char *str = NULL, *filename_esc = NULL;
    int status = 0;
    XM *ret = NULL;

    g_assert(filename != NULL);

    filename_esc = escape_filename(filename);
    st_tmpnam(tmp_path);

    /* test and load zip files. for unzip version 5.31 */
    if (f_extension_cmp (filename,".zip")) {
	str = g_strdup_printf("%s %s -d %s >>/dev/null", gui_settings.unzip_path, filename_esc, tmp_path);
        ret = File_Extract_Archive (str, tmp_path, &status);
    }

    /* test and load lha files. for UNIX V1.00   */
    else if (f_extension_cmp (filename,".lzh") ||
             f_extension_cmp (filename,".lha")) {
	str = g_strdup_printf("%s ew=%s %s >>/dev/null", gui_settings.lha_path, tmp_path, filename_esc);
        ret = File_Extract_Archive (str, tmp_path, &status);
    }

    /* for zcat 1.2.4 */
    else if (f_extension_cmp (filename,".gz")) {
	str = g_strdup_printf("%s %s >> %s", gui_settings.gz_path, filename_esc, tmp_path);
        ret = File_Extract_SingleFile (str, tmp_path, &status);
    }

    /* for bunzip2 Version 0.9.0b     */
    else if (f_extension_cmp (filename,".bz2")) {
	str = g_strdup_printf("%s -c %s >> %s", gui_settings.bz2_path, filename_esc, tmp_path);
        ret = File_Extract_SingleFile (str, tmp_path, &status);
    }

    /* if not compressed, load as normal   */
    else
      ret = XM_Load (filename,&status);

    /*
     * mike: we have this here instead of in xm_load_mod(), because it
     * can get called multiple times when a compressed file has many
     * files of unknown types. So, we have to have it here to prevent
     * lots of dialog boxes popping up with this message, even when a
     * mod file is eventually found in a compressed archive. -- jsno
     */
    if (!(status & LFSTAT_IS_MODULE))
        error_error (_("Not FastTracker XM and not supported MOD format!"));

    g_free(str);
    g_free(filename_esc);

    return ret;
}

gboolean
xm_xp_load_header (FILE *f, int *length)
{
    guint8 pheader[4];
    int version;
    
    if ( fread (pheader, 1, sizeof(pheader), f) != 4) {
	error_error (_("Error when file reading or unexpected end of file"));
	return FALSE;
    }
    if ((version = pheader[0] + (pheader[1] << 8)) != 1) {
	error_error (_("Incorrect or unsupported version of pattern file!"));
	return FALSE;
    }
    if (((*length = pheader[2] + (pheader[3] << 8)) < 0) || (*length > 256)){
	error_error (_("Incorrect pattern length!"));
	return FALSE;
    }
    return TRUE;
}

gboolean
xm_xp_load (FILE *f, int length, XMPattern *patt, XM *xm)
{
    static guint8 buf[32*256*5];
    int i, j, bp;
    
    if (fread (buf, 1, length * 32 * 5, f) != length*32*5) {
	error_error (_("Error when file reading or unexpected end of file"));
	return FALSE;
    }
    for (j = 0; j <= MIN (length, patt->length) - 1; j++) {
	bp = j * 5 * 32;
	for (i = 0; i <= xm->num_channels - 1; i++) {
	    memcpy (&patt->channels[i][j].note, &buf[bp], 5);
	    bp += 5;
	}
    }
    if (length < patt->length)
	for (j = length; j < patt->length; j++) {
	    bp = j * 5 * 32;
	    for (i = 0; i <= xm->num_channels - 1; i++) {
		memset (&patt->channels[i][j].note, 0, 5);
		bp += 5;
	    }
	}
    return TRUE;
}

void
xm_xp_save (gint reply, gpointer data)
{
    struct f_n_p *fnp = (struct f_n_p*) data;

    FILE *f;
    int i, j, bp;
    guint8 pheader[4];
    static guint8 buf[32*256*5];
    
    if (reply == 0){
	if ( !(f = fopen (fnp->name, "wb"))) 
	    error_error (_("Error during saving pattern!"));
	else {
	    put_le_16 (pheader+0, 01);//version
	    put_le_16 (pheader+2, fnp->pattern->length);//length
	    
	    bp = 0;
	    for (j = 0; j <= fnp->pattern->length - 1; j++)//row
		for (i = 0; i <= 31; i++){//ch
		    if ( i <= fnp->xm->num_channels - 1){
			buf[bp + 0] = fnp->pattern->channels[i][j].note;
			buf[bp + 1] = fnp->pattern->channels[i][j].instrument;
			buf[bp + 2] = fnp->pattern->channels[i][j].volume;
			buf[bp + 3] = fnp->pattern->channels[i][j].fxtype;
			buf[bp + 4] = fnp->pattern->channels[i][j].fxparam;
		    } else {
			buf[bp + 0] = 0;
			buf[bp + 1] = 0;
			buf[bp + 2] = 0;
			buf[bp + 3] = 0;
			buf[bp + 4] = 0;
		    }
		    bp += 5;
		}
	    
	    fwrite (pheader, 1, sizeof(pheader), f);
	    fwrite (buf, 1, bp, f);
	    fclose (f);
	}
    }
}
