
/*
 * The Real SoundTracker - file operations page
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

#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include "i18n.h"
#include <gdk/gdktypes.h>
#include "file-operations.h"
#include "keys.h"
#include "track-editor.h"
#include "gui-subs.h"
#include "gui.h"
#include "errors.h"

/* Welcome! Heavy gtk+ hacking going on here! :-) */

GtkWidget *fileops_dialogs[DIALOG_LAST];

static GtkWidget *rightbox;
static GtkWidget *typeradio[DIALOG_LAST];

static GtkFileSelection *fileops_current_dialog = NULL;
static guint handler_id_f, handler_id_d;

static GtkWidget *
fileops_filesel_get_confirm_area (GtkFileSelection *fs)
{
    /* whhaaaaaahooooooooo! */
    return GTK_WIDGET(
	((GtkBoxChild*) g_list_nth_data(
	    GTK_BOX(fs->main_vbox)->children,
	    4))
	->widget);
}

static gboolean
is_single_click_dialog (GtkFileSelection *fs)
{
    GtkWidget *w = GTK_WIDGET(fs);

    return w == fileops_dialogs[DIALOG_LOAD_MOD]
	|| w == fileops_dialogs[DIALOG_LOAD_SAMPLE]
	|| w == fileops_dialogs[DIALOG_LOAD_INSTRUMENT];
}

static void
file_selection_file_button (GtkWidget *widget,
			    gint row, 
			    gint column, 
			    GdkEventButton *bevent,
			    gpointer user_data)
{
    gtk_button_clicked(GTK_BUTTON(GTK_FILE_SELECTION(user_data)->ok_button));
}

static void
file_selection_dir_button (GtkWidget *widget,
			   gint row, 
			   gint column, 
			   GdkEventButton *bevent,
			   gpointer user_data)
{
    gchar *k = NULL;
    static gchar t[256];

    gtk_clist_get_text (GTK_CLIST (widget), row, 0, &k);

    if(k != NULL) {
	strncpy(t, k, 255);
	t[255] = 0;

	gtk_file_selection_set_filename (GTK_FILE_SELECTION(user_data),
					 t);
    }
}

static void
fileops_filesel_pre_insertion (GtkFileSelection *fs)
{
/* Maybe these 2 functions responsible for single-clicking...
    gtk_widget_hide(fs->button_area);
	printf("file list type: %s\n", G_OBJECT_TYPE_NAME(G_OBJECT(fs->file_list)));

    if(is_single_click_dialog(fs)) {
	gtk_widget_hide(fileops_filesel_get_confirm_area(fs));


	handler_id_f = g_signal_connect_after (G_OBJECT (fs->file_list), "select-cursor-row",
						 (GCallback) file_selection_file_button,
						 (gpointer) fs);
    }

    handler_id_d = g_signal_connect_after (G_OBJECT (fs->dir_list), "select-cursor-row",
					   (GCallback) file_selection_dir_button,
					   (gpointer) fs);
*/
}

static void
fileops_filesel_post_removal (GtkFileSelection *fs)
{
/*
    gtk_signal_disconnect(GTK_OBJECT(fs->dir_list), handler_id_d);

    if(is_single_click_dialog(fs)) {
	gtk_signal_disconnect(GTK_OBJECT(fs->file_list), handler_id_f);

	gtk_widget_show(fileops_filesel_get_confirm_area(fs));
    }

    gtk_widget_show(fs->button_area);
*/
}

static void
typeradio_changed (void)
{
    int n = find_current_toggle(typeradio, DIALOG_LAST);

    if(fileops_current_dialog) {
	gtk_container_remove(GTK_CONTAINER(rightbox), fileops_current_dialog->main_vbox);
	fileops_filesel_post_removal(fileops_current_dialog);
	gtk_container_add(GTK_CONTAINER(fileops_current_dialog), fileops_current_dialog->main_vbox);
    }

    fileops_current_dialog = GTK_FILE_SELECTION(fileops_dialogs[n]);
    fileops_refresh_list(GTK_FILE_SELECTION(fileops_dialogs[n]), FALSE);

    gtk_widget_hide(GTK_WIDGET(fileops_current_dialog)); /* close window if it's open */

    gtk_object_ref(GTK_OBJECT(fileops_current_dialog->main_vbox));
    gtk_container_remove(GTK_CONTAINER(fileops_current_dialog), fileops_current_dialog->main_vbox);
    fileops_filesel_pre_insertion(fileops_current_dialog);
    gtk_box_pack_start(GTK_BOX(rightbox), fileops_current_dialog->main_vbox, TRUE, TRUE, 0);
	fileops_refresh_list(fileops_current_dialog, FALSE);
}

void
fileops_page_create (GtkNotebook *nb)
{
    GtkWidget *hbox, *vbox, *thing;
    static const char *labels1[] = {
	N_("Load Module"),
	N_("Save Module"),
	N_("Render WAV"),
	N_("Save Song"),
	N_("Load Sample"),
	N_("Save Sample"),
	N_("Load Instrument"),
	N_("Save Instrument"),
	NULL
    };

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_container_border_width(GTK_CONTAINER(hbox), 10);
    gtk_notebook_append_page(nb, hbox, gtk_label_new(_("File")));
    gtk_widget_show(hbox);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_widget_show(vbox);

    make_radio_group((const char**)labels1, vbox, typeradio, FALSE, FALSE, typeradio_changed);

#if USE_SNDFILE == 0 && defined (NO_AUDIOFILE)
    gtk_widget_set_sensitive(typeradio[DIALOG_SAVE_MOD_AS_WAV], FALSE);
    gtk_widget_set_sensitive(typeradio[DIALOG_LOAD_SAMPLE], FALSE);
    gtk_widget_set_sensitive(typeradio[DIALOG_SAVE_SAMPLE], FALSE);
#endif

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    rightbox = vbox = gtk_vbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    typeradio_changed();
}

gboolean
fileops_page_handle_keys (int shift,
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
fileops_open_dialog (void *dummy,
		     void *index)
{
    int n = GPOINTER_TO_INT(index);

    if(!fileops_dialogs[n]) {
	error_error(_("Operation not supported."));
	return;
    }
    
    if(fileops_dialogs[n] == (GtkWidget*)fileops_current_dialog) {
	gui_go_to_fileops_page();
    } else {
	gtk_widget_show(fileops_dialogs[n]);
	fileops_refresh_list(GTK_FILE_SELECTION(fileops_dialogs[n]), TRUE);
    }
}

void
fileops_refresh_list (GtkFileSelection *fs,
						gboolean grab)
{
	gtk_file_selection_set_filename (GTK_FILE_SELECTION (fs), "." G_DIR_SEPARATOR_S);
	if(grab)
		gtk_widget_grab_focus (GTK_FILE_SELECTION (fs)->selection_entry);
}

/* simple, non-recursive file eraser... make it better! :-) */
void
fileops_tmpclean (void)
{
    DIR *dire;
    struct dirent *entry;
    static char tname[1024], fname[1024];

    strcpy (tname, prefs_get_prefsdir());
    strcat (tname, "/tmp/");

    if(!(dire = opendir(tname))) {
	return;
    }

    while((entry = readdir(dire))) {
	if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
	    strcpy(fname, tname);
	    strcat(fname, entry->d_name);
	    unlink(fname);
        }
    }

    closedir(dire);
}


