
/*
 * The Real SoundTracker - ESD (output) driver.
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

#if DRIVER_ESD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <esd.h>

#include "i18n.h"
#include "driver-out.h"
#include "mixer.h"
#include "errors.h"
#include "gui-subs.h"
#include "preferences.h"


typedef struct esd_driver {
    GtkWidget *configwidget;

    int out_sock, out_bits, out_channels, out_rate;
    int samples_per_frame;
    int mf;

    void *sndbuf;
    gpointer polltag;
    int firstpoll;

    double outtime;
    double playtime;
} esd_driver;

static void
esd_poll_ready_playing (gpointer data,
			gint source,
			GdkInputCondition condition)
{
    esd_driver * const d = data;
    int w;
    struct timeval tv;

    if(!d->firstpoll) {
	if((w = write(d->out_sock, d->sndbuf, ESD_BUF_SIZE) != ESD_BUF_SIZE)) {
	    if(w == -1) {
		fprintf(stderr, "driver_esd: write() returned -1.\n");
	    } else {
		fprintf(stderr, "driver_esd: write not completely done.\n");
	    }
	}

	gettimeofday(&tv, NULL);
	d->outtime = tv.tv_sec + tv.tv_usec / 1e6;
	d->playtime += (double) d->samples_per_frame / d->out_rate;
    }

    d->firstpoll = FALSE;

    audio_mix(d->sndbuf, d->samples_per_frame, d->out_rate, d->mf);
}

static void
esd_make_config_widgets (esd_driver *d)
{
    GtkWidget *thing, *mainbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);
   
    thing = gtk_label_new(_("Note that the ESD output is unusable in\ninteractive mode because of the latency added\nby ESD. Use the OSS or ALSA output plug-ins\nfor serious work."));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
}

static GtkWidget *
esd_getwidget (void *dp)
{
    esd_driver * const d = dp;

    return d->configwidget;
}

static void *
esd_new (void)
{
    esd_driver *d = g_new(esd_driver, 1);

    d->out_bits = ESD_BITS16;
    d->out_rate = 44100;
    d->out_channels = ESD_STEREO;

    esd_make_config_widgets(d);

    return d;
}

static void
esd_destroy (void *dp)
{
    esd_driver * const d = dp;

    gtk_widget_destroy(d->configwidget);

    g_free(dp);
}

static void
esd_release (void *dp)
{
    esd_driver * const d = dp;

    free(d->sndbuf);
    d->sndbuf = NULL;

    audio_poll_remove(d->polltag);
    d->polltag = NULL;

    if(d->out_sock >= 0) {
	esd_close(d->out_sock);
	d->out_sock = -1;
    }
}

static gboolean
esd_open (void *dp)
{
    esd_driver * const d = dp;
    int out_format;

    out_format = d->out_bits | d->out_channels | ESD_STREAM | ESD_PLAY;

    d->out_sock = esd_play_stream_fallback(out_format, d->out_rate, NULL, "SoundTracker ESD Output");
    if(d->out_sock <= 0) {
	char buf[256];
	sprintf(buf, _("Couldn't connect to ESD for sound output:\n%s"), strerror(errno));
	error_error(buf);
	return FALSE;
    }

#ifdef WORDS_BIGENDIAN
    d->mf = ST_MIXER_FORMAT_S16_BE;
#else
    d->mf = ST_MIXER_FORMAT_S16_LE;
#endif

    d->mf |= ST_MIXER_FORMAT_STEREO;

    d->samples_per_frame = ESD_BUF_SIZE;
    if(d->out_bits == ESD_BITS16)
	d->samples_per_frame /= 2;
    if(d->out_channels == ESD_STEREO)
	d->samples_per_frame /= 2;

    d->sndbuf = calloc(1, ESD_BUF_SIZE);

    d->polltag = audio_poll_add(d->out_sock, GDK_INPUT_WRITE, esd_poll_ready_playing, d);
    d->firstpoll = TRUE;
    d->playtime = 0;

    return TRUE;
}

static double
esd_get_play_time (void *dp)
{
    esd_driver * const d = dp;
    struct timeval tv;
    double curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec + tv.tv_usec / 1e6;

    return d->playtime + curtime - d->outtime - 24 * ((double)d->samples_per_frame / d->out_rate);
}

static inline int
esd_get_play_rate (void *d)
{
    esd_driver * const dp = d;
    return dp->out_rate;
}

static gboolean
esd_loadsettings (void *dp,
		  prefs_node *f)
{
//    esd_driver * const d = dp;

    return TRUE;
}

static gboolean
esd_savesettings (void *dp,
		  prefs_node *f)
{
//    esd_driver * const d = dp;

    return TRUE;
}

st_out_driver driver_out_esd = {
    { "ESD Output",

      esd_new,
      esd_destroy,

      esd_open,
      esd_release,

      esd_getwidget,
      esd_loadsettings,
      esd_savesettings,
    },

    esd_get_play_time,
    esd_get_play_rate
};

#endif /* DRIVER_ESD */
