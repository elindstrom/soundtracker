
/*
 * The Real SoundTracker - OSS (input) driver.
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

#if DRIVER_OSS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_SYS_SOUNDCARD_H
# include <sys/soundcard.h>
#elif HAVE_MACHINE_SOUNDCARD_H
# include <machine/soundcard.h>
#elif HAVE_SOUNDCARD_H
# include <soundcard.h>
#endif
#include <sys/time.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "i18n.h"
#include "driver-in.h"
#include "mixer.h"
#include "errors.h"
#include "gui-subs.h"

typedef struct oss_driver {
    GtkWidget *configwidget;
    GtkWidget *prefs_devdsp_w;

    int playrate;
    int stereo;
    int bits;
    int fragsize;
    int mf;

    GMutex *configmutex;

    int soundfd;
    void *sndbuf;
    int polltag;

    gchar p_devdsp[128];
    int p_resolution;
    int p_channels;
    int p_mixfreq;
    int p_fragsize;
} oss_driver;

static void
oss_poll_ready_sampling (gpointer data,
			 gint source,
			 GdkInputCondition condition)
{
    oss_driver * const d = data;
    int size;

    size = (d->stereo + 1) * (d->bits / 8) * d->fragsize;

    read(d->soundfd, d->sndbuf, size);

    sample_editor_sampled(d->sndbuf, d->fragsize, d->playrate, d->mf);
}

static void
prefs_init_from_structure (oss_driver *d)
{
    gtk_entry_set_text(GTK_ENTRY(d->prefs_devdsp_w), d->p_devdsp);
}

static void
oss_devdsp_changed (void *a,
		    oss_driver *d)
{
    strncpy(d->p_devdsp, gtk_entry_get_text(GTK_ENTRY(d->prefs_devdsp_w)), 127);
}

static void
oss_make_config_widgets (oss_driver *d)
{
    GtkWidget *thing, *mainbox, *box2;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);
   
    thing = gtk_label_new(_("These changes won't take effect until you restart sampling."));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    
    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Input device (e.g. '/dev/dsp'):"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);
    thing = gtk_entry_new_with_max_length(126);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_entry_set_text(GTK_ENTRY(thing), d->p_devdsp);
    gtk_signal_connect_after(GTK_OBJECT(thing), "changed",
			     GTK_SIGNAL_FUNC(oss_devdsp_changed), d);
    d->prefs_devdsp_w = thing;

    prefs_init_from_structure(d);
}

static GtkWidget *
oss_getwidget (void *dp)
{
    oss_driver * const d = dp;

    return d->configwidget;
}

static void *
oss_new (void)
{
    oss_driver *d = g_new(oss_driver, 1);

    strcpy(d->p_devdsp, "/dev/dsp");
    d->p_mixfreq = 44100;
    d->p_channels = 1;
    d->p_resolution = 16;
    d->p_fragsize = 9;
    d->soundfd = -1;
    d->sndbuf = NULL;
    d->polltag = 0;
    d->configmutex = g_mutex_new();

    oss_make_config_widgets(d);

    return d;
}

static void
oss_destroy (void *dp)
{
    oss_driver * const d = dp;

    gtk_widget_destroy(d->configwidget);
    g_mutex_free(d->configmutex);

    g_free(dp);
}

static gboolean
oss_try_format (oss_driver *d, int f)
{
    int format = f;

    if(ioctl(d->soundfd, SNDCTL_DSP_SETFMT, &format) == -1) {
	perror("SNDCTL_DSP_SETFMT");
	return FALSE;
    } 
    
    return format == f;
}

static gboolean
oss_try_stereo (oss_driver *d, int f)
{
    int format = f;

    if(ioctl(d->soundfd, SNDCTL_DSP_STEREO, &format) == -1) {
	perror("SNDCTL_DSP_STEREO");
	return FALSE;
    }  
    
    return format == f;
}

static void
oss_release (void *dp)
{
    oss_driver * const d = dp;

    free(d->sndbuf);
    d->sndbuf = NULL;

    if(d->polltag != 0) {
	gdk_input_remove(d->polltag);
	d->polltag = 0;
    }

    if(d->soundfd >= 0) {
	close(d->soundfd);
	d->soundfd = -1;
    }
}

static gboolean
oss_open (void *dp)
{
    oss_driver * const d = dp;
    int mf;
    int i;

    /* O_NONBLOCK is required for the es1370 driver in Linux
       2.2.9, for example. It's open() behaviour is not
       OSS-conformant (though Thomas Sailer says it's okay). */
    if((d->soundfd = open(d->p_devdsp, O_RDONLY | O_NONBLOCK)) < 0) {
	char buf[256];
	sprintf(buf, _("Couldn't open %s for sampling:\n%s"), d->p_devdsp, strerror(errno));
	error_error(buf);
	goto out;
    }

    // Clear O_NONBLOCK again
    fcntl(d->soundfd, F_SETFL, 0);

    d->bits = 0;
    mf = 0;
    if(d->p_resolution == 16) {
	if(oss_try_format(d, AFMT_S16_LE)) {
	    d->bits = 16;
	    mf = ST_MIXER_FORMAT_S16_LE;
	} else if(oss_try_format(d, AFMT_S16_BE)) {
	    d->bits = 16;
	    mf = ST_MIXER_FORMAT_S16_BE;
	} else if(oss_try_format(d, AFMT_U16_LE)) {
	    d->bits = 16;
	    mf = ST_MIXER_FORMAT_U16_LE;
	} else if(oss_try_format(d, AFMT_U16_BE)) {
	    d->bits = 16;
	    mf = ST_MIXER_FORMAT_U16_BE;
	}
    }
    if(d->bits != 16) {
	if(oss_try_format(d, AFMT_S8)) {
	    d->bits = 8;
	    mf = ST_MIXER_FORMAT_S8;
	} else if(oss_try_format(d, AFMT_U8)) {
	    d->bits = 8;
	    mf = ST_MIXER_FORMAT_U8;
	} else {
	    error_error(_("Required sound output format not supported.\n"));
	    goto out;
	}
    }

    if(d->p_channels == 2 && oss_try_stereo(d, 1)) {
	d->stereo = 1;
	mf |= ST_MIXER_FORMAT_STEREO;
    } else if(oss_try_stereo(d, 0)) {
	d->stereo = 0;
    }

    d->mf = mf;

    d->playrate = d->p_mixfreq;
    ioctl(d->soundfd, SNDCTL_DSP_SPEED, &d->playrate);
	
    i = 0x00040000 + d->p_fragsize + d->stereo + (d->bits / 8 - 1);
    ioctl(d->soundfd, SNDCTL_DSP_SETFRAGMENT, &i);
    ioctl(d->soundfd, SNDCTL_DSP_GETBLKSIZE, &d->fragsize);

    d->sndbuf = calloc(1, d->fragsize);

    if(d->stereo == 1) {
	d->fragsize /= 2;
    }
    if(d->bits == 16) {
	d->fragsize /= 2;
    }

    d->polltag = gdk_input_add(d->soundfd, GDK_INPUT_READ, oss_poll_ready_sampling, d);

    // At least my ES1370 requires an initial read...
    read(d->soundfd, d->sndbuf, (d->stereo + 1) * (d->bits / 8) * d->fragsize);

    return TRUE;

  out:
    oss_release(dp);
    return FALSE;
}

static gboolean
oss_loadsettings (void *dp,
		  prefs_node *f)
{
    oss_driver * const d = dp;

    prefs_get_string(f, "oss-devdsp", d->p_devdsp);

    prefs_init_from_structure(d);

    return TRUE;
}

static gboolean
oss_savesettings (void *dp,
		  prefs_node *f)
{
    oss_driver * const d = dp;

    prefs_put_string(f, "oss-devdsp", d->p_devdsp);

    return TRUE;
}

st_in_driver driver_in_oss = {
    { "OSS Sampling",

      oss_new,
      oss_destroy,

      oss_open,
      oss_release,

      oss_getwidget,
      oss_loadsettings,
      oss_savesettings,
    }
};

#endif /* DRIVER_OSS */
