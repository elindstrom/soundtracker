/*
 * The Real SoundTracker - ALSA (output) driver 
 *                       - requires ALSA 0.4.0 or newer
 *
 * Copyright (C) 1999-2000 Kai Vehmanen
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

#if DRIVER_ALSA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/asoundlib.h>
#include <sys/time.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "i18n.h"
#include "driver-out.h"
#include "mixer.h"
#include "errors.h"
#include "gui-subs.h"
#include "preferences.h"

typedef struct alsa_driver {
    GtkWidget *configwidget;
    GtkWidget *prefs_resolution_w[2];
    GtkWidget *prefs_channels_w[2];
    GtkWidget *prefs_mixfreq_w[4];
    GtkWidget *bufsizespin_w, *bufsizelabel_w, *estimatelabel_w;
    GtkWidget *alsacardspin_w, *alsadevicespin_w;

    int playrate;
    int stereo;
    int bits;
    int fragsize;
    int numfrags;
    int mf;

    int card_number;
    int device_number;

    GMutex *configmutex;

    snd_pcm_t *soundfd;
    void *sndbuf;
    gpointer polltag;
    int firstpoll;

    int p_resolution;
    int p_channels;
    int p_mixfreq;
    int p_fragsize;

    double outtime;
    double playtime;
} alsa_driver;

static const int mixfreqs[] = { 8000, 16000, 22050, 44100, -1 };

static void
alsa_poll_ready_playing (gpointer data,
			gint source,
			GdkInputCondition condition)
{
    alsa_driver * const d = data;
    int byte_count = (d->stereo + 1) * (d->bits / 8) * d->fragsize;
    int w;
    struct timeval tv;

    if((w = snd_pcm_write(d->soundfd, d->sndbuf, byte_count) != byte_count)) {
      if(w == -1) {
	fprintf(stderr, "driver_alsa: write() returned -1.\n");
      } else {
	fprintf(stderr, "driver_alsa: write not completely done.\n");
      }
    }
    
    gettimeofday(&tv, NULL);
    d->outtime = tv.tv_sec + tv.tv_usec / 1e6;
    d->playtime += (double) d->fragsize / d->playrate;

    audio_mix(d->sndbuf, d->fragsize, d->playrate, d->mf);
}

static void
prefs_init_from_structure (alsa_driver *d)
{
    int i;

    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(d->prefs_resolution_w[d->p_resolution / 8 - 1]), TRUE);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(d->prefs_channels_w[d->p_channels - 1]), TRUE);

    for(i = 0; mixfreqs[i] != -1; i++) {
	if(d->p_mixfreq == mixfreqs[i])
	    break;
    }
    if(mixfreqs[i] == -1) {
	i = 3;
    }
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(d->prefs_mixfreq_w[i]), TRUE);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(d->bufsizespin_w), d->p_fragsize);
}

static void
prefs_update_estimate (alsa_driver *d)
{
    char buf[64];

    sprintf(buf, _("(%d bytes)"), d->p_fragsize * d->p_channels * d->p_resolution / 8);
    gtk_label_set_text(GTK_LABEL(d->bufsizelabel_w), buf);
    
    sprintf(buf, _("Estimated audio delay: %f milliseconds"), (double)(1000 * (d->p_fragsize) / d->p_mixfreq));
    gtk_label_set_text(GTK_LABEL(d->estimatelabel_w), buf);
}

static void
prefs_resolution_changed (void *a,
			  alsa_driver *d)
{
    d->p_resolution = (find_current_toggle(d->prefs_resolution_w, 2) + 1) * 8;
    prefs_update_estimate(d);
}

static void
prefs_channels_changed (void *a,
			alsa_driver *d)
{
    d->p_channels = find_current_toggle(d->prefs_channels_w, 2) + 1;
    prefs_update_estimate(d);
}

static void
prefs_mixfreq_changed (void *a,
		       alsa_driver *d)
{
    d->p_mixfreq = mixfreqs[find_current_toggle(d->prefs_mixfreq_w, 4)];
    prefs_update_estimate(d);
}

static void
prefs_fragsize_changed (GtkSpinButton *w,
			alsa_driver *d)
{
    char buf[30];

    d->p_fragsize = gtk_spin_button_get_value_as_int(w);
    prefs_update_estimate(d);
}

static void
prefs_alsacard_changed (GtkSpinButton *w,
			alsa_driver *d)
{
    d->card_number = gtk_spin_button_get_value_as_int(w);
}

static void
prefs_alsadevice_changed (GtkSpinButton *w,
			alsa_driver *d)
{
    d->device_number = gtk_spin_button_get_value_as_int(w);
}

static void
alsa_make_config_widgets (alsa_driver *d)
{
    GtkWidget *thing, *mainbox, *box2, *box3, *alsa_card, *alsa_device;
    static const char *resolutionlabels[] = { "8 bits", "16 bits", NULL };
    static const char *channelslabels[] = { "Mono", "Stereo", NULL };
    static const char *mixfreqlabels[] = { "8000", "16000", "22050", "44100", NULL };

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);

    thing = gtk_label_new(_("These changes won't take effect until you restart playing."));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    
    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Resolution:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);
    make_radio_group_full(resolutionlabels, box2, d->prefs_resolution_w, FALSE, TRUE, (void(*)())prefs_resolution_changed, d);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Channels:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);
    make_radio_group_full(channelslabels, box2, d->prefs_channels_w, FALSE, TRUE, (void(*)())prefs_channels_changed, d);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Frequency [Hz]:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);
    make_radio_group_full(mixfreqlabels, box2, d->prefs_mixfreq_w, FALSE, TRUE, (void(*)())prefs_mixfreq_changed, d);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Buffer Size:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);

    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    d->bufsizespin_w = thing = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(d->fragsize, 64.0, 16384.0, 64.0, 0.0, 0.0)), 0, 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect (GTK_OBJECT(thing), "changed",
			GTK_SIGNAL_FUNC(prefs_fragsize_changed), d);

    d->bufsizelabel_w = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    add_empty_hbox(box2);
    d->estimatelabel_w = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    add_empty_hbox(box2);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    alsa_card = gtk_label_new(_("ALSA card number:"));
    gtk_widget_show(alsa_card);
    gtk_box_pack_start(GTK_BOX(box2), alsa_card, FALSE, TRUE, 0);
    add_empty_hbox(box2);

    d->alsacardspin_w = alsa_card = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(d->card_number, 0.0, 256.0, 1.0, 1.0, 0.0)), 0, 0);
    gtk_box_pack_start(GTK_BOX(box2), alsa_card, FALSE, TRUE, 0);
    gtk_widget_show(alsa_card);
    gtk_signal_connect (GTK_OBJECT(alsa_card), "changed",
			GTK_SIGNAL_FUNC(prefs_alsacard_changed), d);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    alsa_device = gtk_label_new(_("ALSA device number:"));
    gtk_widget_show(alsa_device);
    gtk_box_pack_start(GTK_BOX(box2), alsa_device, FALSE, TRUE, 0);
    add_empty_hbox(box2);

    d->alsadevicespin_w = alsa_device = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(d->device_number, 0.0, 256.0, 1.0, 1.0, 0.0)), 0, 0);
    gtk_box_pack_start(GTK_BOX(box2), alsa_device, FALSE, TRUE, 0);
    gtk_widget_show(alsa_device);
    gtk_signal_connect (GTK_OBJECT(alsa_device), "changed",
			GTK_SIGNAL_FUNC(prefs_alsadevice_changed), d);

    prefs_init_from_structure(d);
}

static GtkWidget *
alsa_getwidget (void *dp)
{
    alsa_driver * const d = dp;

    return d->configwidget;
}

static void *
alsa_new (void)
{
    alsa_driver *d = g_new(alsa_driver, 1);

    d->card_number = 0;
    d->device_number = 0;

    d->p_mixfreq = 44100;
    d->p_channels = 2;
    d->p_resolution = 16;
    d->p_fragsize = 2048;
    d->soundfd = 0;
    d->sndbuf = NULL;
    d->polltag = NULL;
    d->configmutex = g_mutex_new();

    alsa_make_config_widgets(d);

    return d;
}

static void
alsa_destroy (void *dp)
{
    alsa_driver * const d = dp;

    gtk_widget_destroy(d->configwidget);
    g_mutex_free(d->configmutex);

    g_free(dp);
}

static void
alsa_release (void *dp)
{
    alsa_driver * const d = dp;

    free(d->sndbuf);
    d->sndbuf = NULL;

    audio_poll_remove(d->polltag);
    d->polltag = NULL;

    if(d->soundfd != 0) {
      snd_pcm_drain_playback(d->soundfd);
      snd_pcm_close(d->soundfd);
      d->soundfd = 0;
    }
}

static gboolean
alsa_open (void *dp)
{
    alsa_driver * const d = dp;
    int mf;

    snd_pcm_format_t pf;
    snd_pcm_playback_info_t pcm_info;
    snd_pcm_playback_params_t pp;
    snd_pcm_playback_status_t pbstat;

    int err = snd_pcm_open(&(d->soundfd), d->card_number, d->device_number,
         		 	  SND_PCM_OPEN_PLAYBACK);
    if (err != 0) {
	char buf[256];
	sprintf(buf, _("Couldn't open ALSA device for sound output (card:%d, device:%d):\n%s"), 
		d->card_number, d->device_number, snd_strerror(err));
	error_error(buf);
	goto out;
    }

    // ---
    // Set non-blocking mode.
    // ---

    snd_pcm_block_mode(d->soundfd, 0);    // enable block mode

    d->outtime = 0;
    d->bits = 0;
    mf = 0;

    // ---
    // Select audio format
    // ---

    memset(&pf, 0, sizeof(pf));

    if (d->p_resolution == 16) {
      pf.format = SND_PCM_SFMT_S16_LE;
      d->bits = 16;
      mf = ST_MIXER_FORMAT_S16_LE;
    }
    else {
      pf.format = SND_PCM_SFMT_U8;
      d->bits = 8;
      mf = ST_MIXER_FORMAT_S8;
    }

    if(d->p_channels == 2) {
	d->stereo = 1;
	pf.channels = d->p_channels;
	mf |= ST_MIXER_FORMAT_STEREO;
    }
    else {
      pf.channels = d->p_channels;
      d->stereo = 0;
    }

    pf.rate = d->p_mixfreq;
    d->playrate = d->p_mixfreq;
    d->mf = mf;

    err = snd_pcm_playback_format(d->soundfd, &pf);
    if (err < 0) {
      error_error(_("Required sound output format not supported.\n"));
      goto out;
    }

    snd_pcm_playback_info(d->soundfd, &pcm_info);

    memset(&pp, 0, sizeof(pp));

    pp.fragment_size = d->p_fragsize * pf.channels * (d->bits / 8);
    pp.fragments_max = 1;
    //    pp.fragments_max = 16pcm_info.buffer_size / pp.fragment_size;
    pp.fragments_room = 1;

    err = snd_pcm_playback_params(d->soundfd, &pp);
    if (err < 0) {
      error_error(_("Required sound output parameters not supported.\n"));
      goto out;
    }

    snd_pcm_playback_status(d->soundfd, &pbstat);
    //    d->numfrags = pbstat.fragments;
    //    d->numfrags = 1;
    d->fragsize = pbstat.fragment_size;
    d->numfrags = 1;
    
    /*    fprintf(stderr, "Numfrags: %d\n", d->numfrags);
	  fprintf(stderr, "Fragsize: %d\n", d->fragsize); */

    d->sndbuf = calloc(1, d->fragsize);

    if(d->stereo == 1) { 
      d->fragsize /= 2;  
    } 
    if(d->bits == 16) {  
      d->fragsize /= 2;  
    } 
    
    d->polltag = audio_poll_add(snd_pcm_file_descriptor(d->soundfd), GDK_INPUT_WRITE, alsa_poll_ready_playing, d);
    /*    d->firstpoll = TRUE; */
    d->firstpoll = TRUE;
    d->playtime = 0;

    return TRUE;

  out:
    alsa_release(dp);
    return FALSE;
}

static double
alsa_get_play_time (void *dp)
{
    alsa_driver * const d = dp;
    struct timeval tv;
    double curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec + tv.tv_usec / 1e6;

    return d->playtime + curtime - d->outtime - d->numfrags * ((double) d->fragsize / d->playrate);
}

static inline int
alsa_get_play_rate (void *d)
{
    alsa_driver * const dp = d;
    return dp->playrate;
}

static gboolean
alsa_loadsettings (void *dp,
		  prefs_node *f)
{
    alsa_driver * const d = dp;

    prefs_get_int(f, "alsa-resolution", &d->p_resolution);
    prefs_get_int(f, "alsa-channels", &d->p_channels);
    prefs_get_int(f, "alsa-mixfreq", &d->p_mixfreq);
    prefs_get_int(f, "alsa-fragsize", &d->p_fragsize);

    prefs_get_int(f, "alsa-card", &d->card_number);
    prefs_get_int(f, "alsa-device", &d->device_number);

    prefs_init_from_structure(d);

    return TRUE;
}

static gboolean
alsa_savesettings (void *dp,
		  prefs_node *f)
{
    alsa_driver * const d = dp;

    prefs_put_int(f, "alsa-resolution", d->p_resolution);
    prefs_put_int(f, "alsa-channels", d->p_channels);
    prefs_put_int(f, "alsa-mixfreq", d->p_mixfreq);
    prefs_put_int(f, "alsa-fragsize", d->p_fragsize);

    prefs_put_int(f, "alsa-card", d->card_number);
    prefs_put_int(f, "alsa-device", d->device_number);

    return TRUE;
}

st_out_driver driver_out_alsa = {
    { "ALSA Output",

      alsa_new,
      alsa_destroy,

      alsa_open,
      alsa_release,

      alsa_getwidget,
      alsa_loadsettings,
      alsa_savesettings,
    },

    alsa_get_play_time,
    alsa_get_play_rate
};

#endif /* DRIVER_ALSA */
