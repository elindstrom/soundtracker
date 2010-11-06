
/*
 * The Real SoundTracker - Mixer module definitions
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

#ifndef _ST_MIXER_H
#define _ST_MIXER_H

#include <glib.h>

typedef struct st_mixer_sample_info {
    guint32 looptype;     /* see ST_MIXER_SAMPLE_LOOPTYPE_ defines below */
    guint32 length;       /* length in samples, not in bytes */
    guint32 loopstart;    /* offset in samples, not in bytes */
    guint32 loopend;      /* offset to first sample not being played */
    gint16 *data;         /* pointer to sample data */
    GMutex *lock;
} st_mixer_sample_info;

/* values for st_mixer_sample_info.looptype */
#define ST_MIXER_SAMPLE_LOOPTYPE_NONE      0
#define ST_MIXER_SAMPLE_LOOPTYPE_AMIGA     1
#define ST_MIXER_SAMPLE_LOOPTYPE_PINGPONG  2

typedef struct st_mixer_channel_status {
    st_mixer_sample_info *current_sample;
    guint32 current_position;
} st_mixer_channel_status;

typedef struct st_mixer {
    const char *id;
    const char *description;

    /* set number of channels to be mixed */
    void     (*setnumch)     (int numchannels);

    /* notify sample update (sample must be locked by caller!) */
    void     (*updatesample) (st_mixer_sample_info *si);

    /* set mixer output format -- signed 16 or 8 (in machine endianness) */
    gboolean (*setmixformat) (int format);

    /* toggle stereo mixing -- interleaved left / right samples */
    gboolean (*setstereo)    (int on);

    /* set mixing frequency */
    void     (*setmixfreq)   (guint16 frequency);

    /* set final amplification factor (0.0 = mute ... 1.0 = normal ... +inf = REAL LOUD! :D)*/
    void     (*setampfactor) (float amplification);

    /* returns true if last mix() call had to clip the signal */
    gboolean (*getclipflag)  (void);

    /* reset internal playing state */
    void     (*reset)        (void);

    /* play sample from the beginning, initialize nothing else */
    void     (*startnote)    (int channel, st_mixer_sample_info *si);

    /* stop note */
    void     (*stopnote)     (int channel);

    /* set curent sample play position */
    void     (*setsmplpos)   (int channel, guint32 offset);

    /* set curent sample play end position */
    void     (*setsmplend)   (int channel, guint32 offset);

    /* set replay frequency (Hz) */
    void     (*setfreq)      (int channel, float frequency);

    /* set sample volume (0.0 ... 1.0) */
    void     (*setvolume)    (int channel, float volume);

    /* set sample panning (-1.0 ... +1.0) */
    void     (*setpanning)   (int channel, float panning);

    /* set channel filter cutoff frequency (-1.0 for off, or 0.0 ... +1.0) */
    void     (*setchcutoff)  (int channel, float freq);

    /* set channel filter resonance (0.0 ... +1.0) */
    void     (*setchreso)    (int channel, float reso);

    /* do the mix, return pointer to end of dest */
    void*    (*mix)          (void *dest, guint32 count, gint16 *scopebufs[], int scopebuf_offset);

    /* get status information */
    void     (*dumpstatus)   (st_mixer_channel_status array[]);
    
    /* load channel settings from tracer */
    void     (*loadchsettings) (int channel);

    guint32 max_sample_length;

    struct st_mixer *next;
} st_mixer;

typedef enum {
    ST_MIXER_FORMAT_S16_LE = 1,
    ST_MIXER_FORMAT_S16_BE,
    ST_MIXER_FORMAT_S8,
    ST_MIXER_FORMAT_U16_LE,
    ST_MIXER_FORMAT_U16_BE,
    ST_MIXER_FORMAT_U8,
    ST_MIXER_FORMAT_STEREO = 16,
} STMixerFormat;

#endif /* _MIXER_H */
