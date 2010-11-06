
/*
 * The Real SoundTracker - GUI (menu bar) (header)
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

#ifndef _ST_MENUBAR_H
#define _ST_MENUBAR_H

#include <config.h>

#include <gtk/gtk.h>

void
menubar_create (GtkWidget *window, GtkWidget *destbox);

void
menubar_quit_requested (void);

void
menubar_init_prefs ();

void
menubar_write_accels ();

void
menubar_block_mode_set(gboolean state);

#ifdef USE_GNOME
extern GnomeUIInfo track_editor_popup_menu[];
#endif

#endif /* ST_MENUBAR_H */
