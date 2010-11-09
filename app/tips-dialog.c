
/*
 * The Real SoundTracker - user tips
 *
 * Copyright (C) 1997-2000 by the GIMP authors
 * Copyright (C) 1999-2002 by Michael Krause
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <gtk/gtk.h>

#include "i18n.h"
#include "tips-dialog.h"
#include "preferences.h"

static int  tips_dialog_hide (GtkWidget *widget, gpointer data);
static int  tips_show_next (GtkWidget *widget, gpointer data);
static void tips_toggle_update (GtkWidget *widget, gpointer data);

static GtkWidget *tips_dialog_vbox = NULL;
static GtkWidget *tips_dialog_content_vbox = NULL;
static GtkWidget *tips_dialog_action_vbox = NULL;
static GtkWidget *tips_dialog = NULL;
static GtkWidget *tips_label;

int tips_dialog_last_tip = 0;
int tips_dialog_show_tips = 1;

#define TIPS_COUNT (sizeof(tips_array)/sizeof(tips_array[0]))

static char const * const tips_array[] = {
    N_("Welcome to SoundTracker!\n"
       "\n"
       "If you are new to this type of program, you will want to get hold of\n"
       "some XM or MOD files first and play with them."),

    N_("You can make SoundTracker's edit mode more responsive to keyboard\n"
       "input by decreasing the mixing buffer size of the \"Editing\" object in\n"
       "the Audio Configuration."),

    N_("You can adjust the loop points in the sample editor by holding Shift\n"
       "and using the left and right mousebuttons.\n"),

    N_("If you want to know more about tracking, and how the various commands\n"
       "work, have a look at http://www.united-trackers.org/"),

    N_("You can assign samples of an instrument to the individual keys by\n"
       "activating its sample and then clicking on the keyboard in the\n"
       "instrument editor page."),

    N_("Is your cursor trapped in a number entry field?\n"
       "Just press Return or Tab to free yourself!")
};

void
tips_dialog_open ()
{
    GtkWidget *thing;

    if(!tips_dialog) {
	tips_dialog = gtk_dialog_new ();
	gtk_window_set_wmclass (GTK_WINDOW (tips_dialog), "tip_of_the_day", "SoundTracker");
	gtk_window_set_title (GTK_WINDOW (tips_dialog), (_("SoundTracker Tip of the day")));
	g_signal_connect(tips_dialog, "delete_event",
			 G_CALLBACK(tips_dialog_hide), NULL);
	thing = tips_dialog_get_content_vbox();
	gtk_container_add(GTK_CONTAINER (gtk_dialog_get_content_area(GTK_DIALOG(tips_dialog))), thing);
	gtk_widget_show(thing);

	thing = tips_dialog_get_action_vbox();
	gtk_container_add(GTK_CONTAINER (gtk_dialog_get_action_area(GTK_DIALOG(tips_dialog))), thing);
	gtk_widget_show(thing);
    }

    if(!GTK_WIDGET_VISIBLE (tips_dialog)) {
	gtk_widget_show (tips_dialog);
    } else {
	gdk_window_raise (tips_dialog->window);
    }
}

static void
tips_dialog_vbox_destroy (GtkObject *object)
{
    tips_dialog_vbox = NULL;
}

static void
tips_dialog_content_vbox_destroy (GtkObject *object)
{
    tips_dialog_content_vbox = NULL;
}

static void
tips_dialog_action_vbox_destroy (GtkObject *object)
{
    tips_dialog_action_vbox = NULL;
}

GtkWidget *
tips_dialog_get_vbox (void)
{
    GtkWidget *vbox;
    GtkWidget *cbox;
    GtkWidget *abox;

    if(tips_dialog_vbox) {
	g_error("tips_dialog_get_vbox() called twice.\n");
	return NULL;
    }

    tips_dialog_vbox = vbox = gtk_vbox_new (FALSE, 0);

    g_signal_connect(tips_dialog_vbox, "destroy",
		     G_CALLBACK(tips_dialog_vbox_destroy), NULL);

    cbox = tips_dialog_get_content_vbox();
    abox = tips_dialog_get_action_vbox();

    gtk_container_set_border_width (GTK_CONTAINER (cbox), 10);
    gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, TRUE, 0);
    gtk_widget_show (cbox);

    gtk_container_set_border_width (GTK_CONTAINER (abox), 10);
    gtk_box_pack_end (GTK_BOX (vbox), abox, FALSE, TRUE, 0);
    gtk_widget_show (abox);

    return vbox;
}

GtkWidget *
tips_dialog_get_content_vbox (void)
{
    GtkWidget *vbox;
    GtkWidget *hbox;

    if(tips_dialog_content_vbox) {
	g_error("tips_dialog_get_content_vbox() called twice.\n");
	return NULL;
    }

    tips_dialog_content_vbox = vbox = gtk_vbox_new (FALSE, 0);

    g_signal_connect(tips_dialog_content_vbox, "destroy",
		     G_CALLBACK(tips_dialog_content_vbox_destroy), NULL);

    hbox = gtk_hbox_new (FALSE, 5);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show (hbox);

    tips_label = gtk_label_new (_(tips_array[tips_dialog_last_tip]));
    gtk_label_set_justify (GTK_LABEL (tips_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (hbox), tips_label, TRUE, TRUE, 3);
    gtk_widget_show (tips_label);

    return vbox;
}

GtkWidget *
tips_dialog_get_action_vbox (void)
{
    GtkWidget *vbox;
    GtkWidget *bbox;
    GtkWidget *button_next;
    GtkWidget *button_prev;
    GtkWidget *vbox_check;
    GtkWidget *button_check;

    if(tips_dialog_action_vbox) {
	g_error("tips_dialog_get_action_vbox() called twice.\n");
	return NULL;
    }

    tips_dialog_action_vbox = vbox = gtk_vbox_new (FALSE, 0);

    g_signal_connect(tips_dialog_action_vbox, "destroy",
		     G_CALLBACK(tips_dialog_action_vbox_destroy), NULL);

    bbox = gtk_hbox_new (TRUE, 5);
    gtk_box_pack_start (GTK_BOX (vbox), bbox, TRUE, FALSE, 0);
    gtk_widget_show(bbox);


    button_prev = gtk_button_new_with_label ((_("Previous Tip")));
    GTK_WIDGET_UNSET_FLAGS (button_prev, GTK_RECEIVES_DEFAULT);
    g_signal_connect(button_prev, "clicked",
		     G_CALLBACK(tips_show_next),
		     (gpointer) "prev");
    gtk_container_add (GTK_CONTAINER (bbox), button_prev);
    gtk_widget_show (button_prev);

    button_next = gtk_button_new_with_label ((_("Next Tip")));
    GTK_WIDGET_UNSET_FLAGS (button_next, GTK_RECEIVES_DEFAULT);
    g_signal_connect(button_next, "clicked",
			G_CALLBACK(tips_show_next),
			(gpointer) "next");
    gtk_container_add (GTK_CONTAINER (bbox), button_next);
    gtk_widget_show (button_next);

    vbox_check = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (bbox), vbox_check, FALSE, TRUE, 0);
    gtk_widget_show (vbox_check);

    button_check = gtk_check_button_new_with_label ((_("Show tip next time")));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_check),
				  tips_dialog_show_tips);
    g_signal_connect(button_check, "toggled",
			G_CALLBACK(tips_toggle_update),
			(gpointer) &tips_dialog_show_tips);
    gtk_box_pack_start (GTK_BOX (vbox_check), button_check, TRUE, FALSE, 0);
    gtk_widget_show (button_check);

    return vbox;
}



static int
tips_dialog_hide (GtkWidget *widget,
		  gpointer data)
{
    gtk_widget_hide (tips_dialog);

    return TRUE;
}

static int
tips_show_next (GtkWidget *widget,
		gpointer  data)
{
  if (!strcmp ((char *)data, "prev"))
    {
      tips_dialog_last_tip--;
      if (tips_dialog_last_tip < 0)
	tips_dialog_last_tip = TIPS_COUNT - 1;
    }
  else
    {
      tips_dialog_last_tip++;
      if (tips_dialog_last_tip >= TIPS_COUNT)
	tips_dialog_last_tip = 0;
    }
  gtk_label_set (GTK_LABEL (tips_label), _(tips_array[tips_dialog_last_tip]));
  return FALSE;
}

static void
tips_toggle_update (GtkWidget *widget,
		    gpointer   data)
{
  int *toggle_val;

  toggle_val = (int *) data;

  if (GTK_TOGGLE_BUTTON (widget)->active)
    *toggle_val = TRUE;
  else
    *toggle_val = FALSE;
}

void
tips_dialog_load_settings (void)
{
    prefs_node *f;

    f = prefs_open_read("tips");
    if(f) {
	prefs_get_int(f, "show-tips", &tips_dialog_show_tips);
	prefs_get_int(f, "last-tip", &tips_dialog_last_tip);
	prefs_close(f);
    }

    if(tips_dialog_last_tip >= TIPS_COUNT || tips_dialog_last_tip < 0) {
	tips_dialog_last_tip = 0;
    }

    return;
}

void
tips_dialog_save_settings (void)
{
    prefs_node *f;

    f = prefs_open_write("tips");
    if(!f)
	return;

    tips_dialog_last_tip++;
    prefs_put_int(f, "show-tips", tips_dialog_show_tips);
    prefs_put_int(f, "last-tip", tips_dialog_last_tip);

    prefs_close(f);
    return;
}

