
/*
 * The Real SoundTracker - Audio configuration dialog
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

#include <gtk/gtk.h>
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

static GtkWidget *configwindow = NULL;
static GtkWidget *cw_clist, *cw_hbox, *audioconfig_mixer_clist;
static int cw_currentobject = -1;
static GtkWidget *driverwidget = NULL;
static st_mixer *audioconfig_current_mixer = NULL;
static gboolean audioconfig_disable_mixer_selection = FALSE;

typedef struct audio_object {
    const char *title;
    const char *shorttitle;
    int type;
    void **driver_object;
    void **driver;
} audio_object;

static audio_object audio_objects[] = {
    { N_("Playback Output"),
      "playback",
      DRIVER_OUTPUT,
      &playback_driver_object,
      (void**)&playback_driver
    },
    { N_("Editing Output"),
      "editing",
      DRIVER_OUTPUT,
      &editing_driver_object,
      (void**)&editing_driver
    },
    { N_("Sampling"),
      "sampling",
      DRIVER_INPUT,
      &sampling_driver_object,
      (void**)&sampling_driver
    }
};

#define NUM_AUDIO_OBJECTS (sizeof(audio_objects) / sizeof(audio_objects[0]))

static void
audioconfig_driver_load_config (audio_object *ao)
{
    st_driver *d = *ao->driver;
    char buf[256];
    prefs_node *f;

    if(d->loadsettings) {
	sprintf(buf, "audio-object-%s", ao->shorttitle);
	f = prefs_open_read(buf);
	if(f) {
	    d->loadsettings(*ao->driver_object, f);
	    prefs_close(f);
	} else {
	    // set default values
	    // will use module defaults for now
	}
    }
}

static void
audioconfig_clist_select (GtkCList *list,
			  gint row,
			  gint column)
{
    if(cw_currentobject != -1) {
	audio_object *object = &audio_objects[cw_currentobject];
	st_driver *old_driver = *object->driver;
	void *old_driver_object = *object->driver_object;
	st_driver *new_driver = g_list_nth_data(drivers[object->type], row);
	GtkWidget *new_driverwidget;

	if(new_driver != old_driver) {
	    prefs_node *f;
	    char buf[256];

	    // stop playing and sampling here
	    sample_editor_stop_sampling();
	    gui_play_stop();

	    // get new driver object
	    *object->driver_object = new_driver->new();
	    *object->driver = new_driver;

	    /* Load object settings, if there are saved settings. */
	    f = prefs_open_read("audio-objects");
	    if(f) {
		if(prefs_get_string(f, object->shorttitle, buf)) {
		    if(!strcmp(buf, new_driver->name)) {
			audioconfig_driver_load_config(object);
		    }
		}
		prefs_close(f);
	    }
	}

	new_driverwidget = new_driver->getwidget(*object->driver_object);

	if(new_driverwidget != driverwidget) {
	    if(driverwidget) {
		gtk_container_remove(GTK_CONTAINER(cw_hbox), driverwidget);
	    }
	    driverwidget = new_driverwidget;
	    gtk_widget_show(driverwidget);
	    /* we don't want the widget to be destroyed upon removal... */
	    gtk_object_ref(GTK_OBJECT(driverwidget));
	    gtk_box_pack_start(GTK_BOX(cw_hbox), driverwidget, TRUE, FALSE, 0);
	}

	if(new_driver != old_driver) {
	    // free old driver object
	    old_driver->destroy(old_driver_object);
	}
    } else {
	// The CList is being updated
    }
}

static void
audioconfig_object_changed (void *a,
			    void *b)
{
    unsigned n = GPOINTER_TO_INT(b);
    GList *l;
    gchar *insertbuf[2] = { NULL };
    int i, active = -1;

    g_assert(n < NUM_AUDIO_OBJECTS);

    if(n == cw_currentobject)
	return;

    cw_currentobject = -1; // disable clist select callback

    gtk_clist_freeze(GTK_CLIST(cw_clist));
    gtk_clist_clear(GTK_CLIST(cw_clist));
    for(i = 0, l = drivers[audio_objects[n].type]; l; i++, l = l->next) {
	insertbuf[0] = *((gchar **)l->data);
	if(l->data == *audio_objects[n].driver)
	    active = i;
	gtk_clist_append(GTK_CLIST(cw_clist), insertbuf);
    }
    gtk_clist_thaw(GTK_CLIST(cw_clist));

    // Now update the GUI
    cw_currentobject = n;
    if(driverwidget) {
	gtk_container_remove(GTK_CONTAINER(cw_hbox), driverwidget);
	driverwidget = NULL;
    }

    if(active != -1) {
	gtk_clist_select_row(GTK_CLIST(cw_clist), active, 0);
    }
}

static void
audioconfig_close_requested (void)
{
    if(driverwidget) {
	gtk_container_remove(GTK_CONTAINER(cw_hbox), driverwidget);
	gtk_widget_hide(driverwidget);
	driverwidget = NULL;
    }
    gtk_widget_destroy(configwindow);
    configwindow = NULL;
    cw_currentobject = -1;
}

static void
audioconfig_mixer_selected (GtkCList *list,
			    gint row,
			    gint column)
{
    st_mixer *new_mixer = g_list_nth_data(mixers, row);

    if(!audioconfig_disable_mixer_selection && new_mixer != audioconfig_current_mixer) {
	audio_set_mixer(new_mixer);
	audioconfig_current_mixer = new_mixer;
    }
}

static void
audioconfig_initialize_mixer_list (void)
{
    GList *l;
    gchar *insertbuf[2] = { NULL };
    int i, active = -1;

    audioconfig_disable_mixer_selection = TRUE;
    gtk_clist_freeze(GTK_CLIST(audioconfig_mixer_clist));
    gtk_clist_clear(GTK_CLIST(audioconfig_mixer_clist));
    for(i = 0, l = mixers; l; i++, l = l->next) {
	st_mixer *mixer = l->data;
	if(mixer == audioconfig_current_mixer) {
	    active = i;
	}
	insertbuf[0] = (gchar*)mixer->id;
	insertbuf[1] = gettext((gchar*)mixer->description);
	gtk_clist_append(GTK_CLIST(audioconfig_mixer_clist), insertbuf);
    }
    gtk_clist_thaw(GTK_CLIST(audioconfig_mixer_clist));
    audioconfig_disable_mixer_selection = FALSE;

    gtk_clist_select_row(GTK_CLIST(audioconfig_mixer_clist), active, 0);
}

void
audioconfig_dialog (void)
{
    GtkWidget *mainbox, *thing, *box1, *hbox, *frame, *box2;
    OptionMenuItem menu1[NUM_AUDIO_OBJECTS];
    static gchar *listtitles[1];
    static gchar *listtitles2[2];
    int i;

    listtitles[0] = gettext("Driver Module");
    listtitles2[0] = gettext("Mixer Module");
    listtitles2[1] = gettext("Description");

    if(configwindow != NULL) {
	gdk_window_raise(configwindow->window);
	return;
    }
    
#ifdef USE_GNOME
    configwindow = gnome_app_new("SoundTracker", _("Audio Configuration"));
#else
    configwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(configwindow), _("Audio Configuration"));
#endif
    gtk_signal_connect (GTK_OBJECT (configwindow), "delete_event",
			GTK_SIGNAL_FUNC (audioconfig_close_requested), NULL);

    mainbox = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(mainbox), 4);
#ifdef USE_GNOME
    gnome_app_set_contents(GNOME_APP(configwindow), mainbox);
#else
    gtk_container_add(GTK_CONTAINER(configwindow), mainbox);
#endif
    gtk_widget_show(mainbox);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_label(GTK_FRAME(frame), _("Drivers"));
    gtk_box_pack_start(GTK_BOX(mainbox), frame, FALSE, TRUE, 0);
    gtk_widget_show(frame);

    box2 = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(box2);
    gtk_container_add (GTK_CONTAINER(frame), box2);
    gtk_container_border_width(GTK_CONTAINER(box2), 4);

    // Driver type selector
    for(i = 0; i < NUM_AUDIO_OBJECTS; i++) {
	menu1[i].name = gettext(audio_objects[i].title);
	menu1[i].func = audioconfig_object_changed;
    }
    thing = gui_build_option_menu(menu1, NUM_AUDIO_OBJECTS, 0);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);

    cw_hbox = box1 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box1);
    gtk_box_pack_start(GTK_BOX(box2), box1, TRUE, TRUE, 0);

    // Driver selection list
    thing = gui_clist_in_scrolled_window(1, listtitles, box1);
    gtk_clist_set_selection_mode(GTK_CLIST(thing), GTK_SELECTION_BROWSE);
    gtk_clist_column_titles_passive(GTK_CLIST(thing));
    gtk_clist_set_column_justification(GTK_CLIST(thing), 0, GTK_JUSTIFY_LEFT);
    gtk_widget_set_usize(thing, 200, 50);
    gtk_signal_connect_after(GTK_OBJECT(thing), "select_row",
			     GTK_SIGNAL_FUNC(audioconfig_clist_select), NULL);
    cw_clist = thing;

    audioconfig_object_changed(NULL, (void*)0);


    // Mixer selection
    frame = gtk_frame_new(NULL);
    gtk_frame_set_label(GTK_FRAME(frame), _("Mixers"));
    gtk_box_pack_start(GTK_BOX(mainbox), frame, TRUE, TRUE, 0);
    gtk_widget_show(frame);

    box2 = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(box2);
    gtk_container_add (GTK_CONTAINER(frame), box2);
    gtk_container_border_width(GTK_CONTAINER(box2), 4);

    thing = gui_clist_in_scrolled_window(2, listtitles2, box2);
    gtk_clist_set_selection_mode(GTK_CLIST(thing), GTK_SELECTION_BROWSE);
    gtk_clist_column_titles_passive(GTK_CLIST(thing));
    gtk_clist_set_column_justification(GTK_CLIST(thing), 0, GTK_JUSTIFY_LEFT);
    gtk_signal_connect_after(GTK_OBJECT(thing), "select_row",
			     GTK_SIGNAL_FUNC(audioconfig_mixer_selected), NULL);
    audioconfig_mixer_clist = thing;
    audioconfig_initialize_mixer_list();

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
    thing = gtk_button_new_with_label (_ ("Close"));
#endif
    GTK_WIDGET_SET_FLAGS(thing, GTK_CAN_DEFAULT);
    gtk_window_set_default(GTK_WINDOW(configwindow), thing);
    gtk_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC (audioconfig_close_requested), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show (thing);


    gtk_widget_show (configwindow);
}

void
audioconfig_load_config (void)
{
    char buf[256];
    prefs_node *f;
    GList *l;
    int i;

    f = prefs_open_read("audio-objects");
    if(f) {
	for(i = 0; i < NUM_AUDIO_OBJECTS; i++) {
	    if(prefs_get_string(f, audio_objects[i].shorttitle, buf)) {
		for(l = drivers[audio_objects[i].type]; l; l = l->next) {
		    if(!strcmp(*((gchar **)l->data), buf)) {
			*audio_objects[i].driver = l->data;
			break;
		    }
		}
	    }
	}
	prefs_close(f);
    }

    for(i = 0; i < NUM_AUDIO_OBJECTS; i++) {
	st_driver *d = *audio_objects[i].driver;

	if(!d) {
	    // set default driver if none has been configured
	    if(drivers[audio_objects[i].type] != NULL) {
		d = *audio_objects[i].driver = drivers[audio_objects[i].type]->data;
	    }
	}

	if(d) {
	    // create driver instance
	    *audio_objects[i].driver_object = d->new();
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
	    sprintf(buf, "audio-object-%s", audio_objects[i].shorttitle);
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
