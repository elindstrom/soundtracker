
/*
 * The Real SoundTracker - time buffer
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

#include "time-buffer.h"

#include <glib.h>

/* This implementation of the time buffer interface might be rather
   suboptimal... */

struct time_buffer {
    GMutex *mutex;
    GList *list;
};

typedef struct time_buffer_item {
    double time;
    /* then user data follows */
} time_buffer_item;

time_buffer *
time_buffer_new (double maxtimedelta)
{
    time_buffer *t = g_new(time_buffer, 1);

    if(t) {
	t->mutex = g_mutex_new();
	t->list = NULL;
    }

    return t;
}

void
time_buffer_destroy (time_buffer *t)
{
    if(t) {
	g_list_free(t->list);
	g_mutex_free(t->mutex);
	g_free(t);
    }
}

void
time_buffer_clear (time_buffer *t)
{
    while(t->list) {
	g_free(t->list->data);
	t->list = g_list_remove_link(t->list, t->list);
    }
}

gboolean
time_buffer_add (time_buffer *t,
		 void *item,
		 double time)
{
    time_buffer_item *a = item;

    g_mutex_lock(t->mutex);
    a->time = time;
    t->list = g_list_append(t->list, a);
    g_mutex_unlock(t->mutex);

    return TRUE;
}

void *
time_buffer_get (time_buffer *t,
		 double time)
{
    int i, j, l;
    void *result = NULL;
    GList *list;

    g_mutex_lock(t->mutex);
    l = g_list_length(t->list);

    if(l == 0) {
	g_mutex_unlock(t->mutex);
	return NULL;
    }

    for(i = 0, list = t->list; i < l - 1; i++, list = list->next) {
     	time_buffer_item *a = list->data;
	if(time < a->time)
	    break;
    }

    for(j = 0; j < i - 1; j++) {
	GList *node = t->list;
	g_free(node->data);
	t->list = g_list_remove_link(t->list, node);
	g_list_free(node);
    }

    result = t->list->data;

    g_mutex_unlock(t->mutex);

    return result;
}
