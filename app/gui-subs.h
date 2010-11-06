
/*
 * The Real SoundTracker - GUI support routines (header)
 *
 * Copyright (C) 1998-2001 Michael Krause
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

#ifndef _GUI_SUBS_H
#define _GUI_SUBS_H

#include <gtk/gtk.h>

/* values for status bar messages */
enum {
    STATUS_IDLE = 0,
    STATUS_PLAYING_SONG,
    STATUS_PLAYING_PATTERN,
    STATUS_LOADING_MODULE,
    STATUS_MODULE_LOADED,
    STATUS_SAVING_MODULE,
    STATUS_MODULE_SAVED,
    STATUS_LOADING_SAMPLE,
    STATUS_SAMPLE_LOADED,
    STATUS_SAVING_SAMPLE,
    STATUS_SAMPLE_SAVED,
    STATUS_LOADING_INSTRUMENT,
    STATUS_INSTRUMENT_LOADED,
    STATUS_SAVING_INSTRUMENT,
    STATUS_INSTRUMENT_SAVED,
    STATUS_SAVING_SONG,
    STATUS_SONG_SAVED,
};

extern guint statusbar_context_id;
extern GtkWidget *status_bar;
extern GtkWidget *st_clock;

void                 statusbar_update                 (int message,
						       gboolean force_gui_update);

GtkWidget*           file_selection_create            (const gchar *title,
						       void(*clickfunc)());
void                 file_selection_save_path         (const gchar *fn,
						       gchar *store);
/* Return TRUE if there is a non-empty basename in the given filename */
gboolean             file_selection_is_valid          (const gchar *fn);

int                  find_current_toggle              (GtkWidget **widgets,
						       int count);

void                 add_empty_hbox                   (GtkWidget *tobox);
void                 add_empty_vbox                   (GtkWidget *tobox);

void                 make_radio_group                 (const char **labels,
						       GtkWidget *tobox,
						       GtkWidget **saveptr,
						       gint t1,
						       gint t2,
						       void (*sigfunc) (void));
void                 make_radio_group_full            (const char **labels,
						       GtkWidget *tobox,
						       GtkWidget **saveptr,
						       gint t1,
						       gint t2,
						       void (*sigfunc) (void),
						       gpointer data);
GtkWidget*           make_labelled_radio_group_box    (const char *title,
						       const char **labels,
						       GtkWidget **saveptr,
						       void (*sigfunc) (void));
GtkWidget*           make_labelled_radio_group_box_full (const char *title,
						       const char **labels,
						       GtkWidget **saveptr,
						       void (*sigfunc) (void),
						       gpointer data);

void                 gui_put_labelled_spin_button     (GtkWidget *destbox,
						       const char *title,
						       int min,
						       int max,
						       GtkWidget **spin,
						       void(*callback)(),
						       void *callbackdata);
void                 gui_update_spin_adjustment       (GtkSpinButton *spin,
						       int min,
						       int max);
void                 gui_update_range_adjustment      (GtkRange *range,
						       int pos,
						       int upper,
						       int window,
						       void(*func)());
void		     gui_hang_tooltip 		      (GtkWidget *widget, const gchar *text);

typedef enum {
    GUI_SUBS_SLIDER_WITH_HSCALE = 0,
    GUI_SUBS_SLIDER_SPIN_ONLY
} gui_subs_slider_type;

typedef struct gui_subs_slider {
    const char *title;
    int min, max;
    void (*changedfunc)(int value);
    gui_subs_slider_type type;
    GtkAdjustment *adjustment1, *adjustment2;
    GtkWidget *slider, *spin;
    gboolean update_without_signal;
} gui_subs_slider;

GtkWidget *          gui_subs_create_slider           (gui_subs_slider *s);

void                 gui_subs_set_slider_value        (gui_subs_slider *s,
						       int v);

int                  gui_subs_get_slider_value        (gui_subs_slider *s);

typedef struct OptionMenuItem {
  const gchar *name;
  void *func;
} OptionMenuItem;

GtkWidget *          gui_build_option_menu            (OptionMenuItem items[],
						       gint           num_items,
						       gint           history);

GtkWidget *          gui_clist_in_scrolled_window     (int n,
						       gchar **tp,
						       GtkWidget *hbox);//!!!

GtkWidget *	     gui_list_in_scrolled_window      (int n, gchar **tp,  GtkWidget *hbox,
						       GType *types, gfloat *alignments,
						        gboolean *expands,
							GtkSelectionMode mode);

GtkWidget *          gui_stringlist_in_scrolled_window(int n,
						       gchar **tp,
						       GtkWidget *hbox);

void		     gui_list_clear		      (GtkWidget *list);

void		     gui_list_clear_with_model	      (GtkTreeModel *model);

GtkTreeModel *	     gui_list_freeze		      (GtkWidget *list);

void		     gui_list_thaw		      (GtkWidget *list, GtkTreeModel *model);

#define GUI_GET_LIST_STORE(list) GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)))

void		     gui_list_handle_selection	      (GtkWidget *list,
						       GCallback handler,
						       gpointer data);

inline gboolean	     gui_list_get_iter		      (guint n,
						       GtkListStore *tree_model,
						       GtkTreeIter *iter);

inline void	     gui_list_moveto                  (GtkWidget *list, guint n);

inline void	     gui_string_list_set_text	      (GtkWidget *list, guint row,
						       guint col, const gchar *string);

inline void	     gui_list_select		      (GtkWidget *list, guint row);

GtkWidget *          gui_button                       (GtkWidget * win,
						       char *stock,
						       void *callback,
						       gpointer userdata,
						       GtkWidget *box);

void
gui_yes_no_cancel_modal (GtkWidget *window,
			   const gchar *text,
			   void (*callback)(gint, gpointer),
			   gpointer data);
#ifndef USE_GNOME

#define GNOME_APP(x) x

void                 gnome_app_ok_cancel_modal        (GtkWidget *window,
						       const gchar *text,
						       void (*callback)(gint, gpointer),
						       gpointer data);
void                 gnome_warning_dialog             (gchar *text);
void                 gnome_error_dialog               (gchar *text);

#endif

#endif /* _GUI_SUBS_H */
