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

#include <glib.h>
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
static GIOChannel *midi_channel;

/* Count the number of notes on to later turn them off gracefully...*/

static int nb_notes_on = 0;

/* Local functions prototypes */

static void close_handle( snd_seq_t *handle);
static void midi_process_note_on( snd_seq_ev_note_t *pnote);
static void midi_process_controller( snd_seq_ev_ctrl_t *pcontrol);
static void midi_process_program_change( snd_seq_ev_ctrl_t *pcontrol);
static gint midi_get_fd( snd_seq_t *handle);
static gint midi_add_to_main_loop( snd_seq_t *handle);

gboolean midi_channel_ready( GIOChannel *src, GIOCondition cond, gpointer data);
void midi_channel_destroy( gpointer data);


/*******************************************************************
 * Called when the IO channel associated with 
 * the MIDI handle is destroyed.
 */

void midi_channel_destroy( gpointer data)
{
  if (IS_MIDI_DEBUG_ON) {
    g_print( "MIDI IO channel destroyed...\n");
  }
}

/*******************************************************************
 * Get file descriptor of MIDI seq. handle.
 * Returns -1 on error.
 */

static gint
midi_get_fd( snd_seq_t *seq_handle)
{
  int npfd;
  struct pollfd *pfd;
  int rc;


  g_return_val_if_fail( seq_handle != NULL, -1);

  npfd = snd_seq_poll_descriptors_count(seq_handle, POLLIN);

  if (IS_MIDI_DEBUG_ON) {
    g_message("Number of FDs = %d\n", npfd);
  }

  if ( npfd != 1 ) {
    g_warning("Number of FDs is not 1...\n");
    return -1;
  }

  pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
  rc = snd_seq_poll_descriptors(seq_handle, pfd, npfd, POLLIN);
  if ( rc != 1 ) {
    g_warning("Number of FDs retrieved is not 1...\n");
  }

  return pfd->fd;
}

/****************************************************
 * Hook up the MIDI sequencer to GTK main loop.
 */

static gint midi_add_to_main_loop( snd_seq_t *handle)
{
  gint fd;
  int rc;


  /* Get the file descriptor associated with the seq.
   * Will be used to let GDK monitor MIDI input.
   */

  fd = midi_get_fd( midi_handle);
  if (IS_MIDI_DEBUG_ON) {
    g_print("FD = %d\n", fd);
  }

  /* Install callback to process MIDI input. */

  midi_channel = g_io_channel_unix_new(fd);

  g_io_add_watch_full( midi_channel,
		       G_PRIORITY_HIGH,
		       G_IO_IN|G_IO_ERR|G_IO_HUP,
		       midi_channel_ready,
		       midi_handle, /*user data */
		       midi_channel_destroy);

  /* Filter some MIDI events...
   * Let pass only interessting events...
   */

  rc = 0;
  rc = rc + snd_seq_set_client_event_filter( midi_handle, SND_SEQ_EVENT_NOTE);
  rc = rc + snd_seq_set_client_event_filter( midi_handle, SND_SEQ_EVENT_NOTEON);
  rc = rc + snd_seq_set_client_event_filter( midi_handle, SND_SEQ_EVENT_NOTEOFF);
  rc = rc + snd_seq_set_client_event_filter( midi_handle, SND_SEQ_EVENT_CONTROLLER);
  rc = rc + snd_seq_set_client_event_filter( midi_handle, SND_SEQ_EVENT_PGMCHANGE);

  if (rc != 0) {
    g_print("Unable to set event filter(s) on MIDI input stream...\n");
  }

  return 0;
}

/***********************************************
 * Create and initialize the MIDI device.
 *
 * We create a sequencer client and port,
 * then connect the kernel client to our user-level client.
 * Finally we setup a callback function to handle MIDI events.
 */

void midi_init() {
    int rc;
    snd_seq_port_subscribe_t *port_sub = NULL;
    int client;
    int port;
    char *str = NULL;


    /* Close handle if already opened.
    *  Stop monitoring MIDI file descriptor.
    */

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

    /* Open the sequencer device, in non-block mode. */

    rc = snd_seq_open( &midi_handle, "default", SND_SEQ_OPEN_INPUT, 0);

    if (rc < 0) {
	g_warning( "error opening ALSA MIDI input stream (%s)\n", snd_strerror(rc));
	return;
    }

    /* Get the client number of the sequencer client.
       Will be used to connect ST and the kernel-level sequencer device.
       Without this, we would need an external program like aconnect to
       connect ST and the sequencer device. */

    client = snd_seq_client_id( midi_handle);

    if (IS_MIDI_DEBUG_ON) {
      g_print("Soundtracker client id : %d\n", client);
    }

    if (client < 0) {
      close_handle( midi_handle);
      g_warning("Get client info error: %s\n", snd_strerror(rc));
      return;
    }

    /* Set client name. Visible with 'cat /proc/asound/seq/clients'. */

    str = g_strdup_printf("SoundTracker-%d", getpid());
    rc = snd_seq_set_client_name(midi_handle, str);
    if (rc < 0) {
	close_handle( midi_handle);
	g_warning("Set client info error: %s\n", snd_strerror(rc));
	return;
    }

    /* Create a port for our user-level client.
     * The port number will be the destination port
     * when we subscribe to the ALSA seq.
     */

    str = g_strdup_printf("SoundTracker-%d-%d", getpid(), 0 /* port number */);
    port = snd_seq_create_simple_port( midi_handle, str,
				     SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
				     SND_SEQ_PORT_TYPE_APPLICATION);

    if (port < 0) {
	close_handle( midi_handle);
	midi_handle = NULL;
	g_warning( "error creating sequencer port (%s)\n",  snd_strerror(port));
	return;
    }

    /* If auto-connect is enabled,
       subscribe to the kernel-level client.
       The kernel-level client is specified in the MIDI configuration
       dialog.
       If auto-connect is disabled, the user will have to connect
       soundtracker MIDI port with kernel MIDI port by another mean,
       e.g. aconnect.
    */

    if (midi_settings.input.auto_connect == 1) {
      snd_seq_addr_t sub_addr; /* don't need to malloc this struct. */


      /* Allocate port_sub on stack.  Don't need to 'free'. */

      snd_seq_port_subscribe_alloca(&port_sub);

      /* First, set the sender info.  The sender is the own
       * who emits MIDI data i.e. the ALSA driver in this case.
       */

      sub_addr.client = midi_settings.input.client;
      sub_addr.port = midi_settings.input.port;

      if (IS_MIDI_DEBUG_ON) {
	g_print("Kernel client : %d\n", midi_settings.input.client);
	g_print("Kernel port : %d\n", midi_settings.input.port);
      }

      snd_seq_port_subscribe_set_sender(port_sub, &sub_addr);

      /* Second, set the destination i.e. Soundtracker. */

      sub_addr.client = client;
      sub_addr.port = port;
      snd_seq_port_subscribe_set_dest(port_sub, &sub_addr);

      rc = snd_seq_subscribe_port( midi_handle, port_sub);
      if (rc < 0) {
	snd_seq_port_subscribe_free(port_sub);
	close_handle( midi_handle);
	midi_handle = NULL;
	g_warning( "error subscribing sequencer port (%s)\n",  snd_strerror(rc));
	return;
      }
    }

    midi_file_tag = midi_add_to_main_loop( midi_handle);

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

/**************************************************
 * Callback for MIDI input events.
 *
 * NOTE:
 *	before leaving, "data" must be freed.
 *	It was created in snd_seq_event_input().
 */

gboolean midi_channel_ready( GIOChannel *channel, GIOCondition cond, gpointer data)
{
  snd_seq_t *handle = (snd_seq_t *)data;
  snd_seq_event_t *ev;
  int rc;

  if (cond != G_IO_IN) {
    g_print( "MIDI IO channel condition not G_IO_IN.\n");
    return FALSE;
  }

  g_return_val_if_fail( handle != NULL, FALSE);

  do {
    /* Process MIDI event.  Don't forget to free the event after usage. */

    rc = snd_seq_event_input( handle, &ev);
    if (rc < 0) {
      g_print("unable to get event");
      return TRUE;
    }

    switch (ev->type) {
    case SND_SEQ_EVENT_NOTEOFF:
	/* Simulate a note on event. */
	ev->data.note.velocity = 0;
	/* no break here. Go to next case...*/
    case SND_SEQ_EVENT_NOTE:
    case SND_SEQ_EVENT_NOTEON:
      if (IS_MIDI_DEBUG_ON) {
	  midi_print_event(ev);
	}
	midi_process_note_on( &(ev->data.note));
	break;

    case SND_SEQ_EVENT_CONTROLLER:
      if (IS_MIDI_DEBUG_ON) {
	  midi_print_event(ev);
	}
	midi_process_controller( &(ev->data.control));
	break;

    case SND_SEQ_EVENT_PGMCHANGE:
      if (IS_MIDI_DEBUG_ON) {
	  midi_print_event(ev);
	}
	midi_process_program_change( &(ev->data.control));
	break;

    default:
      if (IS_MIDI_DEBUG_ON) {
	  /* Some events (like SND_SEQ_EVENT_SENSING) are not printed
	     by the print_event routine. */
	  midi_print_event(ev);
	}
	break;
    }

    snd_seq_free_event(ev);

  } while (snd_seq_event_input_pending(handle,0) > 0);

  return TRUE;
}


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

static void midi_process_program_change( snd_seq_ev_ctrl_t *pcontrol)
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

static void midi_process_controller( snd_seq_ev_ctrl_t *pcontrol)
{
    snd_seq_ev_note_t note;


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

static void midi_process_note_on( snd_seq_ev_note_t *pnote)
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
