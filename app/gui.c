
/*
 * The Real SoundTracker - main user interface handling
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <unistd.h>

#include "poll.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib.h>
#ifdef USE_GNOME
#include <gnome.h>
#endif
#ifndef NO_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#include "i18n.h"
#include "gui.h"
#include "gui-subs.h"
#include "gui-settings.h"
#include "xm.h"
#include "st-subs.h"
#include "audio.h"
#include "xm-player.h"
#include "tracker.h"
#include "main.h"
#include "keys.h"
#include "instrument-editor.h"
#include "sample-editor.h"
#include "track-editor.h"
#include "scope-group.h"
#include "module-info.h"
#include "preferences.h"
#include "menubar.h"
#include "time-buffer.h"
#include "tips-dialog.h"
#include "gui-settings.h"
#include "file-operations.h"
#include "playlist.h"
#include "extspinbutton.h"

int gui_playing_mode = 0;
int notebook_current_page = NOTEBOOK_PAGE_FILE;
GtkWidget *editing_toggle;
GtkWidget *gui_curins_name, *gui_cursmpl_name;
GtkWidget *mainwindow;
GtkWidget *alt[2], *arrow[2];
ScopeGroup *scopegroup;

static GtkWidget *gui_splash_window = NULL;
#ifndef NO_GDK_PIXBUF
static GdkPixbuf *gui_splash_logo = NULL;
static GtkWidget *gui_splash_logo_area;
#endif
static GtkWidget *gui_splash_label;
static GtkWidget *gui_splash_close_button;

static gint pipetag = -1;
static GtkWidget *mainwindow_upper_hbox, *mainwindow_second_hbox;
static GtkWidget *notebook;
static GtkWidget *spin_editpat, *spin_patlen, *spin_numchans;
static GtkWidget *cursmpl_spin;
static GtkWidget *pbutton;
static GtkAdjustment *adj_amplification, *adj_pitchbend;
static GtkWidget *spin_jump, *curins_spin, *spin_octave;
static GtkWidget *toggle_lock_editpat;
static Playlist *playlist;

guint statusbar_context_id;
GtkWidget *status_bar;
GtkWidget *st_clock;

struct f_n_l
{
    FILE *file;
    int length;
};

struct measure
{
    const gchar *title;
    gint major;
    gint minor;
};

static struct measure measure_msr[] = {
    {"2/2", 16, 8},
    {"3/2", 24, 8},
    {"4/2", 32, 8},
    {"2/4", 8,  4},
    {"3/4", 12, 4},
    {"4/4", 16, 4},
    {"5/4", 20, 4},
    {"6/4", 24, 4},
    {"7/4", 28, 4},
    {"3/8", 6, 2},
    {"4/8", 8, 2},
    {"5/8", 10, 2},
    {"6/8", 12, 2},
    {"9/8", 18, 2},
    {"12/8", 24, 2},
    {NULL}
};

#define MAXMEASURE (sizeof(measure_msr) / sizeof(struct measure) - 1)

static GtkWidget *measurewindow = NULL;
static gint measure_chosen;
    
static void gui_tempo_changed (int value);
static void gui_bpm_changed (int value);

gui_subs_slider tempo_slider = {
    N_("Tempo"), 1, 31, gui_tempo_changed, GUI_SUBS_SLIDER_SPIN_ONLY
};
gui_subs_slider bpm_slider = {
    "BPM", 32, 255, gui_bpm_changed, GUI_SUBS_SLIDER_SPIN_ONLY
};

static GdkColor gui_clipping_led_on, gui_clipping_led_off;
static GtkWidget *gui_clipping_led;
static gboolean gui_clipping_led_status;

static int editing_pat = 0;

static int gui_ewc_startstop = 0;

/* gui event handlers */
static void file_selected(GtkWidget *w, GtkFileSelection *fs);
static void current_instrument_changed(GtkSpinButton *spin);
static void current_instrument_name_changed(void);
static void current_sample_changed(GtkSpinButton *spin);
static void current_sample_name_changed(void);
static int keyevent(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void gui_editpat_changed(GtkSpinButton *spin);
static void gui_patlen_changed(GtkSpinButton *spin);
static void gui_numchans_changed(GtkSpinButton *spin);
static void notebook_page_switched(GtkNotebook *notebook, GtkNotebookPage *page, int page_num);
static void gui_adj_amplification_changed(GtkAdjustment *adj);
static void gui_adj_pitchbend_changed(GtkAdjustment *adj);

/* mixer / player communication */
static void read_mixer_pipe(gpointer data, gint source, GdkInputCondition condition);
static void wait_for_player(void);
static void play_pattern(void);
static void play_current_pattern_row(void);

/* gui initialization / helpers */
static void gui_enable(int enable);
static void offset_current_pattern(int offset);
static void offset_current_instrument(int offset);
static void offset_current_sample(int offset);

static void gui_auto_switch_page (void);

static void
editing_toggled (GtkToggleButton *button, gpointer data)
{
    tracker_redraw(tracker);
    if (button->active)
	show_editmode_status();
    else
	statusbar_update(STATUS_IDLE, FALSE);
}

static void
gui_highlight_rows_toggled (GtkWidget *widget)
{
    gui_settings.highlight_rows = GTK_TOGGLE_BUTTON(widget)->active;

    tracker_redraw(tracker);
}

void
gui_accidentals_clicked (GtkWidget *widget, gpointer data)
{
    gui_settings.sharp = !gui_settings.sharp;
    gtk_widget_hide(alt[gui_settings.sharp ? 1 : 0]);
    gtk_widget_show(alt[gui_settings.sharp ? 0 : 1]);
    tracker_redraw(tracker);
}

void
gui_direction_clicked (GtkWidget *widget, gpointer data)
{
    gui_settings.advance_cursor_in_fx_columns = !gui_settings.advance_cursor_in_fx_columns;
    gtk_widget_hide(arrow[gui_settings.advance_cursor_in_fx_columns ? 0 : 1]);
    gtk_widget_show(arrow[gui_settings.advance_cursor_in_fx_columns ? 1 : 0]);
}

static void
measure_close_requested (void)
{
#ifndef USE_GNOME
    gtk_widget_hide(measurewindow);
#endif
/* to make keyboard working immediately after closing the dialog */
    gtk_widget_grab_focus(pbutton);
}

static void
measure_dialog (gint x, gint y)
{
    GtkObject *adj;
    GtkWidget *mainbox, *thing, *vbox;
#ifndef USE_GNOME
    GtkWidget *button;
#endif
    static GtkWidget *majspin;

    if(measurewindow != NULL) {
	gtk_widget_set_uposition(measurewindow, x, y);
	gtk_widget_show(measurewindow);
	gtk_widget_grab_focus(majspin);
	return;
    }
    
#ifdef USE_GNOME
    measurewindow = gnome_dialog_new(_("Row highlighting configuration"),
			GNOME_STOCK_BUTTON_CLOSE, NULL);
    gnome_dialog_close_hides(GNOME_DIALOG(measurewindow), TRUE);
    gnome_dialog_set_close(GNOME_DIALOG(measurewindow), TRUE);
    gtk_signal_connect(GTK_OBJECT(measurewindow), "clicked",
			GTK_SIGNAL_FUNC (measure_close_requested), NULL);
    vbox = GNOME_DIALOG(measurewindow)->vbox;
#else
/* stolen from Gnome UI code. With Gnome life seemed so easy... (yaliaev) */
    measurewindow = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_window_set_title(GTK_WINDOW(measurewindow), _("Row highlighting configuration"));
    gtk_container_border_width(GTK_CONTAINER(measurewindow), 4);
    
    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
    gtk_container_add(GTK_CONTAINER(measurewindow), vbox);
    gtk_widget_show(vbox);

    gtk_window_set_policy (GTK_WINDOW (measurewindow), FALSE, 
			 FALSE, FALSE);

    mainbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start (GTK_BOX (vbox), mainbox, 
		      TRUE, TRUE, 4);
		      
    thing = gtk_hbutton_box_new ();

    gtk_button_box_set_spacing (GTK_BUTTON_BOX (thing), 8);
    
    button = gtk_button_new_with_label(_("Close"));
    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (thing), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC (measure_close_requested), NULL);

    gtk_widget_grab_default (button);
    gtk_widget_show (button);

    gtk_box_pack_end (GTK_BOX (vbox), thing, 
		    FALSE, TRUE, 0);
    gtk_widget_show (thing);

    thing = gtk_hseparator_new ();
    gtk_box_pack_end (GTK_BOX (vbox), thing, 
		      FALSE, TRUE, 4);
    gtk_widget_show (thing);
		      
#endif

    gtk_signal_connect(GTK_OBJECT (measurewindow), "delete_event",
			GTK_SIGNAL_FUNC (measure_close_requested), NULL);

    mainbox = gtk_hbox_new(FALSE, 2);
    
    gtk_widget_show(mainbox);

    thing = gtk_label_new(_("Highlight rows (major / minor):"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    add_empty_hbox(mainbox);
    adj = gtk_adjustment_new((double)gui_settings.highlight_rows_n, 1, 32, 1, 2, 0.0);
    majspin = extspinbutton_new(GTK_ADJUSTMENT(adj), 0, 0);
    gtk_box_pack_start(GTK_BOX(mainbox), majspin, FALSE, TRUE, 0);
    gtk_widget_show(majspin);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(majspin), 0);
    gtk_signal_connect(GTK_OBJECT(majspin), "changed",
		       GTK_SIGNAL_FUNC(gui_settings_highlight_rows_changed), NULL);
    adj = gtk_adjustment_new((double)gui_settings.highlight_rows_minor_n, 1, 16, 1, 2, 0.0);
    thing = extspinbutton_new(GTK_ADJUSTMENT(adj), 0, 0);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(thing), 0);
    gtk_signal_connect(GTK_OBJECT(thing), "changed",
		       GTK_SIGNAL_FUNC(gui_settings_highlight_rows_minor_changed), NULL);
    gtk_widget_set_uposition(measurewindow, x, y);
    
    gtk_box_pack_start(GTK_BOX(vbox), mainbox, TRUE, TRUE, 0);
    gtk_widget_show_all(measurewindow);
    gtk_widget_grab_focus(majspin);
}    

static void
measure_changed (GtkWidget *list, GtkWidget *child, gpointer data)
{
    if((measure_chosen = gtk_list_child_position(GTK_LIST(list), child)) <= (MAXMEASURE - 1)) {
	if ((gui_settings.highlight_rows_n != measure_msr[measure_chosen].major) ||
		(gui_settings.highlight_rows_minor_n != measure_msr[measure_chosen].minor)) {
	    gui_settings.highlight_rows_n = measure_msr[measure_chosen].major;
	    gui_settings.highlight_rows_minor_n = measure_msr[measure_chosen].minor;
	    tracker_redraw(tracker);
/* to make keyboard working immediately after chosing the measure */
	    gtk_widget_grab_focus(pbutton);
	}
    }
}

static void
popwin_hide (GtkWidget *widget, GtkWidget *thing)
{
    gint x, y, w, h, dx, dy;
    
    if (measure_chosen == MAXMEASURE){
	gdk_window_get_geometry (thing->window, &x, &y, &w, &h, NULL);
	gdk_window_get_root_origin (thing->window, &dx, &dy);
	measure_dialog(x+ dx + w + 32, y + dy + h + 24);
    }
}

static void
gui_update_title (const gchar *filename)
{
    gchar *title;

    title = g_strdup_printf("SoundTracker "VERSION": %s", g_basename(filename));
    gtk_window_set_title(GTK_WINDOW(mainwindow), title);
    g_free(title);
}

static void
gui_mixer_play_pattern (int pattern,
			int row,
			int stop_after_row)
{
    audio_ctlpipe_id i = AUDIO_CTLPIPE_PLAY_PATTERN;
    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &pattern, sizeof(pattern));
    write(audio_ctlpipe, &row, sizeof(row));
    write(audio_ctlpipe, &stop_after_row, sizeof(stop_after_row));
}

static void
gui_mixer_stop_playing (void)
{
    audio_ctlpipe_id i = AUDIO_CTLPIPE_STOP_PLAYING;
    write(audio_ctlpipe, &i, sizeof(i));
}

static void
gui_mixer_set_songpos (int songpos)
{
    audio_ctlpipe_id i = AUDIO_CTLPIPE_SET_SONGPOS;
    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &songpos, sizeof(songpos));
}

static void
gui_mixer_set_pattern (int pattern)
{
    audio_ctlpipe_id i = AUDIO_CTLPIPE_SET_PATTERN;
    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &pattern, sizeof(pattern));
}

static void
gui_load_callback (gint reply,
		   gpointer data)
{
    if(reply == 0) {
	gui_load_xm((gchar*)data);
        gui_auto_switch_page();
    }
}

static void
gui_save_callback (gint reply,
		   gpointer data)
{
    if(reply == 0) {
	statusbar_update(STATUS_SAVING_MODULE, TRUE);
	if(XM_Save(xm, (gchar*)data, FALSE)) {
	    xm->modified = 0;
	    gui_auto_switch_page();
	    statusbar_update(STATUS_MODULE_SAVED, FALSE);
	    gui_update_title ((gchar*)data);
	} else {
	    statusbar_update(STATUS_IDLE, FALSE);
	}
    }
}

static void
gui_save_song_callback (gint reply,
	                gpointer data)
{
    if(reply == 0) {
	statusbar_update(STATUS_SAVING_SONG, TRUE);
	if(XM_Save(xm, (gchar*)data, TRUE)) {
	    gui_auto_switch_page();
	    statusbar_update(STATUS_SONG_SAVED, FALSE);
	    gui_update_title ((gchar*)data);
	} else {
	    statusbar_update(STATUS_IDLE, FALSE);
	}
    }
}

static void
gui_save_wav_callback (gint reply,
		       gpointer data)
{
    if(reply == 0) {
	int l = strlen(data);
	audio_ctlpipe_id i = AUDIO_CTLPIPE_RENDER_SONG_TO_FILE;

	gui_play_stop();

	write(audio_ctlpipe, &i, sizeof(i));
	write(audio_ctlpipe, &l, sizeof(l));
	write(audio_ctlpipe, data, l + 1);
	wait_for_player();
    }
}

static void
gui_shrink_callback (gint reply,
		     gpointer data)
{
    if(!reply) {
	st_shrink_pattern((XMPattern *)data);
	gui_update_pattern_data();
	tracker_set_pattern(tracker, NULL);
	tracker_set_pattern(tracker, (XMPattern *)data);
	xm_set_modified(1);
    }
}

void
gui_shrink_pattern ()
{
    XMPattern *patt = tracker->curpattern;

    if(st_check_if_odd_are_not_empty(patt)) {
	gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
	    _("Odd pattern rows contain data which will be lost after shrinking.\n"
	      "Do you want to continue anyway?"),
	    gui_shrink_callback,
	    patt);
    } else {
	gui_shrink_callback(0, patt);
    }
}

static void
gui_expand_callback (gint reply, gpointer data)
{
    if(!reply) {
	st_expand_pattern((XMPattern *)data);
	gui_update_pattern_data();
	tracker_set_pattern(tracker, NULL);
	tracker_set_pattern(tracker, (XMPattern *)data);
	xm_set_modified(1);
    }
}

void
gui_expand_pattern ()
{
    XMPattern *patt = tracker->curpattern;

    if(patt->length > 128) {
	gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
	    _("The pattern is too long for expanding.\n"
	      "Some data at the end of the pattern will be lost.\n"
	      "Do you want to continue anyway?"),
	    gui_expand_callback,
	    patt);
    } else {
	gui_expand_callback(0, patt);
    }
}

static void
gui_pattern_length_correct (gint reply, gpointer data)
{
    XMPattern *patt = tracker->curpattern;
    struct f_n_l *ddata = (struct f_n_l*) data;
    int length = ddata->length;
    FILE *f = ddata->file;

    switch (reply) {
    case 0: /* Yes! */
	st_set_pattern_length (patt, length);
	gui_update_pattern_data ();
    case 1: /* No! */
	if (xm_xp_load (f, length, patt, xm)) {
	    tracker_set_pattern (tracker, NULL);
	    tracker_set_pattern (tracker, patt);
	    xm_set_modified(1);
	}
    case 2: /* Cancel, do nothing */
    default:
	break;
    }
    fclose (f);
}

static void
file_selected (GtkWidget *w,
	       GtkFileSelection *fs)
{
    static    struct f_n_p fnp; /* we need static here 'cause we pass these */
    static    struct f_n_l fnl; /* structures to callback functions */
    int length;

    gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

    gtk_widget_hide(GTK_WIDGET(fs));

    if(!file_selection_is_valid(fn)) {
	/* No file was actually selected. */
	gnome_error_dialog(_("No file selected."));
	return;
    }

    if(fs == GTK_FILE_SELECTION(fileops_dialogs[DIALOG_LOAD_MOD])) {
	file_selection_save_path(fn, gui_settings.loadmod_path);
	if(xm->modified) {
	    gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
				      _("Are you sure you want to free the current project?\nAll changes will be lost!"),
				      gui_load_callback,
				      fn);
	} else {
	    gui_load_callback(0, fn);
	}
    } else if(fs == GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_MOD])) {
	FILE *f = fopen(fn, "r");

	file_selection_save_path(fn, gui_settings.savemod_path);

	if(f != NULL) {
	    fclose(f);
	    gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
				      _("Are you sure you want to overwrite the file?"),
				      gui_save_callback,
				      fn);
	} else {
	    gui_save_callback(0, fn);
		fileops_refresh_list(fs, FALSE);
	}
    } else if(fs == GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_SONG_AS_XM])) {
	FILE *f = fopen(fn, "r");

	file_selection_save_path(fn, gui_settings.savesongasxm_path);

	if(f != NULL) {
	    fclose(f);
	    gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
				      _("Are you sure you want to overwrite the file?"),
				      gui_save_song_callback,
				      fn);
	} else {
	    gui_save_song_callback(0, fn);
		fileops_refresh_list(fs, FALSE);
	}
    } else if(fs == GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_MOD_AS_WAV])) {
	FILE *f = fopen(fn, "r");

	file_selection_save_path(fn, gui_settings.savemodaswav_path);

	if(f != NULL) {
	    fclose(f);
	    gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
				      _("Are you sure you want to overwrite the file?"),
				      gui_save_wav_callback,
				      fn);
	} else {
	    gui_save_wav_callback(0, fn);
		fileops_refresh_list(fs, FALSE);
	}
    } else if(fs == GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_PATTERN])) {
	FILE *f = fopen (fn, "r");
	
	file_selection_save_path(fn, gui_settings.savepat_path);
	
	fnp.name = fn;
	fnp.pattern = tracker->curpattern;
	fnp.xm = xm;
	if(f != NULL) {
	    fclose(f);
		gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
                    			_("Are you sure you want to overwrite the file?"),
                    			xm_xp_save,
                                 	&fnp);
	    } else {
    		xm_xp_save (0, &fnp);
			fileops_refresh_list(fs, FALSE);
	    }
    } else if(fs == GTK_FILE_SELECTION(fileops_dialogs[DIALOG_LOAD_PATTERN])) {
	XMPattern *patt = tracker->curpattern;
	FILE *f = fopen (fn, "r");

	file_selection_save_path(fn, gui_settings.loadpat_path);
	
	if (!f) gnome_error_dialog (_("Error when opening pattern file!"));
	else { if (xm_xp_load_header (f, &length)) {
	    if (length == patt->length) {
		if (xm_xp_load (f, length, patt, xm)) {
		    tracker_set_pattern (tracker, NULL);
		    tracker_set_pattern (tracker, patt);
		    xm_set_modified(1);
		}
		fclose (f);
	    } else {
		fnl.file = f;
		fnl.length = length;
		gui_yes_no_cancel_modal (GTK_WIDGET(mainwindow),
                                	_("The length of the pattern being loaded doesn't match with that of current pattern in module.\nDo you want to change the current pattern length?"),
                            		gui_pattern_length_correct,
                                	&fnl);
	    }
	} else fclose (f); }
    }
}

static void
current_instrument_changed (GtkSpinButton *spin)
{
    int m = xm_get_modified();
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];
    STSample *s = &i->samples[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    instrument_editor_set_instrument(i);
    sample_editor_set_sample(s);
    modinfo_set_current_instrument(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin)) - 1);
    xm_set_modified(m);
}

static void
current_instrument_name_changed (void)
{
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];

    strncpy(i->name, gtk_entry_get_text(GTK_ENTRY(gui_curins_name)), 22);
    i->name[22] = 0;
    modinfo_update_instrument(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1);
    xm_set_modified(1);
}

static void
current_sample_changed (GtkSpinButton *spin)
{
    int m = xm_get_modified();
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];
    STSample *s = &i->samples[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    gtk_entry_set_text(GTK_ENTRY(gui_cursmpl_name), s->name);
    sample_editor_set_sample(s);
    modinfo_set_current_sample(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin)));
    xm_set_modified(m);
}

static void
current_sample_name_changed (void)
{
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];
    STSample *s = &i->samples[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    strncpy(s->name, gtk_entry_get_text(GTK_ENTRY(gui_cursmpl_name)), 22);
    s->name[22] = 0;
    modinfo_update_sample(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin)));
    xm_set_modified(1);
}

static gboolean
gui_handle_standard_keys (int shift,
			  int ctrl,
			  int alt,
			  guint32 keyval)
{
    gboolean handled = FALSE, b;
    int currpos;

    switch (keyval) {
    case GDK_F1 ... GDK_F7:
	if(!shift && !ctrl && !alt) {
	    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_octave), keyval - GDK_F1);
	    handled = TRUE;
	}
	break;
    case '1' ... '8':
	if(ctrl) {
	    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_jump), keyval - '0');
	    handled = TRUE;
        }
        if(alt) {
            switch(keyval)
            {
	    case '1' ... '5':
                gtk_notebook_set_page(GTK_NOTEBOOK(notebook), keyval - '1');
                break;
	    default:
		break;
            }
	    handled = TRUE;
        }
	break;
    case GDK_Left:
	if(ctrl) {
	    /* previous instrument */
	    offset_current_instrument(shift ? -5 : -1);
	    handled = TRUE;
	} else if(alt) {
	    /* previous pattern */
	    offset_current_pattern(shift ? -10 : -1);
	    handled = TRUE;
	} else if(shift){
	    /* previous position */
	    currpos = playlist_get_position (playlist);
	    if((--currpos) >= 0 ) {
		playlist_set_position (playlist, currpos);
		handled = TRUE;
	    }
	}
	break;
    case GDK_Right:
	if(ctrl) {
	    /* next instrument */
	    offset_current_instrument(shift ? 5 : 1);
	    handled = TRUE;
	} else if(alt) {
	    /* next pattern */
	    offset_current_pattern(shift ? 10 : 1);
	    handled = TRUE;
	} else if(shift){
	    /* next position */
	    currpos = playlist_get_position (playlist);
	    if ((++currpos) < xm->song_length ) {
		playlist_set_position (playlist, currpos);
		handled = TRUE;
	    }
        }
	break;
    case GDK_Up:
	if(ctrl) {
	    /* next sample */
	    offset_current_sample(shift ? 4 : 1);
	    handled = TRUE;
	}
	break;
    case GDK_Down:
	if(ctrl) {
	    /* previous sample */
	    offset_current_sample(shift ? -4 : -1);
	    handled = TRUE;
	}
	break;
    case GDK_Alt_R:
    case GDK_Meta_R:
    case GDK_Super_R:
    case GDK_Hyper_R:
    case GDK_Mode_switch: /* well... this is X :D */
    case GDK_Multi_key:
    case GDK_ISO_Level3_Shift:
	play_pattern();
	if(shift)
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), TRUE);
	handled = TRUE;
	break;
    case GDK_Control_R:
	play_song();
	if(shift)
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), TRUE);
	handled = TRUE;
	break;
    case GDK_Menu:
	play_current_pattern_row();
	break;
    case ' ':
        if(ctrl || alt || shift)
            break;
	b = GUI_ENABLED;
	if (!b)
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), FALSE);

	gui_play_stop();
	if(notebook_current_page != NOTEBOOK_PAGE_SAMPLE_EDITOR
	   && notebook_current_page != NOTEBOOK_PAGE_INSTRUMENT_EDITOR
	   && notebook_current_page != NOTEBOOK_PAGE_FILE
	   && notebook_current_page != NOTEBOOK_PAGE_MODULE_INFO) {
	    if(b) {
		/* toggle editing mode (only if we haven't been in playing mode) */
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(editing_toggle), !GUI_EDITING);
	    }
	}
	handled = TRUE;
	break;
    case GDK_Escape:
        if(ctrl || alt || shift)
            break;
	/* toggle editing mode, even if we're in playing mode */
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(editing_toggle), !GUI_EDITING);
	handled = TRUE;
	break;
    }

    return handled;
}

static int
keyevent (GtkWidget *widget,
	  GdkEventKey *event,
	  gpointer data)
{
    static gboolean (*handle_page_keys[])(int,int,int,guint32,gboolean) = {
	fileops_page_handle_keys,
	track_editor_handle_keys,
	instrument_editor_handle_keys,
	sample_editor_handle_keys,
	modinfo_page_handle_keys,
    };
    gboolean pressed = (gboolean)GPOINTER_TO_INT(data);
    gboolean handled = FALSE;
    gboolean entry_focus = GTK_IS_ENTRY(GTK_WINDOW(mainwindow)->focus_widget);

    if(!entry_focus && GTK_WIDGET_VISIBLE(notebook)) {
	int shift = event->state & GDK_SHIFT_MASK;
	int ctrl = event->state & GDK_CONTROL_MASK;
	int alt = event->state & GDK_MOD1_MASK;

	if(pressed)
	    handled = gui_handle_standard_keys(shift, ctrl, alt, event->keyval);
	handled = handled || handle_page_keys[notebook_current_page](shift, ctrl, alt, event->keyval, pressed);

	if(!handled) switch(event->keyval) {
	    /* from gtk+-1.2.8's gtkwindow.c. These keypresses need to
	       be stopped in any case. */
	case GDK_Up:
	case GDK_Down:
	case GDK_Left:
	case GDK_Right:
	case GDK_KP_Up:
	case GDK_KP_Down:
	case GDK_KP_Left:
	case GDK_KP_Right:
	case GDK_Tab:
	case GDK_ISO_Left_Tab:
	    handled = TRUE;
	}

	if(handled) {
	    if(pressed) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
	    } else {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_release_event");
	    }
	}
    } else {
	if(pressed) {
	    switch(event->keyval) {
	    case GDK_Tab:
	    case GDK_Return:
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
		gtk_window_set_focus(GTK_WINDOW(mainwindow), NULL);
		break;
	    }
	}
    }

    return TRUE;
}

static void
gui_playlist_position_changed (Playlist *p,
			       int newpos)
{
    if(gui_playing_mode == PLAYING_SONG) {
	// This will only be executed when the user changes the song position manually
	event_waiter_start(audio_songpos_ew);
	gui_mixer_set_songpos(newpos);
    } else {
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_lock_editpat))) {
	    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_editpat),
				      playlist_get_nth_pattern(p, newpos));
	}
    }
}

static void
gui_playlist_restart_position_changed (Playlist *p,
				       int pos)
{
    xm->restart_position = pos;
    xm_set_modified(1);
}

static void
gui_playlist_entry_changed (Playlist *p,
			    int pos,
			    int pat)
{
    int i;

    if(pos != -1) {
	xm->pattern_order_table[pos] = pat;
	if(pos == playlist_get_position(p)
	    && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_lock_editpat))) {
	    gui_set_current_pattern(pat);
	}
    } else {
	for(i = 0; i < xm->song_length; i++) {
	    xm->pattern_order_table[i] = playlist_get_nth_pattern(p, i);
	}
    }

    xm_set_modified(1);
}

static void
gui_playlist_song_length_changed (Playlist *p,
				  int length)
{
    xm->song_length = length;
    gui_playlist_entry_changed(p, -1, -1);
}

static void
gui_editpat_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    if(n != editing_pat) {
	gui_set_current_pattern(n);

	/* If we are in 'playing pattern' mode and asynchronous
	 * editing is disabled, make the audio thread jump to the new
	 * pattern, too. I think it would be cool to have this for
	 * 'playing song' mode, too, but then modifications in
	 * gui_update_player_pos() will be necessary. */
	if(gui_playing_mode == PLAYING_PATTERN && !ASYNCEDIT) {
	    gui_mixer_set_pattern(n);
	}
    }
}

static void
gui_patlen_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);
    XMPattern *pat = &xm->patterns[editing_pat];

    if(n != pat->length) {
	st_set_pattern_length(pat, n);
	tracker_set_pattern(tracker, NULL);
	tracker_set_pattern(tracker, pat);
	xm_set_modified(1);
    }
}

static void
gui_numchans_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    if(n & 1) {
	gtk_spin_button_set_value(spin, --n);
	return;
    }

    if(xm->num_channels != n) {
	gui_play_stop();
	tracker_set_pattern(tracker, NULL);
	st_set_num_channels(xm, n);
	gui_init_xm(0);
	xm_set_modified(1);
    }
}

static void
gui_tempo_changed (int value)
{
    audio_ctlpipe_id i;
    xm->tempo = value;
    xm_set_modified(1);
    if(gui_playing_mode) {
	event_waiter_start(audio_tempo_ew);
    }
    i = AUDIO_CTLPIPE_SET_TEMPO;
    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &value, sizeof(value));
}

static void
gui_bpm_changed (int value)
{
    audio_ctlpipe_id i;
    xm->bpm = value;
    xm_set_modified(1);
    if(gui_playing_mode) {
	event_waiter_start(audio_bpm_ew);
    }
    i = AUDIO_CTLPIPE_SET_BPM;
    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &value, sizeof(value));
}

static void
gui_adj_amplification_changed (GtkAdjustment *adj)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_SET_AMPLIFICATION;
    float b = 8.0 - adj->value;

    write(audio_ctlpipe, &a, sizeof(a));
    write(audio_ctlpipe, &b, sizeof(b));
}

static void
gui_adj_pitchbend_changed (GtkAdjustment *adj)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_SET_PITCHBEND;
    float b = adj->value;

    write(audio_ctlpipe, &a, sizeof(a));
    write(audio_ctlpipe, &b, sizeof(b));
}

static void
gui_reset_pitch_bender (void)
{
    gtk_adjustment_set_value(adj_pitchbend, 0.0);
}

static void
notebook_page_switched (GtkNotebook *notebook,
			GtkNotebookPage *page,
			int page_num)
{
    notebook_current_page = page_num;
}

/* gui_update_player_pos() is responsible for updating various GUI
   features according to the current player position. ProTracker and
   FastTracker scroll the patterns while the song is playing, and we
   need the time-buffer and event-waiter interfaces here for correct
   synchronization (we're called from
   track-editor.c::tracker_timeout() which hands us time-correct data)

   We have an ImpulseTracker-like editing mode as well ("asynchronous
   editing"), which disables the scrolling, but still updates the
   current song position spin buttons, for example. */

void
gui_update_player_pos (const audio_player_pos *p)
{
    int m = xm_get_modified();

    if(gui_playing_mode == PLAYING_NOTE)
	return;

    if(gui_playing_mode == PLAYING_SONG) {
	if(event_waiter_ready(audio_songpos_ew, p->time)) {
	    /* The following check prevents excessive calls of set_position() */
	    if(p->songpos != playlist_get_position(playlist)) {
		playlist_freeze_signals(playlist);
		playlist_set_position(playlist, p->songpos);
		playlist_thaw_signals(playlist);
	    }
	}
	if(!ASYNCEDIT) {
	    /* The following is a no-op if we're already in the right pattern */
	    gui_set_current_pattern(xm->pattern_order_table[p->songpos]);
	}
    }

    if(!ASYNCEDIT) {
	tracker_set_patpos(tracker, p->patpos);

	if(notebook_current_page == 0) {
	    /* Prevent accumulation of X drawing primitives */
	    gdk_flush();
	}
    }

    if(gui_settings.tempo_bpm_update) {
	if(event_waiter_ready(audio_tempo_ew, p->time)) {
	    tempo_slider.update_without_signal = TRUE;
	    gui_subs_set_slider_value(&tempo_slider, p->tempo);
	    tempo_slider.update_without_signal = FALSE;
	}

	if(event_waiter_ready(audio_bpm_ew, p->time)) {
	    bpm_slider.update_without_signal = TRUE;
	    gui_subs_set_slider_value(&bpm_slider, p->bpm);
	    bpm_slider.update_without_signal = FALSE;
	}
    }

    xm_set_modified(m);
}

void
gui_clipping_indicator_update (double songtime)
{
    if(songtime < 0.0) {
	gui_clipping_led_status = 0;
    } else {
	audio_clipping_indicator *c = time_buffer_get(audio_clipping_indicator_tb, songtime);
	gui_clipping_led_status = c && c->clipping;
    }
    gtk_widget_draw(gui_clipping_led, NULL);
}

static void
read_mixer_pipe (gpointer data,
		 gint source,
		 GdkInputCondition condition)
{
    audio_backpipe_id a;
    struct pollfd pfd = { source, POLLIN, 0 };
    int x;

    static char *msgbuf = NULL;
    static int msgbuflen = 0;

  loop:
    if(poll(&pfd, 1, 0) <= 0)
	return;

    read(source, &a, sizeof(a));
//    printf("read %d\n", a);

    switch(a) {
    case AUDIO_BACKPIPE_PLAYING_STOPPED:
        statusbar_update(STATUS_IDLE, FALSE);
#ifdef USE_GNOME
        gtk_clock_stop(GTK_CLOCK(st_clock));
#endif

        if(gui_ewc_startstop > 0) {
	    /* can be equal to zero when the audio subsystem decides to stop playing on its own. */
	    gui_ewc_startstop--;
	}
	gui_playing_mode = 0;
	scope_group_stop_updating(scopegroup);
	tracker_stop_updating();
	sample_editor_stop_updating();
	gui_enable(1);
	break;

    case AUDIO_BACKPIPE_PLAYING_STARTED:
        statusbar_update(STATUS_PLAYING_SONG, FALSE);
	/* fall through */

    case AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED:
        if(a == AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED)
	    statusbar_update(STATUS_PLAYING_PATTERN, FALSE);
#ifdef USE_GNOME
        gtk_clock_set_seconds(GTK_CLOCK(st_clock), 0);
        gtk_clock_start(GTK_CLOCK(st_clock));
#endif

        gui_ewc_startstop--;
	gui_playing_mode = (a == AUDIO_BACKPIPE_PLAYING_STARTED) ? PLAYING_SONG : PLAYING_PATTERN;
	if(!ASYNCEDIT) {
	    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(editing_toggle), FALSE);
	}
	gui_enable(0);
	scope_group_start_updating(scopegroup);
	tracker_start_updating();
	sample_editor_start_updating();
	break;

    case AUDIO_BACKPIPE_PLAYING_NOTE_STARTED:
	gui_ewc_startstop--;
	if(!gui_playing_mode) {
	    gui_playing_mode = PLAYING_NOTE;
	    scope_group_start_updating(scopegroup);
	    tracker_start_updating();
	    sample_editor_start_updating();
	}
	break;

    case AUDIO_BACKPIPE_DRIVER_OPEN_FAILED:
	gui_ewc_startstop--;
        break;

    case AUDIO_BACKPIPE_ERROR_MESSAGE:
    case AUDIO_BACKPIPE_WARNING_MESSAGE:
        statusbar_update(STATUS_IDLE, FALSE);
        readpipe(source, &x, sizeof(x));
	if(msgbuflen < x + 1) {
	    g_free(msgbuf);
	    msgbuf = g_new(char, x + 1);
	    msgbuflen = x + 1;
	}
	readpipe(source, msgbuf, x + 1);
	if(a == AUDIO_BACKPIPE_ERROR_MESSAGE)
            gnome_error_dialog(msgbuf);
	else
            gnome_warning_dialog(msgbuf);
	break;

    default:
	fprintf(stderr, "\n\n*** read_mixer_pipe: unexpected backpipe id %d\n\n\n", a);
	g_assert_not_reached();
	break;
    }

    goto loop;
}

static void
wait_for_player (void)
{
    struct pollfd pfd = { audio_backpipe, POLLIN, 0 };

    gui_ewc_startstop++;
    while(gui_ewc_startstop != 0) {
	g_return_if_fail(poll(&pfd, 1, -1) > 0);
	read_mixer_pipe(NULL, audio_backpipe, 0);
    }
}

void
play_song (void)
{
    int sp = playlist_get_position(playlist);
    int pp = 0;
    audio_ctlpipe_id i = AUDIO_CTLPIPE_PLAY_SONG;

    g_assert(xm != NULL);

    gui_play_stop();

    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &sp, sizeof(sp));
    write(audio_ctlpipe, &pp, sizeof(pp));
    wait_for_player();
}

static void
play_pattern (void)
{
    gui_play_stop();
    gui_mixer_play_pattern(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)), 0, 0);
    wait_for_player();
}

static void
play_current_pattern_row (void)
{
    gui_play_stop();
    gui_mixer_play_pattern(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)), tracker->patpos, 1);
    gui_ewc_startstop++;
}

void
gui_play_stop (void)
{
    gui_mixer_stop_playing();
    wait_for_player();
}

void
gui_init_xm (int new_xm)
{
    int m = xm_get_modified();
    audio_ctlpipe_id i;

    i = AUDIO_CTLPIPE_INIT_PLAYER;
    write(audio_ctlpipe, &i, sizeof(i));
    tracker_reset(tracker);
    if(new_xm) {
	gui_playlist_initialize();
	editing_pat = -1;
	gui_set_current_pattern(xm->pattern_order_table[0]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(curins_spin), 1);
	current_instrument_changed(GTK_SPIN_BUTTON(curins_spin));
	modinfo_set_current_instrument(0);
	modinfo_set_current_sample(0);
	modinfo_update_all();
    } else {
	i = editing_pat;
	editing_pat = -1;
	gui_set_current_pattern(i);
    }
    gui_subs_set_slider_value(&tempo_slider, xm->tempo);
    gui_subs_set_slider_value(&bpm_slider, xm->bpm);
    track_editor_set_num_channels(xm->num_channels);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_numchans), xm->num_channels);
    scope_group_set_num_channels(scopegroup, xm->num_channels);
    xm_set_modified(m);
}

void
gui_free_xm (void)
{
    gui_play_stop();
    instrument_editor_set_instrument(NULL);
    sample_editor_set_sample(NULL);
    tracker_set_pattern(tracker, NULL);
    XM_Free(xm);
    xm = NULL;
}

void
gui_new_xm (void)
{
    xm = XM_New();

    if(!xm) {
	fprintf(stderr, "Whooops, having memory problems?\n");
	exit(1);
    }
    gui_init_xm(1);
}

void
gui_load_xm (const char *filename)
{
    statusbar_update(STATUS_LOADING_MODULE, TRUE);

    gui_free_xm();
    xm = File_Load(filename);

    if(!xm) {
	gui_new_xm();
	statusbar_update(STATUS_IDLE, FALSE);
    } else {
	gui_init_xm(1);
	statusbar_update(STATUS_MODULE_LOADED, FALSE);
	gui_update_title (filename);
    }
}

void
gui_play_note (int channel,
	       int note)
{
    int instrument = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
    audio_ctlpipe_id a = AUDIO_CTLPIPE_PLAY_NOTE;

    write(audio_ctlpipe, &a, sizeof(a));
    write(audio_ctlpipe, &channel, sizeof(channel));
    write(audio_ctlpipe, &note, sizeof(note));
    write(audio_ctlpipe, &instrument, sizeof(instrument));
    gui_ewc_startstop++;
}

void
gui_play_note_full (unsigned channel,
		    unsigned note,
		    STSample *sample,
		    guint32 offset,
		    guint32 count)
{
    int x;
    audio_ctlpipe_id a = AUDIO_CTLPIPE_PLAY_NOTE_FULL;

    g_assert(sizeof(int) == sizeof(unsigned));

    write(audio_ctlpipe, &a, sizeof(a));
    write(audio_ctlpipe, &channel, sizeof(channel));
    write(audio_ctlpipe, &note, sizeof(note));
    write(audio_ctlpipe, &sample, sizeof(sample));
    x = offset; write(audio_ctlpipe, &x, sizeof(x));
    x = count; write(audio_ctlpipe, &x, sizeof(x));
    gui_ewc_startstop++;
}

void
gui_play_note_keyoff (int channel)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_PLAY_NOTE_KEYOFF;
    write(audio_ctlpipe, &a, sizeof(a));
    write(audio_ctlpipe, &channel, sizeof(channel));
}

static void
gui_enable (int enable)
{
    if(!ASYNCEDIT) {
	gtk_widget_set_sensitive(vscrollbar, enable);
	gtk_widget_set_sensitive(spin_patlen, enable);
    }
    playlist_enable(playlist, enable);
}

void
gui_set_current_pattern (int p)
{
    int m;

    if(editing_pat == p)
	return;

    m = xm_get_modified();

    editing_pat = p;
    tracker_set_pattern(tracker, &xm->patterns[p]);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_editpat), p);
    gui_update_pattern_data();

    xm_set_modified(m);
}

void
gui_update_pattern_data (void)
{
    int p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat));

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_patlen), xm->patterns[p].length);
}

int
gui_get_current_pattern (void)
{
    return editing_pat;
}

static void
offset_current_pattern (int offset)
{
    int nv;

    nv = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)) + offset;
    if(nv < 0)
	nv = 0;
    else if(nv > 255)
	nv = 255;

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_editpat), nv);
}

void
gui_set_current_instrument (int n)
{
    int m = xm_get_modified();
    g_return_if_fail(n >= 1 && n <= 128);
    if(n != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(curins_spin), n);
    }
    xm_set_modified(m);
}

void
gui_set_current_sample (int n)
{
    int m = xm_get_modified();
    g_return_if_fail(n >= 0 && n <= 15);
    if(n != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(cursmpl_spin), n);
    }
    xm_set_modified(m);
}

int
gui_get_current_sample (void)
{
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin));
}

int
gui_get_current_octave_value (void)
{
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_octave));
}

int
gui_get_current_jump_value (void)
{
    if(!GUI_ENABLED && !ASYNCEDIT)
	return 0;
    else
	return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_jump));
}

void
gui_set_jump_value (int value)
{
    if(GUI_ENABLED || ASYNCEDIT){
 	g_return_if_fail (value >= 0 && value <= 16);
 	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_jump), value);
    }
}

int
gui_get_current_instrument (void)
{
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
}

static void
offset_current_instrument (int offset)
{
    int nv, v;

    v = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
    nv = v + offset;

    if(nv < 1)
	nv = 1;
    else if(nv > 128)
	nv = 128;

    gui_set_current_instrument(nv);
}

static void
offset_current_sample (int offset)
{
    int nv, v;

    v = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin));
    nv = v + offset;

    if(nv < 0)
	nv = 0;
    else if(nv > 15)
	nv = 15;

    gui_set_current_sample(nv);
}

static void
gui_add_free_pattern (GtkWidget *w, Playlist *p)
{
    int pos = playlist_get_position(p) + 1;

    if(p->length < p->max_length) {
	int n = st_find_first_unused_and_empty_pattern(xm);

	if(n != -1) {
	    playlist_insert_pattern(p, pos, n);
	    playlist_set_position(p, pos);
	}
    }
}

static void
gui_add_free_pattern_and_copy (GtkWidget *w, Playlist *p)
{
    int pos = playlist_get_position(p) + 1;

    if(p->length < p->max_length) {
	int n = st_find_first_unused_and_empty_pattern(xm);
	int c = gui_get_current_pattern();

	if(n != -1) {
	    st_copy_pattern(&xm->patterns[n], &xm->patterns[c]);
	    playlist_insert_pattern(p, pos, n);
	    playlist_set_position(p, pos);
	}
    }
}

void
gui_get_text_entry (int length,
		    void(*changedfunc)(),
		    GtkWidget **widget)
{
    GtkWidget *thing;

    thing = gtk_entry_new_with_max_length(length);

    gtk_signal_connect_after(GTK_OBJECT(thing), "changed",
			     GTK_SIGNAL_FUNC(changedfunc), NULL);

    *widget = thing;
}

void
gui_playlist_initialize (void)
{
    int i;

    playlist_freeze_signals(playlist);
    playlist_freeze(playlist);
    playlist_set_position(playlist, 0);
    playlist_set_length(playlist, xm->song_length);
    for(i = 0; i < xm->song_length; i++) {
	playlist_set_nth_pattern(playlist, i, xm->pattern_order_table[i]);
    }
    playlist_set_restart_position(playlist, xm->restart_position);
    playlist_thaw(playlist);
    playlist_thaw_signals(playlist);
}

static gint
gui_clipping_led_event (GtkWidget *thing,
			GdkEvent *event)
{
    static GdkGC *clipping_led_gc = NULL;

    if(!clipping_led_gc)
	clipping_led_gc = gdk_gc_new(thing->window);

    switch (event->type) {
    case GDK_MAP:
    case GDK_EXPOSE:
	gdk_gc_set_foreground(clipping_led_gc, gui_clipping_led_status ? &gui_clipping_led_on : &gui_clipping_led_off);
	gdk_draw_rectangle(thing->window, clipping_led_gc, 1, 0, 0, -1, -1);
    default:
	break;
    }
    
    return 0;
}

void
gui_go_to_fileops_page (void)
{
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook),
			  0);
}

void
gui_auto_switch_page (void)
{
    if(gui_settings.auto_switch)
        gtk_notebook_set_page(GTK_NOTEBOOK(notebook),
                              1);
}

#ifndef NO_GDK_PIXBUF
static gint
gui_splash_logo_expose (GtkWidget *widget,
			GdkEventExpose *event,
			gpointer data)
{
    gdk_pixbuf_render_to_drawable (gui_splash_logo,
				   widget->window,
				   widget->style->black_gc,
				   event->area.x, event->area.y,
				   event->area.x, event->area.y,
				   event->area.width, event->area.height,
				   GDK_RGB_DITHER_NORMAL,
				   0, 0);

    return TRUE;
}
#endif

static void
gui_splash_close (void)
{
    gtk_widget_destroy(gui_splash_window);
    gui_splash_window = NULL;
}

static void
gui_splash_set_label (const gchar *text,
		      gboolean update)
{
    char buf[256];

    strcpy(buf, "SoundTracker v" VERSION " - ");
    strncat(buf, text, 255-sizeof(buf));

    gtk_label_set_text(GTK_LABEL(gui_splash_label), buf);

    while(update && gtk_events_pending ()) {
	gtk_main_iteration ();
    }
}

int
gui_splash (int argc,
	    char *argv[])
{
    GtkWidget *vbox, *thing;
#ifndef NO_GDK_PIXBUF
    GtkWidget *hbox, *logo_area, *frame;
#endif

#ifdef USE_GNOME
    gnome_init("SoundTracker", VERSION, argc, argv);
    gdk_rgb_init();
#else
    gtk_init(&argc, &argv);
    gdk_rgb_init();
#endif

    gui_splash_window = gtk_window_new (GTK_WINDOW_DIALOG);

    gtk_window_set_title (GTK_WINDOW(gui_splash_window), _("SoundTracker Startup"));
//    gtk_window_set_wmclass (GTK_WINDOW (gui_splash_window), "soundtracker_startup", "SoundTracker");
    gtk_window_set_position (GTK_WINDOW (gui_splash_window), GTK_WIN_POS_CENTER);
    gtk_window_set_policy (GTK_WINDOW (gui_splash_window), FALSE, FALSE, FALSE);
    gtk_window_set_modal(GTK_WINDOW(gui_splash_window), TRUE);

    gtk_signal_connect(GTK_OBJECT (gui_splash_window), "delete_event",
		       GTK_SIGNAL_FUNC (gui_splash_close), NULL);

    vbox = gtk_vbox_new (FALSE, 4);
    gtk_container_add (GTK_CONTAINER (gui_splash_window), vbox);

    gtk_container_border_width(GTK_CONTAINER(vbox), 4);

    /* Show splash screen if enabled and image available. */

#ifndef NO_GDK_PIXBUF
    gui_splash_logo = gdk_pixbuf_new_from_file(PREFIX"/share/soundtracker/soundtracker_splash.png");
    if(gui_splash_logo) {
	thing = gtk_hseparator_new();
	gtk_widget_show(thing);
	gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	add_empty_vbox(hbox);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(hbox), frame, FALSE, TRUE, 0);
	gtk_widget_show(frame);

	gui_splash_logo_area = logo_area = gtk_drawing_area_new ();
	gtk_container_add (GTK_CONTAINER(frame), logo_area);
	gtk_widget_show(logo_area);

	add_empty_vbox(hbox);

	gtk_signal_connect (GTK_OBJECT (logo_area), "expose_event",
			    GTK_SIGNAL_FUNC (gui_splash_logo_expose),
			    NULL);

	gtk_drawing_area_size (GTK_DRAWING_AREA (logo_area),
			       gdk_pixbuf_get_width(gui_splash_logo),
			       gdk_pixbuf_get_height(gui_splash_logo));
    }
#endif

    /* Show version number. */

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

    gui_splash_label = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gui_splash_set_label(_("Loading..."), FALSE);

    /* Show tips if enabled. */

    if(tips_dialog_show_tips) {
	GtkWidget *tipsbox = tips_dialog_get_vbox();

	if(tipsbox) {
	    thing = gtk_hseparator_new();
	    gtk_widget_show(thing);
	    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

	    gtk_box_pack_start(GTK_BOX(vbox), tipsbox, FALSE, FALSE, 0);
	    gtk_widget_show(tipsbox);
	}
    }

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

    if(
#ifndef NO_GDK_PIXBUF
       gui_splash_logo ||
#endif
       tips_dialog_show_tips) {
	gui_splash_close_button = thing = gtk_button_new_with_label(_("Use SoundTracker!"));
	gtk_widget_show(thing);
	gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT (thing), "clicked",
			   GTK_SIGNAL_FUNC(gui_splash_close), NULL);
	gtk_widget_set_sensitive(thing, FALSE);
    }

    gtk_widget_show(vbox);
    gtk_widget_show(gui_splash_window);

    gui_splash_set_label(_("Loading..."), TRUE);

    return 1;
}

int
gui_final (int argc,
	   char *argv[])
{
    GtkWidget *thing, *mainvbox, *table, *hbox, *frame, *mainvbox0, *pmw, *vbox, *list, *entry;
    GdkColormap *colormap;
    GtkStyle *style;
    GdkPixmap *pm;
    GdkBitmap *mask;
    gint i, wdth, cur, selected;
    GList *glist;
    gchar *other;
#ifdef USE_GNOME
    GtkWidget *dockitem;
#endif

    pipetag = gdk_input_add(audio_backpipe, GDK_INPUT_READ, read_mixer_pipe, NULL);

#ifdef USE_GNOME
    mainwindow = gnome_app_new("SoundTracker", "SoundTracker " VERSION);
#else
    mainwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (mainwindow), "SoundTracker " VERSION);
#endif

    gtk_signal_connect (GTK_OBJECT (mainwindow), "delete_event",
			GTK_SIGNAL_FUNC (menubar_quit_requested), NULL);

    if(gui_splash_window) {
	gtk_window_set_transient_for(GTK_WINDOW(gui_splash_window),
				     GTK_WINDOW(mainwindow));
    }

    if(gui_settings.st_window_x != -666) {
	gtk_window_set_default_size (GTK_WINDOW (mainwindow),
				     gui_settings.st_window_w,
				     gui_settings.st_window_h);
	gtk_widget_set_uposition (GTK_WIDGET (mainwindow),
				  gui_settings.st_window_x,
				  gui_settings.st_window_y);
    }

    fileops_dialogs[DIALOG_LOAD_MOD] = file_selection_create(_("Load XM..."), file_selected);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_LOAD_MOD]), gui_settings.loadmod_path);
    fileops_dialogs[DIALOG_SAVE_MOD] = file_selection_create(_("Save XM..."), file_selected);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_MOD]), gui_settings.savemod_path);
#if USE_SNDFILE || !defined (NO_AUDIOFILE)
    fileops_dialogs[DIALOG_SAVE_MOD_AS_WAV] = file_selection_create(_("Render module as WAV..."), file_selected);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_MOD_AS_WAV]), gui_settings.savemodaswav_path);
#endif
    fileops_dialogs[DIALOG_SAVE_SONG_AS_XM] = file_selection_create(_("Save song as XM..."), file_selected);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_SONG_AS_XM]), gui_settings.savesongasxm_path);
    fileops_dialogs[DIALOG_LOAD_PATTERN] = file_selection_create(_("Load current pattern..."), file_selected);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_LOAD_PATTERN]), gui_settings.loadpat_path);
    fileops_dialogs[DIALOG_SAVE_PATTERN] = file_selection_create(_("Save current pattern..."), file_selected);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_PATTERN]), gui_settings.savepat_path);

    mainvbox0 = gtk_vbox_new(FALSE, 0);
    gtk_container_border_width(GTK_CONTAINER(mainvbox0), 0);
#ifdef USE_GNOME
    gnome_app_set_contents(GNOME_APP(mainwindow), mainvbox0);
#else
    gtk_container_add(GTK_CONTAINER(mainwindow), mainvbox0);
#endif
    gtk_widget_show(mainvbox0);

    menubar_create(mainwindow, mainvbox0);
	
    mainvbox = gtk_vbox_new(FALSE, 4);
    gtk_container_border_width(GTK_CONTAINER(mainvbox), 4);
    gtk_box_pack_start(GTK_BOX(mainvbox0), mainvbox, TRUE, TRUE, 0);
    gtk_widget_show(mainvbox);

    /* The upper part of the window */
    mainwindow_upper_hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainvbox), mainwindow_upper_hbox, FALSE, TRUE, 0);
    gtk_widget_show(mainwindow_upper_hbox);

    /* Program List */
    thing = playlist_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    playlist = PLAYLIST(thing);
    gtk_signal_connect (GTK_OBJECT (playlist), "current_position_changed",
			GTK_SIGNAL_FUNC (gui_playlist_position_changed), NULL);
    gtk_signal_connect (GTK_OBJECT (playlist), "restart_position_changed",
			GTK_SIGNAL_FUNC (gui_playlist_restart_position_changed), NULL);
    gtk_signal_connect (GTK_OBJECT (playlist), "song_length_changed",
			GTK_SIGNAL_FUNC (gui_playlist_song_length_changed), NULL);
    gtk_signal_connect (GTK_OBJECT (playlist), "entry_changed",
			GTK_SIGNAL_FUNC (gui_playlist_entry_changed), NULL);
    gtk_signal_connect (GTK_OBJECT (playlist->ifbutton), "clicked",
			GTK_SIGNAL_FUNC (gui_add_free_pattern), playlist);
    gtk_signal_connect (GTK_OBJECT (playlist->icbutton), "clicked",
			GTK_SIGNAL_FUNC (gui_add_free_pattern_and_copy), playlist);
    
    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    /* Basic editing commands and properties */
    
    table = gtk_table_new(5, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), table, FALSE, TRUE, 0);
    gtk_widget_show(table);

    gtk_widget_realize(mainwindow); /* to produce the correct style... */
    style = gtk_widget_get_style(mainwindow);
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, 0, 1);
    gtk_widget_show(hbox);

    pm = gdk_pixmap_create_from_xpm(mainwindow->window,
	&mask, &style->bg[GTK_STATE_NORMAL],
	PREFIX"/share/soundtracker/play.xpm");
    pmw = gtk_pixmap_new(pm, mask);
    pbutton = thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC(play_song), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE,0);
    gui_hang_tooltip(thing, _("Play Song"));
    gtk_widget_show_all(thing);

    pm = gdk_pixmap_create_from_xpm(mainwindow->window,
	&mask, &style->bg[GTK_STATE_NORMAL],
	PREFIX"/share/soundtracker/play_cur.xpm");
    pmw = gtk_pixmap_new(pm, mask);
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC(play_pattern), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE,0);
    gui_hang_tooltip(thing, _("Play Pattern"));
    gtk_widget_show_all(thing);

    pm = gdk_pixmap_create_from_xpm(mainwindow->window,
	&mask, &style->bg[GTK_STATE_NORMAL],
	PREFIX"/share/soundtracker/stop.xpm");
    pmw = gtk_pixmap_new(pm, mask);
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC(gui_play_stop), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE,0);		
    gui_hang_tooltip(thing, _("Stop"));
    gtk_widget_show_all(thing);
    
    add_empty_hbox(hbox);

    thing = gtk_label_new(_("Pat"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    spin_editpat = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 255, 1.0, 10.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_editpat));
    gui_hang_tooltip(spin_editpat, _("Edited pattern"));
    gtk_box_pack_start(GTK_BOX(hbox), spin_editpat, FALSE, TRUE, 0);
    gtk_widget_show(spin_editpat);
    gtk_signal_connect(GTK_OBJECT(spin_editpat), "changed",
		       GTK_SIGNAL_FUNC(gui_editpat_changed), NULL);
		       
    pm = gdk_pixmap_create_from_xpm(mainwindow->window,
	&mask, &style->bg[GTK_STATE_NORMAL],
	PREFIX"/share/soundtracker/lock.xpm");
    pmw = gtk_pixmap_new(pm, mask);
    toggle_lock_editpat = thing = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE,0);		
    gui_hang_tooltip(thing, _("When enabled, browsing the playlist does not change the edited pattern."));
    gtk_widget_show_all(thing);

    thing = gui_subs_create_slider(&tempo_slider);
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 3, 4);

    gtk_widget_show(thing);

    thing = gui_subs_create_slider(&bpm_slider);
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 1, 2, 3, 4);
    gtk_widget_show(thing);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, 1, 2);
    gtk_widget_show(hbox);

    thing = gtk_label_new(_("Number of Channels:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    add_empty_hbox(hbox);

    spin_numchans = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(8, 2, 32, 2.0, 8.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_numchans));
    gtk_box_pack_start(GTK_BOX(hbox), spin_numchans, FALSE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(spin_numchans), "changed",
		       GTK_SIGNAL_FUNC(gui_numchans_changed), NULL);
    gtk_widget_show(spin_numchans);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, 2, 3);
    gtk_widget_show(hbox);

    thing = gtk_label_new(_("Pattern Length"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    add_empty_hbox(hbox);

    spin_patlen = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(64, 1, 256, 1.0, 16.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_patlen));
    gtk_box_pack_start(GTK_BOX(hbox), spin_patlen, FALSE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(spin_patlen), "changed",
		       GTK_SIGNAL_FUNC(gui_patlen_changed), NULL);
    gtk_widget_show(spin_patlen);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, 4, 5);
    gtk_widget_show(hbox);
    
    vbox = gtk_vbox_new(FALSE, 0);
    pm = gdk_pixmap_create_from_xpm(mainwindow->window,
	&mask, &style->bg[GTK_STATE_NORMAL],
	PREFIX"/share/soundtracker/sharp.xpm");
    alt[0] = gtk_pixmap_new(pm, mask);
    gtk_box_pack_start(GTK_BOX(vbox), alt[0], FALSE, FALSE, 0);
    
    pm = gdk_pixmap_create_from_xpm(mainwindow->window,
	&mask, &style->bg[GTK_STATE_NORMAL],
	PREFIX"/share/soundtracker/flat.xpm");
    alt[1] = gtk_pixmap_new(pm, mask);
    gtk_widget_show(alt[gui_settings.sharp ? 0 : 1]);
    gtk_box_pack_start(GTK_BOX(vbox), alt[1], FALSE, FALSE, 0);
    gtk_widget_show(vbox);
    
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), vbox);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gui_hang_tooltip(thing, _("Set preferred accidental type"));
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(gui_accidentals_clicked), NULL);

    add_empty_hbox(hbox);
    thing = gtk_toggle_button_new_with_label(_("Measure"));
    gui_hang_tooltip(thing, _("Enable row highlighting"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing),gui_settings.highlight_rows);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(gui_highlight_rows_toggled), NULL);
    gtk_widget_show(thing);

    thing = gtk_combo_new();
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);
    entry = GTK_COMBO(thing)->entry;
    gui_hang_tooltip(entry, _("Row highlighting configuration"));
    style = gtk_widget_get_style(entry);
    
    glist = NULL;
    wdth = 0;
    selected = -1;
    for(i = 0; measure_msr[i].title != NULL; i++) {
	glist = g_list_append(glist, (gpointer)measure_msr[i].title);
	if ((cur = gdk_string_width(style->font, measure_msr[i].title)) > wdth)
	    wdth = cur;
	if ((measure_msr[i].major == gui_settings.highlight_rows_n) &&
	    (measure_msr[i].minor == gui_settings.highlight_rows_minor_n))
		selected = i;
    }
    if (selected == -1) selected = i;
    other = _("Other...");
    glist = g_list_append(glist, other);
    if ((cur = gdk_string_width(style->font,other)) > wdth) wdth = cur;

    gtk_combo_set_popdown_strings(GTK_COMBO(thing), glist);
    list = GTK_COMBO(thing)->list;
    gtk_list_select_item(GTK_LIST(list), selected);
    gtk_signal_connect(GTK_OBJECT(list), "select_child",
		       GTK_SIGNAL_FUNC(measure_changed), NULL);
    /* the direct use of combo->popwin is not recommended, but I couldn't find another way... */
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(thing)->popwin), "hide",
		       GTK_SIGNAL_FUNC(popwin_hide), entry);
    
    gtk_widget_set_usize(entry, wdth + 5, -1);//-1 to suppress the height changing
    gtk_entry_set_editable(GTK_ENTRY(entry), FALSE);
    
    add_empty_hbox(hbox);
    
    vbox = gtk_vbox_new(FALSE, 0);
    pm = gdk_pixmap_create_from_xpm(mainwindow->window,
	&mask, &style->bg[GTK_STATE_NORMAL],
	PREFIX"/share/soundtracker/downarrow.xpm");
    arrow[0] = gtk_pixmap_new(pm, mask);
    gtk_box_pack_start(GTK_BOX(vbox), arrow[0], FALSE, FALSE, 0);
    
    pm = gdk_pixmap_create_from_xpm(mainwindow->window,
	&mask, &style->bg[GTK_STATE_NORMAL],
	PREFIX"/share/soundtracker/rightarrow.xpm");
    arrow[1] = gtk_pixmap_new(pm, mask);
    gtk_box_pack_start(GTK_BOX(vbox), arrow[1], FALSE, FALSE, 0);
    gtk_widget_show(arrow[gui_settings.advance_cursor_in_fx_columns ? 1 : 0]);
    gtk_widget_show(vbox);
    
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), vbox);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gui_hang_tooltip(thing, _("Change effect column editing direction"));
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(gui_direction_clicked), NULL);
    
    /* Scopes Group or Instrument / Sample Listing */

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

#ifndef NO_GDK_PIXBUF
    scopegroup = SCOPE_GROUP(scope_group_new(gdk_pixbuf_new_from_file(PREFIX"/share/soundtracker/muted.png")));
#else
    scopegroup = SCOPE_GROUP(scope_group_new());
#endif
    gtk_widget_show(GTK_WIDGET(scopegroup));
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), GTK_WIDGET(scopegroup), TRUE, TRUE, 0);

    /* Amplification and Pitchbender */

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    
    hbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), hbox, FALSE, TRUE, 0);

    adj_amplification = GTK_ADJUSTMENT(gtk_adjustment_new(7.0, 0, 8.0, 0.1, 0.1, 0.1));
    thing = gtk_vscale_new(adj_amplification);
    gui_hang_tooltip(thing, _("Global amplification"));
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    gtk_signal_connect (GTK_OBJECT(adj_amplification), "value_changed",
			GTK_SIGNAL_FUNC(gui_adj_amplification_changed), NULL);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(hbox), frame, FALSE, TRUE, 0);
    gtk_widget_show(frame);

    gui_clipping_led = thing = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(thing), 15, 15);
    gtk_widget_set_events(thing, GDK_EXPOSURE_MASK);
    gtk_container_add (GTK_CONTAINER(frame), thing);
    colormap = gtk_widget_get_colormap(thing);
    gui_clipping_led_on.red = 0xffff;
    gui_clipping_led_on.green = 0;
    gui_clipping_led_on.blue = 0;
    gui_clipping_led_on.pixel = 0;
    gui_clipping_led_off.red = 0;
    gui_clipping_led_off.green = 0;
    gui_clipping_led_off.blue = 0;
    gui_clipping_led_off.pixel = 0;
    gdk_color_alloc(colormap, &gui_clipping_led_on);
    gdk_color_alloc(colormap, &gui_clipping_led_off);
    gtk_signal_connect(GTK_OBJECT(thing), "event", GTK_SIGNAL_FUNC(gui_clipping_led_event), thing);
    gtk_widget_show (thing);

    hbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), hbox, FALSE, TRUE, 0);

    adj_pitchbend = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, -20.0, +20.0, 1, 1, 1));
    thing = gtk_vscale_new(adj_pitchbend);
    gui_hang_tooltip(thing, _("Pitchbend"));
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    gtk_signal_connect (GTK_OBJECT(adj_pitchbend), "value_changed",
			GTK_SIGNAL_FUNC(gui_adj_pitchbend_changed), NULL);

    thing = gtk_button_new_with_label("R");
    gui_hang_tooltip(thing, _("Reset pitchbend to its normal value"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC(gui_reset_pitch_bender), NULL);

    /* Instrument, sample, editing status */

    mainwindow_second_hbox = hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainvbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    editing_toggle = thing = gtk_check_button_new_with_label(_("Editing"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect (GTK_OBJECT(thing), "toggled",
			GTK_SIGNAL_FUNC(editing_toggled), NULL);

    thing = gtk_label_new(_("Octave"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    spin_octave = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(3.0, 0.0, 6.0, 1.0, 1.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_octave));
    gtk_box_pack_start(GTK_BOX(hbox), spin_octave, FALSE, TRUE, 0);
    gtk_widget_show(spin_octave);

    thing = gtk_label_new(_("Jump"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    spin_jump = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(1.0, 0.0, 16.0, 1.0, 1.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_jump));
    gtk_box_pack_start(GTK_BOX(hbox), spin_jump, FALSE, TRUE, 0);
    gtk_widget_show(spin_jump);

    thing = gtk_label_new(_("Instr"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    curins_spin = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(1.0, 1.0, 128.0, 1.0, 16.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(curins_spin));
    gtk_box_pack_start(GTK_BOX(hbox), curins_spin, FALSE, TRUE, 0);
    gtk_widget_show(curins_spin);
    gtk_signal_connect (GTK_OBJECT(curins_spin), "changed",
			GTK_SIGNAL_FUNC(current_instrument_changed), NULL);

    gui_get_text_entry(22, current_instrument_name_changed, &gui_curins_name);
    gtk_box_pack_start(GTK_BOX(hbox), gui_curins_name, TRUE, TRUE, 0);
    gtk_widget_show(gui_curins_name);
    gtk_widget_set_usize(gui_curins_name, 100, gui_curins_name->requisition.height);

    thing = gtk_label_new(_("Sample"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    cursmpl_spin = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 15.0, 1.0, 4.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(cursmpl_spin));
    gtk_box_pack_start(GTK_BOX(hbox), cursmpl_spin, FALSE, TRUE, 0);
    gtk_widget_show(cursmpl_spin);
    gtk_signal_connect (GTK_OBJECT(cursmpl_spin), "changed",
			GTK_SIGNAL_FUNC(current_sample_changed), NULL);

    gui_get_text_entry(22, current_sample_name_changed, &gui_cursmpl_name);
    gtk_box_pack_start(GTK_BOX(hbox), gui_cursmpl_name, TRUE, TRUE, 0);
    gtk_widget_show(gui_cursmpl_name);
    gtk_widget_set_usize(gui_cursmpl_name, 100, gui_cursmpl_name->requisition.height);

    /* The notebook */

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(mainvbox), notebook, TRUE, TRUE, 0);
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_widget_show(notebook);
    gtk_container_border_width(GTK_CONTAINER(notebook), 0);
    gtk_signal_connect(GTK_OBJECT(notebook), "switch_page",
		       GTK_SIGNAL_FUNC(notebook_page_switched), NULL);

    fileops_page_create(GTK_NOTEBOOK(notebook));
    tracker_page_create(GTK_NOTEBOOK(notebook));
    instrument_page_create(GTK_NOTEBOOK(notebook));
    sample_editor_page_create(GTK_NOTEBOOK(notebook));
    modinfo_page_create(GTK_NOTEBOOK(notebook));

    // Activate tracker page
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook),
			  1);
    notebook_current_page = 1;

    /* Status Bar */

#define WELCOME_MESSAGE _("Welcome to SoundTracker!")

#ifdef USE_GNOME
    dockitem = gnome_dock_item_new("Status Bar", (GNOME_DOCK_ITEM_BEH_EXCLUSIVE | GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL));
    gnome_app_add_dock_item(GNOME_APP(mainwindow), GNOME_DOCK_ITEM(dockitem), GNOME_DOCK_BOTTOM, 0, 0, 0);
    gtk_widget_show(dockitem);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(hbox), 2);
    gtk_container_add(GTK_CONTAINER(dockitem), hbox);
    gtk_widget_show(hbox);

    status_bar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_NEVER);
    gtk_widget_show (status_bar);
    gtk_box_pack_start (GTK_BOX (hbox), status_bar, TRUE, TRUE, 0);
    gtk_widget_set_usize (status_bar, 300 , 20); /* so that it doesn't vanish when undocked */

    thing = gtk_frame_new (NULL);
    gtk_widget_show (thing);
    gtk_box_pack_start (GTK_BOX (hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_usize (thing, 48, 20);
    gtk_frame_set_shadow_type (GTK_FRAME (thing), GTK_SHADOW_IN);

    st_clock = gtk_clock_new (GTK_CLOCK_INCREASING);
    gtk_widget_show (st_clock);
    gtk_container_add (GTK_CONTAINER (thing), st_clock);
    gtk_widget_set_usize (st_clock, 48, 20);
    gtk_clock_set_format (GTK_CLOCK (st_clock), _("%M:%S"));
    gtk_clock_set_seconds(GTK_CLOCK (st_clock), 0);

    gnome_appbar_set_status(GNOME_APPBAR(status_bar), WELCOME_MESSAGE);
#else
    thing = gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(mainvbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    status_bar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX (thing), status_bar, TRUE, TRUE, 0);
    gtk_widget_show(status_bar);
    gtk_widget_set_usize(status_bar, -2, 20);

    statusbar_context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(status_bar), "ST Statusbar");
    gtk_statusbar_push(GTK_STATUSBAR(status_bar), statusbar_context_id, WELCOME_MESSAGE);
#endif
    
    /* capture all key presses */
    gtk_widget_add_events(GTK_WIDGET(mainwindow), GDK_KEY_RELEASE_MASK);
    gtk_signal_connect(GTK_OBJECT(mainwindow), "key_press_event", GTK_SIGNAL_FUNC(keyevent), (gpointer)1);
    gtk_signal_connect(GTK_OBJECT(mainwindow), "key_release_event", GTK_SIGNAL_FUNC(keyevent), (gpointer)0);

    if(argc == 2) {
	gui_load_xm(argv[1]);
    } else {
	gui_new_xm();
    }

    menubar_init_prefs();

    gtk_widget_show (mainwindow);

    if(!keys_init()) {
	return 0;
    }

    if(gui_splash_window) {
	if(!gui_settings.gui_disable_splash && (
#ifndef NO_GDK_PIXBUF
           gui_splash_logo ||
#endif
           tips_dialog_show_tips)) {
	    gdk_window_raise(gui_splash_window->window);
//	    gtk_window_set_transient_for(GTK_WINDOW(gui_splash_window), GTK_WINDOW(mainwindow));
// (doesn't do anything on WindowMaker)
	    gui_splash_set_label(_("Ready."), TRUE);
//	    gdk_window_hide(gui_splash_window->window);
//	    gdk_window_show(gui_splash_window->window);
#ifndef NO_GDK_PIXBUF
	    if(gui_splash_logo) {
		gtk_widget_add_events(gui_splash_logo_area,
				      GDK_BUTTON_PRESS_MASK);
		gtk_signal_connect (GTK_OBJECT (gui_splash_logo_area), "button_press_event",
				    GTK_SIGNAL_FUNC (gui_splash_close),
				    NULL);
	    }
#endif
	    gtk_widget_set_sensitive(gui_splash_close_button, TRUE);
	} else {
	    gui_splash_close();
	}
    }

    return 1;
}
