/*
 * The Real SoundTracker - JACK (output) driver
 *
 * Copyright (C) 2003 Anthony Van Groningen
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

/*
 * TODO: 
 * scopes: I think this is an ST issue, but I don't think our clock updates enough.
 * Clientname code could be improved. Max 10 clients soundtracker_0-9
 * need better declicking?
 * endianness?
 * slave transport was removed
 * should master transport always work? even for pattern? Can we determine this info anyway?
 * general thread safety: d->state should be wrapped in state_mx locks as a matter of principle
 *                        In practice this is needed only when we are waiting on state_cv.
 * XRUN counter
 */

#include <config.h>

#if DRIVER_JACK

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

#include <jack/jack.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "i18n.h"
#include "driver-inout.h"
#include "mixer.h"
#include "errors.h"
#include "gui.h"
#include "preferences.h"

// suggested by Erik de Castro Lopo
#define INT16_MAX 32767.0f
static inline float 
sample_convert_s16_to_float(gint16 inval)
{
	return inval * (1.0 / (INT16_MAX + 1));
}

typedef jack_default_audio_sample_t audio_t;
typedef jack_nframes_t nframes_t;

typedef enum {
	JackDriverStateIsRolling,
	JackDriverStateIsDeclicking,
	JackDriverStateIsStopping,    // declicking is done, we want to transition to stopped
	JackDriverStateIsStopped
} jack_driver_state;

typedef enum {
	JackDriverTransportIsInternal = 0,
	JackDriverTransportIsMaster = 1,
	JackDriverTransportIsSlave = 2
} jack_driver_transport;

typedef struct jack_driver {
	// prefs stuff
	GtkWidget *configwidget;
	GtkWidget *client_name_label;
	GtkWidget *status_label;
	GtkWidget *transport_check;
	guint transport_check_id;
	GtkWidget *declick_check;
	gboolean do_declick;

	// jack + audio stuff
	nframes_t buffer_size;     
	unsigned long sample_rate;  
	char client_name[15];      
	jack_client_t *client;
	jack_port_t *left,*right;  
	void *mix;                      // passed to audio_mix, big enough for stereo 16 bit nframes = nframes*4
	STMixerFormat mf;
	jack_transport_info_t ti;        

	// internal state stuff
	jack_driver_state state;         
	nframes_t position;              // frames since ST called open()
	pthread_mutex_t *process_mx;     // try to lock this around process_core()
	pthread_cond_t *state_cv;        // trigger after declicking if we have the lock
	gboolean locked;                 // set true if we get it. then we can trigger any CV's during process_core()
	gboolean is_active;              // jack seems to be running fine
	jack_driver_transport transport; // who do we serve?

} jack_driver;

static inline float
jack_driver_declick_coeff (nframes_t total, nframes_t current)
{
	// total = # of frames it takes from 1.0 to 0.0
	// current = total ... 0
	return (float)current/(float)total;
}

static void
jack_driver_process_core (nframes_t nframes, jack_driver *d)
{
	audio_t *lbuf,*rbuf;
	gint16 *mix = d->mix;
	nframes_t cnt = nframes;
	float gain = 1.0f;
	jack_driver_state state = d->state;
	
	lbuf = (audio_t *) jack_port_get_buffer(d->left,nframes);
	rbuf = (audio_t *) jack_port_get_buffer(d->right,nframes);
	
      	switch (state) {

	case JackDriverStateIsRolling: 
		audio_mix (mix, nframes, d->sample_rate, d->mf);
		d->position += nframes;
		while (cnt--) {
			*(lbuf++) = sample_convert_s16_to_float (*mix++);
			*(rbuf++) = sample_convert_s16_to_float (*mix++);
		}
		d->ti.transport_state = JackTransportRolling; // redundant or reassuring?
		break;
		
	case JackDriverStateIsDeclicking:
		audio_mix (mix, nframes, d->sample_rate, d->mf);
		d->position += nframes;
		while (cnt--) {
			gain = jack_driver_declick_coeff (nframes, cnt);
			*(lbuf++) = gain * sample_convert_s16_to_float (*mix++);
			*(rbuf++) = gain * sample_convert_s16_to_float (*mix++);
		}
		// safe because ST shouldn't call open() with pending release()
		d->state = JackDriverStateIsStopping; 
		d->ti.transport_state = JackTransportStopped;
		break;

	case JackDriverStateIsStopping:
		// if locked, then trigger change of state, otherwise keep silent
		if (d->locked) {
			d->state = JackDriverStateIsStopped;
			pthread_cond_signal (d->state_cv);
		}		
		// fall down

	case JackDriverStateIsStopped:
	default:
		memset (lbuf, 0, nframes * sizeof (audio_t));
		memset (rbuf, 0, nframes * sizeof (audio_t));
		d->ti.transport_state = JackTransportStopped;
	}
		
	if (d->transport == JackDriverTransportIsMaster) {
		d->ti.frame = d->position;
		d->ti.valid = JackTransportPosition | JackTransportState;
		jack_set_transport_info (d->client, &(d->ti));
	}
	else {
		d->ti.valid = 0;
		jack_set_transport_info (d->client, &d->ti);
	}
}

static int
jack_driver_process_wrapper (nframes_t nframes, void *arg)
{
	jack_driver *d = arg;

	if (pthread_mutex_trylock (d->process_mx) == 0) {
		d->locked = TRUE;
		jack_driver_process_core (nframes, d);
		pthread_mutex_unlock (d->process_mx);
	} else {
		d->locked = FALSE;
		jack_driver_process_core (nframes, d);
	}
	return 0;
}

static void
jack_driver_prefs_transport_callback (void *a, jack_driver *d)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (d->transport_check))) {
		if (d->is_active && (jack_engine_takeover_timebase (d->client) == 0)) {
			d->transport = JackDriverTransportIsMaster;
			return;
		} else {
			// reset
			// gtk_signal_handler_block (GTK_OBJECT(d->transport_check), d->transport_check_id);
			// gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(d->transport_check), FALSE);
			// gtk_signal_handler_unblock (GTK_OBJECT(d->transport_check), d->transport_check_id);
			return;
		}
	} else {
		d->transport = JackDriverTransportIsInternal;
	}
}

static void
jack_driver_prefs_declick_callback (void *a, jack_driver *d)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (d->declick_check)))
		d->do_declick = TRUE;
	else
		d->do_declick = FALSE;
}

static void
jack_driver_make_config_widgets (jack_driver *d)
{
	GtkWidget *thing, *mainbox, *hbox;
	
	d->configwidget = mainbox = gtk_vbox_new (FALSE,2);

	d->client_name_label = thing = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
	gtk_widget_show (thing);
	
	thing = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
	gtk_widget_show (thing);

	d->status_label = thing = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
	gtk_widget_show (thing);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX(mainbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show (hbox);

 	thing = d->transport_check = gtk_check_button_new_with_label (_("transport master"));
	gtk_box_pack_start (GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
	d->transport_check_id = g_signal_connect(thing, "clicked", G_CALLBACK(jack_driver_prefs_transport_callback),d);
	gtk_widget_show (thing);

	thing = d->declick_check = gtk_check_button_new_with_label (_("declick"));
	gtk_box_pack_start (GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
	g_signal_connect(thing, "clicked", G_CALLBACK(jack_driver_prefs_declick_callback),d);
	gtk_widget_show (thing);
}

static int  
jack_driver_sample_rate_callback (nframes_t nframes, void *arg)
{
	jack_driver *d = arg;
	d->sample_rate = nframes;
	return 0;
}

static void
jack_driver_prefs_update (jack_driver *d)
{
	char status_buf[128];

	if (d->is_active) {
		g_sprintf (status_buf, _("Running at %d Hz with %d frames"), (int)d->sample_rate, (int)d->buffer_size);
		gtk_label_set_text (GTK_LABEL (d->client_name_label), d->client_name);
	}
	else 
		g_sprintf (status_buf, _("Jack server not running?"));
	gtk_label_set_text (GTK_LABEL (d->status_label), status_buf);
       
}

static void
jack_driver_server_has_shutdown (void *arg)
{
	jack_driver *d = arg;
	d->is_active = FALSE;
	jack_driver_prefs_update (d);
}

static void
jack_driver_error (const char *s)
{
	
}

static void *
jack_driver_new (void)
{
	jack_driver *d = g_new(jack_driver, 1);
	int i;

	d->mix = NULL;
	d->mf = ST_MIXER_FORMAT_S16_LE | ST_MIXER_FORMAT_STEREO;
	// d->mf = ST_MIXER_FORMAT_S16_BE | ST_MIXER_FORMAT_STEREO;
	d->state = JackDriverStateIsStopped;
	//d->transport = JackDriverTransportIsSlave;
	d->transport = JackDriverTransportIsInternal;
	d->position = 0;
	d->is_active = FALSE;
	d->process_mx = (pthread_mutex_t*)malloc(sizeof (pthread_mutex_t));
	d->state_cv = (pthread_cond_t *)malloc(sizeof (pthread_mutex_t));
	d->do_declick = TRUE;
	pthread_mutex_init (d->process_mx, NULL);
	pthread_cond_init (d->state_cv, NULL);
	jack_driver_make_config_widgets (d);	

	jack_set_error_function (jack_driver_error);

	// TODO: this should be improved, both error handling and saving the string
	// I'm probably not taking advantage of libjack
	g_sprintf (d->client_name, _("soundtracker"));
	d->client_name[12] = '_';
	d->client_name[14] = 0;
	for (i = 0; i < 9; i++) {
		d->client_name[13] = 48 + i; // "0"-"9"
		if ((d->client = jack_client_new (d->client_name)) != 0) {
			break;
		}
	}
	if (d->client == NULL) {
		// we've failed here, but we should have a working dummy driver
		// because ST will segfault on NULL return
		return d;
	}

	// Jack-dependent setup only 
	d->sample_rate = jack_get_sample_rate (d->client);
	d->buffer_size = jack_get_buffer_size (d->client);
	d->mix = calloc(1, d->buffer_size * 4);
	
	d->left = jack_port_register (d->client,_("out_1"),JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput,0);
	d->right = jack_port_register (d->client,_("out_2"),JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput,0);

	jack_set_process_callback (d->client,jack_driver_process_wrapper, d);
	jack_set_sample_rate_callback (d->client,jack_driver_sample_rate_callback, d);
	jack_on_shutdown (d->client, jack_driver_server_has_shutdown, d);
	
	if (jack_activate (d->client)) {
		d->is_active = FALSE;
	} else {
		d->is_active = TRUE;
	}
	return d;
}


static gboolean
jack_driver_open (void *dp)
{
	jack_driver *d = dp;

	if (!d->is_active) {
	        // TODO: need a pop-up message, and bail out for now
		return FALSE;
	}
	d->position = 0;
	d->state = JackDriverStateIsRolling;
	return TRUE;
}


static void
jack_driver_release (void *dp)
{
	jack_driver *d = dp;

	pthread_mutex_lock (d->process_mx);
	if (d->do_declick) {
		d->state = JackDriverStateIsDeclicking;
	} else {
		d->state = JackDriverStateIsStopping;
	}
	pthread_cond_wait (d->state_cv,d->process_mx);
	// at this point process() has set state to stopped
	pthread_mutex_unlock (d->process_mx);
}

static void
jack_driver_destroy (void *dp)
{
	jack_driver *d = dp;

	printf("destroy in\n");

	if (d->is_active) {
		d->is_active = FALSE;
		jack_client_close (d->client);
	}
	gtk_widget_destroy (d->configwidget);
	if (d->mix != NULL) {
		free (d->mix);
	}
	pthread_mutex_destroy (d->process_mx);
	pthread_cond_destroy (d->state_cv);
	g_free(d);
	printf("destroy out\n");
}

static GtkWidget *
jack_driver_getwidget (void *dp)
{
	jack_driver *d = dp;
	jack_driver_prefs_update (d);
	return d->configwidget;
}

static gboolean
jack_driver_loadsettings (void *dp, prefs_node *f)
{
	jack_driver *d = dp;
	// prefs_get_string (f, "jack_client_name", d->client_name);
	prefs_get_int (f, "jack-declick", &(d->do_declick));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(d->declick_check), d->do_declick);
	return TRUE;
}

static gboolean
jack_driver_savesettings (void *dp, prefs_node *f)
{
	jack_driver *d = dp;
	//	prefs_put_string (f, "jack-client_name", d->client_name);
	prefs_put_int (f, "jack-declick", d->do_declick);
	return TRUE;
}

static double
jack_driver_get_play_time (void *dp)
{
	jack_driver * const d = dp;
	return (double)d->position / (double)d->sample_rate;
}

static inline int
jack_driver_get_play_rate (void *d)
{
    jack_driver * const dp = d;
    return (int)dp->sample_rate;
}

st_io_driver driver_out_jack = {
    { "JACK Output",
      jack_driver_new,             // create new instance of this driver class   
      jack_driver_destroy,         // destroy instance of this driver class
      jack_driver_open,            // open the driver
      jack_driver_release,         // close the driver, release audio
      jack_driver_getwidget,       // get pointer to configuration widget
      jack_driver_loadsettings,    // load configuration from provided prefs_node
      jack_driver_savesettings,    // save configuration to specified prefs_node
    },
    jack_driver_get_play_time,     // get time offset since first sound output
    jack_driver_get_play_rate
};

#endif /* DRIVER_JACK */
