
/*
 * The Real SoundTracker - Transposition dialog
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

#include "i18n.h"
#include "transposition.h"
#include "main.h"
#include "xm.h"
#include "gui-subs.h"
#include "gui.h"
#include "st-subs.h"
#include "track-editor.h"

static GtkWidget *transposition_window = NULL,
    *transposition_scope_w[4],
    *transposition_instrument_mode_w[2],
    *transposition_instrument_w[2];

static void
transposition_close_requested (void)
{
    gtk_widget_destroy(transposition_window);
    transposition_window = NULL;
}

static void
transposition_for_each (void (*function)(XMNote *track, int patlen, int data),
			int functiondata)
{
    int i, j;
    int mode = find_current_toggle(transposition_scope_w, 4);

    switch(mode) {
    case 0: // Whole Song
	for(i = 0; i < sizeof(xm->patterns) / sizeof(xm->patterns[0]); i++) {
	    if(st_is_pattern_used_in_song(xm, i)) {
		for(j = 0; j < xm->num_channels; j++) {
		    function(xm->patterns[i].channels[j], xm->patterns[i].length, functiondata);
		}
	    }
	}
	break;
    case 1: // All Patterns
	for(i = 0; i < sizeof(xm->patterns) / sizeof(xm->patterns[0]); i++) {
	    for(j = 0; j < xm->num_channels; j++) {
		function(xm->patterns[i].channels[j], xm->patterns[i].length, functiondata);
	    }
	}
	break;
    case 2: // Current Pattern
	i = gui_get_current_pattern();
	for(j = 0; j < xm->num_channels; j++) {
	    function(xm->patterns[i].channels[j], xm->patterns[i].length, functiondata);
	}
	break;
    case 3: // Current Track
	i = gui_get_current_pattern();
	j = tracker->cursor_ch;
	function(xm->patterns[i].channels[j], xm->patterns[i].length, functiondata);
	break;
    }
}

static void
transposition_transpose_notes_full (XMNote *track,
				    int length,
				    int add,
				    int instrument)
{
    while(length--) {
	if(track->note != 0 && track->note != 97
	   && (instrument == -1 || instrument == track->instrument)) {
	    track->note = CLAMP(track->note + add, 1, 96);
	}
	track++;
    }
}

static void
transposition_transpose_notes_sub (XMNote *track,
				   int patlen,
				   int add)
{
    gboolean all_instruments = find_current_toggle(transposition_instrument_mode_w, 2);
    int current_instrument = gui_get_current_instrument();

    transposition_transpose_notes_full(track, patlen, add, all_instruments ? -1 : current_instrument);
}

static void
transposition_change_instruments_sub (XMNote *track,
				      int patlen,
				      int mode)
{
    int i1 = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(transposition_instrument_w[0]));
    int i2 = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(transposition_instrument_w[1]));

    while(patlen--) {
	if(track->instrument == i1)
	    track->instrument = i2;
	else if(mode == 0 && track->instrument == i2)
	    track->instrument = i1;
	track++;
    }
}

static void
transposition_transpose_notes (GtkWidget *w,
			       gpointer data)
{
    int add = 0;

    switch(GPOINTER_TO_INT(data)) {
    case 0:
	add = +1;
	break;
    case 1:
	add = -1;
	break;
    case 2:
	add = +12;
	break;
    case 3:
	add = -12;
	break;
    }

    transposition_for_each (transposition_transpose_notes_sub, add);
    tracker_redraw(tracker);
    xm_set_modified(1);
}

void
transposition_transpose_selection (Tracker *t,
				   int by)
{
    int height, width, chStart, rowStart;
    int i;

    if(!tracker_is_valid_selection(t)) {
	return;
    }

    tracker_get_selection_rect(t, &chStart, &rowStart, &width, &height);

    for(i = chStart; i < chStart + width; i++) {
	transposition_transpose_notes_full(t->curpattern->channels[i] + rowStart,
					   height, by, -1);
    }

    xm_set_modified(1);
    tracker_redraw(t);
}

static void
transposition_change_instruments (GtkWidget *w,
				  gpointer data)
{
    int b = GPOINTER_TO_INT(data);

    transposition_for_each (transposition_change_instruments_sub, b);
    tracker_redraw(tracker);
    xm_set_modified(1);
}

static void
transposition_current_instrument_clicked (GtkWidget *w,
					  gpointer data)
{
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(transposition_instrument_w[
        		      GPOINTER_TO_INT(data)]),
			      gui_get_current_instrument());		     
}

void
transposition_dialog (void)
{
    gint i;
    GtkWidget *mainbox, *thing, *box1, *box2, *frame, *hbox;
    const char *labels1[] = {
	_("Whole Song"),
	_("All Patterns"),
	_("Current Pattern"),
	_("Current Track"),
	NULL
    };
    const char *labels2[] = {
	_("Current Instrument"),
	_("All Instruments"),
	NULL
    };
    const char *labels3[] = {
	_("Half note up"),
	_("Half note down"),
	_("Octave up"),
	_("Octave down")
    };
    const char *labels4[] = {
	_("Exchange 1 <-> 2"),
	_("Change 1 -> 2")
    };

    if(transposition_window != NULL) {
	gdk_window_raise(transposition_window->window);
	return;
    }
    
#ifdef USE_GNOME
    transposition_window = gnome_app_new("SoundTracker", _("Transposition Tools"));
#else
    transposition_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(transposition_window), _("Transposition Tools"));
#endif
    gtk_signal_connect (GTK_OBJECT (transposition_window), "delete_event",
			GTK_SIGNAL_FUNC (transposition_close_requested), NULL);

    gtk_window_set_transient_for(GTK_WINDOW(transposition_window), GTK_WINDOW(mainwindow));
    gtk_window_set_policy(GTK_WINDOW(transposition_window), FALSE, FALSE, FALSE);

    mainbox = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(mainbox), 4);
#ifdef USE_GNOME
    gnome_app_set_contents(GNOME_APP(transposition_window), mainbox);
#else
    gtk_container_add(GTK_CONTAINER(transposition_window), mainbox);
#endif
    gtk_widget_show(mainbox);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box1);
    gtk_box_pack_start(GTK_BOX(mainbox), box1, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Scope of the operation:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    add_empty_hbox(box1);
    make_radio_group_full(labels1, box1, transposition_scope_w, FALSE, TRUE, NULL, NULL);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_label(GTK_FRAME(frame), _("Note Transposition"));
    gtk_box_pack_start(GTK_BOX(mainbox), frame, FALSE, TRUE, 0);
    gtk_widget_show(frame);

    box2 = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(box2);
    gtk_container_add (GTK_CONTAINER(frame), box2);
    gtk_container_border_width(GTK_CONTAINER(box2), 4);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box1);
    gtk_box_pack_start(GTK_BOX(box2), box1, FALSE, TRUE, 0);

    add_empty_hbox(box1);
    make_radio_group_full(labels2, box1, transposition_instrument_mode_w, FALSE, TRUE, NULL, NULL);
    add_empty_hbox(box1);

    box1 = gtk_hbox_new(TRUE, 4);
    gtk_widget_show(box1);
    gtk_box_pack_start(GTK_BOX(box2), box1, FALSE, TRUE, 0);

    for(i = 0; i < 4; i++) {
	thing = gtk_button_new_with_label(labels3[i]);
	gtk_widget_show(thing);
	gtk_box_pack_start(GTK_BOX(box1), thing, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(thing), "clicked",
			   GTK_SIGNAL_FUNC(transposition_transpose_notes),
			   GINT_TO_POINTER(i));
    }

    frame = gtk_frame_new(NULL);
    gtk_frame_set_label(GTK_FRAME(frame), _("Instrument Changing"));
    gtk_box_pack_start(GTK_BOX(mainbox), frame, FALSE, TRUE, 0);
    gtk_widget_show(frame);

    box2 = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(box2);
    gtk_container_add (GTK_CONTAINER(frame), box2);
    gtk_container_border_width(GTK_CONTAINER(box2), 4);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box1);
    gtk_box_pack_start(GTK_BOX(box2), box1, FALSE, TRUE, 0);

    add_empty_hbox(box1);

    gui_put_labelled_spin_button(box1, _("Instrument 1:"), 1, 128, &transposition_instrument_w[0], NULL, NULL);

    thing = gtk_button_new_with_label(_("Current instrument"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(transposition_current_instrument_clicked), (gpointer)0);

    add_empty_hbox(box1);

    gui_put_labelled_spin_button(box1, _("Instrument 2:"), 1, 128, &transposition_instrument_w[1], NULL, NULL);

    thing = gtk_button_new_with_label(_("Current instrument"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(transposition_current_instrument_clicked), (gpointer)1);

    add_empty_hbox(box1);

    box1 = gtk_hbox_new(TRUE, 4);
    gtk_widget_show(box1);
    gtk_box_pack_start(GTK_BOX(box2), box1, FALSE, TRUE, 0);

    for(i = 0; i < 2; i++) {
	thing = gtk_button_new_with_label(labels4[i]);
	gtk_widget_show(thing);
	gtk_box_pack_start(GTK_BOX(box1), thing, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(thing), "clicked",
			   GTK_SIGNAL_FUNC(transposition_change_instruments),
			   GINT_TO_POINTER(i));
    }

    /* The button area */
    hbox = gtk_hbutton_box_new ();
    gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbox), 4);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
    gtk_box_pack_start (GTK_BOX (mainbox), hbox,
			FALSE, FALSE, 0);
    gtk_widget_show (hbox);

#ifdef USE_GNOME
    thing = gnome_stock_button (GNOME_STOCK_BUTTON_CLOSE);
#else
    thing = gtk_button_new_with_label (_ ("Close"));
#endif
    GTK_WIDGET_SET_FLAGS(thing, GTK_CAN_DEFAULT);
    gtk_window_set_default(GTK_WINDOW(transposition_window), thing);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC (transposition_close_requested), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show (thing);

    gtk_widget_show(transposition_window);
}
