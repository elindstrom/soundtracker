
/*
 * The Real SoundTracker - Transposition dialog (header)
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

#ifndef _ST_TRANSPOSITION_H
#define _ST_TRANSPOSITION_H

#include <glib.h>

#include "tracker.h"

void        transposition_dialog                   (void);

void        transposition_transpose_selection      (Tracker *t,
						    int by);

#endif /* _ST_TRANSPOSITION_H */
