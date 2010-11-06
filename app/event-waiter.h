
/*
 * The Real SoundTracker - event waiter (header)
 *
 * Copyright (C) 2001 Michael Krause
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

#ifndef _EVENT_WAITER_H
#define _EVENT_WAITER_H

#include <glib.h>

typedef struct event_waiter event_waiter;

/* An event waiter is used to synchronize certain events between the
   GUI and the audio thread. As opposed to the `time buffer', we don't
   pass actual data here, but we have a semaphore-like interface which
   can be made 'unready' through the start() call, and can be made
   ready for a specific time through the confirm() call.

   The current status, relative to a given time, can then be checked
   through the ready() call.

   start() and confirm() can nest. All functions are thread-safe.

   Usage Example:
   --------------

   When the user changes the song position while playing, and the
   mixing buffer is long enough, it could happen that the GUI would
   reset the song position to the previous value, because it still
   finds it in the player position time buffer. So the GUI stops
   updating the song position for as long as we haven't replied to the
   position change request. This is what event waiter does.

   The same applies to Tempo / BPM changes. */

event_waiter *   event_waiter_new          (void);
void             event_waiter_destroy      (event_waiter *e);

void             event_waiter_reset        (event_waiter *e);
void             event_waiter_start        (event_waiter *e);
void             event_waiter_confirm      (event_waiter *e, double readytime);
gboolean         event_waiter_ready        (event_waiter *e, double currenttime);

#endif /* _EVENT_WAITER_H */
