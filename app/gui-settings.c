
/*
 * The Real SoundTracker - GUI configuration dialog
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

#include <string.h>

#include <gtk/gtk.h>
#ifdef USE_GNOME
#include <gnome.h>
#endif

#include "i18n.h"
#include "gui.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "preferences.h"
#include "scope-group.h"
#include "track-editor.h"
#include "extspinbutton.h"
#include "tracker-settings.h"

gui_prefs gui_settings = {
    "---0000000",
    1,
    0,
    1,
    16,
    8,

    0,
    0,

    1,
    0,

    1,
    0,
    0,

    0,
    1,

    TRUE,/* sharps or flats */
    FALSE,
    
    50,
    40,
    500000,

    -666,
    0,
    0,
    0,

    TRUE,
    0,

    "~/",
    "~/",
    "~/",
    "~/",
    "~/",
    "~/",
    "~/",
    "~/",

    "rm",
    "unzip",
    "lha",
    "zcat",
    "bunzip2"
};

static GtkWidget *configwindow = NULL;
static GtkWidget *ts_box = NULL;

static void           prefs_scopesfreq_changed                  (int value);
static void           prefs_trackerfreq_changed                 (int value);

static gui_subs_slider prefs_scopesfreq_slider = {
    N_("Scopes Frequency"), 1, 80, prefs_scopesfreq_changed
};
static gui_subs_slider prefs_trackerfreq_slider = {
    N_("Tracker Frequency"), 1, 80, prefs_trackerfreq_changed
};

static void
gui_settings_close_requested (void)
{
/* I needed to change this code because I use some object from gui settings
   window in the other place and these objects must not be destroyed. (yaliaev)
    gtk_container_remove(GTK_CONTAINER(ts_box), trackersettings);
    gtk_widget_destroy(configwindow);
*/
    gdk_window_hide(configwindow->window);
//    configwindow = NULL;
}

static void
prefs_scopesfreq_changed (int value)
{
    extern ScopeGroup *scopegroup;
    gui_settings.scopes_update_freq = value;
    scope_group_set_update_freq(scopegroup, value);
}

static void
prefs_trackerfreq_changed (int value)
{
    gui_settings.tracker_update_freq = value;
    tracker_set_update_freq(value);
}

static void
gui_settings_hexmode_toggled (GtkWidget *widget)
{
    int o = gui_settings.tracker_hexmode;
    if(o != (gui_settings.tracker_hexmode = GTK_TOGGLE_BUTTON(widget)->active)) {
	gtk_widget_queue_resize(GTK_WIDGET(tracker));
    }
}

static void
gui_settings_upcase_toggled (GtkWidget *widget)
{
    int o = gui_settings.tracker_upcase;
    if(o != (gui_settings.tracker_upcase = GTK_TOGGLE_BUTTON(widget)->active)) {
	gtk_widget_queue_resize(GTK_WIDGET(tracker));
    }
}

static void
gui_settings_asyncedit_toggled (GtkWidget *widget)
{
    gui_play_stop();
    gui_settings.asynchronous_editing = GTK_TOGGLE_BUTTON(widget)->active;
}

static void
gui_settings_tempo_bpm_update_toggled (GtkWidget *widget)
{
    int o = gui_settings.tempo_bpm_update;
    if(o != (gui_settings.tempo_bpm_update = GTK_TOGGLE_BUTTON(widget)->active)) {
	gtk_widget_queue_resize(GTK_WIDGET(tracker));
    }
}

static void
gui_settings_auto_switch_toggled (GtkWidget *widget)
{
    int o = gui_settings.auto_switch;
    if(o != (gui_settings.auto_switch = GTK_TOGGLE_BUTTON(widget)->active)) {
       gtk_widget_queue_resize(GTK_WIDGET(tracker));
    }
}

static void
gui_settings_save_geometry_toggled (GtkWidget *widget)
{
    int o = gui_settings.save_geometry;
    if(o != (gui_settings.save_geometry = GTK_TOGGLE_BUTTON(widget)->active)) {
       gtk_widget_queue_resize(GTK_WIDGET(tracker));
    }
}

static void
gui_settings_scopebufsize_changed (GtkSpinButton *spin)
{
    double n = gtk_spin_button_get_value_as_float(spin);

    gui_settings.scopes_buffer_size = n * 1000000;
}

static void
gui_settings_bh_toggled (GtkWidget *widget)
{
    gui_settings.bh = GTK_TOGGLE_BUTTON(widget)->active;
    tracker_redraw (tracker);
}

static void
gui_settings_perm_toggled (GtkWidget *widget)
{
    gui_settings.store_perm = GTK_TOGGLE_BUTTON(widget)->active;
}

void
gui_settings_highlight_rows_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    gui_settings.highlight_rows_n = n;
    if(gui_settings.highlight_rows)
	tracker_redraw(tracker);
}

void
gui_settings_highlight_rows_minor_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    gui_settings.highlight_rows_minor_n = n;
    if(gui_settings.highlight_rows)
	tracker_redraw(tracker);
}

static void
gui_settings_tracker_line_note_modified(GtkEntry *entry)
{
    gchar *text = gtk_entry_get_text(entry);
    int i;

    for(i=0 ; i<3 ; i++)    {
        if(!text[i])    {
            text[i] = ' ';
            text[i+1] = 0;
        }
    }
    text[3] = 0;
    if(strncmp(gui_settings.tracker_line_format, text, 3)) {
	strncpy(gui_settings.tracker_line_format, text, 3);
	tracker_redraw(tracker);
    }
}

static void
gui_settings_tracker_line_ins_modified(GtkEntry *entry)
{
    gchar *text = gtk_entry_get_text(entry);
    int i;

    for(i=0 ; i<2 ; i++)    {
        if(!text[i])    {
            text[i] = ' ';
            text[i+1] = 0;
        }
    }
    text[2] = 0;
   	if(strncmp(gui_settings.tracker_line_format+3, text, 2))  {
          strncpy(gui_settings.tracker_line_format+3, text, 2);
     	   tracker_redraw(tracker);
    }
}

static void
gui_settings_tracker_line_vol_modified(GtkEntry *entry)
{
    gchar *text = gtk_entry_get_text(entry);
    int i;

    for(i=0 ; i<2 ; i++)    {
        if(!text[i])    {
            text[i] = ' ';
            text[i+1] = 0;
        }
    }
    text[2] = 0;
   	if(strncmp(gui_settings.tracker_line_format+5, text, 2))  {
          strncpy(gui_settings.tracker_line_format+5, text, 2);
     	   tracker_redraw(tracker);
    }
}

static void
gui_settings_tracker_line_effect_modified(GtkEntry *entry, GdkEventKey *event)
{
    gchar *text = gtk_entry_get_text(entry);
    int i;

    for(i=0 ; i<3 ; i++)    {
        if(!text[i])    {
            text[i] = ' ';
            text[i+1] = 0;
        }
    }
    text[3] = 0;
  	if(strncmp(gui_settings.tracker_line_format+7, text, 3))  {
          strncpy(gui_settings.tracker_line_format+7, text, 3);
     	   tracker_redraw(tracker);
    }
}

void
gui_settings_dialog (void)
{
    GtkWidget *mainbox, *mainhbox, *thing, *box1, *hbox, *vbox1;
    GtkTooltips *tooltips;
    gchar stmp[5];

    if(configwindow != NULL) {
	if (!gdk_window_is_visible(configwindow->window))
	    gdk_window_show(configwindow->window);
	gdk_window_raise(configwindow->window);
	return;
    }

#ifdef USE_GNOME
    configwindow = gnome_app_new("SoundTracker", _("GUI Configuration"));
#else
    configwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(configwindow), _("GUI Configuration"));
#endif
    gtk_signal_connect (GTK_OBJECT (configwindow), "delete_event",
			GTK_SIGNAL_FUNC (gui_settings_close_requested), NULL);
//    gtk_window_set_policy(GTK_WINDOW(configwindow), FALSE, FALSE, FALSE);

    mainbox = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(mainbox), 4);
#ifdef USE_GNOME
    gnome_app_set_contents(GNOME_APP(configwindow), mainbox);
#else
    gtk_container_add(GTK_CONTAINER(configwindow), mainbox);
#endif
    gtk_widget_show(mainbox);

    mainhbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), mainhbox, TRUE, TRUE, 0);
    gtk_widget_show(mainhbox);

    vbox1 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mainhbox), vbox1, FALSE, TRUE, 0);
    gtk_widget_show(vbox1);

    tooltips = gtk_tooltips_new();
    gtk_object_set_data(GTK_OBJECT(configwindow), "tooltips", tooltips);

    thing = gui_subs_create_slider(&prefs_scopesfreq_slider);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gui_subs_create_slider(&prefs_trackerfreq_slider);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_check_button_new_with_label(_("Hexadecimal row numbers"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.tracker_hexmode);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(gui_settings_hexmode_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Use upper case letters for hex numbers"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.tracker_upcase);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(gui_settings_upcase_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Asynchronous (IT-style) pattern editing"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.asynchronous_editing);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(gui_settings_asyncedit_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Fxx command updates Tempo/BPM sliders"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.tempo_bpm_update);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(gui_settings_tempo_bpm_update_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Switch to tracker after loading/saving"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.auto_switch);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(gui_settings_auto_switch_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Save window geometry on exit"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.save_geometry);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(gui_settings_save_geometry_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Use note name B instead of H"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.bh);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(gui_settings_bh_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Save and restore permanent channels"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.store_perm);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(gui_settings_perm_toggled), NULL);

    gui_subs_set_slider_value(&prefs_scopesfreq_slider, gui_settings.scopes_update_freq);
    gui_subs_set_slider_value(&prefs_trackerfreq_slider, gui_settings.tracker_update_freq);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box1);
    gtk_box_pack_start(GTK_BOX(vbox1), box1, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Scopes buffer size [MB]"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    add_empty_hbox(box1);
    thing = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new((double)gui_settings.scopes_buffer_size / 1000000, 0.5, 5.0, 0.1, 1.0, 0.0)), 0, 0);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(thing), 1);
    gtk_signal_connect(GTK_OBJECT(thing), "changed",
		       GTK_SIGNAL_FUNC(gui_settings_scopebufsize_changed), NULL);

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);

    /* Track line format */
    box1 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box1);
    gtk_box_pack_start(GTK_BOX(vbox1), box1, FALSE, TRUE, 0);		
    thing = gtk_label_new(_("Track line format:"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_entry_new();
    gtk_widget_set_usize(thing, 13*3, thing->requisition.height);
    gtk_entry_set_max_length((GtkEntry*)thing, 3);
    strncpy(stmp, gui_settings.tracker_line_format, 3);
    stmp[3] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "changed",
        GTK_SIGNAL_FUNC(gui_settings_tracker_line_note_modified), 0);

    thing = gtk_entry_new();
    gtk_widget_set_usize(thing, 13*2, thing->requisition.height);
    gtk_entry_set_max_length((GtkEntry*)thing, 2);
    strncpy(stmp, gui_settings.tracker_line_format+3, 2);
    stmp[2] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "changed",
        GTK_SIGNAL_FUNC(gui_settings_tracker_line_ins_modified), 0);

    thing = gtk_entry_new();
    gtk_widget_set_usize(thing, 13*2, thing->requisition.height);
    gtk_entry_set_max_length((GtkEntry*)thing, 2);
    strncpy(stmp, gui_settings.tracker_line_format+5, 2);
    stmp[2] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "changed",
        GTK_SIGNAL_FUNC(gui_settings_tracker_line_vol_modified), 0);

    thing = gtk_entry_new();
    gtk_widget_set_usize(thing, 13*3, thing->requisition.height);
    gtk_entry_set_max_length((GtkEntry*)thing, 3);
    strncpy(stmp, gui_settings.tracker_line_format+7, 3);
    stmp[3] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "changed",
        GTK_SIGNAL_FUNC(gui_settings_tracker_line_effect_modified), 0);

    thing = gtk_vseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainhbox), thing, FALSE, TRUE, 0);

    /* The tracker widget settings */
    ts_box = vbox1 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mainhbox), vbox1, TRUE, TRUE, 0);
    gtk_widget_show(vbox1);

    gtk_object_ref(GTK_OBJECT(trackersettings));
    gtk_box_pack_start(GTK_BOX(vbox1), trackersettings, TRUE, TRUE, 0);
    gtk_widget_show(trackersettings);

    /* The button area */
    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    hbox = gtk_hbutton_box_new ();
    gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbox), 4);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
    gtk_box_pack_start (GTK_BOX (mainbox), hbox,
			FALSE, FALSE, 0);
    gtk_widget_show (hbox);

#ifdef USE_GNOME
    thing = gnome_stock_button (GNOME_STOCK_BUTTON_CLOSE);
#else
    thing = gtk_button_new_with_label (_("Close"));
#endif
    GTK_WIDGET_SET_FLAGS(thing, GTK_CAN_DEFAULT);
    gtk_window_set_default(GTK_WINDOW(configwindow), thing);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC (gui_settings_close_requested), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show (thing);


    gtk_widget_show (configwindow);
}

void
gui_settings_load_config (void)
{
    prefs_node *f;

    f = prefs_open_read("settings");
    if(f) {
	prefs_get_int(f, "st-window-x", &gui_settings.st_window_x);
	prefs_get_int(f, "st-window-y", &gui_settings.st_window_y);
	prefs_get_int(f, "st-window-w", &gui_settings.st_window_w);
	prefs_get_int(f, "st-window-h", &gui_settings.st_window_h);

	prefs_get_int(f, "gui-use-hexadecimal-numbers", &gui_settings.tracker_hexmode);
	prefs_get_int(f, "gui-use-upper-case", &gui_settings.tracker_upcase);
	prefs_get_int(f, "gui-advance-cursor-in-fx-columns", &gui_settings.advance_cursor_in_fx_columns);
	prefs_get_int(f, "gui-asynchronous-editing", &gui_settings.asynchronous_editing);
	prefs_get_string(f, "gui-tracker-line-format", gui_settings.tracker_line_format);
	prefs_get_int(f, "gui-tempo-bpm-update", &gui_settings.tempo_bpm_update);
	prefs_get_int(f, "gui-auto-switch", &gui_settings.auto_switch);
	prefs_get_int(f, "gui-display-scopes", &gui_settings.gui_display_scopes);
	prefs_get_int(f, "gui-use-backing-store", &gui_settings.gui_use_backing_store);
	prefs_get_int(f, "tracker-highlight-rows", &gui_settings.highlight_rows);
	prefs_get_int(f, "tracker-highlight-rows-n", &gui_settings.highlight_rows_n);
	prefs_get_int(f, "tracker-highlight-rows-minor-n", &gui_settings.highlight_rows_minor_n);
	prefs_get_int(f, "save-geometry", &gui_settings.save_geometry);
	prefs_get_int(f, "save-settings-on-exit", &gui_settings.save_settings_on_exit);
	prefs_get_int(f, "tracker-update-frequency", &gui_settings.tracker_update_freq);
	prefs_get_int(f, "scopes-update-frequency", &gui_settings.scopes_update_freq);
	prefs_get_int(f, "scopes-buffer-size", &gui_settings.scopes_buffer_size);
  	prefs_get_int(f, "sharp", &gui_settings.sharp);
  	prefs_get_int(f, "bh", &gui_settings.bh);
	prefs_get_int(f, "store-permanent", &gui_settings.store_perm);
	
	if(gui_settings.store_perm)
	    prefs_get_int(f, "permanent-channels", &gui_settings.permanent_channels);

	prefs_close(f);
    }

    f = prefs_open_read("settings-always");
    if(f) {
	prefs_get_string(f, "loadmod-path", gui_settings.loadmod_path);
	prefs_get_string(f, "savemod-path", gui_settings.savemod_path);
	prefs_get_string(f, "savemodaswav-path", gui_settings.savemodaswav_path);
	prefs_get_string(f, "savesongasxm-path", gui_settings.savesongasxm_path);
	prefs_get_string(f, "loadsmpl-path", gui_settings.loadsmpl_path);
	prefs_get_string(f, "savesmpl-path", gui_settings.savesmpl_path);
	prefs_get_string(f, "loadinstr-path", gui_settings.loadinstr_path);
	prefs_get_string(f, "saveinstr-path", gui_settings.saveinstr_path);
	prefs_get_string(f, "loadpat-path", gui_settings.loadpat_path);
	prefs_get_string(f, "savepat-path", gui_settings.savepat_path);

	prefs_get_string(f, "rm-path", gui_settings.rm_path);
	prefs_get_string(f, "unzip-path", gui_settings.unzip_path);
	prefs_get_string(f, "lha-path", gui_settings.lha_path);
	prefs_get_string(f, "gz-path", gui_settings.gz_path);
	prefs_get_string(f, "bz2-path", gui_settings.bz2_path);

	prefs_get_int(f, "gui-disable-splash", &gui_settings.gui_disable_splash);

	prefs_close(f);
    }
}

void
gui_settings_save_config (void)
{
    prefs_node *f;

    f = prefs_open_write("settings");
    if(!f)
	return;

    if(gui_settings.save_geometry) {
	gdk_window_get_size (mainwindow->window,
			     &gui_settings.st_window_w,
			     &gui_settings.st_window_h);
	gdk_window_get_root_origin (mainwindow->window,
				    &gui_settings.st_window_x,
				    &gui_settings.st_window_y);
    }

    prefs_put_int(f, "st-window-x", gui_settings.st_window_x);
    prefs_put_int(f, "st-window-y", gui_settings.st_window_y);
    prefs_put_int(f, "st-window-w", gui_settings.st_window_w);
    prefs_put_int(f, "st-window-h", gui_settings.st_window_h);

    prefs_put_int(f, "gui-use-hexadecimal-numbers", gui_settings.tracker_hexmode);
    prefs_put_int(f, "gui-use-upper-case", gui_settings.tracker_upcase);
    prefs_put_int(f, "gui-advance-cursor-in-fx-columns", gui_settings.advance_cursor_in_fx_columns);
    prefs_put_int(f, "gui-asynchronous-editing", gui_settings.asynchronous_editing);
    prefs_put_string(f, "gui-tracker-line-format", gui_settings.tracker_line_format);
    prefs_put_int(f, "gui-tempo-bpm-update", gui_settings.tempo_bpm_update);
    prefs_put_int(f, "gui-auto-switch", gui_settings.auto_switch);
    prefs_put_int(f, "gui-display-scopes", gui_settings.gui_display_scopes);
    prefs_put_int(f, "gui-use-backing-store", gui_settings.gui_use_backing_store);
    prefs_put_int(f, "tracker-highlight-rows", gui_settings.highlight_rows);
    prefs_put_int(f, "tracker-highlight-rows-n", gui_settings.highlight_rows_n);
    prefs_put_int(f, "tracker-highlight-rows-minor-n", gui_settings.highlight_rows_minor_n);
    prefs_put_int(f, "save-geometry", gui_settings.save_geometry);
    prefs_put_int(f, "save-settings-on-exit", gui_settings.save_settings_on_exit);
    prefs_put_int(f, "tracker-update-frequency", gui_settings.tracker_update_freq);
    prefs_put_int(f, "scopes-update-frequency", gui_settings.scopes_update_freq);
    prefs_put_int(f, "scopes-buffer-size", gui_settings.scopes_buffer_size);
    prefs_put_int(f, "sharp", gui_settings.sharp);
    prefs_put_int(f, "bh", gui_settings.bh);
    prefs_put_int(f, "store-permanent", gui_settings.store_perm);

    if(gui_settings.store_perm)
	prefs_put_int(f, "permanent-channels", gui_settings.permanent_channels);

    prefs_close(f);
}

void
gui_settings_save_config_always (void)
{
    prefs_node *f;

    f = prefs_open_write("settings-always");
    if(!f)
	return;

    prefs_put_string(f, "loadmod-path", gui_settings.loadmod_path);
    prefs_put_string(f, "savemod-path", gui_settings.savemod_path);
    prefs_put_string(f, "savemodaswav-path", gui_settings.savemodaswav_path);
    prefs_put_string(f, "savesongasxm-path", gui_settings.savesongasxm_path);
    prefs_put_string(f, "loadsmpl-path", gui_settings.loadsmpl_path);
    prefs_put_string(f, "savesmpl-path", gui_settings.savesmpl_path);
    prefs_put_string(f, "loadinstr-path", gui_settings.loadinstr_path);
    prefs_put_string(f, "saveinstr-path", gui_settings.saveinstr_path);
    prefs_put_string(f, "loadpat-path", gui_settings.loadpat_path);
    prefs_put_string(f, "savepat-path", gui_settings.savepat_path);

    prefs_put_string(f, "rm-path", gui_settings.rm_path);
    prefs_put_string(f, "unzip-path", gui_settings.unzip_path);
    prefs_put_string(f, "lha-path", gui_settings.lha_path);
    prefs_put_string(f, "gz-path", gui_settings.gz_path);
    prefs_put_string(f, "bz2-path", gui_settings.bz2_path);

    prefs_put_int(f, "gui-disable-splash", gui_settings.gui_disable_splash);

    prefs_close(f);
}
