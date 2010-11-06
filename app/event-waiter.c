
/*
 * The Real SoundTracker - event waiter
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

#include "event-waiter.h"

#include <glib.h>

struct event_waiter {
    GMutex *mutex;
    int counter;
    double time;
};

event_waiter *
event_waiter_new (void)
{
    event_waiter *e = g_new(event_waiter, 1);

    if(e) {
	e->mutex = g_mutex_new();
	event_waiter_reset(e);
    }

    return e;
}

void
event_waiter_destroy (event_waiter *e)
{
    if(e) {
	g_mutex_free(e->mutex);
	g_free(e);
    }
}

void
event_waiter_reset (event_waiter *e)
{
    g_assert(e);

    g_mutex_lock(e->mutex);
    e->counter = 0;
    e->time = 0.0;
    g_mutex_unlock(e->mutex);
}

void
event_waiter_start (event_waiter *e)
{
    g_assert(e);

    g_mutex_lock(e->mutex);
    e->counter++;
    g_mutex_unlock(e->mutex);
}

void
event_waiter_confirm (event_waiter *e,
		      double readytime)
{
    g_assert(e);

    g_mutex_lock(e->mutex);
    if(e->counter > 0) {
	e->counter--;
    }
    if(readytime >= e->time) {
	e->time = readytime;
    }
    g_mutex_unlock(e->mutex);
}

gboolean
event_waiter_ready (event_waiter *e,
		    double currenttime)
{
    gboolean result;

    g_assert(e);

    g_mutex_lock(e->mutex);    
    result = (e->counter == 0 && currenttime >= e->time);
    g_mutex_unlock(e->mutex);

    return result;
}
