
/*
 * The Real SoundTracker - user tips (header)
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

#ifndef _ST_TIPS_DIALOG_H
#define _ST_TIPS_DIALOG_H

#include <gtk/gtk.h>

/* Open separate tips dialog. */
void        tips_dialog_open               (void);

/* Return dialog main vbox without creating window. An existing tips
   dialog window will be closed. */
GtkWidget * tips_dialog_get_vbox           (void);

void        tips_dialog_load_settings      (void);
void        tips_dialog_save_settings      (void);

extern int tips_dialog_last_tip;
extern int tips_dialog_show_tips;

#endif /* _ST_TIPS_DIALOG_H */
