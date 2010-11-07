
/*
 * The Real SoundTracker - File (output) driver.
 *
 * Copyright (C) 1999-2001 Michael Krause
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

#if USE_SNDFILE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>

#include <sndfile.h>

#include "i18n.h"
#include "driver-inout.h"
#include "mixer.h"
#include "errors.h"
#include "gui-subs.h"
#include "preferences.h"

typedef struct sndfile_driver {
    gchar *filename; /* must be the first entry. is altered by audio.c (hack, hack) */

    SNDFILE *outfile;
    SF_INFO sfinfo;

    int pipe[2];
    gpointer polltag;
    int firstpoll;

    gint16 *sndbuf;
    int sndbuf_size;
    double playtime;

    int p_resolution;
    int p_channels;
    int p_mixfreq;

    GtkWidget *configwidget;
} sndfile_driver;

static void
sndfile_poll_ready_playing (gpointer data,
			 gint source,
			 GdkInputCondition condition)
{
    sndfile_driver * const d = data;

    if(!d->firstpoll) {
	sf_writef_short (d->outfile, d->sndbuf, d->sndbuf_size / 4);
	d->playtime += (double)(d->sndbuf_size) / 4 / d->p_mixfreq;
    }

    d->firstpoll = FALSE;
#ifdef WORDS_BIGENDIAN
    audio_mix(d->sndbuf, d->sndbuf_size / 4, d->p_mixfreq, ST_MIXER_FORMAT_S16_BE | ST_MIXER_FORMAT_STEREO);
#else
    audio_mix(d->sndbuf, d->sndbuf_size / 4, d->p_mixfreq, ST_MIXER_FORMAT_S16_LE | ST_MIXER_FORMAT_STEREO);
#endif

}

static void
sndfile_make_config_widgets (sndfile_driver *d)
{
    GtkWidget *thing, *mainbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);
   
    thing = gtk_label_new(_("no settings (yet), sorry!"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
}

static GtkWidget *
sndfile_getwidget (void *dp)
{
    sndfile_driver * const d = dp;

    return d->configwidget;
}

static void *
sndfile_new (void)
{
    sndfile_driver *d = g_new(sndfile_driver, 1);

    d->p_mixfreq = 44100;
    d->p_channels = 2;
    d->p_resolution = 16;
    d->sndbuf = NULL;
    d->polltag = NULL;

    sndfile_make_config_widgets(d);

    pipe(d->pipe);

    return d;
}

static void
sndfile_destroy (void *dp)
{
    sndfile_driver * const d = dp;

    close(d->pipe[0]);
    close(d->pipe[1]);

    gtk_widget_destroy(d->configwidget);

    g_free(d->filename);

    g_free(dp);
}

static void
sndfile_release (void *dp)
{
    sndfile_driver * const d = dp;

    free(d->sndbuf);
    d->sndbuf = NULL;

    audio_poll_remove(d->polltag);
    d->polltag = NULL;

    if(d->outfile != NULL) {
	sf_close(d->outfile);
	d->outfile = NULL;
    }
}

static gboolean
sndfile_open (void *dp)
{
    sndfile_driver * const d = dp;

    d->sfinfo.channels = 2 ;
    d->sfinfo.samplerate = 44100 ;
    d->sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 ;

    d->outfile = sf_open (d->filename, SFM_WRITE, &d->sfinfo);

    if(!d->outfile) {
	error_error(_("Can't open file for writing."));
	goto out;
    }

    /* In case we're running setuid root... */
    chown(d->filename, getuid(), getgid());

    d->sndbuf_size = 16384;
    d->sndbuf = malloc(d->sndbuf_size);
    if(!d->sndbuf) {
	error_error("Can't allocate mix buffer.");
	goto out;
    }

    d->polltag = audio_poll_add(d->pipe[1], GDK_INPUT_WRITE, sndfile_poll_ready_playing, d);
    d->firstpoll = TRUE;
    d->playtime = 0.0;

    return TRUE;

  out:
    sndfile_release(dp);
    return FALSE;
}

static double
sndfile_get_play_time (void *dp)
{
  //    sndfile_driver * const d = dp;

    return d->playtime;
}

static gboolean
sndfile_loadsettings (void *dp,
		   prefs_node *f)
{
//    sndfile_driver * const d = dp;

    return TRUE;
}

static gboolean
sndfile_savesettings (void *dp,
		   prefs_node *f)
{
//    sndfile_driver * const d = dp;

    return TRUE;
}

st_io_driver driver_out_file = {
    { "WAV Rendering Output using libsndfile",

      sndfile_new,
      sndfile_destroy,

      sndfile_open,
      sndfile_release,

      sndfile_getwidget,
      sndfile_loadsettings,
      sndfile_savesettings,
    },

    sndfile_get_play_time,
};

#elif !defined (NO_AUDIOFILE)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>

#include <audiofile.h>

#include "i18n.h"
#include "driver-inout.h"
#include "mixer.h"
#include "errors.h"
#include "gui-subs.h"
#include "preferences.h"

typedef struct file_driver {
    gchar *filename; /* must be the first entry. is altered by audio.c (hack, hack) */

    AFfilehandle outfile;
    int pipe[2];
    gpointer polltag;
    int firstpoll;

    gint16 *sndbuf;
    int sndbuf_size;
    double playtime;

    int p_resolution;
    int p_channels;
    int p_mixfreq;

    GtkWidget *configwidget;
} file_driver;

static void
file_poll_ready_playing (gpointer data,
			 gint source,
			 GdkInputCondition condition)
{
    file_driver * const d = data;

    if(!d->firstpoll) {
	afWriteFrames(d->outfile, AF_DEFAULT_TRACK, d->sndbuf, d->sndbuf_size / 4);
	d->playtime += (double)(d->sndbuf_size) / 4 / d->p_mixfreq;
    }

    d->firstpoll = FALSE;

    audio_mix(d->sndbuf, d->sndbuf_size / 4, d->p_mixfreq, ST_MIXER_FORMAT_S16_LE | ST_MIXER_FORMAT_STEREO);
}

static void
file_make_config_widgets (file_driver *d)
{
    GtkWidget *thing, *mainbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);
   
    thing = gtk_label_new(_("no settings (yet), sorry!"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
}

static GtkWidget *
file_getwidget (void *dp)
{
    file_driver * const d = dp;

    return d->configwidget;
}

static void *
file_new (void)
{
    file_driver *d = g_new(file_driver, 1);

    d->p_mixfreq = 44100;
    d->p_channels = 2;
    d->p_resolution = 16;
    d->sndbuf = NULL;
    d->polltag = NULL;

    file_make_config_widgets(d);

    pipe(d->pipe);

    return d;
}

static void
file_destroy (void *dp)
{
    file_driver * const d = dp;

    close(d->pipe[0]);
    close(d->pipe[1]);

    gtk_widget_destroy(d->configwidget);

    g_free(d->filename);

    g_free(dp);
}

static void
file_release (void *dp)
{
    file_driver * const d = dp;

    free(d->sndbuf);
    d->sndbuf = NULL;

    audio_poll_remove(d->polltag);
    d->polltag = NULL;

    if(d->outfile != 0) {
	afCloseFile(d->outfile);
	d->outfile = 0;
    }
}

static gboolean
file_open (void *dp)
{
    file_driver * const d = dp;
    AFfilesetup outfilesetup;

    outfilesetup = afNewFileSetup();
    afInitFileFormat(outfilesetup, AF_FILE_WAVE);
    afInitChannels(outfilesetup, AF_DEFAULT_TRACK, 2);
    afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    d->outfile = afOpenFile(d->filename, "w", outfilesetup);
    afFreeFileSetup(outfilesetup);

    if(!d->outfile) {
	error_error(_("Can't open file for writing."));
	goto out;
    }

    /* In case we're running setuid root... */
    chown(d->filename, getuid(), getgid());

    d->sndbuf_size = 16384;
    d->sndbuf = malloc(d->sndbuf_size);
    if(!d->sndbuf) {
	error_error("Can't allocate mix buffer.");
	goto out;
    }

    d->polltag = audio_poll_add(d->pipe[1], GDK_INPUT_WRITE, file_poll_ready_playing, d);
    d->firstpoll = TRUE;
    d->playtime = 0.0;

    return TRUE;

  out:
    file_release(dp);
    return FALSE;
}

static double
file_get_play_time (void *dp)
{
    file_driver * const d = dp;

    return d->playtime;
}

static inline int
file_get_play_rate (void *d)
{
  //    sndfile_driver * const dp = d;
  //    return dp->p_mixfreq;
  return -1;
}

static gboolean
file_loadsettings (void *dp,
		   prefs_node *f)
{
//    file_driver * const d = dp;

    return TRUE;
}

static gboolean
file_savesettings (void *dp,
		   prefs_node *f)
{
//    file_driver * const d = dp;

    return TRUE;
}

st_io_driver driver_out_file = {
    { "WAV Rendering Output",

      file_new,
      file_destroy,

      file_open,
      file_release,

      file_getwidget,
      file_loadsettings,
      file_savesettings,
    },

    file_get_play_time,
    file_get_play_rate
};

#endif /* NO_AUDIOFILE */
