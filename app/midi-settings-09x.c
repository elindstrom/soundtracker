/* -*- Mode: C; tab-width: 3; indent-tabs-mode: nil -*-
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

#include <config.h>

#if !(defined(DRIVER_ALSA_050) || defined(DRIVER_ALSA_09x))

#include <gtk/gtk.h>

/*
 * To allow proper linking of menubar.o,
 * define rudimentary midi_settings_dialog().
 * The menu should be inactive when ALSA is not supported.
 * See menubar_create() in menubar.c.
 */

void
midi_settings_dialog (void)
{
  g_warning("MIDI Settings dialog not allowed\n");
}

#else

#if defined(DRIVER_ALSA_09x)

#ifdef USE_GNOME
#include <gnome.h>
#endif

#include "i18n.h"
#include "gui.h"
#include "preferences.h"
#include "gui-subs.h"
#include "midi-settings.h"
#include "midi.h"

/* Define value for notebook pages border width */

#define PAGE_BORDER_WIDTH 6

/* Macro to check if debug is activated */

#define IS_MIDI_DEBUG_ON (midi_settings.misc.debug_level > (MIDI_DEBUG_OFF))

/******************************************************************/
/* Global variables. */

/* MIDI preferences default values
 * and keys in the preferences file.
 */

midi_prefs midi_settings = {
  /* MIDI input parameters */
  {
    1,  /* key=input-auto-connect */
    64, /* key=input-client */
    0,  /* key=input-port */
    0,  /* key=input-channel-enabled */
    1   /* key=input-volume-enabled */
  },
  /* MIDI output parameters */
  {
    128, /* key=output-client */
    0  /* key=output-port */
  },
  /* MIDI misc. parameters */
  {
    MIDI_DEBUG_OFF /* key=debug-level */
  }
};

/* Page numbers for the MIDI settings notebook. */
/* Depends on the order the pages are added to the notebook. */
/* See create_midi_notebook(). */

enum MidiSettingsPage {
  MIDI_SETTINGS_INPUT_PAGE = 0,
//  MIDI_SETTINGS_OUTPUT_PAGE, /* future plan... */
  MIDI_SETTINGS_MISC_PAGE,
  MIDI_SETTINGS_COUNT_OF_PAGES
};

/****************************************************/
/* Local variables */

/* MIDI Dialog Box */

static GtkWidget* midi_dialog = NULL;
static GtkWidget* midi_notebook = NULL;

/*
  Variables with prefix "mi" are related to MIDI Input settings.
  Variables with prefix "mo" are related to MIDI Output settings. 
  Variables with prefix "mm" are related to MIDI Misc settings.
*/

static GtkWidget *mi_spin_client = NULL;
static GtkWidget *mi_spin_port = NULL;
static GtkWidget *mm_spin_debug = NULL;

/* For each, keep a flag to know if the page has changed.
   Will be queried when the dialog is close. */

static gboolean midi_settings_changed[MIDI_SETTINGS_COUNT_OF_PAGES];

/*
 * New settings set by the user thru the dialog box.
 * Must be reset whenever the dialog is called.
 *
 * User actions will modify the values in this structure.
 * Allow us to detect which values, if any, were modified,
 * and then take appropriate actions (for exemple to reinitialize the
 * MIDI input interface if the input client number has changed).
 */

static midi_prefs new_midi_settings;

/* Local functions prototypes */

static void dialog_apply_callback (GtkWidget *notebook);
static void dialog_cancel_callback(void);
static void dialog_okay_callback(void);
static void reset_page_changed_flags(void);

/************************************************************************/

/*****************************************************
 * Load MIDI configuration parameters.
 */

void
midi_load_config (void)
{
  prefs_node *f;


  f = prefs_open_read("midi");

  if (f) {

    prefs_get_int(f, "debug-level",
                  &midi_settings.misc.debug_level);
    /* Validate debug level */
    if ( midi_settings.misc.debug_level < 0) {
      midi_settings.misc.debug_level = MIDI_DEBUG_OFF;
    } else if (midi_settings.misc.debug_level > MIDI_DEBUG_HIGHEST) {
      midi_settings.misc.debug_level = MIDI_DEBUG_HIGHEST;
    }

    prefs_get_int(f, "input-auto-connect",
                  &midi_settings.input.auto_connect);
    prefs_get_int(f, "input-client",
                  &midi_settings.input.client);
    prefs_get_int(f, "input-port",
                  &midi_settings.input.port);
    prefs_get_int(f, "input-channel-enabled",
                  &midi_settings.input.channel_enabled);
    prefs_get_int(f, "input-volume-enabled",
                  &midi_settings.input.volume_enabled);

    prefs_get_int(f, "output-client",
                  &midi_settings.output.client);
    prefs_get_int(f, "output-port",
                  &midi_settings.output.port);

    prefs_close(f);
  }

} /* midi_load_config() */

/*****************************************************
 * Save MIDI configuration parameters.
 */

void
midi_save_config (void)
{
  prefs_node *f;

  f = prefs_open_write("midi");
  if(!f)
    return;

  if (IS_MIDI_DEBUG_ON) {
    g_print( "MIDI settings saved to file\n");
  }

  prefs_put_int(f, "debug-level", midi_settings.misc.debug_level);

  prefs_put_int(f, "input-auto-connect", midi_settings.input.auto_connect);
  prefs_put_int(f, "input-client", midi_settings.input.client);
  prefs_put_int(f, "input-port", midi_settings.input.port);
  prefs_put_int(f, "input-channel-enabled", midi_settings.input.channel_enabled);
  prefs_put_int(f, "input-volume-enabled", midi_settings.input.volume_enabled);

  prefs_put_int(f, "output-client", midi_settings.output.client);
  prefs_put_int(f, "output-port", midi_settings.output.port);

  prefs_close(f);

} /* midi_save_config() */

/***************************************************************************
 * MIDI configuration dialog functions.
 **************************************************************************/

/****************************************
 * MIDI dialog box callbacks.
 */

/*
  Process the cancel event.
*/

static void
dialog_cancel_callback(void)
{
  /* Just close the dialog box (discard any changes). */

  if (midi_notebook != NULL) {
    gtk_widget_destroy(midi_notebook);
    midi_notebook = NULL;
  }

  if (midi_dialog != NULL ) {
    gtk_widget_destroy(midi_dialog);
    midi_dialog = NULL;
  }

}

/*
  Process the Ok event.
*/

static void
dialog_okay_callback(void)
{
  int page_num;

  /*
   * For each page, set the notebook page to it and then call
   * the Apply callback.
   */

  for (page_num = 0; page_num < MIDI_SETTINGS_COUNT_OF_PAGES; page_num++ ) {
    if (IS_MIDI_DEBUG_ON) {
      g_print( "MIDI page %d changed: %d\n",
               page_num, midi_settings_changed[page_num]);
    }

    gtk_notebook_set_page( GTK_NOTEBOOK(midi_notebook), page_num);
    dialog_apply_callback( midi_notebook);
  }

  /* Close the dialog by calling the Cancel callback. */

  dialog_cancel_callback();
}

/*
  Process the "apply" event.
  Apply the changes of the current page of the notebook if it has changed.
*/

static void
dialog_apply_callback (GtkWidget *notebook)
{
  gboolean reinit_midi = FALSE;
  int page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));

  if ( midi_settings_changed[page_num] == FALSE ) {
    if (IS_MIDI_DEBUG_ON) {
      g_print("No changes to apply for page #%d\n", page_num);
    }

    return;
  }


  switch (page_num){
  case MIDI_SETTINGS_INPUT_PAGE:

    if (IS_MIDI_DEBUG_ON) {
      g_print( "new input settings: volume %d channel %d client %d port %d\n",
               new_midi_settings.input.volume_enabled,
               new_midi_settings.input.channel_enabled,
               new_midi_settings.input.client,
               new_midi_settings.input.port);
    }

    /*
      If the user changed the input client or port,
      we need to reinitialize the MIDI channel.
      The auto_connect feature does not need to reinit. the MIDI channel.
    */

    if ( new_midi_settings.input.client != midi_settings.input.client
         || new_midi_settings.input.port != midi_settings.input.port) {
      reinit_midi = TRUE;
    }

    /* Copy the new settings into the settings structure. */

    midi_settings.input = new_midi_settings.input;

    if ( reinit_midi ) {
      midi_init();
    }

    break;

#if 0
  case MIDI_SETTINGS_OUTPUT_PAGE:
    if (IS_MIDI_DEBUG_ON) {
      g_print( "new output settings: client %d port %d\n",
               new_midi_settings.output.client,
               new_midi_settings.output.port);
    }

    break;
#endif

  case MIDI_SETTINGS_MISC_PAGE:

    if (IS_MIDI_DEBUG_ON) {
      g_print( "new misc settings: debug %d\n",
               new_midi_settings.misc.debug_level);
    }

    /* Copy the new settings into the settings structure. */

    midi_settings.misc = new_midi_settings.misc;

    break;

  default:
    g_warning("MIDI Settings: unknown page\n");
  }

  /* Prepare for next call. */

  midi_settings_changed[page_num] = FALSE;

} /* dialog_apply_callback() */

/************************************************************************
 * MIDI Input settings dialog box functions.
 */

static void
input_auto_connect_toggled( GtkWidget *window)
{
  /* If auto-connect is disabled,
     make "client" and "port" inactive.
  */

  if ( new_midi_settings.input.auto_connect ) {
    new_midi_settings.input.auto_connect = 0;
    gtk_widget_set_sensitive( mi_spin_client, FALSE);
    gtk_widget_set_sensitive( mi_spin_port, FALSE);
  } else {
    /* If auto-connect is enabled,
       make "client" and "port" active.
    */

    new_midi_settings.input.auto_connect = 1;
    gtk_widget_set_sensitive( mi_spin_client, TRUE);
    gtk_widget_set_sensitive( mi_spin_port, TRUE);
  }

  midi_settings_changed[MIDI_SETTINGS_INPUT_PAGE] = TRUE;

}

static void
input_volume_toggled( GtkWidget *window)
{
  if ( new_midi_settings.input.volume_enabled ) {
    new_midi_settings.input.volume_enabled = 0;
  } else {
    new_midi_settings.input.volume_enabled = 1;
  }

  midi_settings_changed[MIDI_SETTINGS_INPUT_PAGE] = TRUE;

}

static void
input_channel_toggled( GtkWidget *window)
{
  if ( new_midi_settings.input.channel_enabled ) {
    new_midi_settings.input.channel_enabled = 0;
  } else {
    new_midi_settings.input.channel_enabled = 1;
  }

  midi_settings_changed[MIDI_SETTINGS_INPUT_PAGE] = TRUE;

}

static void
input_client_changed( GtkWidget *widget, GtkSpinButton **pspin)
{
  new_midi_settings.input.client = gtk_spin_button_get_value_as_int(*pspin);
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(*pspin),
                             new_midi_settings.input.client);

  midi_settings_changed[MIDI_SETTINGS_INPUT_PAGE] = TRUE;

}

static void
input_port_changed( GtkWidget *widget, GtkSpinButton **pspin)
{
  new_midi_settings.input.port = gtk_spin_button_get_value_as_int(*pspin);
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(*pspin),
                             new_midi_settings.input.port);

  midi_settings_changed[MIDI_SETTINGS_INPUT_PAGE] = TRUE;

}

/************************************************************************
 * MIDI Misc settings dialog box functions.
 */

static void
misc_debug_changed( GtkWidget *widget, GtkSpinButton **pspin)
{
  new_midi_settings.misc.debug_level =gtk_spin_button_get_value_as_int(*pspin);
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(*pspin),
                             new_midi_settings.misc.debug_level);

  midi_settings_changed[MIDI_SETTINGS_MISC_PAGE] = TRUE;

}

/**********************************************************************
 * Utility functions.
 */

/*
 * reset_page_changed_flags()
 *
 * Called after the creation of the notebook since by setting the value
 * in some fields marks the page has being CHANGED which it is not
 * true.  Only when a user changes something from the GUI that the page
 * should be flagged as CHANGED.
 */

static void reset_page_changed_flags(void)
{
  int i;

  for ( i = 0; i < MIDI_SETTINGS_COUNT_OF_PAGES; i++) {
    midi_settings_changed[i] = FALSE;
  }
}

/***********************************************************************
  Create the MIDI notebook. Called from the MIDI Settings dialog box.
  Gets a copy of the current MIDI settings to set initial state
  of the different controls inside the notebook pages.
*/

static GtkWidget*
create_midi_notebook( GtkDialog* dialog, midi_prefs settings)
{
  GtkWidget *notebook;
  GtkWidget *page;
  GtkWidget *thing;


  notebook = gtk_notebook_new();

  /*****************************************************************/
  /* Create the Input page */

  page = gtk_vbox_new(FALSE, 2);
  gtk_container_border_width(GTK_CONTAINER(page), PAGE_BORDER_WIDTH);

  thing = gtk_check_button_new_with_label(_("Auto connect"));
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing),
                              settings.input.auto_connect);
  gtk_box_pack_start(GTK_BOX(page), thing, FALSE, TRUE, 0);
  g_signal_connect(thing, "toggled",
                     G_CALLBACK(input_auto_connect_toggled), NULL);

  thing = gtk_check_button_new_with_label(_("Volume"));
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing),
                              settings.input.volume_enabled);
  gtk_box_pack_start(GTK_BOX(page), thing, FALSE, TRUE, 0);

  g_signal_connect(thing, "toggled",
                     G_CALLBACK(input_volume_toggled), NULL);

  thing = gtk_check_button_new_with_label(_("Channel"));
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing),
                              settings.input.channel_enabled);
  gtk_box_pack_start(GTK_BOX(page), thing, FALSE, TRUE, 0);
  g_signal_connect(thing, "toggled",
                     G_CALLBACK(input_channel_toggled), NULL);

  /* Create the spin button for the input client number. */

  gui_put_labelled_spin_button(page, _("Client number"), 0, 255,
                               &mi_spin_client, input_client_changed, 
                               &mi_spin_client);
  /* Calling the next function sets the CHANGED flag
     for the input page.  Needs to be reset before leaving this call. */
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(mi_spin_client),
                             settings.input.client);

  /* Create the spin button for the input port number. */

  gui_put_labelled_spin_button(page,	 _("Port number"), 0, 255,
                               &mi_spin_port, input_port_changed, 
                               &mi_spin_port);
  /* Calling the next function sets the CHANGED flag
     for the input page.  Needs to be reset before leaving this call. */
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(mi_spin_port),
                             settings.input.port);

  /* If auto-connect is disabled, "client" and "port" must be inactive. */

  if (settings.input.auto_connect == 0) {
    gtk_widget_set_sensitive( mi_spin_client, FALSE);
    gtk_widget_set_sensitive( mi_spin_port, FALSE);
  }

  /* Add the MIDI Input settings page to the notebook.*/

  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                           page, gtk_label_new(_("Input")));

#if 0 /* mkrause 20020516 */
  /*****************************************************************/
  /* Create the Output page */

  page = gtk_vbox_new(FALSE, 2);
  gtk_container_border_width(GTK_CONTAINER(page), PAGE_BORDER_WIDTH);

  thing = gtk_label_new(_("For future development"));
  gtk_box_pack_start(GTK_BOX(page), thing, FALSE, TRUE, 0);

  /* Add the MIDI Output settings page to the notebook.*/

  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                           page, gtk_label_new(_("Output")));
#endif

  /*****************************************************************/
  /* Create the Misc page */

  page = gtk_vbox_new(FALSE, 2);
  gtk_container_border_width(GTK_CONTAINER(page), PAGE_BORDER_WIDTH);

  /* Create the spin button for the debug level. */

  gui_put_labelled_spin_button(page, _("Debug level"), MIDI_DEBUG_OFF,
                               MIDI_DEBUG_HIGHEST - 1,
                               &mm_spin_debug, misc_debug_changed, 
                               &mm_spin_debug);
  /* Calling the next function sets the CHANGED flag
     for the misc. page.  Needs to be reset before leaving this call. */
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(mm_spin_debug),
                             settings.misc.debug_level);

  /* Add the MIDI Misc settings page to the notebook.*/

  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                           page, gtk_label_new(_("Misc")));

  /* For all the pages, set the "changed" flag to FALSE.
     This is done here because setting some fields in pages
     to their default values will have flagged the pages
     as being "changed". */

  reset_page_changed_flags();

  /* That's it */

  return notebook;

} /* create_midi_notebook() */

/******************************************
 * MIDI Settings dialog box.
 * Use a GtkNotebook with page #1 is for input settings,
 * and page #2 is for output settings (for future enhancements).
 */

void
midi_settings_dialog (void)
{
  GtkWidget *okay_button;
  GtkWidget *apply_button;
  GtkWidget *cancel_button;

  /* If dialog already created, just raise it. */

  if ( midi_dialog != NULL) {
    gdk_window_raise(midi_dialog->window);
    return;
  }

  /* Begins with current MIDI settings. */

  new_midi_settings = midi_settings;

  /* Create the dialog box.
     The action area will contain 3 buttons: Ok, Apply and Cancel.
     The top area will contain a notebook. */

  midi_dialog = gtk_dialog_new();
  gtk_window_set_title( GTK_WINDOW( midi_dialog), _("MIDI Configuration"));

  /* Create the buttons. */

  okay_button = gtk_button_new_with_label( _("Ok"));
  apply_button = gtk_button_new_with_label( _("Apply"));
  cancel_button = gtk_button_new_with_label( _("Cancel"));

  /* Add buttons to action area. */

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(midi_dialog)->action_area),
                     okay_button);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(midi_dialog)->action_area),
                     apply_button);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(midi_dialog)->action_area),
                     cancel_button);

  /* Create the notebook (if necessary). */

  if (midi_notebook == NULL) {
    midi_notebook = create_midi_notebook(GTK_DIALOG(midi_dialog),
                                         new_midi_settings);
  }

  /* Connect dialog/notebook callbacks. */

  g_signal_connect_swapped(okay_button,
                             "clicked",
                             G_CALLBACK(dialog_okay_callback),
                             0);
  g_signal_connect_swapped(apply_button,
                             "clicked",
                             G_CALLBACK(dialog_apply_callback),
                             G_OBJECT(midi_notebook));
  g_signal_connect_swapped(cancel_button,
                             "clicked",
                             G_CALLBACK(dialog_cancel_callback),
                             0);

  /* Add the notebook to the upper part of the dialog box. */
  /* To access the notebook from the dialog box object:
     g_list_first(gtk_container_children(midi_dialog->vbox))
  */

  gtk_container_add( GTK_CONTAINER(GTK_DIALOG(midi_dialog)->vbox),
                     GTK_WIDGET(midi_notebook));

  gtk_widget_show_all (midi_dialog);

} /* midi_settings_dialog() */

#endif /* defined(DRIVER_ALSA_09x) */
#endif /* !(defined(DRIVER_ALSA_050) || defined(DRIVER_ALSA_09x))*/
