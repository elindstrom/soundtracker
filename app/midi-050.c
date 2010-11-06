/*
 * Copyright (C) 2000 Luc Tanguay <luc.tanguay@bell.ca>
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

/*
 * For ALSA driver, we use the ALSA sequencer API (not the rawmidi API).
 */

#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <string.h>
#include "midi.h"
#include "midi-utils.h"
#include "midi-settings.h"
#include "gui.h"
#include "tracker.h"
#include "xm.h"
#include "gui-settings.h"

/*********************************************************
 * Macro to transform a MIDI note (pitch) into a XM note
 * and vice versa.
 *
 * Translation goes like this:
 *       MIDI   XM
 * note pitch note
 * C-0     12    1
 * B-7    107   96
 *
 * i.e. xm_note = midi_pitch - (12+1)
 *
 * NOTE: C-4 (middle C) is MIDI note 60.
 *
 * The sustain pedal is used to input XM note off.
 ********************************************************/

#define midi_note_2_xm_note(inote) ((inote)-12+1)
#define xm_note_2_midi_note(inote) ((inote)-1+12)

/* Other macros. */

#define SND_SEQ_CTRL_SUSTAIN   0x40
#define MIDI_VELOCITY_MAX      127


extern Tracker *tracker; /* from track-editor.c */

/* Handle to sequencer device. */

static snd_seq_t *midi_handle = NULL;
static gint midi_file_tag = -1;

/* Count the number of notes on to later turn them off gracefully...*/

static int nb_notes_on = 0;

/* Local functions prototypes */

static void midi_in_cb (gpointer data, int fd, GdkInputCondition condition);
static int set_seq_name( snd_seq_t *handle);
static void close_handle( snd_seq_t *handle);
static void midi_process_note_on( snd_seq_ev_note *pnote);
static void midi_process_controller( snd_seq_ev_ctrl *pcontrol);
static void midi_process_program_change( snd_seq_ev_ctrl *pcontrol);

/************************************************************************/

/***********************************************
 * Create and initialize the MIDI device.
 *
 * We create a sequencer client and port,
 * then connect the kernel client to our user-level client.
 * Finally we setup a callback function to handle MIDI events.
 */

void midi_init() {
    int rc;
    snd_seq_port_info_t port;
    snd_seq_port_subscribe_t sub;
    int client;


    if (midi_handle != NULL ) {
      if (IS_MIDI_DEBUG_ON) {
	g_print( "Reinitializing MIDI input\n");
      }

      if (midi_file_tag >= 0) {
	gdk_input_remove( midi_file_tag);
	midi_file_tag = -1;
      }

      close_handle( midi_handle);
      midi_handle = NULL;
    }

    /* Open the sequencer device, in non-block mode.
       Don't use O_NONBLOCK here, it crashes the application
       as for ALSA 0.5.5. (LT 15-mar-2000) */

    rc = snd_seq_open( &midi_handle, SND_SEQ_OPEN_IN);
    if (rc < 0) {
	g_warning( "error opening ALSA MIDI input stream (%s)\n",
		 snd_strerror(rc));
	return;
    }

    /* Set nonblock mode i.e. enable==0. */

    rc = snd_seq_block_mode( midi_handle, 0);
    if (rc < 0) {
	close_handle( midi_handle);
	midi_handle = NULL;
	g_warning( "error disabling sequencer block mode (%s)\n",
		 snd_strerror(rc));
	return;
    }

    /* Get client id.  Needed to subscribe to the kernel-level client. */

    client = snd_seq_client_id( midi_handle);

    if (client < 0) {
	close_handle( midi_handle);
	midi_handle = NULL;
	g_warning( "error naming sequencer client (%s)\n", snd_strerror(client));
	return;
    }


    /* Set client name. Visible with 'cat /proc/asound/seq/clients'. */

    rc = set_seq_name( midi_handle);

    if (rc < 0) {
	close_handle( midi_handle);
	midi_handle = NULL;
	g_warning( "error naming sequencer client (%s)\n",snd_strerror(rc));
	return;
    }

    /* Create a port for our user-level client. */

    memset( &port, 0, sizeof(port));
    strcpy( port.name, "tracker");
    port.capability = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE;
    port.capability |= SND_SEQ_PORT_CAP_SUBS_WRITE; /* necessary??? */
    port.type = SND_SEQ_PORT_TYPE_APPLICATION;

    rc = snd_seq_create_port( midi_handle, &port);

    if (rc < 0) {
	close_handle( midi_handle);
	midi_handle = NULL;
	g_warning( "error creating sequencer port (%s)\n",  snd_strerror(rc));
	return;
    }

    /* Subscribe to the kernel client. */

    memset( &sub, 0, sizeof(sub));
    sub.sender.client = midi_settings.input.client;
    sub.sender.port = midi_settings.input.port;
    sub.dest.client = client;
    sub.dest.port = port.port;

    rc = snd_seq_subscribe_port( midi_handle, &sub);

    if (rc < 0) {
	close_handle( midi_handle);
	midi_handle = NULL;
	g_warning( "error subscribing to client %d port %d (%s)\n",
		 sub.sender.client, sub.sender.port, snd_strerror(rc));
	return;
    }

    /* Install callback to process MIDI input. */

    midi_file_tag = gdk_input_add( snd_seq_file_descriptor( midi_handle),
						 GDK_INPUT_READ,
						 (GdkInputFunction)midi_in_cb,
						 midi_handle);
    if (midi_file_tag < 0) {
	close_handle( midi_handle);
	midi_handle = NULL;

	g_warning( "error installing MIDI input callback (%s)\n",
		 g_strerror(midi_file_tag));
	return;
    }

    if (IS_MIDI_DEBUG_ON) {
      g_print( "MIDI input initialized\n");
    }

    return;

} /* midi_init() */

/************************************************************************
 ************************* local functions ******************************
 ************************************************************************/

/**************************************************
 * Callback for MIDI input events.
 *
 * NOTE:
 *	before leaving, "data" must be freed.
 *	It was created in snd_seq_event_input().
 */

static void midi_in_cb (gpointer data, int fd, GdkInputCondition condition)
{
    snd_seq_t *handle = (snd_seq_t *)data;
    snd_seq_event_t *pev;
    int rc;

    /* Perform sanity checks. */

    if ((int)condition != GDK_INPUT_READ) {
	g_print( "wrong condition (%d). %d was expected\n",
		 condition, GDK_INPUT_READ);
	return;
    }

    if (handle == NULL) {
	g_print( "NULL handle detected\n");
	return;
    }

    /* Process MIDI event.  Don't forget to free the event after usage. */

    rc = snd_seq_event_input( handle, &pev);
    if (rc < 0) {
	g_print("unable to get event");
	return;
    }

    switch (pev->type) {
    case SND_SEQ_EVENT_NOTEOFF:
	/* Simulate a note on event. */
	pev->data.note.velocity = 0;
	/* no break here. Go to next case...*/
    case SND_SEQ_EVENT_NOTE:
    case SND_SEQ_EVENT_NOTEON:
      if (IS_MIDI_DEBUG_ON) {
	  midi_print_event(pev);
	}
	midi_process_note_on( &(pev->data.note));
	break;

    case SND_SEQ_EVENT_CONTROLLER:
      if (IS_MIDI_DEBUG_ON) {
	  midi_print_event(pev);
	}
	midi_process_controller( &(pev->data.control));
	break;

    case SND_SEQ_EVENT_PGMCHANGE:
      if (IS_MIDI_DEBUG_ON) {
	  midi_print_event(pev);
	}
	midi_process_program_change( &(pev->data.control));
	break;

    default:
      if (IS_MIDI_DEBUG_ON) {
	  /* Some events (like SND_SEQ_EVENT_SENSING) are not printed
	     by the print_event routine. */
	  midi_print_event(pev);
	}
	break;
    }

    rc = snd_seq_free_event( pev);
    if (rc < 0) {
	g_print("unable to free event");
	return;
    }

    return;

} /* midi_in_cb() */

/*****************************************
 * Set name of our sequencer client.
 */

static int set_seq_name(snd_seq_t *handle)
{
    int err;
    snd_seq_client_info_t info;
	
    memset(&info, 0, sizeof(info));
    info.client = snd_seq_client_id(handle);
    info.type = USER_CLIENT;
    snprintf(info.name, sizeof(info.name), "SoundTracker(%i)", getpid());
    err = snd_seq_set_client_info(handle, &info);
    if (err < 0) {
	fprintf(stderr, "Set client info error: %s\n", snd_strerror(err));
	return -1;
    }

    return 0;

} /* set_seq_name() */

/**********************************
 * Close sequencer handle.
 */

static void close_handle(snd_seq_t *handle)
{
    int rc;

    rc = snd_seq_close( handle);
    if (rc < 0) {
	g_print( "error closing handle (%s)\n",  snd_strerror(rc));
    }

    return;

} /* close_handle() */

/*******************************************************
 * Process MIDI program change event.
 * Called from the MIDI input callback.
 *
 * Change the XM instrument.
 */

static void midi_process_program_change( snd_seq_ev_ctrl *pcontrol)
{
    /* In XM, instrument number is from 1 to 127.
       0 is reserved. */

    if (pcontrol->value > 0) {
	gui_set_current_instrument(pcontrol->value);
    }

    if ( 0 ) {
	g_print( "program change: param %#x, channel %d, value %#x\n",
		 pcontrol->param, pcontrol->channel, pcontrol->value);
    }

    return;

} /* midi_process_program_change() */

/*******************************************************
 * Process MIDI controller event.
 * Called from the MIDI input callback.
 *
 * If we receive Sustain event, transform it into a XM note off.
 */

static void midi_process_controller( snd_seq_ev_ctrl *pcontrol)
{
    snd_seq_ev_note note;


    switch (pcontrol->param) {
    case SND_SEQ_CTRL_SUSTAIN:
	/* Build a pseudo-note. */

	memset( &note, 0, sizeof(note));
	note.note = xm_note_2_midi_note(XM_PATTERN_NOTE_OFF);
	if (midi_settings.input.channel_enabled) {
	    note.channel = pcontrol->channel;
	}

	/* Based on MIDI standard, a value less than 63 means
	   sustain off. */

	if (pcontrol->value < 63) {
	    note.velocity = 0;
	} else {
	    note.velocity = MIDI_VELOCITY_MAX;
	}
	midi_process_note_on( &note);
	break;

    default:
	if ( 0 ) {
	    g_print( "control event not processed %#x\n", pcontrol->param);
	}
	break;
    }

    return;

} /* midi_process_controller() */


/*******************************************************
 * Process MIDI note ON.
 * Called from the MIDI input callback (most of the time).
 *
 * If the note velocity is 0, just turn off the sound. Don't
 * see it as a XM note off event.
 *
 * We keep track on the number of note ON events.  With slow fingers
 * on the MIDI controller, it is easy to press a second key before
 * releasing the previous one.  If we did turned the note off when
 * a MIDI note off was received (i.e. when the first finger released
 * the first key), we would turn off the sound of the second
 * note.  With the counter, the note off is done at the right time,
 * when the last key is released...
 */

static void midi_process_note_on( snd_seq_ev_note *pnote)
{
    gint note;
    int channel;
    int volume;
    gboolean note_on = pnote->velocity > 0 ? 1 : 0;

    /* Set local value for channel. */

    if (midi_settings.input.channel_enabled) {
	channel = (int)pnote->channel;
    } else {
	channel = tracker->cursor_ch;
    }

    /* Set local value for volume. */
    /* The value -1 means don't change the volume in the pattern. */

    if (midi_settings.input.volume_enabled) {
      volume = (int)pnote->velocity;
      /* Since XM volume range is limited, adjust it here. */
      if (volume < XM_NOTE_VOLUME_MIN) {
	volume = XM_NOTE_VOLUME_MIN;
      } else if (volume > XM_NOTE_VOLUME_MAX) {
	volume = XM_NOTE_VOLUME_MAX;
      }	
    } else {
      volume = -1;
    }

    /* Perform some validation on the event.
       - is the note a valid XM note ?
       - is the channel in the valid range of channels ?
    */

    note = midi_note_2_xm_note(pnote->note);

    if (note != XM_PATTERN_NOTE_OFF && (note < XM_PATTERN_NOTE_MIN || note > XM_PATTERN_NOTE_MAX)) {
	g_print("note out of range\n");
	return;
    }

    if (channel > tracker->num_channels) {
	g_warning( "Channel out of range");
	return;
    }

    if ( 0 ) {
	g_print( "note %d velocity %d channel %d, xmnote %d\n",
		 pnote->note, pnote->velocity,
		 channel,
		 note);
    }

    /* If necessary, jump to channel */

    if (tracker->cursor_ch != channel) {
	int diff = channel - tracker->cursor_ch;

	/*g_warning("MIDI channel and current channel are not the same..."); */

	tracker_step_cursor_channel( tracker, diff);
    }

    /* Play the note. Record it if we're in the track editor. */

    if ( note_on ) {
	int row;
	XMNote *xmnote;

	/* Increment the number of notes on. */

	nb_notes_on++;

	/* Play the note in the channel specified by the MIDI channel. */

	gui_play_note( channel, note);

	/* Give warning when MIDI channel and cursor channel are different. */

	if(tracker->cursor_ch != channel) {
	    g_warning( "MIDI channel and current channel are not the same...");
	}

 	/* Edit track if:
 	   1- we're in the track editor tab,
 	   2- edit mode is active...
 	*/
 
 	if ( !GUI_EDITING || notebook_current_page != NOTEBOOK_PAGE_TRACKER) {
 	    return;
 	}

	/* Current position in channel. */

	row = tracker->patpos;

	/* Get and set current XM note pitch. */

	xmnote = &(tracker->curpattern->channels[channel][row]);

	xmnote->note = note;
	xmnote->instrument = gui_get_current_instrument();
	if ( volume >= 0 ) {
	  xmnote->volume = volume;
	}

	/* Redraw screen and if not in ASYNCEDIT mode,
	   jump to next position in the channel. */

	tracker_redraw_current_row(tracker);
	if (!ASYNCEDIT) {
	    tracker_step_cursor_row(tracker, gui_get_current_jump_value());
	}
	
	/* Don't forget: the XM has been changed... */

	xm_set_modified(1);

    } else {
	/* Decrement the number of note on.
	   If it is 0, then turn the note off in the track (channel). */

	nb_notes_on--;

	if (nb_notes_on <= 0) {
	    gui_play_note_keyoff( channel);
	    nb_notes_on = 0;
	}
    }
 
    return;

} /* midi_process_note_on() */
