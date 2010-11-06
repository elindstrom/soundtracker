
/*
 * The Real SoundTracker - Cubically interpolating mixing routines
 *                         with IT style filter support
 *
 *                         Very unoptimized portable C version.
 *
 * Copyright (C) 2001 Michael Krause
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

#include <config.h>

#include "kb-x86-asm.h"

#if defined(NO_ASM) || defined(NO_GASP) || !defined(__i386__)

gboolean
kbasm_post_mixing (float *tempbuf,
		   gint16 *outbuf,
		   unsigned n,
		   float amp)
{
    gboolean clipped = FALSE;

    n *= 2;

    while(n--) {
	float a = *tempbuf++ * amp;
	if(a < -32768.0) {
	    a = -32768.0;
	    clipped = TRUE;
	}
	if(a > 32767.0) {
	    a = 32767.0;
	    clipped = TRUE;
	}
	*outbuf ++= (gint16)a;
    }

    return clipped;
}

#define CUBICMIXER_COMMON_HEAD \
    gint16 *positioni = data->positioni; \
    guint32 positionf = data->positionf; \
    float *mixbuffer = data->mixbuffer;  \
    float fl1 = data->fl1;               \
    float fb1 = data->fb1;               \
    float voll = data->volleft;          \
    float volr = data->volright;         \
    unsigned n = data->numsamples;

#define CUBICMIXER_COMMON_LOOP_START \
    while(n--) { \
        float s0; \
        guint32 positionf_new;

#define CUBICMIXER_LOOP_FORWARD \
        s0 = positioni[0] * kb_x86_ct0[positionf >> 24]; \
	s0 += positioni[1] * kb_x86_ct1[positionf >> 24]; \
	s0 += positioni[2] * kb_x86_ct2[positionf >> 24]; \
	s0 += positioni[3] * kb_x86_ct3[positionf >> 24];
        
#define CUBICMIXER_LOOP_BACKWARD \
        s0 = positioni[0] * kb_x86_ct0[-positionf >> 24]; \
	s0 += positioni[-1] * kb_x86_ct1[-positionf >> 24]; \
	s0 += positioni[-2] * kb_x86_ct2[-positionf >> 24]; \
	s0 += positioni[-3] * kb_x86_ct3[-positionf >> 24];

#define CUBICMIXER_ADVANCE_POINTER \
	positionf_new = positionf + data->freqf; \
	if(positionf_new < positionf) { \
	    positioni++; \
	} \
	positionf = positionf_new; \
	positioni += data->freqi;

#define CUBICMIXER_FILTER \
        fb1 = data->freso * fb1 + data->ffreq * (s0 - fl1); \
        fl1 += data->ffreq * fb1; \
        s0 = fl1; 

#define CUBICMIXER_SCOPES \
        *scopebuf++ = (gint16)(s0 * (voll + volr));

#define CUBICMIXER_WRITE_OUT \
	*mixbuffer++ += s0 * voll; \
	*mixbuffer++ += s0 * volr;

#define CUBICMIXER_VOLRAMP \
        voll += data->volrampl; \
        volr += data->volrampr;
        
#define CUBICMIXER_COMMON_FOOT \
    data->volleft = voll;                \
    data->volright = volr;               \
    data->positioni = positioni;         \
    data->positionf = positionf;         \
    data->mixbuffer = mixbuffer;         \
    data->fl1 = fl1;                     \
    data->fb1 = fb1;

/* --- 0 --- */
static void
kbfloat_mix_cubic_noscopes_unfiltered_forward_noramp (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_backward_noramp (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_forward_noramp (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_backward_noramp (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT
    }

    CUBICMIXER_COMMON_FOOT
}

/* --- 4 --- */
static void
kbfloat_mix_cubic_scopes_unfiltered_forward_noramp (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD
    gint16 *scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_SCOPES
    CUBICMIXER_WRITE_OUT
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_unfiltered_backward_noramp (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD
    gint16 *scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_SCOPES
    CUBICMIXER_WRITE_OUT
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_forward_noramp (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD
    gint16 *scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_SCOPES
    CUBICMIXER_WRITE_OUT
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_backward_noramp (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD
    gint16 *scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_SCOPES
    CUBICMIXER_WRITE_OUT
    }

    CUBICMIXER_COMMON_FOOT
}

/* --- 8 --- */
static void
kbfloat_mix_cubic_noscopes_unfiltered_forward (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_VOLRAMP
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_backward (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_VOLRAMP
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_forward (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_VOLRAMP
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_backward (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_VOLRAMP
    }

    CUBICMIXER_COMMON_FOOT
}

/* --- 12 --- */
static void
kbfloat_mix_cubic_scopes_unfiltered_forward (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD
    gint16 *scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_SCOPES
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_VOLRAMP
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_unfiltered_backward (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD
    gint16 *scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_SCOPES
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_VOLRAMP
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_forward (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD
    gint16 *scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_SCOPES
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_VOLRAMP
    }

    CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_backward (kb_x86_mixer_data *data)
{
    CUBICMIXER_COMMON_HEAD
    gint16 *scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_SCOPES
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_VOLRAMP
    }

    CUBICMIXER_COMMON_FOOT
}

static void (*kbfloat_mixers[16])(kb_x86_mixer_data *) = {
    kbfloat_mix_cubic_noscopes_unfiltered_forward_noramp,
    kbfloat_mix_cubic_noscopes_unfiltered_backward_noramp,
    kbfloat_mix_cubic_noscopes_filtered_forward_noramp,
    kbfloat_mix_cubic_noscopes_filtered_backward_noramp,
    kbfloat_mix_cubic_scopes_unfiltered_forward_noramp,
    kbfloat_mix_cubic_scopes_unfiltered_backward_noramp,
    kbfloat_mix_cubic_scopes_filtered_forward_noramp,
    kbfloat_mix_cubic_scopes_filtered_backward_noramp,
    kbfloat_mix_cubic_noscopes_unfiltered_forward,
    kbfloat_mix_cubic_noscopes_unfiltered_backward,
    kbfloat_mix_cubic_noscopes_filtered_forward,
    kbfloat_mix_cubic_noscopes_filtered_backward,
    kbfloat_mix_cubic_scopes_unfiltered_forward,
    kbfloat_mix_cubic_scopes_unfiltered_backward,
    kbfloat_mix_cubic_scopes_filtered_forward,
    kbfloat_mix_cubic_scopes_filtered_backward
};

void
kbasm_mix (kb_x86_mixer_data *data)
{
    kbfloat_mixers[data->flags >> 2](data);
}

#endif
