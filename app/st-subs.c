
/*
 * The Real SoundTracker - General support routines
 *
 * Copyright (C) 1998-2001 Michael Krause
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

#include <pthread.h>

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "st-subs.h"
#include "xm.h"
#include "gui-settings.h"

int
st_init_pattern_channels (XMPattern *p,
			  unsigned length,
			  int num_channels)
{
    int i;
    
    p->length = p->alloc_length = length;
    for(i = 0; i < num_channels; i++) {
	if(!(p->channels[i] = calloc(1, length * sizeof(XMNote)))) {
	    st_free_pattern_channels(p);
	    return 0;
	}
    }
	    
    return 1;
}

void
st_free_pattern_channels (XMPattern *pat)
{
    int i;
    
    for(i = 0; i < 32; i++) {
	free(pat->channels[i]);
	pat->channels[i] = NULL;
    }
}

void
st_free_all_pattern_channels (XM *xm)
{
    int i;
    
    for(i = 0; i < 256; i++)
	st_free_pattern_channels(&xm->patterns[i]);
}

int
st_copy_pattern (XMPattern *dst,
		 XMPattern *src)
{
    XMNote *oldchans[32];
    int i;

    for(i = 0; i < 32; i++) {
	oldchans[i] = dst->channels[i];
	if(src->channels[i]) {
	    if(!(dst->channels[i] = st_dup_track(src->channels[i], src->length))) {
		// Out of memory, restore previous pattern, return error
		for(; i >= 0; i--) {
		    free(dst->channels[i]);
		    dst->channels[i] = oldchans[i];
		}
		return 0;
	    }
	} else {
	    dst->channels[i] = NULL;
	}
    }

    for(i = 0; i < 32; i++)
	free(oldchans[i]);

    dst->length = dst->alloc_length = src->length;

    return 1;
}

XMPattern *
st_dup_pattern (XMPattern *p)
{
    XMPattern *r;

    r = g_new0(XMPattern, 1);
    if(!r)
	return NULL;

    if(!st_copy_pattern(r, p)) {
	g_free(r);
	return NULL;
    }

    return r;
}

XMNote *
st_dup_track (XMNote *n,
	      int length)
{
    XMNote *r;

    if(!n)
	return NULL;

    r = malloc(length * sizeof(XMNote));
    if(r) {
	memcpy(r, n, length * sizeof(XMNote));
    }

    return r;
}

/* Duplicate part of a track, wrap-around at end is handled */
XMNote *
st_dup_track_wrap (XMNote *n,
		   int tracklength,
		   int copystart,
		   int copylength)
{
    XMNote *r;

    if(!n || copylength > tracklength || copystart >= tracklength)
	return NULL;

    if(copylength > tracklength - copystart) {
	r = malloc(copylength * sizeof(XMNote));
	if(r) {
	    memcpy(r, n + copystart, (tracklength - copystart) * sizeof(XMNote));
	    memcpy(r + tracklength - copystart, n, (copylength - (tracklength - copystart)) * sizeof(XMNote));
	}
    } else {
	r = st_dup_track(n + copystart, copylength);
    }

    return r;
}

void
st_clear_track (XMNote *n,
		int length)
{
    if(!n)
	return;

    memset(n, 0, length * sizeof(*n));
}

/* Clear part of a track, wrap-around at end is handled */
void
st_clear_track_wrap (XMNote *n,
		     int tracklength,
		     int clearstart,
		     int clearlength)
{
    if(!n || clearlength > tracklength || clearstart >= tracklength)
	return;

    if(clearlength > tracklength - clearstart) {
	st_clear_track(n + clearstart, tracklength - clearstart);
	st_clear_track(n, clearlength - (tracklength - clearstart));
    } else {
	st_clear_track(n + clearstart, clearlength);
    }
}

void
st_paste_track_into_track_wrap (XMNote *from,
				XMNote *to,
				int tolength,
				int insertstart,
				int fromlength)
{
    if(!from || !to || tolength < fromlength || insertstart >= tolength)
	return;

    if(fromlength > tolength - insertstart) {
	memcpy(to + insertstart, from, (tolength - insertstart) * sizeof(XMNote));
	memcpy(to, from + tolength - insertstart, (fromlength - (tolength - insertstart)) * sizeof(XMNote));
    } else {
	memcpy(to + insertstart, from, fromlength * sizeof(XMNote));
    }
}

void
st_clear_pattern (XMPattern *p)
{
    int i;

    for(i = 0; i < 32; i++) {
	st_clear_track(p->channels[i], p->alloc_length);
    }
}

void
st_pattern_delete_track (XMPattern *p,
			 int t)
{
    int i;
    XMNote *a;

    a = p->channels[t];
    g_assert(a != NULL);

    for(i = t; i < 31 && p->channels[i + 1] != NULL; i++) {
	p->channels[i] = p->channels[i + 1];
    }

    p->channels[i] = a;
    st_clear_track(a, p->alloc_length);
}

void
st_pattern_insert_track (XMPattern *p,
			 int t)
{
    int i;
    XMNote *a;
    
    a = p->channels[31];

    for(i = 31; i > t; i--) {
	p->channels[i] = p->channels[i - 1];
    }

    if(!a) {
	if(!(a = calloc(1, p->alloc_length * sizeof(XMNote)))) {
	    g_assert_not_reached();
	}
    }

    p->channels[t] = a;
    st_clear_track(a, p->alloc_length);
}

gboolean
st_instrument_used_in_song (XM *xm,
			    int instr)
{
    int i, j, k;
    XMPattern *p;
    XMNote *c;

    for(i = 0; i < xm->song_length; i++) {
	p = &xm->patterns[(int)xm->pattern_order_table[i]];
	for(j = 0; j < xm->num_channels; j++) {
	    c = p->channels[j];
	    for(k = 0; k < p->length; k++) {
		if(c[k].instrument == instr)
		    return TRUE;
	    }
	}
    }

    return FALSE;
}

int
st_instrument_num_samples (STInstrument *instr)
{
    int i, n;

    for(i = 0, n = 0; i < sizeof(instr->samples) / sizeof(instr->samples[0]); i++) {
	if(instr->samples[i].sample.length != 0)
	    n++;
    }
    return n;
}

int
st_instrument_num_save_samples (STInstrument *instr)
{
    int i, n;

    for(i = 0, n = 0; i < sizeof(instr->samples) / sizeof(instr->samples[0]); i++) {
	if(instr->samples[i].sample.length != 0 || instr->samples[i].name[0] != 0)
	    n = i + 1;
    }
    return n;
}

int
st_num_save_instruments (XM *xm)
{
    int i, n;

    for(i = 0, n = 0; i < 128; i++) {
	if(st_instrument_num_save_samples(&xm->instruments[i]) != 0 || xm->instruments[i].name[0] != 0)
	    n = i + 1;
    }
    return n;
}

int
st_num_save_patterns (XM *xm)
{
    int i, n;

    for(i = 0, n = 0; i < 256; i++) {
	if(!st_is_empty_pattern(&xm->patterns[i]))
	    n = i;
    }

    return n + 1;
}

void
st_copy_instrument (STInstrument *src, STInstrument *dest)
{
    int i;
    guint32 length;
    GMutex *lock[16];

    st_clean_instrument(dest, NULL);

    for(i = 0; i < sizeof(dest->samples) / sizeof(dest->samples[0]); i++)
	lock[i] = dest->samples[i].sample.lock; // Preserve pointers to GMutex'es from modification
    memcpy(dest, src, sizeof(STInstrument));
    for(i = 0; i < sizeof(src->samples) / sizeof(src->samples[0]); i++){
	if ((length = dest->samples[i].sample.length * sizeof(dest->samples[i].sample.data[0]))){
	    dest->samples[i].sample.data = malloc(length);
	    memcpy(dest->samples[i].sample.data, src->samples[i].sample.data, length);
	}
	dest->samples[i].sample.lock = lock[i];
    }
}

void
st_clean_instrument (STInstrument *instr,
		     const char *name)
{
    int i;

    for(i = 0; i < sizeof(instr->samples) / sizeof(instr->samples[0]); i++)
	st_clean_sample(&instr->samples[i], NULL);

    if(!name)
	memset(instr->name, 0, sizeof(instr->name));
    else
	strncpy(instr->name, name, 22);
    memset(instr->samplemap, 0, sizeof(instr->samplemap));
    memset(&instr->vol_env, 0, sizeof(instr->vol_env));
    memset(&instr->pan_env, 0, sizeof(instr->pan_env));
    instr->vol_env.num_points = 1;
    instr->vol_env.points[0].val = 64;
    instr->pan_env.num_points = 1;
    instr->pan_env.points[0].val = 32;
}

void
st_clean_sample (STSample *s,
		 const char *name)
{
    GMutex *lock = s->sample.lock;
    free(s->sample.data);
    memset(s, 0, sizeof(STSample));
    if(name)
	strncpy(s->name, name, 22);
    s->sample.loopend = 1;
    if(lock)
	s->sample.lock = lock;
    else
	s->sample.lock = g_mutex_new();
}

void
st_clean_song (XM *xm)
{
    int i;

    st_free_all_pattern_channels(xm);

    xm->song_length = 1;
    xm->restart_position = 0;
    xm->num_channels = 8;
    xm->tempo = 6;
    xm->bpm = 125;
    xm->flags = 0;
    memset(xm->pattern_order_table, 0, sizeof(xm->pattern_order_table));

    for(i = 0; i < 256; i++)
	st_init_pattern_channels(&xm->patterns[i], 64, xm->num_channels);
}

void
st_set_num_channels (XM *xm,
		     int n)
{
    int i, j;
    XMPattern *pat;

    for(i = 0; i < sizeof(xm->patterns) / sizeof(xm->patterns[0]); i++) {
	pat = &xm->patterns[i];
	for(j = 0; j < n; j++) {
	    if(!pat->channels[j]) {
		pat->channels[j] = calloc(1, sizeof(XMNote) * pat->alloc_length);
	    }
	}
    }

    xm->num_channels = n;
}

void
st_set_pattern_length (XMPattern *pat,
		       int l)
{
    int i;
    XMNote *n;

    if(l > pat->alloc_length) {
	for(i = 0; i < 32 && pat->channels[i] != NULL; i++) {
	    n = calloc(1, sizeof(XMNote) * l);
	    memcpy(n, pat->channels[i], sizeof(XMNote) * pat->length);
	    free(pat->channels[i]);
	    pat->channels[i] = n;
	}
	pat->alloc_length = l;
    }

    pat->length = l;
}

void
st_sample_fix_loop (STSample *sts)
{
    st_mixer_sample_info *s = &sts->sample;

    if(s->loopend > s->length)
	s->loopend = s->length;
    if(s->loopstart >= s->loopend)
	s->loopstart = s->loopend - 1;
}

int
st_find_first_unused_and_empty_pattern (XM *xm)
{
    int i;

    for(i = 0; i < sizeof(xm->patterns) / sizeof(xm->patterns[0]); i++) {
	if(!st_is_pattern_used_in_song(xm, i) && st_is_empty_pattern(&xm->patterns[i])) {
	    return i;
	}
    }

    return -1;
}

gboolean
st_is_empty_pattern (XMPattern *p)
{
    int i;
    
    for(i = 0; i < 32; i++) {
	if(p->channels[i]) {
	    if(!st_is_empty_track(p->channels[i], p->length)) {
		return 0;
	    }
	}
    }

    return 1;
}

gboolean
st_is_empty_track (XMNote *notes,
		   int length)
{
    for(; length > 0; length--, notes++) {
	if(notes->note != 0 || notes->instrument != 0 || notes->volume != 0
	   || notes->fxtype != 0 || notes->fxparam != 0) {
	    return 0;
	}
    }

    return 1;
}
		   
gboolean
st_is_pattern_used_in_song (XM *xm,
			    int patnum)
{
    int i;

    for(i = 0; i < xm->song_length; i++)
	if(xm->pattern_order_table[i] == patnum)
	    return 1;
    
    return 0;
}

void
st_exchange_patterns (XM *xm,
		      int p1,
		      int p2)
{
    XMPattern tmp;
    int i;

    memcpy(&tmp, &xm->patterns[p1], sizeof(XMPattern));
    memcpy(&xm->patterns[p1], &xm->patterns[p2], sizeof(XMPattern));
    memcpy(&xm->patterns[p2], &tmp, sizeof(XMPattern));

    for(i = 0; i < xm->song_length; i++) {
	if(xm->pattern_order_table[i] == p1)
	    xm->pattern_order_table[i] = p2;
	else if(xm->pattern_order_table[i] == p2)
	    xm->pattern_order_table[i] = p1;
    }
}

gboolean
st_check_if_odd_are_not_empty (XMPattern *p)
{
    int i, j;

    for(i = 0; i < 32 && p->channels[i]; i++)
	for(j = 1; j < p->length; j += 2)
	    if((p->channels[i][j].note && p->channels[i][j].instrument) ||
	       (p->channels[i][j].volume > 15) ||
	        p->channels[i][j].fxtype || p->channels[i][j].fxparam)
		return TRUE;

    return FALSE;
}

void
st_shrink_pattern (XMPattern *p)
{
    int i, j, length = p->length;
    
    for(i = 0; i < 32 && p->channels[i]; i++){
	for(j = 1; j <= (length - 1) / 2; j++)
	    memcpy(&p->channels[i][j], &p->channels[i][2 * j], sizeof(XMNote));
	for(;j < p->alloc_length; j++){/* clear the rest of the pattern */
	    p->channels[i][j].note = 0;
	    p->channels[i][j].instrument = 0;
	    p->channels[i][j].volume = 0;
	    p->channels[i][j].fxtype = 0;
	    p->channels[i][j].fxparam = 0;
	}
    }
    
    st_set_pattern_length(p, (length - 1) / 2 + 1);
}

void
st_expand_pattern (XMPattern *p)
{
    int i, j, length = MIN(p->length * 2, 256);

    st_set_pattern_length(p, length);

    for(i = 0; i < 32 && p->channels[i]; i++){
	for(j = length / 2 - 1; j >= 0; j--){
	    /* copy to even positions and clear odd */
	    memcpy(&p->channels[i][2 * j], &p->channels[i][j], sizeof(XMNote));
	    p->channels[i][2 * j + 1].note = 0;
	    p->channels[i][2 * j + 1].instrument = 0;
	    p->channels[i][2 * j + 1].volume = 0;
	    p->channels[i][2 * j + 1].fxtype = 0;
	    p->channels[i][2 * j + 1].fxparam = 0;
	}
    }
}

void
st_convert_sample (void *src,
		   void *dst,
		   int srcformat,
		   int dstformat,
		   int count)
{
    gint16 *d16;
    gint8 *d8;

    if(srcformat == dstformat) {
	memcpy(dst, src, count * (srcformat / 8));
    } else {
	if(dstformat == 16) {
	    /* convert to 16 bit */
	    d16 = dst;
	    d8 = src;
	    while(count--)
		*d16++ = (*d8++ << 8);
	} else {
	    /* convert to 8 bit */
	    d8 = dst;
	    d16 = src;
	    while(count--)
		*d8++ = (*d16++ >> 8);
	}
    }
}

void
st_sample_cutoff_lowest_8_bits (gint16 *data,
				int count)
{
    while(count) {
	gint16 d = *data;
	d &= 0xff00;
	*data++ = d;
	count--;
    }
}

void
st_sample_8bit_signed_unsigned (gint8 *data,
				int count)
{
    while(count) {
	*data = *data++ + 128;
	count--;
    }
}

void
st_sample_16bit_signed_unsigned (gint16 *data,
				 int count)
{
    while(count) {
	*data = *data++ + 32768;
	count--;
    }
}

