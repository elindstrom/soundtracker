
/*
 * The Real SoundTracker - Pseudo-mixer for sample playback parameter tracing (header)
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

#ifndef _TRACER_H
#define _TRACER_H

#include "mixer.h"

typedef struct tracer_channel {
    st_mixer_sample_info *sample;

    void *data;                   // for updatesample() to see if sample has changed
    int looptype;
    guint32 length;

    guint32 flags;                // see below
    float volume;                 // 0.0 ... 1.0
    float panning;                // 0.0 ... 1.0
    int direction;                // +1 for forward, -1 for backward
    guint32 playend;              // for a forced premature end of the sample

    guint32 positionw;            // current sample position (whole part of 32.32)
    guint32 positionf;            // current sample position (fractional part of 32.32)
    guint32 freqw;                // frequency (whole part of 32.32)
    guint32 freqf;                // frequency (fractional part of 32.32)

    float ffreq;                  // filter frequency (0<=x<=1)
    float freso;                  // filter resonance (0<=x<1)
} tracer_channel;

#define TR_FLAG_LOOP_UNIDIRECTIONAL       1
#define TR_FLAG_LOOP_BIDIRECTIONAL        2
#define TR_FLAG_SAMPLE_RUNNING            4

void			tracer_trace			(int mixfreq, int songpos, int patpos);
tracer_channel*		tracer_return_channel		(int number);
void			tracer_setnumch			(int n);
#endif
