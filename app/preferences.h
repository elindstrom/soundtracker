
/*
 * The Real SoundTracker - Preferences handling (header)
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

#ifndef _ST_PREFERENCES_H
#define _ST_PREFERENCES_H

#include <stdio.h>

#include <gtk/gtk.h>

void           prefs_page_handle_keys         (int shift,
					       int ctrl,
					       int alt,
					       guint32 keyval);

int            prefs_fragsize_log2            (int fragsize);

typedef struct prefs_node prefs_node;

prefs_node *   prefs_open_read                (const char *name);
prefs_node *   prefs_open_write               (const char *name);
void           prefs_close                    (prefs_node *f);

// Use the following function if you need direct access to the preferences file
// (only directly after opening)
FILE *         prefs_get_file_pointer         (prefs_node *p);

// Returns the file location of a configuration node
char *         prefs_get_prefsdir             (void);
const char *   prefs_get_filename             (const char *name);

void           prefs_put_int                  (prefs_node *f,
					       const char *key,
					       int value);
void           prefs_put_string               (prefs_node *f,
					       const char *key,
					       const char *value);
gboolean       prefs_get_int                  (prefs_node *f,
					       const char *key,
					       int *dest);
gboolean       prefs_get_string               (prefs_node *f,
					       const char *key,
					       char *dest);

#endif /* _ST_PREFERENCES_H */
