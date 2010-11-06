
/*
 * The Real SoundTracker - Audio configuration dialog
 *
 * Copyright (C) 1999-2001 Michael Krause
 * Copyright (C) 2005 Yury Aliaev (GTK+-2 porting)
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
#include <glib/gprintf.h>
#ifdef USE_GNOME
#include <gnome.h>
#endif
#include <string.h>

#include "i18n.h"
#include "audioconfig.h"
#include "gui-subs.h"
#include "audio.h"
#include "sample-editor.h"
#include "driver.h"
#include "preferences.h"
#include "sample-editor.h"
#include "gui.h"
#include "mixer.h"

/* List of available output and input drivers in this compilation of
   SoundTracker. */

GList *drivers[2] = { NULL, NULL };
GList *mixers = NULL;

static GtkWidget *audioconfig_mixer_list;
static st_mixer *audioconfig_current_mixer = NULL;
static gboolean audioconfig_disable_mixer_selection = FALSE;

typedef struct audio_object {
    const char *title;
    const char *shorttitle;
    int type;
    void **driver_object;
    st_io_driver **driver;
    GtkWidget *drivernbook;
} audio_object;

static audio_object audio_objects[] = {
    { N_("Playback Output"),
      "playback",
      DRIVER_OUTPUT,
      &playback_driver_object,
      &playback_driver,
      NULL
    },
    { N_("Editing Output"),
      "editing",
      DRIVER_OUTPUT,
      &editing_driver_object,
      &editing_driver,
      NULL
    },
    { N_("Sampling"),
      "sampling",
      DRIVER_INPUT,
      &sampling_driver_object,
      &sampling_driver,
      NULL
    }
};

#define NUM_AUDIO_OBJECTS (sizeof(audio_objects) / sizeof(audio_objects[0]))

static void ***audio_driver_objects[NUM_AUDIO_OBJECTS];

static void
audioconfig_driver_load_config (audio_object *ao)
{
    st_io_driver *d = *ao->driver;
    char buf[256];
    prefs_node *f;

    if(d->common.loadsettings) {
	g_sprintf(buf, "audio-object-%s", ao->shorttitle);
	f = prefs_open_read(buf);
	if(f) {
	    d->common.loadsettings(*ao->driver_object, f);
	    prefs_close(f);
	} else {
	    // set default values
	    // will use module defaults for now
	}
    }
}

static void
audioconfig_list_select (GtkTreeSelection *sel, guint page)
{
    GtkTreeModel *mdl;
    GtkTreeIter iter;
    gchar *str;
    
    if(gtk_tree_selection_get_selected(sel, &mdl, &iter)) {

	guint row = atoi(str = gtk_tree_model_get_string_from_iter(mdl, &iter));
	audio_object *object = &audio_objects[page];
	st_io_driver *old_driver = *object->driver;
	st_io_driver *new_driver = g_list_nth_data(drivers[object->type], row);

	g_free(str);

	if(new_driver != old_driver) {
	    // stop playing and sampling here
	    sample_editor_stop_sampling();
	    gui_play_stop();

	    // get new driver object
	    *object->driver_object = audio_driver_objects[page][row];
	    *object->driver = new_driver;
	}
	gtk_notebook_set_current_page(GTK_NOTEBOOK(object->drivernbook), row);
    }
}

static void
audioconfig_close_requested (GtkWidget *window)
{
    gtk_widget_hide(window);
}

static void
audioconfig_mixer_selected (GtkTreeSelection *sel)
{
    GtkTreeModel *mdl;
    GtkTreeIter iter;
    gchar *str;
    
    if(gtk_tree_selection_get_selected(sel, &mdl, &iter)) {

	guint row = atoi(str = gtk_tree_model_get_string_from_iter(mdl, &iter));
	st_mixer *new_mixer = g_list_nth_data(mixers, row);

	g_free(str);

	if(!audioconfig_disable_mixer_selection && new_mixer != audioconfig_current_mixer) {
	    audio_set_mixer(new_mixer);
	    audioconfig_current_mixer = new_mixer;
	}
    }
}

static void
audioconfig_initialize_mixer_list (void)
{
    GList *l;
    int i, active = -1;
    GtkListStore *list_store = GUI_GET_LIST_STORE(audioconfig_mixer_list);
    GtkTreeIter iter;
    GtkTreeModel *model;

    audioconfig_disable_mixer_selection = TRUE;
    model = gui_list_freeze(audioconfig_mixer_list);
    gui_list_clear_with_model(model);
    for(i = 0, l = mixers; l; i++, l = l->next) {
	st_mixer *mixer = l->data;
	if(mixer == audioconfig_current_mixer) {
	    active = i;
	}
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set(list_store, &iter, 0, (gchar*)mixer->id,
			   1, gettext((gchar*)mixer->description), -1);
    }
    gui_list_thaw(audioconfig_mixer_list, model);
    audioconfig_disable_mixer_selection = FALSE;

    gui_list_select(audioconfig_mixer_list, active);
}

static void
audioconfig_notebook_add_page (GtkNotebook *nbook, guint n)
{
    GtkWidget	*label, *box1, *list, *widget, *dnbook, *alignment;
    GList *l;
    guint i, active = -1;
    static gchar *listtitles[1];
    GtkListStore *list_store;
    GtkTreeIter iter;

    listtitles[0] = gettext("Driver Module");

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box1);

    // Driver selection list
    list = gui_stringlist_in_scrolled_window(1, listtitles, box1);
    gtk_widget_set_size_request(list, 200, -1);
    list_store = GUI_GET_LIST_STORE(list);

    /* Driver configuration widgets' notebook (with hidden tabs, as multi-layer container) */
    audio_objects[n].drivernbook = dnbook = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(dnbook), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(dnbook), FALSE);
    gtk_widget_show(dnbook);
    gtk_box_pack_start(GTK_BOX(box1), dnbook, TRUE, TRUE, 0);

    for(i = 0, l = drivers[audio_objects[n].type]; l; i++, l = l->next) {
	st_io_driver *driver = l->data;
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set(list_store, &iter, 0, *((gchar **)l->data), -1);

	if(driver == *audio_objects[n].driver)
	    active = i;
	widget = driver->common.getwidget(audio_driver_objects[n][i]);
	alignment = gtk_alignment_new(0.4, 0.5, 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(alignment), widget);
	gtk_notebook_append_page(GTK_NOTEBOOK(dnbook), alignment, NULL);
	gtk_widget_show_all(alignment);
    }
    
    gui_list_handle_selection(list, G_CALLBACK(audioconfig_list_select), (gpointer)n);
    if(active != -1) {
	gui_list_select(list, active);
    }

    label = gtk_label_new(gettext(audio_objects[n].title));
    gtk_widget_show(label);

    gtk_notebook_append_page(nbook, box1, label);
}

void
audioconfig_dialog (void)
{
    static GtkWidget *configwindow = NULL;
    GtkWidget *mainbox, *thing, *hbox, *nbook, *box2, *frame;
    static gchar *listtitles2[2];
    int i;

    listtitles2[0] = gettext("Mixer Module");
    listtitles2[1] = gettext("Description");

    if(configwindow != NULL) {
	if(!GTK_WIDGET_VISIBLE(configwindow))
	    gtk_widget_show(configwindow);
	gdk_window_raise(configwindow->window);
	return;
    }
    
#ifdef USE_GNOME
    configwindow = gnome_app_new("SoundTracker", _("Audio Configuration"));
#else
    configwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(configwindow), _("Audio Configuration"));
#endif
    g_signal_connect_swapped (configwindow, "delete_event",
			G_CALLBACK (audioconfig_close_requested), configwindow);

    mainbox = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(mainbox), 4);
#ifdef USE_GNOME
    gnome_app_set_contents(GNOME_APP(configwindow), mainbox);
#else
    gtk_container_add(GTK_CONTAINER(configwindow), mainbox);
#endif
    gtk_widget_show(mainbox);

    // Each driver (playback,capture,editing,etc...) occupies the notebook page
    nbook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(mainbox), nbook, FALSE, TRUE, 0);
    for(i = 0; i < NUM_AUDIO_OBJECTS; i++) {
	audioconfig_notebook_add_page(GTK_NOTEBOOK(nbook), i);
    }
    gtk_widget_show(nbook);

    // Mixer selection
    frame = gtk_frame_new(NULL);
    gtk_frame_set_label(GTK_FRAME(frame), _("Mixers"));
    gtk_box_pack_start(GTK_BOX(mainbox), frame, TRUE, TRUE, 0);
    gtk_widget_show(frame);

    box2 = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(box2);
    gtk_container_add (GTK_CONTAINER(frame), box2);
    gtk_container_border_width(GTK_CONTAINER(box2), 4);

    thing = gui_stringlist_in_scrolled_window(2, listtitles2, box2);
    gui_list_handle_selection(thing, G_CALLBACK(audioconfig_mixer_selected), NULL);
    audioconfig_mixer_list = thing;
    audioconfig_initialize_mixer_list();

    /* The button area */
    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    hbox = gtk_hbutton_box_new();
    gtk_button_box_set_spacing(GTK_BUTTON_BOX (hbox), 4);
    gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
    gtk_box_pack_start(GTK_BOX (mainbox), hbox,
			FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    thing = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    GTK_WIDGET_SET_FLAGS(thing, GTK_CAN_DEFAULT);
    gtk_window_set_default(GTK_WINDOW(configwindow), thing);
    g_signal_connect_swapped(thing, "clicked",
			G_CALLBACK(audioconfig_close_requested), configwindow);
    gtk_box_pack_start(GTK_BOX (hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);


    gtk_widget_show(configwindow);
}

void
audioconfig_load_config (void)
{
    char buf[256];
    prefs_node *f;
    GList *l;
    guint i, n = 0;

    f = prefs_open_read("audio-objects");
    if(f) {
	for(i = 0; i < NUM_AUDIO_OBJECTS; i++) {
	    guint j;

	    if(prefs_get_string(f, audio_objects[i].shorttitle, buf)) {
		for(j = 0, l = drivers[audio_objects[i].type]; l; l = l->next, j++) {
		    if(!strcmp(*((gchar **)l->data), buf)) {
			*audio_objects[i].driver = l->data;
			n = j;
			break;
		    }
		}
	    }
	}
	prefs_close(f);
    }

    for(i = 0; i < NUM_AUDIO_OBJECTS; i++) {
	guint j;
	st_io_driver *d = *audio_objects[i].driver;

	audio_driver_objects[i] = g_new(void**,
					  g_list_length(drivers[audio_objects[i].type]));
	for(j = 0, l = drivers[audio_objects[n].type]; l; j++, l = l->next) {
	    st_io_driver *driver = l->data;
	    audio_driver_objects[i][j] = driver->common.new();
	}

	if(!d) {
	    // set default driver if none has been configured
	    if(drivers[audio_objects[i].type] != NULL) {
		d = *audio_objects[i].driver = drivers[audio_objects[i].type]->data;
	    }
	}

	if(d) {
	    // create driver instance
	    *audio_objects[i].driver_object = audio_driver_objects[i][n];
	    audioconfig_driver_load_config(&audio_objects[i]);
	}
    }
}

void
audioconfig_load_mixer_config (void)
{
    char buf[256];
    prefs_node *f;
    GList *l;

    f = prefs_open_read("mixer");
    if(f) {
	if(prefs_get_string(f, "mixer", buf)) {
	    for(l = mixers; l; l = l->next) {
		st_mixer *m = l->data;
		if(!strcmp(m->id, buf)) {
		    mixer = m;
		    audioconfig_current_mixer = m;
		}
	    }
	}
	prefs_close(f);
    }

    if(!audioconfig_current_mixer) {
	mixer = mixers->data;
	audioconfig_current_mixer = mixers->data;
    }
}

void
audioconfig_save_config (void)
{
    char buf[256];
    prefs_node *f;
    int i;

    f = prefs_open_write("audio-objects");
    if(!f)
	return;

    // Write the driver module names
    for(i = 0; i < NUM_AUDIO_OBJECTS; i++) {
	prefs_put_string(f, audio_objects[i].shorttitle,
			 ((st_driver*)*(audio_objects[i].driver))->name);
    }

    prefs_close(f);

    /* Write the driver module's configurations in extra files */
    for(i = 0; i < NUM_AUDIO_OBJECTS; i++) {
	gboolean (*savesettings)(void *, prefs_node *) = ((st_driver*)*(audio_objects[i].driver))->savesettings;

	if(savesettings) {
	    g_sprintf(buf, "audio-object-%s", audio_objects[i].shorttitle);
	    f = prefs_open_write(buf);
	    if(f) {
		savesettings(*audio_objects[i].driver_object, f);
		prefs_close(f);
	    }
	}
    }

    f = prefs_open_write("mixer");
    if(f) {
	prefs_put_string(f, "mixer", audioconfig_current_mixer->id);
	prefs_close(f);
    }

    return;
}
