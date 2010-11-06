
/*
 * The Real SoundTracker - XM support routines
 *
 * Copyright (C) 1998-2001 Michael Krause
 *
 * This file is based on the "maube" 0.10.4 source code, Copyright (C)
 * 1997 by Conrad Parker.
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

#ifndef _XM_H
#define _XM_H

#include <stdio.h>

#include <glib.h>

#include "mixer.h"

/* -- XM definitions -- */

#define XM_PATTERN_NOTE_MIN  0
#define XM_PATTERN_NOTE_MAX 95
#define XM_PATTERN_NOTE_OFF 97

#define XM_NOTE_VOLUME_MIN 0x10
#define XM_NOTE_VOLUME_MAX 0x50

#define PITCH_NOTE        (16 * 4 * 4)
#define PITCH_OCTAVE      (12 * PITCH_NOTE)

typedef struct XMNote {
    unsigned char note;
    unsigned char instrument;
    unsigned char volume;
    unsigned char fxtype;
    unsigned char fxparam;
} XMNote;

typedef struct XMPattern {
    int length, alloc_length;
    XMNote *channels[32];
} XMPattern;

/* -- Sample definitions -- */

typedef struct STSample {
    st_mixer_sample_info sample;

    char name[23];

    guint8 volume;               /* eigenvolume (0..64) */
    gint8 finetune;              /* finetune (-128 ... 127) */
    guint8 panning;              /* panning (0 ... 255) */
    gint8 relnote;

    gboolean treat_as_8bit;
} STSample;

/* -- Instrument definitions -- */

/* values for STEnvelope.flags */
#define EF_ON           1
#define EF_SUSTAIN      2
#define EF_LOOP         4

#define ST_MAX_ENVELOPE_POINTS 12

typedef struct STEnvelopePoint {
    guint16 pos;
    guint16 val;
} STEnvelopePoint;

typedef struct STEnvelope {
    STEnvelopePoint points[ST_MAX_ENVELOPE_POINTS];
    unsigned char num_points;
    unsigned char sustain_point;
    unsigned char loop_start;
    unsigned char loop_end;
    unsigned char flags;
} STEnvelope;

typedef struct STInstrument {
    char name[24];

    STEnvelope vol_env;
    STEnvelope pan_env;

    guint8 vibtype;
    guint16 vibrate;
    guint16 vibdepth;
    guint16 vibsweep;

    guint16 volfade;

    gint8 samplemap[96];
    STSample samples[16];
} STInstrument;

/* That the following structure is called 'XM' is a relic from old
   times, when SoundTracker still loaded the XM file into memory as
   is; nowadays it is ripped apart and stored in much saner
   structures, as you can see... */

typedef struct XM {
    char name[21];
    char modified;                   // indicates necessity of security questions before quitting etc.

    int flags;                       /* see XM_FLAGS_ defines below */
    int num_channels;
    int tempo;
    int bpm;


    int song_length;
    int restart_position;
    guint8 pattern_order_table[256];

    XMPattern patterns[256];
    STInstrument instruments[128];
} XM;

struct f_n_p {
    gchar *name;
    XMPattern *pattern;
    XM *xm;
};

#define XM_FLAGS_AMIGA_FREQ               1
#define XM_FLAGS_IS_MOD                   2

XM*           File_Load                                (const char *filename);
XM*           XM_Load                                  (const char *filename,int *status);
int           XM_Save                                  (XM *xm, const char *filename, gboolean song);
XM*           XM_New                                   (void);
void          XM_Free                                  (XM*);

gboolean      xm_load_xi                               (STInstrument *instr,
							FILE *f);
gboolean      xm_save_xi                               (STInstrument *instr,
							FILE *f);
gboolean      xm_xp_load_header			       (FILE *f, int *length);
gboolean      xm_xp_load			       (FILE *f, int length,
							XMPattern *patt, XM *xm);
void          xm_xp_save			       (gint reply, gpointer data);

static inline int
xm_get_modified (void)
{
    extern XM *xm;
    if(xm != NULL) {
	return xm->modified;
    } else {
	return 0;
    }
}

static inline void
xm_set_modified (int val)
{
    extern XM *xm;
    if(xm != NULL) {
	xm->modified = val;
    }
}

void
xm_freq_note_to_relnote_finetune (float frequency,
				  unsigned note,
				  gint8 *relnote,
				  gint8 *finetune);

#endif /* _XM_H */
