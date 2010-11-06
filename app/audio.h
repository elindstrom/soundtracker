
/*
 * The Real SoundTracker - Audio handling thread (header)
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

#ifndef _ST_AUDIO_H
#define _ST_AUDIO_H

#include <sys/time.h>

#include <glib.h>

#include "mixer.h"
#include "driver-inout.h"
#include "time-buffer.h"
#include "event-waiter.h"

/* === Thread communication stuff */

typedef enum audio_ctlpipe_id {
    AUDIO_CTLPIPE_INIT_PLAYER=2000,    /* void */
    AUDIO_CTLPIPE_RENDER_SONG_TO_FILE, /* int len, string (len+1 bytes) */
    AUDIO_CTLPIPE_PLAY_SONG,           /* int songpos, int patpos */
    AUDIO_CTLPIPE_PLAY_PATTERN,        /* int pattern, int patpos, int only_one_row */
    AUDIO_CTLPIPE_PLAY_NOTE,           /* int channel, int note, int instrument */
    AUDIO_CTLPIPE_PLAY_NOTE_FULL,      /* int channel, int note, STSample *sample, int offset, int count */
    AUDIO_CTLPIPE_PLAY_NOTE_KEYOFF,    /* int channel */
    AUDIO_CTLPIPE_STOP_PLAYING,        /* void */
    AUDIO_CTLPIPE_SET_SONGPOS,         /* int songpos */
    AUDIO_CTLPIPE_SET_PATTERN,         /* int pattern */
    AUDIO_CTLPIPE_SET_AMPLIFICATION,   /* float */
    AUDIO_CTLPIPE_SET_PITCHBEND,       /* float */
    AUDIO_CTLPIPE_SET_MIXER,           /* st_mixer* */
    AUDIO_CTLPIPE_SET_TEMPO,           /* int */
    AUDIO_CTLPIPE_SET_BPM,             /* int */
} audio_ctlpipe_id;

typedef enum audio_backpipe_id {
    AUDIO_BACKPIPE_DRIVER_OPEN_FAILED=1000,
    AUDIO_BACKPIPE_PLAYING_STARTED,
    AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED,
    AUDIO_BACKPIPE_PLAYING_NOTE_STARTED,
    AUDIO_BACKPIPE_PLAYING_STOPPED,
    AUDIO_BACKPIPE_ERROR_MESSAGE,      /* int len, string (len+1 bytes) */
    AUDIO_BACKPIPE_WARNING_MESSAGE,    /* int len, string (len+1 bytes) */
} audio_backpipe_id;

extern int audio_ctlpipe, audio_backpipe;

/* === Oscilloscope stuff

   There's a buffer for each channel that gets filled in a circular fashion.
   The earliest and the latest valid entry in this "ring buffer" are indicated
   in scopebuf_start and scopebuf_end.

   So the amount of data contained in this buffer at any time is simply
   scopebuf_end.time - scopebuf_start.time, regardless of the offsets. Then you
   simply walk through the scopebufs[] array starting at offset scopebuf_start.offset,
   modulo scopebuf_length, until you reach scopebuf_end.offset. */

extern gboolean scopebuf_ready;
extern gint16 *scopebufs[32];
extern gint32 scopebuf_length;
typedef struct scopebuf_endpoint {
    guint32 offset; // always < scopebuf_length
    double time;
} scopebuf_endpoint;
extern scopebuf_endpoint scopebuf_start, scopebuf_end;
extern int scopebuf_freq;

/* === Player position time buffer */

typedef struct audio_player_pos {
    double time;
    int songpos;
    int patpos;
    int tempo;
    int bpm;
} audio_player_pos;

extern time_buffer *audio_playerpos_tb;

/* === Clipping indicator time buffer */

typedef struct audio_clipping_indicator {
    double time;
    gboolean clipping;
} audio_clipping_indicator;

extern time_buffer *audio_clipping_indicator_tb;

/* === Mixer (sample) position time buffer */

typedef struct audio_mixer_position {
    double time;
    st_mixer_channel_status dump[32];
} audio_mixer_position;

extern time_buffer *audio_mixer_position_tb;

/* === Other stuff */

extern st_mixer *mixer;
extern st_io_driver *playback_driver, *editing_driver, *current_driver;
extern void *playback_driver_object, *editing_driver_object, *current_driver_object;

extern gint8 player_mute_channels[32];

extern event_waiter *audio_songpos_ew;
extern event_waiter *audio_tempo_ew;
extern event_waiter *audio_bpm_ew;


gboolean     audio_init               (int ctlpipe, int backpipe);

void         audio_set_mixer          (st_mixer *mixer);

void         readpipe                 (int fd, void *p, int count);

/* --- Functions called by the player */

void     driver_setnumch      (int numchannels);
void     driver_startnote     (int channel,
			       st_mixer_sample_info *si);
void     driver_stopnote      (int channel);
void     driver_setsmplpos    (int channel,
			       guint32 offset);
void     driver_setsmplend    (int channel,
			       guint32 offset);
void     driver_setfreq       (int channel,
			       float frequency);
void     driver_setvolume     (int channel,
			       float volume);
void     driver_setpanning    (int channel,
			       float panning);
void     driver_set_ch_filter_freq   (int channel,
				      float freq);
void     driver_set_ch_filter_reso   (int channel,
				      float freq);

#endif /* _ST_AUDIO_H */
