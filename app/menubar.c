
/*
 * The Real SoundTracker - GUI (menu bar)
 *
 * Copyright (C) 1999-2003 Michael Krause
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

#ifndef USE_GNOME
#include "X11/Xlib.h"
#include <gdk/gdkkeysyms.h>
#endif

#include "i18n.h"
#include "menubar.h"
#include "gui.h"
#include "gui-subs.h"
#include "main.h"
#include "st-subs.h"
#include "keys.h"
#include "module-info.h"
#include "preferences.h"
#include "scope-group.h"
#include "track-editor.h"
#include "audioconfig.h"
#include "gui-settings.h"
#include "tips-dialog.h"
#include "transposition.h"
#include "cheat-sheet.h"
#include "file-operations.h"
#include "instrument-editor.h"
#include "tracker-settings.h"
#include "midi-settings.h"
#include "sample-editor.h"

#ifdef USE_GNOME
#include <gnome.h>
#else
static GtkItemFactory *item_factory;
static GtkAccelGroup *accel_group;
static GtkItemFactoryEntry *menubar_gtk_items = NULL;
#endif

static gboolean mark_mode_toggle_ignore = FALSE;

extern ScopeGroup *scopegroup;

#ifndef USE_GNOME
static GtkWidget *about = NULL;

static void
about_close (void)
{
    gtk_widget_destroy(about);
    about = NULL;
}
#endif

static void
about_dialog (void)
{
#ifdef USE_GNOME
    const gchar *authors[] = {"Michael Krause <rawstyle@soundtracker.org>", NULL};
    GtkWidget *about = gnome_about_new("SoundTracker",
				       VERSION,
				       "Copyright (C) 1998-2003 Michael Krause",
				       "Includes OpenCP player from Niklas Beisert and Tammo Hinrichs.",
				       authors,
					  NULL, NULL, NULL);
    gtk_widget_show (about);
#else
    GtkWidget *label, *button;

    if(about)
	gtk_widget_destroy(about);

    about = gtk_dialog_new();
    gtk_window_position (GTK_WINDOW(about), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(about), "About SoundTracker");

    label = gtk_label_new("SoundTracker " VERSION "\n\nCopyright (C) 1998-2003 Michael Krause\n<rawstyle@soundtracker.org>"
			  "\n\nIncludes OpenCP player from Niklas Beisert and Tammo Hinrichs.");
    gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(about)->vbox), 10);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(about)->vbox), label, TRUE, TRUE, 10);
    gtk_widget_show(label);
    
    button = gtk_button_new_with_label (_("Ok"));
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(about)->action_area), button, TRUE, TRUE, 10);
    g_signal_connect_swapped(button, "clicked",
                               G_CALLBACK(about_close), 0);
    gtk_widget_grab_default (button);
    gtk_widget_show (button);
    
    gtk_widget_show(about);
#endif
}

static void
menubar_clear_callback (gint reply,
			gpointer data)
{
    if(reply == 0) {
	if(GPOINTER_TO_INT(data) == 0) {
	    gui_free_xm();
	    gui_new_xm();
	    xm->modified = 0;
	} else {
	    gui_play_stop();
	    st_clean_song(xm);
	    gui_init_xm(1, TRUE);
	    xm->modified = 0;
	}
    }
}

static void
menubar_clear_clicked (void *a,
		       gpointer b)
{
    if(xm->modified) {
	gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
				  _("Are you sure you want to do this?\nAll changes will be lost!"),
				  menubar_clear_callback,
				  b);
    } else {
	menubar_clear_callback(0, b);
    }
}

static void
menubar_quit_requested_callback (gint reply,
				 gpointer data)
{
    if(reply == 0) {
	gtk_main_quit();
    }
}

void
menubar_quit_requested (void)
{
    if(xm->modified) {
	gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
				  _("Are you sure you want to quit?\nAll changes will be lost!"),
				  menubar_quit_requested_callback,
				  0);
    } else {
	menubar_quit_requested_callback(0, NULL);
    }
}

/* Arghl. When will I be able to make GNOME a requirement such as GTK+? */

#ifdef USE_GNOME
#define _FUNCMACRO_(name) name (GtkWidget *widget)
#else
#define _FUNCMACRO_(name) name (void *dummy, void *dummy2, GtkWidget *widget)
#endif

static void
_FUNCMACRO_(menubar_backing_store_toggled)
{
    gui_settings.gui_use_backing_store = GTK_CHECK_MENU_ITEM(widget)->active;
    tracker_set_backing_store(tracker, gui_settings.gui_use_backing_store);
}

static void
_FUNCMACRO_(menubar_scopes_toggled)
{
    gui_settings.gui_display_scopes = GTK_CHECK_MENU_ITEM(widget)->active;
    scope_group_enable_scopes(scopegroup, gui_settings.gui_display_scopes);
}

static void
_FUNCMACRO_(menubar_splash_toggled)
{
    gui_settings.gui_disable_splash = GTK_CHECK_MENU_ITEM(widget)->active;
}

static void
_FUNCMACRO_(menubar_mark_mode_toggled)
{
    if(!mark_mode_toggle_ignore) {
	gboolean state = GTK_CHECK_MENU_ITEM(widget)->active;
	tracker_mark_selection(tracker, state);
    }
}

static void
_FUNCMACRO_(menubar_save_settings_on_exit_toggled)
{
    gui_settings.save_settings_on_exit = GTK_CHECK_MENU_ITEM(widget)->active;
}

static void
menubar_save_settings_now (void)
{
    gui_settings_save_config();
    keys_save_config();
    audioconfig_save_config();
    trackersettings_write_settings();
#if (defined(DRIVER_ALSA_050) || defined(DRIVER_ALSA_09x)) && defined(USE_GNOME)
    midi_save_config();
#endif
}

static void
menubar_handle_cutcopypaste (void *p, guint a)
{
    Tracker *t = tracker;
    int ci = gui_get_current_instrument() - 1;
    STInstrument *curins = &xm->instruments[ci];

    switch(a){
    case 0:		//Cut
	switch(notebook_current_page){
	case NOTEBOOK_PAGE_TRACKER:
	    track_editor_cut_selection(t);
	    break;
	case NOTEBOOK_PAGE_INSTRUMENT_EDITOR:
	case NOTEBOOK_PAGE_MODULE_INFO:
	    instrument_editor_cut_instrument(curins);
	    xm_set_modified(1);
	    instrument_editor_update();
	    sample_editor_update();
	    modinfo_update_instrument(ci);
	    break;
	case NOTEBOOK_PAGE_SAMPLE_EDITOR:
	    sample_editor_copy_cut_common(TRUE, TRUE);
	    xm_set_modified(1);
	    break;
	}
	break;
    case 1:		//Copy
	switch(notebook_current_page){
	case NOTEBOOK_PAGE_TRACKER:
	    track_editor_copy_selection(t);
	    break;
	case NOTEBOOK_PAGE_INSTRUMENT_EDITOR:
	case NOTEBOOK_PAGE_MODULE_INFO:
	    instrument_editor_copy_instrument(curins);
	    break;
	case NOTEBOOK_PAGE_SAMPLE_EDITOR:
	    sample_editor_copy_cut_common(TRUE, FALSE);
	    break;
	}
	break;
    case 2:		//Paste
	switch(notebook_current_page){
	case NOTEBOOK_PAGE_TRACKER:
	    track_editor_paste_selection(t);
	    break;
	case NOTEBOOK_PAGE_INSTRUMENT_EDITOR:
	case NOTEBOOK_PAGE_MODULE_INFO:
	    instrument_editor_paste_instrument(curins);
	    xm_set_modified(1);
	    instrument_editor_update();
	    sample_editor_update();
	    modinfo_update_instrument(ci);
	    break;
	case NOTEBOOK_PAGE_SAMPLE_EDITOR:
	    sample_editor_paste_clicked();
	    xm_set_modified(1);
	    break;
	}
	break;
    }
}

static void
menubar_handle_edit_menu (void *p,
			  guint a)
{
    Tracker *t = tracker;

    static void (* const functions[]) (Tracker *) = {
	track_editor_cut_pattern,                       // 0
	track_editor_copy_pattern,                      // 1
	track_editor_paste_pattern,                     // 2
	track_editor_cut_track,                         // 3
        track_editor_copy_track,                        // 4
        track_editor_paste_track,                       // 5
	track_editor_insert_track,                      // 6
        track_editor_delete_track,                      // 7
	track_editor_interpolate_fx,                    // 8
        track_editor_clear_mark_selection,              // 9
        track_editor_cut_selection,                     // 10
        track_editor_copy_selection,                    // 11
        track_editor_paste_selection,                   // 12
	track_editor_kill_notes_track,                  // 13
        NULL,                                           // 14
        NULL,                                           // 15
        NULL,                                           // 16
        NULL,                                           // 17
        NULL,                                           // 18
        NULL,                                           // 19
    };

#ifdef USE_GNOME
    GtkWidget *active;
    GtkMenu *menu = (GtkMenu *)a;
    /* Check if it's call from popup */
    if(a > sizeof(functions)/sizeof(functions[0]) && GTK_IS_WIDGET(menu)) {
	/* Get active menu */
	active = gtk_object_get_data (GTK_OBJECT (menu), "gnome_popup_menu_active_item");
	/* Get user_data for this menu */
	a = (gint)gtk_object_get_data (GTK_OBJECT (active), GNOMEUIINFO_KEY_UIDATA);
    }
#endif

    switch(a) {
    case 14:
        track_editor_cmd_mvalue(t, TRUE);   /* increment CMD value */
        break;
    case 15:
        track_editor_cmd_mvalue(t, FALSE);  /* decrement CMD value */
        break;
    case 16:
	transposition_transpose_selection(t, +1);
	break;
    case 17:
	transposition_transpose_selection(t, -1);
	break;
    case 18:
	transposition_transpose_selection(t, +12);
	break;
    case 19:
	transposition_transpose_selection(t, -12);
	break;

    default:
        functions[a](t);
        break;
    }
}

static void
menubar_settings_tracker_next_font (void)
{
    trackersettings_cycle_font_forward(TRACKERSETTINGS(trackersettings));
}

static void
menubar_settings_tracker_prev_font (void)
{
    trackersettings_cycle_font_backward(TRACKERSETTINGS(trackersettings));
}

static void
menubar_toggle_perm_wrapper (GtkObject *object, gboolean all)
{
    track_editor_toggle_permanentness(tracker, all);
}

#ifndef USE_GNOME

/* Define GNOME stuff for our GNOME->GtkItemFactory converter. */

typedef struct GnomeUIInfo {
    int type;
    gchar *title;
    gpointer dummy1;
    gpointer func;
    gpointer funcparam;
    gpointer dummy2;
    int dummy3, dummy4;
    int shortcut;
    int shortcutmod;
    gpointer dummy5;
    GtkWidget *widget;
} GnomeUIInfo;

#define GNOME_APP_UI_END 0
#define GNOME_APP_UI_ITEM 1
#define GNOME_APP_UI_SEPARATOR 2
#define GNOME_APP_UI_TOGGLEITEM 3
#define GNOME_APP_UI_SUBTREE 4

#define GNOME_APP_PIXMAP_STOCK 0
#define GNOME_APP_PIXMAP_NONE 0

#define GNOME_STOCK_MENU_OPEN 0
#define GNOME_STOCK_MENU_SAVE_AS 0
#define GNOME_STOCK_MENU_QUIT 0
#define GNOME_STOCK_MENU_NEW 0
#define GNOME_STOCK_MENU_CUT 0
#define GNOME_STOCK_MENU_COPY 0
#define GNOME_STOCK_MENU_PASTE 0
#define GNOME_STOCK_MENU_PREF 0
#define GNOME_STOCK_MENU_SAVE 0
#define GNOME_STOCK_MENU_ABOUT 0
#define GNOME_STOCK_MENU_BOOK_RED 0

#define GNOMEUIINFO_SEPARATOR { GNOME_APP_UI_SEPARATOR, "-", }
#define GNOMEUIINFO_END { GNOME_APP_UI_END, }
#define GNOMEUIINFO_SUBTREE(X,Y) { GNOME_APP_UI_SUBTREE, (X), (Y) }

#endif

static GnomeUIInfo file_menu[] = {
    { GNOME_APP_UI_ITEM, N_("_Open..."), NULL, fileops_open_dialog, (gpointer)DIALOG_LOAD_MOD, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN, 'O', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("Save _as..."), NULL, fileops_open_dialog, (gpointer)DIALOG_SAVE_MOD, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE_AS, 'A', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_SEPARATOR,

#define SAVE_MOD_AS_WAV_POSITION 3
    { GNOME_APP_UI_ITEM, N_("Save Module as _WAV..."), NULL, fileops_open_dialog, (gpointer)DIALOG_SAVE_MOD_AS_WAV, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE_AS, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("Save XM without samples..."), NULL, fileops_open_dialog, (gpointer)DIALOG_SAVE_SONG_AS_XM, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE_AS, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("_Quit"), NULL, menubar_quit_requested, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_QUIT, 'Q', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_END
};

static GnomeUIInfo module_menu[] = {
    { GNOME_APP_UI_ITEM, N_("Clear _All"), NULL, menubar_clear_clicked, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("Clear _Patterns Only"), NULL, menubar_clear_clicked, (gpointer)1, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("_Optimize Module"), NULL, modinfo_optimize_module, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },

    GNOMEUIINFO_END
};

static GnomeUIInfo edit_pattern_menu[] = {
    { GNOME_APP_UI_ITEM, N_("C_ut"), NULL, menubar_handle_edit_menu, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CUT, GDK_F3, GDK_MOD1_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("_Copy"), NULL, menubar_handle_edit_menu, (gpointer)1, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_COPY, GDK_F4, GDK_MOD1_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("_Paste"), NULL, menubar_handle_edit_menu, (gpointer)2, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE, GDK_F5, GDK_MOD1_MASK, NULL },

    GNOMEUIINFO_END
};

static GnomeUIInfo edit_track_menu[] = {
    { GNOME_APP_UI_ITEM, N_("C_ut"), NULL, menubar_handle_edit_menu, (gpointer)3, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CUT, GDK_F3, GDK_SHIFT_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("_Copy"), NULL, menubar_handle_edit_menu, (gpointer)4, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_COPY, GDK_F4, GDK_SHIFT_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("_Paste"), NULL, menubar_handle_edit_menu, (gpointer)5, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE, GDK_F5, GDK_SHIFT_MASK, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("_Kill notes"), NULL, menubar_handle_edit_menu, (gpointer)13, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 'K', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("_Insert track"), NULL, menubar_handle_edit_menu, (gpointer)6, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("_Delete track"), NULL, menubar_handle_edit_menu, (gpointer)7, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("Increment cmd value"), NULL, menubar_handle_edit_menu, (gpointer)14, NULL,
      GNOME_APP_PIXMAP_NONE, GNOME_STOCK_MENU_NEW, '=', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("Decrement cmd value"), NULL, menubar_handle_edit_menu, (gpointer)15, NULL,
      GNOME_APP_PIXMAP_NONE, GNOME_STOCK_MENU_NEW, '-', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_END
};

static GnomeUIInfo edit_selection_menu[] = {
    { GNOME_APP_UI_TOGGLEITEM, N_("_Mark mode"), NULL, menubar_mark_mode_toggled, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'B', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("C_lear block marks"), NULL, menubar_handle_edit_menu, (gpointer)9, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'B', GDK_CONTROL_MASK | GDK_SHIFT_MASK, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("_Interpolate effects"), NULL, menubar_handle_edit_menu, (gpointer)8, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'I', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("Transpose half-note up"), NULL, menubar_handle_edit_menu, (gpointer)16, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'Q', GDK_MOD1_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("Transpose half-note down"), NULL, menubar_handle_edit_menu, (gpointer)17, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'A', GDK_MOD1_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("Transpose octave up"), NULL, menubar_handle_edit_menu, (gpointer)18, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'Q', GDK_MOD1_MASK | GDK_SHIFT_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("Transpose octave down"), NULL, menubar_handle_edit_menu, (gpointer)19, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'A', GDK_MOD1_MASK | GDK_SHIFT_MASK, NULL },

    GNOMEUIINFO_END

};

static GnomeUIInfo edit_menu[] = {
    { GNOME_APP_UI_ITEM, N_("C_ut"), NULL, menubar_handle_cutcopypaste, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CUT, 'X', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("_Copy"), NULL, menubar_handle_cutcopypaste, (gpointer)1, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_COPY, 'C', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("P_aste"), NULL, menubar_handle_cutcopypaste, (gpointer)2, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE, 'V', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_TOGGLEITEM, N_("_Jazz Edit Mode"), NULL, track_editor_toggle_jazz_edit, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, ' ', GDK_SHIFT_MASK, NULL }, 

    { GNOME_APP_UI_TOGGLEITEM, N_("_Record keyreleases"), NULL, track_editor_toggle_insert_noteoff, 
      (gpointer)0, NULL, GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },  
    
    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("Transp_osition..."), NULL, transposition_dialog, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'T', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_SEPARATOR,

    GNOMEUIINFO_SUBTREE (N_("_Pattern"), edit_pattern_menu),
    GNOMEUIINFO_SUBTREE (N_("_Track"), edit_track_menu),
    GNOMEUIINFO_SUBTREE (N_("_Selection"), edit_selection_menu),

    GNOMEUIINFO_END
};

#ifdef USE_GNOME

static GnomeUIInfo track_editor_popup_edit_selection_menu[] = {
    { GNOME_APP_UI_ITEM, N_("C_lear block marks"), NULL, menubar_handle_edit_menu, (gpointer)9, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'B', GDK_CONTROL_MASK | GDK_SHIFT_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("C_ut"), NULL, menubar_handle_edit_menu, (gpointer)10, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CUT, 'X', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("_Copy"), NULL, menubar_handle_edit_menu, (gpointer)11, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_COPY, 'C', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("_Paste"), NULL, menubar_handle_edit_menu, (gpointer)12, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE, 'V', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("_Interpolate effects"), NULL, menubar_handle_edit_menu, (gpointer)8, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'I', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_END
};

GnomeUIInfo track_editor_popup_menu[] =	{
    GNOMEUIINFO_SUBTREE (N_("_Pattern"), edit_pattern_menu),
    GNOMEUIINFO_SUBTREE (N_("_Track"), edit_track_menu),
    GNOMEUIINFO_SUBTREE (N_("_Selection"), track_editor_popup_edit_selection_menu),
    GNOMEUIINFO_END
};

#endif

static GnomeUIInfo pattern_menu[] = {
    { GNOME_APP_UI_ITEM, N_("_Find Unused Pattern"), NULL, modinfo_find_unused_pattern, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'F', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("_Copy Current to Unused Pattern"), NULL, modinfo_copy_to_unused_pattern, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'G', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("C_lear Unused Patterns"), NULL, modinfo_clear_unused_patterns, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("_Pack Patterns"), NULL, modinfo_pack_patterns, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("_Save Current Pattern"), NULL, fileops_open_dialog, (gpointer)DIALOG_SAVE_PATTERN, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE_AS, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("L_oad Pattern"), NULL, fileops_open_dialog, (gpointer)DIALOG_LOAD_PATTERN, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("Sh_rink Current Pattern"), NULL, gui_shrink_pattern, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("_Expand Current Pattern"), NULL, gui_expand_pattern, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },

    GNOMEUIINFO_END
};

static GnomeUIInfo track_menu[] = {
    { GNOME_APP_UI_ITEM, N_("_Toggle Current Channel Permanentness"), NULL, menubar_toggle_perm_wrapper, (gpointer)FALSE, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'P', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("Toggle _All Channels Permanentness"), NULL, menubar_toggle_perm_wrapper, (gpointer)TRUE, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 'P', GDK_CONTROL_MASK | GDK_SHIFT_MASK, NULL },

    GNOMEUIINFO_END
};


static GnomeUIInfo instrument_menu[] = {
    { GNOME_APP_UI_ITEM, N_("_Load XI..."), NULL, fileops_open_dialog, (gpointer)DIALOG_LOAD_INSTRUMENT, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("_Save XI..."), NULL, fileops_open_dialog, (gpointer)DIALOG_SAVE_INSTRUMENT, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE_AS, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("_Clear Current"), NULL, instrument_editor_clear_current_instrument, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("_Delete Unused Instruments"), NULL, modinfo_delete_unused_instruments, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },

    GNOMEUIINFO_END
};

static GnomeUIInfo settings_tracker_menu[] = {
    { GNOME_APP_UI_TOGGLEITEM, N_("_Flicker-free scrolling"), 0, menubar_backing_store_toggled, 0, 0,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },

    { GNOME_APP_UI_ITEM, N_("_Previous font"), NULL, menubar_settings_tracker_prev_font, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, GDK_KP_Subtract, GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("_Next font"), NULL, menubar_settings_tracker_next_font, (gpointer)0, NULL,
      GNOME_APP_PIXMAP_NONE, 0, GDK_KP_Add, GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("Change preferred _accidental type"), 0, gui_accidentals_clicked, 0, 0,
      GNOME_APP_PIXMAP_NONE, 0, 'S', GDK_CONTROL_MASK, NULL },
    { GNOME_APP_UI_ITEM, N_("Change effect column editing _direction"), 0, gui_direction_clicked, 0, 0,
      GNOME_APP_PIXMAP_NONE, 0, 'D', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_END
};

// Take care to adjust the indices in menubar_init_prefs() when reordering stuff here!
static GnomeUIInfo settings_menu[] = {
    { GNOME_APP_UI_TOGGLEITEM, N_("Display _Oscilloscopes"), 0, menubar_scopes_toggled, 0, 0,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },

    GNOMEUIINFO_SUBTREE (N_("_Tracker"), settings_tracker_menu),

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("_Keyboard Configuration..."), NULL, keys_dialog, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PREF, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("_Audio Configuration..."), NULL, audioconfig_dialog, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PREF, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("_GUI Configuration..."), NULL, gui_settings_dialog, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PREF, 0, 0, NULL },
    /* Define a constant for the index of the MIDI settings item.
       Will allow to deactivate (grey out) this menu item 
       if MIDI is not supported. See menubar_create(). */
#define SETTINGS_MENU_MIDI_INDEX 6
    { GNOME_APP_UI_ITEM, N_("_MIDI Configuration..."), NULL, midi_settings_dialog, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PREF, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_TOGGLEITEM, N_("Disable splash screen"), 0, menubar_splash_toggled, 0, 0,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("_Save Settings now"), NULL, menubar_save_settings_now, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE, 0, 0, NULL },
    { GNOME_APP_UI_TOGGLEITEM, N_("Save Settings on _Exit"), 0, menubar_save_settings_on_exit_toggled, 0, 0,
      GNOME_APP_PIXMAP_NONE, 0, 0, 0, NULL },

    GNOMEUIINFO_END
};

static GnomeUIInfo help_menu[] = {
    { GNOME_APP_UI_ITEM, N_("_About..."), NULL, about_dialog, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("Show _Tips..."), NULL, tips_dialog_open, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_BOOK_RED, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("_XM Effects..."), NULL, cheat_sheet_dialog, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_BOOK_RED, 'H', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_END
};

static GnomeUIInfo main_menu[] = {
    GNOMEUIINFO_SUBTREE (N_("_File"), file_menu),
    GNOMEUIINFO_SUBTREE (N_("_Module"), module_menu),
    GNOMEUIINFO_SUBTREE (N_("_Edit"), edit_menu),
    GNOMEUIINFO_SUBTREE (N_("_Pattern"), pattern_menu),
    GNOMEUIINFO_SUBTREE (N_("_Track"), track_menu),
    GNOMEUIINFO_SUBTREE (N_("_Instrument"), instrument_menu),
    GNOMEUIINFO_SUBTREE (N_("_Settings"), settings_menu),
    GNOMEUIINFO_SUBTREE (N_("_Help"), help_menu),
    GNOMEUIINFO_END
};

void
menubar_init_prefs ()
{
#ifndef USE_GNOME
    gtk_item_factory_parse_rc(prefs_get_filename("non-gnome-accels"));
#endif

    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(settings_menu[0].widget), gui_settings.gui_display_scopes);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(settings_tracker_menu[0].widget), gui_settings.gui_use_backing_store);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(settings_menu[8].widget), gui_settings.gui_disable_splash);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(settings_menu[10].widget), gui_settings.save_settings_on_exit);

    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(edit_menu[5].widget), TRUE); // Record aftertouch
#if USE_SNDFILE == 0 && defined (NO_AUDIOFILE)
    gtk_widget_set_sensitive(file_menu[SAVE_MOD_AS_WAV_POSITION].widget, FALSE);
#endif
#if ! (defined(DRIVER_ALSA_050) || defined(DRIVER_ALSA_09x))
    gtk_widget_set_sensitive(settings_menu[SETTINGS_MENU_MIDI_INDEX].widget,
			     FALSE);
#endif	
}

void
menubar_block_mode_set(gboolean state)
{
    mark_mode_toggle_ignore = TRUE;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(edit_selection_menu[0].widget), state);
    mark_mode_toggle_ignore = FALSE;
}

#ifdef USE_GNOME

void
menubar_create (GtkWidget *window, GtkWidget *destbox)
{
    gnome_app_create_menus(GNOME_APP(window), main_menu);	
}

void
menubar_write_accels ()
{
    // GNOME does this for us
}

#else

/*

  Coming up is the code that generates the non-GNOME menu bar from the
  GNOME menu structure.

  Earlier versions of SoundTracker used an extra array of
  GtkItemFactoryEntry's -- problem: the translators had to do double
  work, and if some of the entries were not translated (in a new
  release, for example), the whole menu bar translation had to be
  disabled, because non-translated entries could interfere with
  translated ones.

 */

static void
strip_underscores (char *dest, const char *src)
{
    char c;

    while((c = *src++)) {
	if(c != '_') {
	    *dest++ = c;
	}
    }

    *dest = '\0';
}

/* This function recursively builds an array of GtkItemFactoryEntry's
   from the GNOME menu structure. The GtkItemFactoryEntry array is in
   the global variable 'menubar_gtk_items'. */

static int
menubar_gnome_to_gtk (GnomeUIInfo *gnometree,
		      int n,
		      gchar *nameprefix)
{
    gchar buf[256], acc[100];
    char *xname;

    for(; gnometree->type != GNOME_APP_UI_END; gnometree++, n++) {
	menubar_gtk_items = g_renew(GtkItemFactoryEntry, menubar_gtk_items, n + 1);

	strcpy(buf, nameprefix);
	strcat(buf, "/");

	acc[0] = 0;
	if(gnometree->shortcutmod & GDK_MOD1_MASK) {
	    strcat(acc, "<alt>");
	}
	if(gnometree->shortcutmod & GDK_SHIFT_MASK) {
	    strcat(acc, "<shift>");
	}
	if(gnometree->shortcutmod & GDK_CONTROL_MASK) {
	    strcat(acc, "<control>");
	}
	if((xname = XKeysymToString(gnometree->shortcut))) {
	    strcat(acc, xname);
	}
	menubar_gtk_items[n].accelerator = g_strdup(acc);

	strcat(buf, _(gnometree->title));
	menubar_gtk_items[n].path = g_strdup(buf);

	switch(gnometree->type) {
	case GNOME_APP_UI_SUBTREE:
	    menubar_gtk_items[n].callback = (GtkItemFactoryCallback)gnometree->func;
	    menubar_gtk_items[n].callback_action =
	        GPOINTER_TO_UINT(gnometree->funcparam);
	    menubar_gtk_items[n].item_type = "<Branch>";
	    strip_underscores(buf, buf);
	    n = menubar_gnome_to_gtk((GnomeUIInfo*)gnometree->dummy1, n + 1, buf) - 1;
	    break;
	case GNOME_APP_UI_ITEM:
	    menubar_gtk_items[n].callback = (GtkItemFactoryCallback)gnometree->func;
	    menubar_gtk_items[n].callback_action =
	        GPOINTER_TO_UINT(gnometree->funcparam);
	    menubar_gtk_items[n].item_type = NULL;
	    break;
	case GNOME_APP_UI_TOGGLEITEM:
	    menubar_gtk_items[n].callback = (GtkItemFactoryCallback)gnometree->func;
	    menubar_gtk_items[n].callback_action =
	        GPOINTER_TO_UINT(gnometree->funcparam);
	    menubar_gtk_items[n].item_type = "<ToggleItem>";
	    break;
	case GNOME_APP_UI_SEPARATOR:
	    menubar_gtk_items[n].callback = NULL;
	    menubar_gtk_items[n].callback_action = 0;
	    menubar_gtk_items[n].item_type = "<Separator>";
	    break;
	default:
	    break;
	}
    }

    return n;
}

/* After generating the GTK+ menu, this function gets GtkWidget
   pointers to all the menu items and stores them in the GNOME
   array. */

static int
menubar_gnome_to_gtk_get_widgets (GnomeUIInfo *gnometree,
				  int n)
{
    for(; gnometree->type != GNOME_APP_UI_END; gnometree++, n++) {
	switch(gnometree->type) {
	case GNOME_APP_UI_SUBTREE:
	    n = menubar_gnome_to_gtk_get_widgets((GnomeUIInfo*)gnometree->dummy1, n+1) - 1;
	    break;
	case GNOME_APP_UI_ITEM:
	case GNOME_APP_UI_TOGGLEITEM:
	    strip_underscores(menubar_gtk_items[n].path, menubar_gtk_items[n].path);
	    gnometree->widget = gtk_item_factory_get_widget(item_factory, menubar_gtk_items[n].path);
	    g_free(menubar_gtk_items[n].path);
	    break;
	default:
	    break;
	}
    }

    return n;
}

void
menubar_create (GtkWidget *window, GtkWidget *destbox)
{
    GtkWidget *thing;
    int n;

    accel_group = gtk_accel_group_new ();

    item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR,
					 "<main>",
					 accel_group);

    n = menubar_gnome_to_gtk(main_menu, 0, "");

    gtk_item_factory_create_items (item_factory, n, menubar_gtk_items, NULL);

    menubar_gnome_to_gtk_get_widgets(main_menu, 0);
    g_free(menubar_gtk_items);

    gtk_accel_group_attach (accel_group, GTK_OBJECT (window));

    thing = gtk_item_factory_get_widget (item_factory, "<main>");
    gtk_box_pack_start(GTK_BOX(destbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
}

void
menubar_write_accels ()
{
    gtk_item_factory_dump_rc(prefs_get_filename("non-gnome-accels"), NULL, TRUE);
}

#endif

