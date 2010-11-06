
/*
 * The Real SoundTracker - file operations page (header)
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

#ifndef _FILE_OPERATIONS_H
#define _FILE_OPERATIONS_H

#include <gtk/gtk.h>

/* When adding new dialogs, add them in the "not included in the File
   tab" section first, or create appropriate labels in
   file-operations.c. */

enum {
    DIALOG_LOAD_MOD = 0,
    DIALOG_SAVE_MOD,
    DIALOG_SAVE_MOD_AS_WAV,
    DIALOG_SAVE_SONG_AS_XM,
    DIALOG_LOAD_SAMPLE,
    DIALOG_SAVE_SAMPLE,
    DIALOG_LOAD_INSTRUMENT,
    DIALOG_SAVE_INSTRUMENT,

    DIALOG_SAVE_RGN_SAMPLE, // is not included in the "File" tab
    DIALOG_LOAD_PATTERN,
    DIALOG_SAVE_PATTERN,
    DIALOG_LAST
};

extern GtkWidget *fileops_dialogs[DIALOG_LAST];

void            fileops_page_create                (GtkNotebook *nb);

gboolean        fileops_page_handle_keys           (int shift,
						    int ctrl,
						    int alt,
						    guint32 keyval,
						    gboolean pressed);

void            fileops_open_dialog                (void *dummy,
						    void *index);

void		fileops_refresh_list 		   (GtkFileSelection *fs,
						    gboolean grab);

void		fileops_tmpclean                   (void);
						    
#endif /* _FILE_OPERATIONS_H */
