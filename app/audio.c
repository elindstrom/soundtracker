
/*
 * The Real SoundTracker - Audio handling thread
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

#include <config.h>

#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "poll.h"

#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif

#include <glib.h>

#include "i18n.h"
#include "audio.h"
#include "mixer.h"
#include "driver-out.h"
#include "main.h"
#include "xm-player.h"
#include "endian-conv.h"
#include "scope-group.h"
#include "errors.h"
#include "time-buffer.h"
#include "event-waiter.h"
#include "gui-settings.h"

st_mixer *mixer = NULL;
st_out_driver *playback_driver = NULL;
st_out_driver *editing_driver = NULL;
st_out_driver *current_driver = NULL;
void *playback_driver_object = NULL;
void *editing_driver_object = NULL;
void *current_driver_object = NULL;
void *file_driver_object = NULL;

int audio_ctlpipe, audio_backpipe;
gint8 player_mute_channels[32];

/* Time buffers for Visual<->Audio synchronization */

time_buffer *audio_playerpos_tb;
time_buffer *audio_clipping_indicator_tb;
time_buffer *audio_mixer_position_tb;

static guint32 audio_visual_feedback_counter;
static guint32 audio_visual_feedback_clipping;
static guint32 audio_visual_feedback_update_interval = 2000; // just a dummy value

static const guint audio_visual_feedback_updates_per_second = 50;
static const guint audio_visual_feedback_smear_clipping = 3; // keep clipflag on for how long

/* Event waiters */

event_waiter *audio_songpos_ew;
event_waiter *audio_tempo_ew;
event_waiter *audio_bpm_ew;

static int set_songpos_wait_for = -1;
static int confirm_tempo = 0, confirm_bpm = 0;

/* Internal variables */

static int nice_value = 0;
static int ctlpipe, backpipe;
static pthread_t threadid;

static int playing = 0;
static gboolean playing_noloop;

// --- for audio_mix() "main loop":

static int mixfmt_req, mixfmt, mixfmt_conv;
static int mixfreq_req;
static int audio_numchannels;
static float audio_ampfactor = 1.0;

static double pitchbend = +0.0, pitchbend_req;
static double audio_next_tick_time_unbent, audio_next_tick_time_bent, audio_current_playback_time_bent;
static double audio_mixer_current_time;

#define MIXFMT_16                   1
#define MIXFMT_STEREO               2

#define MIXFMT_CONV_TO_16           1
#define MIXFMT_CONV_TO_8            2
#define MIXFMT_CONV_TO_UNSIGNED     4
#define MIXFMT_CONV_TO_STEREO       8
#define MIXFMT_CONV_TO_MONO        16
#define MIXFMT_CONV_BYTESWAP       32

typedef struct PollInput {
    int fd;
    GdkInputCondition condition;
    GdkInputFunction function;
    gpointer data;
} PollInput;

static GList *inputs = NULL;

/* Oscilloscope buffers */

gint16 *scopebufs[32];
gint32 scopebuf_length;
scopebuf_endpoint scopebuf_start, scopebuf_end;
int scopebuf_freq;
gboolean scopebuf_ready;

void                audio_prepare_for_playing                (void);

static void
audio_raise_priority (void)
{
// The RT code tends to crash my kernel :-(
#if defined(_POSIX_PRIORITY_SCHEDULING) && 0
    struct sched_param sp;
    sp.sched_priority = 40;

    if(sched_setscheduler(0, SCHED_FIFO, &sp) == 0) {
	printf(">> running as realtime process now\n");
    } else {
	fprintf(stderr,"WARNING: Can't get realtime priority (try running as root)!\n");
    }
#else
    if(nice_value == 0) {
	if(!nice(-14)) {
	    nice_value = -14;
	}
    }
#endif
}

static void
audio_restore_priority (void)
{
    nice(- nice_value);
    nice_value = 0;
}

static void
audio_ctlpipe_init_player (void)
{
    g_assert(xm != NULL);

    xmplayer_init_module();
}

static void
audio_ctlpipe_play_song (int songpos,
			 int patpos)
{
    audio_backpipe_id a;

    g_assert(playback_driver != NULL);
    g_assert(mixer != NULL);
    g_assert(xm != NULL);
    g_assert(!playing);

    if(playback_driver->common.open(playback_driver_object)) {
	current_driver_object = playback_driver_object;
	current_driver = playback_driver;
	audio_prepare_for_playing();
	xmplayer_init_play_song(songpos, patpos);
	a = AUDIO_BACKPIPE_PLAYING_STARTED;
    } else {
	a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
    }

    write(backpipe, &a, sizeof(a));
}

static void
audio_ctlpipe_render_song_to_file (gchar *filename)
{
    audio_backpipe_id a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
    extern st_out_driver driver_out_file;

    g_assert(playback_driver != NULL);
    g_assert(mixer != NULL);
    g_assert(xm != NULL);
    g_assert(!playing);

#if USE_SNDFILE || !defined (NO_AUDIOFILE)
    if(file_driver_object != NULL) {
	driver_out_file.common.destroy(file_driver_object);
    }
    file_driver_object = driver_out_file.common.new();
    *((gchar**)file_driver_object) = g_strdup(filename);

    if(driver_out_file.common.open(file_driver_object)) {
	current_driver_object = file_driver_object;
	current_driver = &driver_out_file;
	audio_prepare_for_playing();
	playing_noloop = TRUE;
	xmplayer_init_play_song(0, 0);
	a = AUDIO_BACKPIPE_PLAYING_STARTED;
	audio_restore_priority();
    }
#endif

    write(backpipe, &a, sizeof(a));
}

static void
audio_ctlpipe_play_pattern (int pattern,
			    int patpos,
			    int only1row)
{
    audio_backpipe_id a;

    g_assert(playback_driver != NULL);
    g_assert(mixer != NULL);
    g_assert(xm != NULL);
    g_assert(!playing);

    if(only1row) {
	if(editing_driver->common.open(editing_driver_object)) {
	    current_driver_object = editing_driver_object;
	    current_driver = editing_driver;
	    audio_prepare_for_playing();
	    xmplayer_init_play_pattern(pattern, patpos, only1row);
	    a = AUDIO_BACKPIPE_PLAYING_NOTE_STARTED;
	} else {
	    a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
	}
    } else {
	if(playback_driver->common.open(playback_driver_object)) {
	    current_driver_object = playback_driver_object;
	    current_driver = playback_driver;
	    audio_prepare_for_playing();
	    xmplayer_init_play_pattern(pattern, patpos, only1row);
	    a = AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED;
	} else {
	    a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
	}
    }

    write(backpipe, &a, sizeof(a));
}

static void
audio_ctlpipe_play_note (int channel,
			 int note,
			 int instrument)
{
    audio_backpipe_id a = AUDIO_BACKPIPE_PLAYING_NOTE_STARTED;

    if(!playing) {
	if(editing_driver->common.open(editing_driver_object)) {
	    current_driver_object = editing_driver_object;
	    current_driver = editing_driver;
	    audio_prepare_for_playing();
	} else {
	    a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
	}
    }

    write(backpipe, &a, sizeof(a));

    if(!playing)
	return;

    xmplayer_play_note(channel, note, instrument);
}

static void
audio_ctlpipe_play_note_full (int channel,
			      int note,
			      STSample *sample,
			      guint32 offset,
			      guint32 count)
{
    audio_backpipe_id a = AUDIO_BACKPIPE_PLAYING_NOTE_STARTED;

    if(!playing) {
	if(editing_driver->common.open(editing_driver_object)) {
	    current_driver_object = editing_driver_object;
	    current_driver = editing_driver;
	    audio_prepare_for_playing();
	} else {
	    a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
	}
    }

    write(backpipe, &a, sizeof(a));

    if(!playing)
	return;

    xmplayer_play_note_full(channel, note, sample, offset, count);
}

static void
audio_ctlpipe_play_note_keyoff (int channel)
{
    if(!playing)
	return;

    xmplayer_play_note_keyoff(channel);
}

static void
audio_ctlpipe_stop_playing (void)
{
    audio_backpipe_id a = AUDIO_BACKPIPE_PLAYING_STOPPED;

    if(playing == 1) {
	xmplayer_stop();
	current_driver->common.release(current_driver_object);
	current_driver = NULL;
	current_driver_object = NULL;
	playing = 0;
    }

    if(set_songpos_wait_for != -1) {
	/* confirm pending request */
	event_waiter_confirm(audio_songpos_ew, 0.0);
	set_songpos_wait_for = -1;
    }

    write(backpipe, &a, sizeof(a));

    audio_raise_priority();
}

static void
audio_ctlpipe_set_songpos (int songpos)
{
    g_assert(playing);

    xmplayer_set_songpos(songpos);
    if(set_songpos_wait_for != -1) {
	/* confirm previous request */
	event_waiter_confirm(audio_songpos_ew, 0.0);
    }
    set_songpos_wait_for = songpos;
}

static void
audio_ctlpipe_set_tempo (int tempo)
{
    xmplayer_set_tempo(tempo);
    if(confirm_tempo != 0) {
	/* confirm previous request */
	event_waiter_confirm(audio_tempo_ew, 0.0);
    }
    confirm_tempo = 1;
}

static void
audio_ctlpipe_set_bpm (int bpm)
{
    xmplayer_set_bpm(bpm);
    if(confirm_bpm != 0) {
	/* confirm previous request */
	event_waiter_confirm(audio_bpm_ew, 0.0);
    }
    confirm_bpm = 1;
}

static void
audio_ctlpipe_set_pattern (int pattern)
{
    g_assert(playing);

    xmplayer_set_pattern(pattern);
}

static void
audio_ctlpipe_set_amplification (float af)
{
    g_assert(mixer != NULL);

    audio_ampfactor = af;
    mixer->setampfactor(af);
}

/* read()s on pipes are always non-blocking, so when we want to read 8
   bytes, we might only get 4. this routine is a workaround. seems
   like i can't put a pipe into blocking mode. alternatively the other
   end of the pipe could send packets in one big chunk instead of
   using single write()s. */
void
readpipe (int fd, void *p, int count)
{
    struct pollfd pfd =	{ fd, POLLIN, 0 };
    int n;

    while(count) {
	n = read(fd, p, count);
	count -= n;
	p += n;
	if(count != 0) {
	    pfd.revents = 0;
	    poll(&pfd, 1, -1);
	}
    }
}

static void
audio_thread (void)
{
    struct pollfd pfd[5] = {
	{ ctlpipe, POLLIN, 0 },
    };
    GList *pl;
    PollInput *pi;
    audio_ctlpipe_id c;
    int a[4], i, npl;
    void *b;
    float af;

    static char *msgbuf = NULL;
    static int msgbuflen = 0;

    audio_raise_priority();

  loop:
    pfd[0].revents = 0;

    for(pl = inputs, npl = 1; pl; pl = pl->next, npl++) {
	pi = pl->data;
	if(pi->fd == -1) {
	    inputs = g_list_remove(inputs, pi);
	    goto loop;
	}
	pfd[npl].events = pfd[npl].revents = 0;
	pfd[npl].fd = pi->fd;
	if(pi->condition & GDK_INPUT_READ)
	    pfd[npl].events |= POLLIN;
	if(pi->condition & GDK_INPUT_WRITE)
	    pfd[npl].events |= POLLOUT;
    }

    if(poll(pfd, npl, -1) == -1) {
	if(errno == EINTR)
	    goto loop;
	perror("audio_thread:poll():");
	pthread_exit(NULL);
    }

    if(pfd[0].revents & POLLIN) {
	readpipe(ctlpipe, &c, sizeof(c));
	switch(c) {
	case AUDIO_CTLPIPE_INIT_PLAYER:
	    audio_ctlpipe_init_player();
	    break;
	case AUDIO_CTLPIPE_PLAY_SONG:
	    readpipe(ctlpipe, a, 2 * sizeof(a[0]));
	    audio_ctlpipe_play_song(a[0], a[1]);
	    break;
	case AUDIO_CTLPIPE_PLAY_PATTERN:
	    readpipe(ctlpipe, a, 3 * sizeof(a[0]));
	    audio_ctlpipe_play_pattern(a[0], a[1], a[2]);
	    break;
	case AUDIO_CTLPIPE_PLAY_NOTE:
	    readpipe(ctlpipe, a, 3 * sizeof(a[0]));
	    audio_ctlpipe_play_note(a[0], a[1], a[2]);
	    break;
	case AUDIO_CTLPIPE_PLAY_NOTE_FULL:
	    readpipe(ctlpipe, &a[0], 2 * sizeof(a[0]));
	    readpipe(ctlpipe, &b, 1 * sizeof(b));
	    readpipe(ctlpipe, &a[2], 2 * sizeof(a[0]));
	    audio_ctlpipe_play_note_full(a[0], a[1], b, a[2], a[3]);
	    break;
	case AUDIO_CTLPIPE_PLAY_NOTE_KEYOFF:
	    readpipe(ctlpipe, a, 1 * sizeof(a[0]));
	    audio_ctlpipe_play_note_keyoff(a[0]);
	    break;
	case AUDIO_CTLPIPE_STOP_PLAYING:
	    audio_ctlpipe_stop_playing();
	    break;
	case AUDIO_CTLPIPE_RENDER_SONG_TO_FILE:
	    readpipe(ctlpipe, a, sizeof(a[0]));
	    if(msgbuflen < a[0] + 1) {
		g_free(msgbuf);
		msgbuf = g_new(char, a[0] + 1);
		msgbuflen = a[0] + 1;
	    }
	    readpipe(ctlpipe, msgbuf, a[0] + 1);
	    audio_ctlpipe_render_song_to_file(msgbuf);
	    break;
	case AUDIO_CTLPIPE_SET_SONGPOS:
	    read(ctlpipe, a, 1 * sizeof(a[0]));
	    audio_ctlpipe_set_songpos(a[0]);
	    break;
	case AUDIO_CTLPIPE_SET_PATTERN:
	    read(ctlpipe, a, 1 * sizeof(a[0]));
	    audio_ctlpipe_set_pattern(a[0]);
	    break;
	case AUDIO_CTLPIPE_SET_AMPLIFICATION:
	    read(ctlpipe, &af, sizeof(af));
	    audio_ctlpipe_set_amplification(af);
	    break;
	case AUDIO_CTLPIPE_SET_PITCHBEND:
	    read(ctlpipe, &af, sizeof(af));
	    pitchbend_req = af;
	    break;
	case AUDIO_CTLPIPE_SET_MIXER:
	    read(ctlpipe, &b, sizeof(b));
	    mixer = b;
	    if(playing) {
		mixer->reset();
		mixfmt_req = -666;
		mixer->setnumch(audio_numchannels);
	    }
	    mixer->setampfactor(audio_ampfactor);
	    break;
	case AUDIO_CTLPIPE_SET_TEMPO:
	    readpipe(ctlpipe, a, 1 * sizeof(a[0]));
	    audio_ctlpipe_set_tempo(a[0]);
	    break;
	case AUDIO_CTLPIPE_SET_BPM:
	    readpipe(ctlpipe, a, 1 * sizeof(a[0]));
	    audio_ctlpipe_set_bpm(a[0]);
	    break;
	default:
	    fprintf(stderr, "\n\n*** audio_thread: unknown ctlpipe id %d\n\n\n", c);
	    pthread_exit(NULL);
	    break;
	}
    }

    for(pl = inputs, i = 1; i < npl; pl = pl->next, i++) {
	pi = pl->data;
	if(pi->fd == -1)
	    continue;
	if(pfd[i].revents & pfd[i].events) {
	    int x = 0;
	    if(pfd[i].revents & POLLIN)
		x |= GDK_INPUT_READ;
	    if(pfd[i].revents & POLLOUT)
		x |= GDK_INPUT_WRITE;
	    pi->function(pi->data, pi->fd, x);

	    if(playing_noloop & player_looped) {
		// "noloop" mode for file renderer -- need to flush output buffer
		// and then stop playing
		pi->function(pi->data, pi->fd, x);
		audio_ctlpipe_stop_playing();
	    }
	}
    }

    goto loop;
}

gboolean
audio_init (int c,
	    int b)
{
    int i;

    ctlpipe = c;
    backpipe = b;

    for(i = 0; i < 32; i++) {
	scopebufs[i] = NULL;
    }
    scopebuf_ready = FALSE;
    scopebuf_length = -1;

    memset(player_mute_channels, 0, sizeof(player_mute_channels));

    if(!(audio_playerpos_tb = time_buffer_new(10.0)))
	return FALSE;
    if(!(audio_clipping_indicator_tb = time_buffer_new(10.0)))
	return FALSE;
    if(!(audio_mixer_position_tb = time_buffer_new(10.0)))
	return FALSE;
    if(!(audio_songpos_ew = event_waiter_new()))
	return FALSE;
    if(!(audio_tempo_ew = event_waiter_new()))
	return FALSE;
    if(!(audio_bpm_ew = event_waiter_new()))
	return FALSE;

    if(0 == pthread_create(&threadid, NULL, (void*(*)(void*))audio_thread, NULL))
	return TRUE;

    return FALSE;
}

void
audio_set_mixer (st_mixer *newmixer)
{
    audio_ctlpipe_id i = AUDIO_CTLPIPE_SET_MIXER;
    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &newmixer, sizeof(newmixer));
}

static void
mixer_mix_format (STMixerFormat m, int s)
{
    g_assert(mixer != NULL);

    mixfmt_conv = 0;
    mixfmt = 0;

    switch(m) {
    case ST_MIXER_FORMAT_S16_BE:
    case ST_MIXER_FORMAT_S16_LE:
    case ST_MIXER_FORMAT_U16_BE:
    case ST_MIXER_FORMAT_U16_LE:
	if(mixer->setmixformat(16)) {
	    mixfmt = MIXFMT_16;
	} else if(mixer->setmixformat(8)) {
	    mixfmt_conv |= MIXFMT_CONV_TO_16;
	} else {
	    g_error("Weird mixer. No 8 or 16 bits modes.\n");
	}
	break;
    case ST_MIXER_FORMAT_S8:
    case ST_MIXER_FORMAT_U8:
	if(mixer->setmixformat(8)) {
	} else if(mixer->setmixformat(16)) {
	    mixfmt = MIXFMT_16;
	    mixfmt_conv |= MIXFMT_CONV_TO_8;
	} else {
	    g_error("Weird mixer. No 8 or 16 bits modes.\n");
	}
	break;
    default:
	g_error("Unknown argument for STMixerFormat.\n");
	break;
    }

    if(mixfmt & MIXFMT_16) {
	switch(m) {
#ifdef WORDS_BIGENDIAN
	case ST_MIXER_FORMAT_S16_LE:
	case ST_MIXER_FORMAT_U16_LE:
#else
	case ST_MIXER_FORMAT_S16_BE:
	case ST_MIXER_FORMAT_U16_BE:
#endif
	    mixfmt_conv |= MIXFMT_CONV_BYTESWAP;
	    break;
	default:
	    break;
	}
    }

    switch(m) {
    case ST_MIXER_FORMAT_U16_LE:
    case ST_MIXER_FORMAT_U16_BE:
    case ST_MIXER_FORMAT_U8:
	mixfmt_conv |= MIXFMT_CONV_TO_UNSIGNED;
	break;
    default:
	break;
    }

    mixfmt |= s ? MIXFMT_STEREO : 0;
    if(!mixer->setstereo(s)) {
	if(s) {
	    mixfmt &= ~MIXFMT_STEREO;
	    mixfmt_conv |= MIXFMT_CONV_TO_STEREO;
	} else {
	    mixfmt_conv |= MIXFMT_CONV_TO_MONO;
	}
    }
}

void
audio_prepare_for_playing (void)
{
    int i;

    g_assert(mixer != NULL);

    mixer->reset();
    mixfmt_req = -666;
    pitchbend = pitchbend_req;

    playing = 1;
    playing_noloop = FALSE;

    audio_next_tick_time_bent = 0.0;
    audio_next_tick_time_unbent = 0.0;
    audio_current_playback_time_bent = 0.0;
    audio_mixer_current_time = 0.0;

    time_buffer_clear(audio_playerpos_tb);
    time_buffer_clear(audio_clipping_indicator_tb);
    time_buffer_clear(audio_mixer_position_tb);
    audio_visual_feedback_counter = audio_visual_feedback_update_interval;
    audio_visual_feedback_clipping = 0;

    event_waiter_reset(audio_songpos_ew);
    event_waiter_reset(audio_tempo_ew);
    event_waiter_reset(audio_bpm_ew);

    scopebuf_start.offset = 0;
    scopebuf_start.time = 0.0;
    scopebuf_end.offset = 0;
    scopebuf_end.time = 0.0;
    scopebuf_ready = FALSE;
    if(gui_settings.scopes_buffer_size / 32 > scopebuf_length) {
	scopebuf_length = gui_settings.scopes_buffer_size / 32;
	for(i = 0; i < 32; i++) {
	    g_free(scopebufs[i]);
	    scopebufs[i] = g_new(gint16, scopebuf_length);
	    if(!scopebufs[i])
		return;
	}
    }
    scopebuf_ready = TRUE;
}

static void *
mixer_mix_and_handle_scopes (void *dest,
			     guint32 count)
{
    int n;
    extern ScopeGroup *scopegroup;
    audio_clipping_indicator *c;
    audio_mixer_position *p;

    // See comments in audio.h for Oscilloscope stuff

    while(count) {
	n = scopebuf_length - scopebuf_end.offset;
	if(n > count)
	    n = count;

	if(n > audio_visual_feedback_counter) {
	    n = audio_visual_feedback_counter;
	}

     	dest = scopegroup->scopes_on && scopebuf_ready ? mixer->mix(dest, n, scopebufs, scopebuf_end.offset) : mixer->mix(dest, n, NULL, 0);

	scopebuf_end.offset += n;
	scopebuf_end.time += (double)n / scopebuf_freq;
	audio_mixer_current_time += (double)n / scopebuf_freq;
	count -= n;
	audio_visual_feedback_counter -= n;
	
	if(scopebuf_end.offset == scopebuf_length) {
	    scopebuf_end.offset = 0;
	}

	if(mixer->getclipflag()) {
	    audio_visual_feedback_clipping = audio_visual_feedback_smear_clipping;
	}

	if(audio_visual_feedback_counter == 0) {
	    /* Get up-to-date info from mixer about current sample positions */
	    audio_visual_feedback_counter = audio_visual_feedback_update_interval;
	    if((p = g_new(audio_mixer_position, 1))) {
		mixer->dumpstatus(p->dump);
		time_buffer_add(audio_mixer_position_tb, p, audio_mixer_current_time);
	    }
	    if((c = g_new(audio_clipping_indicator, 1))) {
		c->clipping = audio_visual_feedback_clipping;
		if(audio_visual_feedback_clipping) {
		    audio_visual_feedback_clipping--;
		}
		time_buffer_add(audio_clipping_indicator_tb, c, audio_mixer_current_time);
	    }
	}

	if(scopebuf_end.time - scopebuf_start.time >= (double)scopebuf_length / scopebuf_freq) {
	    // adjust scopebuf_start
	    int d = scopebuf_end.offset - scopebuf_start.offset;
	    if(d < 0)
		d += scopebuf_length;
	    g_assert(d >= 0);
	    scopebuf_start.offset += d;
	    scopebuf_start.offset %= scopebuf_length;
	    scopebuf_start.time += (double)d / scopebuf_freq;
	}
    }

    return dest;
}

static void *
mixer_mix (void *dest,
	   guint32 count)
{
    static int bufsize = 0;
    static void *buf = NULL;
    int b, i, c, d;
    void *ende;

    if(count == 0)
	return dest;

    g_assert(mixer != NULL);

    /* The mixer doesn't have to support a format that the driver
       requires. This routine converts between any formats if
       necessary: 16 bits / 8 bits, mono / stereo, little endian / big
       endian, unsigned / signed */

    if(mixfmt_conv == 0) {
	return mixer_mix_and_handle_scopes(dest, count);
    }

    b = count;
    c = 8;
    d = 1;
    if(mixfmt & MIXFMT_16) {
	b *= 2;
	c = 16;
    }
    if((mixfmt & MIXFMT_STEREO) || (mixfmt_conv & MIXFMT_CONV_TO_MONO)) {
	b *= 2;
	d = 2;
    }

    if(b > bufsize) {
	g_free(buf);
	buf = g_new(guint8, b);
	bufsize = b;
    }

    g_assert(buf != NULL);
    ende = mixer_mix_and_handle_scopes(buf, count);

    if(mixfmt_conv & MIXFMT_CONV_TO_MONO) {
	if(mixfmt & MIXFMT_16) {
	    gint16 *a = buf, *b = buf;
	    for(i = 0; i < count; i++, a+=2, b+=1)
		*b = (a[0] + a[1]) / 2;
	} else {
	    gint8 *a = buf, *b = buf;
	    for(i = 0; i < count; i++, a+=2, b+=1)
		*b = (a[0] + a[1]) / 2;
	}
	d = 1;
    }

    if(mixfmt_conv & MIXFMT_CONV_TO_8) {
	gint16 *a = buf;
	gint8 *b = dest;
	for(i = 0; i < count * d; i++)
	    *b++ = *a++ >> 8;
	c = 8;
	ende = b;
    } else if(mixfmt_conv & MIXFMT_CONV_TO_16) {
	gint8 *a = buf;
	gint16 *b = dest;
	for(i = 0; i < count * d; i++)
	    *b++ = *a++ << 8;
	c = 16;
	ende = b;
    } else {
	memcpy(dest, buf, count * d * (c / 8));
	ende = dest + (count * d * (c / 8));
    }
    
    if(mixfmt_conv & MIXFMT_CONV_TO_UNSIGNED) {
	if(c == 16) {
	    gint16 *a = dest;
	    guint16 *b = dest;
	    for(i = 0; i < count * d; i++)
		*b++ = *a++ + 32768;
	} else {
	    gint8 *a = dest;
	    guint8 *b = dest;
	    for(i = 0; i < count * d; i++)
		*b++ = *a++ + 128;
	}
    }

    if(mixfmt_conv & MIXFMT_CONV_BYTESWAP) {
	le_16_array_to_host_order(dest, count * d);
    }

    if(mixfmt_conv & MIXFMT_CONV_TO_STEREO) {
	g_assert(d == 1);
	if(c == 16) {
	    gint16 *a = dest, *b = dest;
	    ende = b;
	    for(i = 0, a += count, b += 2 * count; i < count; i++, a-=1, b-=2)
	        b[-1] = b[-2] = a[-1];
	} else {
	    gint8 *a = dest, *b = dest;
	    ende = b;
	    for(i = 0, a += count, b += 2 * count; i < count; i++, a-=1, b-=2)
	        b[-1] = b[-2] = a[-1];
	}
    }

    return ende;
}

void
driver_setnumch (int numchannels)
{
    g_assert(numchannels >= 1 && numchannels <= 32);
    audio_numchannels = numchannels;
    mixer->setnumch(numchannels);
}

void
driver_startnote (int channel,
		  st_mixer_sample_info *si)
{
    if(si->length != 0) {
	mixer->startnote(channel, si);
    }
}

void
driver_stopnote (int channel)
{
    mixer->stopnote(channel);
}

void
driver_setsmplpos (int channel,
		   guint32 offset)
{
    mixer->setsmplpos(channel, offset);
}

void
driver_setsmplend (int channel,
		   guint32 offset)
{
    mixer->setsmplend(channel, offset);
}

void
driver_setfreq (int channel,
		float frequency)
{
    mixer->setfreq(channel, frequency * ((100.0 + pitchbend) / 100.0));
}

void
driver_setvolume (int channel,
		  float volume)
{
    g_assert(volume >= 0.0 && volume <= 1.0);
    
    mixer->setvolume(channel, volume);
}

void
driver_setpanning (int channel,
		   float panning)
{
    g_assert(panning >= -1.0 && panning <= +1.0);

    mixer->setpanning(channel, panning);
}

void
driver_set_ch_filter_freq (int channel,
			   float freq)
{
    if(mixer->setchcutoff) {
	mixer->setchcutoff(channel, freq);
    }
}

void
driver_set_ch_filter_reso (int channel,
			   float freq)
{
    if(mixer->setchreso) {
	mixer->setchreso(channel, freq);
    }
}

gpointer
audio_poll_add (int fd,
		GdkInputCondition cond,
		GdkInputFunction func,
		gpointer data)
{
    PollInput *input;

    g_assert(g_list_length(inputs) < 5);

    input = g_new(PollInput, 1);
    input->fd = fd;
    input->condition = cond;
    input->function = func;
    input->data = data;

    inputs = g_list_prepend(inputs, input);
    return input;
}

void
audio_poll_remove (gpointer input)
{
    if(input) {
	((PollInput*)input)->fd = -1;
    }
}

void
audio_mix (void *dest,
	   guint32 count,
	   int mixfreq,
	   int mixformat)
{
    int nonewtick = FALSE;

    // Set mixer parameters
    if(mixfmt_req != mixformat) {
	mixfmt_req = mixformat;
	mixer_mix_format(mixformat & 15, (mixformat & ST_MIXER_FORMAT_STEREO) != 0);
    }
    scopebuf_freq = mixfreq_req = mixfreq;
    mixer->setmixfreq(mixfreq);

    audio_visual_feedback_update_interval = mixfreq / audio_visual_feedback_updates_per_second;

    while(count) {
	// Mix either until the next time is reached when we should call the XM player,
	// or until the current mixing buffer is full.
	int samples_left = (audio_next_tick_time_bent - audio_current_playback_time_bent) * mixfreq;

	if(samples_left > count) {
	    // No new player tick this time...
	    samples_left = count;
	    nonewtick = TRUE;
	}

	if(playing_noloop && player_looped) {
	    // "noloop" mode for file renderer -- make rest of buffer silent
	    memset(dest, 0,
		   samples_left * ((mixfmt & MIXFMT_16) ? 2 : 1)
		                * ((mixfmt & MIXFMT_STEREO) ? 2 : 1));
	} else {
	    dest = mixer_mix(dest, samples_left);
	}
	count -= samples_left;
	audio_current_playback_time_bent += (double) samples_left / mixfreq;

	if(!nonewtick) {
	    double t;
	    audio_player_pos *p = g_new(audio_player_pos, 1);

	    // Pitchbend variable must be updated directly before or after a tick,
	    // not in the middle of a filled mixing buffer.
	    if(pitchbend_req != pitchbend) {
		pitchbend = pitchbend_req;
	    }

	    // The following three lines, and the stuff in driver_setfreq() contain all
	    // necessary code to handle the pitchbending feature.
	    t = xmplayer_play();
	    audio_next_tick_time_bent += (t - audio_next_tick_time_unbent) * (100.0 / (100.0 + pitchbend));
	    audio_next_tick_time_unbent = t;

	    // Update player position time buffer
	    if(p) {
		p->songpos = player_songpos;
		p->patpos = player_patpos;
		p->tempo = player_tempo;
		p->bpm = player_bpm;
                time_buffer_add(audio_playerpos_tb, p, audio_current_playback_time_bent);
	    }

	    // Confirm pending event requests
	    if(set_songpos_wait_for != -1 && player_songpos == set_songpos_wait_for) {
		event_waiter_confirm(audio_songpos_ew, audio_current_playback_time_bent);
		set_songpos_wait_for = -1;
	    }
	    if(confirm_tempo) {
		event_waiter_confirm(audio_tempo_ew, audio_current_playback_time_bent);
		confirm_tempo = 0;
	    }
	    if(confirm_bpm) {
		event_waiter_confirm(audio_bpm_ew, audio_current_playback_time_bent);
		confirm_bpm = 0;
	    }
	}
    }
}
