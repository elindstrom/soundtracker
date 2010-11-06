
/*
 * The Real SoundTracker - track editor (header)
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

#ifndef _TRACK_EDITOR_H
#define _TRACK_EDITOR_H

#include <gtk/gtknotebook.h>

#include "tracker.h"
#include "tracker-settings.h"

extern Tracker *tracker;
extern GtkWidget *trackersettings;
extern GtkWidget *vscrollbar;

void      tracker_page_create              (GtkNotebook *nb);

gboolean  track_editor_handle_keys         (int shift,
					    int ctrl,
					    int alt,
					    guint32 keyval,
					    gboolean pressed);

void      track_editor_do_the_note_key     (int note,
					    gboolean pressed,
					    guint32 xkeysym,
					    int modifiers);

void      track_editor_toggle_jazz_edit    (void);

void      track_editor_set_num_channels    (int n);

void      track_editor_load_config         (void);
void      track_editor_save_config         (void);

/* Handling of real-time scrolling */
void      tracker_start_updating      (void);
void      tracker_stop_updating       (void);
void      tracker_set_update_freq     (int);

/* c'n'p operations */

void      track_editor_copy_pattern        (Tracker *t);
void      track_editor_cut_pattern         (Tracker *t);
void      track_editor_paste_pattern       (Tracker *t);
void      track_editor_copy_track          (Tracker *t);
void      track_editor_cut_track           (Tracker *t);
void      track_editor_paste_track         (Tracker *t);
void      track_editor_delete_track        (Tracker *t);
void      track_editor_insert_track        (Tracker *t);
void      track_editor_kill_notes_track    (Tracker *t);
void      track_editor_mark_selection      (Tracker *t);
void      track_editor_clear_mark_selection(Tracker *t);
void      track_editor_copy_selection      (Tracker *t);
void      track_editor_cut_selection       (Tracker *t);
void      track_editor_paste_selection     (Tracker *t);

void      track_editor_interpolate_fx      (Tracker *t);

void      track_editor_cmd_mvalue (Tracker *t, gboolean mode);

#endif /* _TRACK_EDITOR_H */
