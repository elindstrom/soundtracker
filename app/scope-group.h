
/*
 * The Real SoundTracker - main window oscilloscope group (header)
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

#ifndef _SCOPE_GROUP_H
#define _SCOPE_GROUP_H

#include <gtk/gtk.h>

#include "sample-display.h"

#ifndef NO_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#define SCOPE_GROUP(obj)          GTK_CHECK_CAST (obj, scope_group_get_type (), ScopeGroup)
#define SCOPE_GROUP_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, scope_group_get_type (), ScopeGroupClass)
#define IS_SCOPE_GROUP(obj)       GTK_CHECK_TYPE (obj, scope_group_get_type ())

typedef struct _ScopeGroup       ScopeGroup;
typedef struct _ScopeGroupClass  ScopeGroupClass;

struct _ScopeGroup
{
    GtkHBox hbox;

    GtkWidget *table;
    SampleDisplay *scopes[32];
    GtkWidget *scopebuttons[32];
#ifndef NO_GDK_PIXBUF
    GtkWidget *mutedpic[32];
#endif
    int numchan;
    int scopes_on;
    int update_freq;
    int gtktimer;
    gint32 on_mask;
};

struct _ScopeGroupClass
{
    GtkHBoxClass parent_class;
};

guint
scope_group_get_type (void);

#ifndef NO_GDK_PIXBUF
GtkWidget *
scope_group_new (GdkPixbuf *mutedpic);
#else
GtkWidget *
scope_group_new (void);
#endif

void
scope_group_set_num_channels (ScopeGroup *s, int num_channels);

void
scope_group_enable_scopes (ScopeGroup *s, int enable);

void
scope_group_start_updating (ScopeGroup *s);

void
scope_group_stop_updating (ScopeGroup *s);

void
scope_group_set_update_freq (ScopeGroup *s, int freq);

#endif /* _SCOPE_GROUP_H */
