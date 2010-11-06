
/*
 * The Real SoundTracker - time buffer (header)
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

#ifndef _TIME_BUFFER_H
#define _TIME_BUFFER_H

#include <glib.h>

/* A time buffer is used to store time-discrete player info for use by
   the main program -- because of the audio buffer, displaying the
   info must be delayed to coincide with the audio output in the
   speakers.

   The _add and _get functions are thread-safe. */

typedef struct time_buffer time_buffer;

time_buffer *   time_buffer_new          (double maxtimedelta);
void            time_buffer_destroy      (time_buffer *t);

gboolean        time_buffer_add          (time_buffer *t, void *item, double time);
void            time_buffer_clear        (time_buffer *t);
void *          time_buffer_get          (time_buffer *t, double time);

#endif /* _TIME_BUFFER_H */
