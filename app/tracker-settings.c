
/*
 * The Real SoundTracker - GTK+ Tracker widget settings
 *
 * Copyright (C) 2001 Michael Krause
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

#include <gtk/gtk.h>

#include <string.h>

#include "tracker-settings.h"
#include "i18n.h"
#include "gui-subs.h"
#include "preferences.h"

/* A global font list is kept here, so that when (at some time in the
   future) we have to deal with multiple Tracker widgets, this can be
   easily adapted. */

static GList *trackersettings_fontlist;

static void
trackersettings_gui_add_font (TrackerSettings *ts,
			      gchar *fontname,
			      int position)
{
    gchar *insertbuf[1];

    trackersettings_fontlist = g_list_insert(trackersettings_fontlist, fontname, position);

    insertbuf[0] = fontname;
    
    gtk_clist_insert(GTK_CLIST(ts->clist), position, insertbuf);
}

static void
trackersettings_gui_delete_font (TrackerSettings *ts,
				 int position)
{
    GList *ll = g_list_nth(trackersettings_fontlist, position);

    g_free(ll->data);
    trackersettings_fontlist = g_list_remove_link(trackersettings_fontlist, ll);

    gtk_clist_remove(GTK_CLIST(ts->clist), position);
    ts->clist_selected_row = -1;
}

static void
trackersettings_gui_exchange_font (TrackerSettings *ts,
				   int p1,
				   int p2)
{
    gtk_clist_swap_rows(GTK_CLIST(ts->clist), p1, p2);
    gtk_clist_select_row(GTK_CLIST(ts->clist), p2, 0);
}

static void
trackersettings_gui_populate_clist (TrackerSettings *ts)
{
    gchar *insertbuf[1];
    GList *l;

    for(l = trackersettings_fontlist; l != NULL; l = l->next) {
	insertbuf[0] = (gchar*)l->data;
        gtk_clist_append(GTK_CLIST(ts->clist), insertbuf);
    }
}

static void
trackersettings_clist_selected (GtkCList *clist,
				gint row,
				gint column,
				GdkEvent *event,
				TrackerSettings *ts)
{
    ts->clist_selected_row = row;
}

static void
trackersettings_add_font (GtkWidget *widget,
			  TrackerSettings *ts)
{
    gtk_widget_show(ts->fontsel_dialog);
}

static void
trackersettings_add_font_ok (GtkWidget *widget,
			     TrackerSettings *ts)
{
    int row;

    gtk_widget_hide(ts->fontsel_dialog);

    if(gtk_font_selection_dialog_get_font(GTK_FONT_SELECTION_DIALOG(ts->fontsel_dialog))) {
	row = ts->clist_selected_row;
	if(row == -1) {
	    row = 0;
	}

	trackersettings_gui_add_font(ts,
				       gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(ts->fontsel_dialog)),
				       row);
    }
}

static void
trackersettings_add_font_cancel (GtkWidget *widget,
				 TrackerSettings *ts)
{
    gtk_widget_hide(ts->fontsel_dialog);
}

static void
trackersettings_delete_font (GtkWidget *widget,
			     TrackerSettings *ts)
{
    int sel = ts->clist_selected_row;

    if(sel != -1) {
	trackersettings_gui_delete_font(ts, sel);
    }
}

static void
trackersettings_apply_font (GtkWidget *widget,
			    TrackerSettings *ts)
{
    int sel = ts->clist_selected_row;

    if(sel != -1) {
	tracker_set_font(ts->tracker, g_list_nth_data(trackersettings_fontlist, sel));
    }
}

void
trackersettings_cycle_font_forward (TrackerSettings *t)
{
    int len = g_list_length(trackersettings_fontlist);

    t->current_font++;
    t->current_font %= len;

    tracker_set_font(t->tracker, g_list_nth_data(trackersettings_fontlist, t->current_font));
}

void
trackersettings_cycle_font_backward (TrackerSettings *t)
{
    int len = g_list_length(trackersettings_fontlist);

    t->current_font--;
    if(t->current_font < 0) {
	t->current_font = len - 1;
    }

    tracker_set_font(t->tracker, g_list_nth_data(trackersettings_fontlist, t->current_font));
}

static GList *
my_g_list_swap (GList *l,
		int p1,
		int p2)
{
    GList *l1, *l2;
    gpointer l1d, l2d;

    if(p1 > p2) {
	int p3 = p2;
	p2 = p1;
	p1 = p3;
    }

    l1 = g_list_nth(l, p1);
    l2 = g_list_nth(l, p2);
    l1d = l1->data;
    l2d = l2->data;

    l = g_list_remove_link(l, l1);
    l = g_list_remove_link(l, l2);
    l = g_list_insert(l, l2d, p1);
    l = g_list_insert(l, l1d, p2);

    return l;
}

static void
trackersettings_font_exchange (TrackerSettings *ts,
			       int p1,
			       int p2)
{
    trackersettings_fontlist = my_g_list_swap(trackersettings_fontlist, p1, p2);
    trackersettings_gui_exchange_font(ts, p1, p2);
}

static void
trackersettings_font_up (GtkWidget *button,
			 TrackerSettings *ts)
{
    if(ts->clist_selected_row > 0) {
	trackersettings_font_exchange(ts, ts->clist_selected_row, ts->clist_selected_row - 1);
    }
}

static void
trackersettings_font_down (GtkWidget *button,
			   TrackerSettings *ts)
{
    if(ts->clist_selected_row != -1 && ts->clist_selected_row < g_list_length(trackersettings_fontlist) - 1) {
	trackersettings_font_exchange(ts, ts->clist_selected_row, ts->clist_selected_row + 1);
    }
}

GtkWidget*
trackersettings_new (void)
{
    TrackerSettings *ts;
    gchar *clisttitles[] = { _("Font list") };
    GtkWidget *hbox1, *thing;

    ts = gtk_type_new(trackersettings_get_type());
    GTK_BOX(ts)->spacing = 2;
    GTK_BOX(ts)->homogeneous = FALSE;

    ts->clist_selected_row = -1;
    ts->current_font = 0;

    ts->clist = gui_clist_in_scrolled_window(1, clisttitles, GTK_WIDGET(ts));
    gtk_signal_connect (GTK_OBJECT (ts->clist), "select_row",
			GTK_SIGNAL_FUNC (trackersettings_clist_selected), ts);

    trackersettings_gui_populate_clist(ts);

    hbox1 = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(ts), hbox1, FALSE, TRUE, 0);
    gtk_widget_show(hbox1);

    thing = ts->add_button = gtk_button_new_with_label(_("Add font"));
    gtk_box_pack_start(GTK_BOX(hbox1), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC (trackersettings_add_font), ts);

    thing = ts->delete_button = gtk_button_new_with_label(_("Delete font"));
    gtk_box_pack_start(GTK_BOX(hbox1), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC (trackersettings_delete_font), ts);

    thing = ts->apply_button = gtk_button_new_with_label(_("Apply font"));
    gtk_box_pack_start(GTK_BOX(hbox1), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC (trackersettings_apply_font), ts);

    hbox1 = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(ts), hbox1, FALSE, TRUE, 0);
    gtk_widget_show(hbox1);

#ifndef USE_GNOME
#define GNOME_STOCK_BUTTON_UP 0
#define GNOME_STOCK_BUTTON_DOWN 0
#endif
    
    ts->up_button = gui_button(GTK_WIDGET(ts), GNOME_STOCK_BUTTON_UP, _("Up"),
			       trackersettings_font_up, ts, hbox1);

    ts->down_button = gui_button(GTK_WIDGET(ts), GNOME_STOCK_BUTTON_DOWN, _("Down"),
				 trackersettings_font_down, ts, hbox1);

    ts->fontsel_dialog = gtk_font_selection_dialog_new(_("Select font..."));
    gtk_window_set_modal(GTK_WINDOW(ts->fontsel_dialog), TRUE);
    gtk_signal_connect (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG(ts->fontsel_dialog)->ok_button), "clicked",
			GTK_SIGNAL_FUNC (trackersettings_add_font_ok), ts);
    gtk_signal_connect (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG(ts->fontsel_dialog)->cancel_button), "clicked",
			GTK_SIGNAL_FUNC (trackersettings_add_font_cancel), ts);

    return GTK_WIDGET(ts);
}

void
trackersettings_set_tracker_widget (TrackerSettings *ts,
				    Tracker *t)
{
    ts->tracker = t;

    tracker_set_font(t, g_list_nth_data(trackersettings_fontlist, ts->current_font));
}

static void
trackersettings_read_fontlist (void)
{
    char buf[256];
    FILE *f;
    prefs_node *p;

    trackersettings_fontlist = NULL;

    p = prefs_open_read("tracker-fonts");
    if(p) {
	f = prefs_get_file_pointer(p);
	while(!feof(f)) {
	    buf[0] = 0;
	    fgets(buf, 255, f);
	    buf[255] = 0;
	    if(strlen(buf) > 0) {
		buf[strlen(buf) - 1] = 0;
		trackersettings_fontlist = g_list_append(trackersettings_fontlist, g_strdup(buf));
	    }
	}
	prefs_close(p);
    }

    if(g_list_length(trackersettings_fontlist) == 0) {
	trackersettings_fontlist = g_list_append(trackersettings_fontlist, "fixed");
    }
}

void
trackersettings_write_settings (void)
{
    FILE *f;
    prefs_node *p;
    GList *l;

    p = prefs_open_write("tracker-fonts");
    if(!p) {
	return;
    }

    f = prefs_get_file_pointer(p);

    for(l = trackersettings_fontlist; l != NULL; l = l->next) {
	fprintf(f, "%s\n", (gchar*)l->data);
    }

    prefs_close(p);
}

static void
trackersettings_class_init (TrackerSettingsClass *class)
{
    trackersettings_read_fontlist();
}

guint
trackersettings_get_type (void)
{
    static guint trackersettings_type = 0;
    
    if (!trackersettings_type) {
	GtkTypeInfo trackersettings_info =
	{
	    "TrackerSettings",
	    sizeof(TrackerSettings),
	    sizeof(TrackerSettingsClass),
	    (GtkClassInitFunc) NULL,
	    (GtkObjectInitFunc) trackersettings_class_init,
	    (GtkArgSetFunc) NULL,
	    (GtkArgGetFunc) NULL,
	};
	
	trackersettings_type = gtk_type_unique (gtk_vbox_get_type (), &trackersettings_info);
    }
    
    return trackersettings_type;
}
