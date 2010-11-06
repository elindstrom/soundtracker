
/*
 * The Real SoundTracker - main user interface handling (header)
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

#ifndef _GUI_H
#define _GUI_H

#include <gtk/gtk.h>
#include "gui-subs.h"
#include "audio.h"

struct STSample;

/* values for gui_playing_mode */
enum {
    PLAYING_SONG = 1,
    PLAYING_PATTERN,
    PLAYING_NOTE,     /* is overridden by PLAYING_SONG / PLAYING_PATTERN */
};

extern int gui_playing_mode; /* one of the above or 0 */

/* Notebook page numbers.  Should be kept in sync
   when pages are added to notebook [See gui_init() in app/gui.c]. */

enum {
    NOTEBOOK_PAGE_FILE = 0,
    NOTEBOOK_PAGE_TRACKER,
    NOTEBOOK_PAGE_INSTRUMENT_EDITOR,
    NOTEBOOK_PAGE_SAMPLE_EDITOR,
    NOTEBOOK_PAGE_MODULE_INFO
};

extern int notebook_current_page; /* one of the above */

#define GUI_ENABLED (gui_playing_mode != PLAYING_SONG && gui_playing_mode != PLAYING_PATTERN)
#define GUI_EDITING (GTK_TOGGLE_BUTTON(editing_toggle)->active)

extern GtkWidget *editing_toggle;
extern GtkWidget *gui_curins_name, *gui_cursmpl_name;
extern GtkWidget *mainwindow;

extern void show_editmode_status (void);

int                  gui_splash                       (int argc, char *argv[]);
int                  gui_final                        (int argc, char *argv[]);

void                 gui_playlist_initialize          (void);

void                 gui_play_note                    (int channel,
						       int note);
void                 gui_play_note_full               (unsigned channel,
						       unsigned note,
						       struct STSample *sample,
						       guint32 offset,
						       guint32 count);
void                 gui_play_note_keyoff             (int channel);

void                 gui_play_stop                    (void);
void                 gui_start_sampling               (void);
void                 gui_stop_sampling                (void);

void                 gui_get_text_entry               (int length,
						       void(*changedfunc)(),
						       GtkWidget **widget);

void                 gui_go_to_fileops_page           (void);

void                 gui_set_current_instrument       (int);
void                 gui_set_current_sample           (int);
void                 gui_set_current_pattern          (int, gboolean);
void                 gui_update_pattern_data          (void);

int                  gui_get_current_instrument       (void);
int                  gui_get_current_sample           (void);
int                  gui_get_current_pattern          (void);

int                  gui_get_current_jump_value       (void);
int                  gui_get_current_octave_value     (void);
void		     gui_set_jump_value		      (int value);

void                 gui_update_player_pos            (const audio_player_pos *p);
void                 gui_clipping_indicator_update    (double songtime);

void                 gui_init_xm                      (int new_xm, gboolean updatechspin);
void                 gui_free_xm                      (void);
void                 gui_new_xm                       (void);
void                 gui_load_xm                      (const char *filename);

void		     gui_direction_clicked 	      (GtkWidget *widget,
						       gpointer data);
void		     gui_accidentals_clicked 	      (GtkWidget *widget,
						       gpointer data);

void		     gui_shrink_pattern		      (void);
void		     gui_expand_pattern		      (void);	

GtkStyle*	     gui_get_style		      (void);
#endif /* _GUI_H */
