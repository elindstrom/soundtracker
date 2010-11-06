
/*
 * The Real SoundTracker - module info page
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

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

//#include <gtk/gtkfeatures.h>

#include "i18n.h"
#include "module-info.h"
#include "gui.h"
#include "gui-subs.h"
#include "xm.h"
#include "st-subs.h"
#include "main.h"
#include "sample-editor.h"
#include "instrument-editor.h"
#include "keys.h"
#include "track-editor.h"
#include "entry-workaround.h"

static GtkWidget *ilist, *slist, *songname;
static GtkWidget *freqmode_w[2], *ptmode_toggle;
static int curi = 0, curs = 0;

static void
ptmode_changed (GtkWidget *widget)
{
    int m = GTK_TOGGLE_BUTTON(widget)->active;
    if(xm) {
	xm->flags &= ~XM_FLAGS_IS_MOD;
	xm->flags |= m * XM_FLAGS_IS_MOD;
    }
    xm_set_modified(1);
}

static void
freqmode_changed (void)
{
    int m = find_current_toggle(freqmode_w, 2);
    if(xm) {
	xm->flags &= ~XM_FLAGS_AMIGA_FREQ;
	xm->flags |= m * XM_FLAGS_AMIGA_FREQ;
    }
    xm_set_modified(1);

}

static void
songname_changed (GtkEntry *entry)
{
    strncpy(xm->name, gtk_entry_get_text(entry), 20);
    xm->name[20] = 0;
    xm_set_modified(1);
}

static void
ilist_select (GtkTreeSelection *sel)
{
    GtkTreeModel *mdl;
    GtkTreeIter iter;
    gchar *str;
    gint row;
    
    if(gtk_tree_selection_get_selected(sel, &mdl, &iter)) {
	row = atoi(str = gtk_tree_model_get_string_from_iter(mdl, &iter));
	g_free(str);
	if(row == curi)
	    return;
	curi = row;
	gui_set_current_instrument(row + 1);
    }
}

static void
slist_select (GtkTreeSelection *sel)
{
    GtkTreeModel *mdl;
    GtkTreeIter iter;
    gchar *str;
    gint row;
    
    if(gtk_tree_selection_get_selected(sel, &mdl, &iter)) {
	row = atoi(str = gtk_tree_model_get_string_from_iter(mdl, &iter));
	g_free(str);
	if(row == curs)
	    return;
	curs = row;
	gui_set_current_sample(row);
    }
}

void
modinfo_page_create (GtkNotebook *nb)
{
    GtkWidget *hbox, *thing, *vbox;
    GtkListStore *list_store;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *ititles[3] = { "n", _("Instrument Name"), _("#smpl") };
    gchar *stitles[2] = { "n", _("Sample Name") };
    GType itypes[3] = {G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT};
    GType stypes[2] = {G_TYPE_INT, G_TYPE_STRING};
    gfloat ialignments[3] = {0.5, 0.0, 0.5};
    gfloat salignments[2] = {0.5, 0.0};
    gboolean iexpands[3] = {FALSE, TRUE, FALSE};
    gboolean sexpands[3] = {FALSE, TRUE};
    static const char *freqlabels[] = { N_("Linear"), N_("Amiga"), NULL };
    int i;

    hbox = gtk_hbox_new(TRUE, 10);
    gtk_container_border_width(GTK_CONTAINER(hbox), 10);
    gtk_notebook_append_page(nb, hbox, gtk_label_new(_("Module Info")));
    gtk_widget_show(hbox);

    ilist = gui_list_in_scrolled_window(3, ititles, hbox, itypes, ialignments,
					iexpands, GTK_SELECTION_BROWSE);
    list_store = GUI_GET_LIST_STORE(ilist);
    model = gui_list_freeze(ilist);
    for(i = 1; i <= 128; i++) {
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set(list_store, &iter, 0, i, 1, "", 2, 0, -1);
    }
    gui_list_thaw(ilist, model);

    gui_list_handle_selection(ilist, G_CALLBACK(ilist_select), NULL);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    slist = gui_list_in_scrolled_window(2, stitles, vbox, stypes, salignments,
					sexpands, GTK_SELECTION_BROWSE);
    list_store = GUI_GET_LIST_STORE(slist);
    model = gui_list_freeze(slist);
    for(i = 1; i <= 16; i++) {
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set(list_store, &iter, 0, i, 1, "", -1);
    }
    gui_list_thaw(slist, model);
    gui_list_handle_selection(slist, G_CALLBACK(slist_select), NULL);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_label_new(_("Songname:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    gui_get_text_entry(20, songname_changed, &songname);
    gtk_box_pack_start(GTK_BOX(hbox), songname, TRUE, TRUE, 0);
    g_signal_connect_after(songname, "changed",
			     G_CALLBACK(songname_changed), NULL);
    gtk_widget_show(songname);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    add_empty_hbox(hbox);
    thing = make_labelled_radio_group_box(_("Frequencies:"), freqlabels, freqmode_w, freqmode_changed);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    add_empty_hbox(hbox);
    
    ptmode_toggle = gtk_check_button_new_with_label(_("ProTracker Mode"));
    gtk_box_pack_start(GTK_BOX(hbox), ptmode_toggle, FALSE, TRUE, 0);
    gtk_widget_show(ptmode_toggle);
    g_signal_connect(ptmode_toggle, "toggled",
			G_CALLBACK(ptmode_changed), NULL);

    add_empty_hbox(hbox);
}

void
modinfo_delete_unused_instruments (void)
{
    int i;

    for(i = 0; i < sizeof(xm->instruments) / sizeof(xm->instruments[0]); i++) {
	if(!st_instrument_used_in_song(xm, i + 1)) {
	    st_clean_instrument(&xm->instruments[i], NULL);
	    xm_set_modified(1);
	}
    }

    sample_editor_update();
    instrument_editor_update();
    modinfo_update_all();
}

gboolean
modinfo_page_handle_keys (int shift,
			  int ctrl,
			  int alt,
			  guint32 keyval,
			  gboolean pressed)
{
    int i;

    i = keys_get_key_meaning(keyval, ENCODE_MODIFIERS(shift, ctrl, alt));
    if(i != -1 && KEYS_MEANING_TYPE(i) == KEYS_MEANING_NOTE) {
	track_editor_do_the_note_key(i, pressed, keyval, ENCODE_MODIFIERS(shift, ctrl, alt));
	return TRUE;
    }

    return FALSE;
}

void
modinfo_update_instrument (int n)
{
    int i;
    GtkTreeIter iter;
    GtkListStore *list_store = GUI_GET_LIST_STORE(ilist);

    if(!gui_list_get_iter(n, list_store, &iter))
       return; /* Some bullshit happens :-/ */
    gtk_list_store_set(list_store, &iter, 1, xm->instruments[n].name,
		       2, st_instrument_num_samples(&xm->instruments[n]), -1);

    if(n == curi) {
	for(i = 0; i < 16; i++)
	    modinfo_update_sample(i);
    }
}

void
modinfo_update_sample (int n)
{
    GtkTreeIter iter;
    GtkListStore *list_store = GUI_GET_LIST_STORE(slist);

    if(!gui_list_get_iter(n, list_store, &iter))
       return; /* Some bullshit happens :-/ */
    gtk_list_store_set(list_store, &iter, 1,
		       xm->instruments[curi].samples[n].name, -1);
}

void
modinfo_update_all (void)
{
    int i;
    int m = xm_get_modified();

    for(i = 0; i < 128; i++)
	modinfo_update_instrument(i);

    wa_entry_set_text(GTK_ENTRY(songname), xm->name);

    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ptmode_toggle), xm->flags & XM_FLAGS_IS_MOD);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(freqmode_w[xm->flags & XM_FLAGS_AMIGA_FREQ]), TRUE);
    xm_set_modified(m);
}

void
modinfo_set_current_instrument (int n)
{
    int i;

    g_return_if_fail(n >= 0 && n <= 127);
    curi = n;
    gui_list_select(ilist, n);

    for(i = 0; i < 16; i++)
	modinfo_update_sample(i);
}

void
modinfo_set_current_sample (int n)
{
    g_return_if_fail(n >= 0 && n <= 15);
    curs = n;
    gui_list_select(slist, n);
}

void
modinfo_find_unused_pattern (void)
{
    int n = st_find_first_unused_and_empty_pattern(xm);

    printf("%d\n", n);

    if(n != -1)
	gui_set_current_pattern(n, TRUE);
}

void
modinfo_copy_to_unused_pattern (void)
{
    int n = st_find_first_unused_and_empty_pattern(xm);
    int c = gui_get_current_pattern();

    if(n != -1 && !st_is_empty_pattern(&xm->patterns[c])) {
	gui_play_stop();
	st_copy_pattern(&xm->patterns[n], &xm->patterns[c]);
	xm_set_modified(1);
	gui_set_current_pattern(n, TRUE);
    }
}

// Move unused patterns to the end of the pattern space
void
modinfo_pack_patterns (void)
{
    int i, j, last, used;

    // Get number of last used pattern and number of used patterns
    for(i = 0, last = 0, used = 0; i < sizeof(xm->patterns) / sizeof(xm->patterns[0]); i++) {
	if(st_is_pattern_used_in_song(xm, i)) {
	    last = i;
	    used++;
	}
    }

    // Put unused patterns to the end
    for(i = 0; i < used; ) {
	if(!st_is_pattern_used_in_song(xm, i)) {
	    for(j = i; j < last; j++) 
		st_exchange_patterns(xm, j, j + 1);
	} else {
	    i++;
	}
    }

    gui_playlist_initialize();
    xm_set_modified(1);
}

// Put patterns in playback order, move unused patterns to the end of the pattern space
void
modinfo_reorder_patterns (void)
{
    modinfo_pack_patterns();
    xm_set_modified(1);
}

// Clear patterns which are not in the playlist
void
modinfo_clear_unused_patterns (void)
{
    int i;
    
    for(i = 0; i < sizeof(xm->patterns) / sizeof(xm->patterns[0]); i++)
	if(!st_is_pattern_used_in_song(xm, i))
	    st_clear_pattern(&xm->patterns[i]);

    tracker_redraw(tracker);
    xm_set_modified(1);
}

// Optimize -- clear everything unused

static void
modinfo_optimize_module_callback (gint reply,
		   gpointer data)
{
    if(reply == 0) {
	modinfo_clear_unused_patterns();
    	modinfo_delete_unused_instruments();
	modinfo_reorder_patterns();
    	xm_set_modified(1);
    }
}

void
modinfo_optimize_module (void)
{
    char infbuf[512];
    int a, b, c, d, e;

    d = sizeof(xm->patterns) / sizeof(xm->patterns[0]);
    e = sizeof(xm->instruments) / sizeof(xm->instruments[0]);
    for(a = 0, b = 0, c = 0; a < d; a++) {
	if(!st_is_pattern_used_in_song(xm, a))
	    b++;
	
        if(a<e)
	    if(!st_instrument_used_in_song(xm, a + 1)) c++;
    }

    g_sprintf(infbuf, _("Unused patterns: %d (used: %d)\nUnused instruments: %d (used: %d)\n\nClear unused and reorder playlist?\n"),
					b, d-b, c, e-c);

    gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
			      infbuf, modinfo_optimize_module_callback, NULL);

}

