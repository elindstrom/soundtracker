
/*
 * The Real SoundTracker - Input (sampling) driver module definitions
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

#ifndef _ST_DRIVER_IN_H
#define _ST_DRIVER_IN_H

#include <glib.h>

#include "driver.h"

typedef struct st_in_driver {
    st_driver common;
} st_in_driver;

void     sample_editor_sampled            (void *dest,
					   guint32 count,
					   int mixfreq,
					   int mixformat);

#endif /* _ST_DRIVER_IN_H */
