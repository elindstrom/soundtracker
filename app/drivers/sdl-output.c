
/*
 * The Real SoundTracker - SDL output driver.
 *
 * Copyright (C) 2004 Markku Reunanen
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

#if DRIVER_SDL

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <SDL.h>

#include "i18n.h"
#include "driver-out.h"
#include "mixer.h"
#include "errors.h"
#include "gui-subs.h"
#include "preferences.h"

#define SDL_BUFSIZE 1024

typedef struct sdl_driver {
    GtkWidget *configwidget;

    int out_bits, out_channels, out_rate;
    int played;
    int mf;
    SDL_AudioSpec spec;

    gpointer polltag;
} sdl_driver;

void sdl_callback(void *udata, Uint8 *stream, int len)
{
    sdl_driver * const d = udata;

    audio_mix(stream, len/4, d->out_rate, d->mf);
    d->played+=len/4;
}

static void
sdl_poll_ready_playing (gpointer data,
			gint source,
			GdkInputCondition condition)
{
    SDL_Delay(10);
}

static void
sdl_make_config_widgets (sdl_driver *d)
{
    GtkWidget *thing, *mainbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);
   
    thing = gtk_label_new(_("Experimental SDL support."));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
}

static GtkWidget *
sdl_getwidget (void *dp)
{
    sdl_driver * const d = dp;

    return d->configwidget;
}

static void *
sdl_new (void)
{
    sdl_driver *d = g_new(sdl_driver, 1);

    d->out_bits = AUDIO_S16SYS;
    d->out_rate = 44100;
    d->out_channels = 2;

    sdl_make_config_widgets(d);

    return d;
}

static void
sdl_destroy (void *dp)
{
    sdl_driver * const d = dp;

    gtk_widget_destroy(d->configwidget);

    g_free(dp);
}

static void
sdl_release (void *dp)
{
    sdl_driver * const d = dp;

    audio_poll_remove(d->polltag);
    d->polltag = NULL;

    SDL_PauseAudio(1);
    SDL_Quit();
}

static gboolean
sdl_open (void *dp)
{
    sdl_driver * const d = dp;

    SDL_Init(SDL_INIT_AUDIO);
    d->spec.freq=44100;
    d->spec.format=AUDIO_S16SYS;
    d->spec.channels=2;
    d->spec.samples=SDL_BUFSIZE;
    d->spec.callback=sdl_callback;
    d->spec.userdata=dp;
    SDL_OpenAudio(&d->spec,NULL);
    SDL_PauseAudio(0);

#ifdef WORDS_BIGENDIAN
    d->mf = ST_MIXER_FORMAT_S16_BE;
#else
    d->mf = ST_MIXER_FORMAT_S16_LE;
#endif

    d->mf |= ST_MIXER_FORMAT_STEREO;

    d->polltag = audio_poll_add(0, GDK_INPUT_WRITE, sdl_poll_ready_playing, d);
    d->played = 0;

    SDL_PauseAudio(0);
    return TRUE;
}

static double
sdl_get_play_time (void *dp)
{
    sdl_driver * const d = dp;

    return((double)d->played/44100.0);
}

static gboolean
sdl_loadsettings (void *dp,
		  prefs_node *f)
{
    return TRUE;
}

static gboolean
sdl_savesettings (void *dp,
		  prefs_node *f)
{
    return TRUE;
}

st_out_driver driver_out_sdl = {
    { "SDL Output",

      sdl_new,
      sdl_destroy,

      sdl_open,
      sdl_release,

      sdl_getwidget,
      sdl_loadsettings,
      sdl_savesettings,
    },

    sdl_get_play_time,
};

#endif /* DRIVER_SDL */
