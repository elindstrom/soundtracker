
/*
 * The Real SoundTracker - X keyboard handling (header)
 *
 * Copyright (C) 1997-2001 Michael Krause
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

#ifndef _KEYS_H
#define _KEYS_H

#include <gdk/gdktypes.h>

int
keys_init (void);

guint32
keys_get_key_meaning (guint32 keysym,
		      int modifier); // modifier is 1 shift, 2 ctrl, 4 meta

#define KEYS_MEANING_TYPE(x) (x >> 16)
#define KEYS_MEANING_TYPE_MAKE(x) (x << 16)
#define KEYS_MEANING_VALUE(x) (x & 0xFFFF)

#define KEYS_MEANING_NOTE   0
#define KEYS_MEANING_KEYOFF 1
#define KEYS_MEANING_FUNC   2
#define KEYS_MEANING_CH     3

#define ENCODE_MODIFIERS(shift, ctrl, alt) (1 * (shift != 0) + 2 * (ctrl != 0) + 4 * (alt != 0))

void
keys_dialog (void);

int
keys_save_config (void);

gboolean
keys_is_key_pressed (guint32 keysym,
		     int modifiers);

#endif /* _NOTEKEYS_H */
