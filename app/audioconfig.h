
/*
 * The Real SoundTracker - Audio configuration dialog (header)
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

#ifndef _ST_AUDIOCONFIG_H
#define _ST_AUDIOCONFIG_H

#include <glib.h>

void        audioconfig_dialog                   (void);

void        audioconfig_load_config              (void);
void        audioconfig_load_mixer_config        (void);
void        audioconfig_save_config              (void);

// Currently initialized in main.c
extern GList *drivers[2];
extern GList *mixers;

#define DRIVER_OUTPUT 0
#define DRIVER_INPUT  1

#endif /* _ST_AUDIOCONFIG_H */
