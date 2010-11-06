
/*
 * The Real SoundTracker - GTK+ Tracker widget settings (header)
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

#ifndef _TRACKER_SETTINGS_H
#define _TRACKER_SETTINGS_H

#include <gtk/gtkvbox.h>

#include "tracker.h"

#define TRACKERSETTINGS(obj)          GTK_CHECK_CAST (obj, trackersettings_get_type (), TrackerSettings)
#define TRACKERSETTINGS_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, trackersettings_get_type (), TrackerSettingsClass)
#define IS_TRACKERSETTINGS(obj)       GTK_CHECK_TYPE (obj, trackersettings_get_type ())

typedef struct _TrackerSettings       TrackerSettings;
typedef struct _TrackerSettingsClass  TrackerSettingsClass;

struct _TrackerSettings
{
    GtkVBox widget;

    Tracker *tracker;
    int current_font;

    GtkWidget *clist;
    GtkWidget *add_button, *delete_button, *apply_button;
    GtkWidget *up_button, *down_button;
    GtkWidget *fontsel_dialog;

    gint clist_selected_row;
};

struct _TrackerSettingsClass
{
    GtkVBoxClass parent_class;
};

guint                                 trackersettings_get_type             (void);
GtkWidget *                           trackersettings_new                  (void);
void                                  trackersettings_write_settings       (void);
void                                  trackersettings_set_tracker_widget   (TrackerSettings *ts, Tracker *t);

void                                  trackersettings_cycle_font_forward   (TrackerSettings *t);
void                                  trackersettings_cycle_font_backward  (TrackerSettings *t);
#endif /* _TRACKER_SETTINGS_H */
