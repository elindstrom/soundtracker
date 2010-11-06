
/*
 * The Real SoundTracker - module info page (header)
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

#ifndef _MODULE_INFO_H
#define _MODULE_INFO_H

#include <gtk/gtk.h>

void            modinfo_page_create                (GtkNotebook *nb);

gboolean        modinfo_page_handle_keys           (int shift,
						    int ctrl,
						    int alt,
						    guint32 keyval,
						    gboolean pressed);

void            modinfo_update_instrument          (int instr);
void            modinfo_update_sample              (int sample);
void            modinfo_update_all                 (void);

void            modinfo_set_current_instrument     (int);
void            modinfo_set_current_sample         (int);

void
modinfo_delete_unused_instruments (void);

void
modinfo_find_unused_pattern (void);

void
modinfo_copy_to_unused_pattern (void);

void
modinfo_pack_patterns (void);

void
modinfo_reorder_patterns (void);

void
modinfo_clear_unused_patterns (void);

void
modinfo_optimize_module (void);

#endif /* _MODULE_INFO_H */
