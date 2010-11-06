
/*
 * The Real SoundTracker - GTK+ Tracker widget (header)
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

#ifndef _TRACKER_H
#define _TRACKER_H

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#include "xm.h"

#define TRACKER(obj)          GTK_CHECK_CAST (obj, tracker_get_type (), Tracker)
#define TRACKER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, tracker_get_type (), TrackerClass)
#define IS_TRACKER(obj)       GTK_CHECK_TYPE (obj, tracker_get_type ())

typedef struct _Tracker       Tracker;
typedef struct _TrackerClass  TrackerClass;

enum {
    TRACKERCOL_BG,
    TRACKERCOL_BG_CURSOR,
    TRACKERCOL_BG_MAJHIGH,
    TRACKERCOL_BG_MINHIGH,
    TRACKERCOL_BG_SELECTION,
    TRACKERCOL_NOTES,	
    TRACKERCOL_BARS,
    TRACKERCOL_CHANNUMS,
    TRACKERCOL_CURSOR,
    TRACKERCOL_CURSOR_EDIT,
    TRACKERCOL_LAST,
};

struct _Tracker
{
    GtkWidget widget;

    int disp_rows;
    int disp_starty;
    int disp_numchans;
    int disp_startx;
    int disp_chanwidth;
    int disp_cursor;

    GdkFont *font;
    int fonth, fontw;
    int baselineskip;

    GdkGC *bg_gc, *bg_cursor_gc, *bg_majhigh_gc, *bg_minhigh_gc, *notes_gc, *misc_gc;
    GdkColor colors[TRACKERCOL_LAST];
    int enable_backing_store;
    GdkPixmap *pixmap;
    guint idle_handler;

    XMPattern *curpattern;
    int patpos, oldpos;
    int num_channels;

    int cursor_ch, cursor_item;
    int leftchan;

    /* Block selection stuff */
    gboolean inSelMode;
    int sel_start_ch, sel_start_row;
    int sel_end_ch, sel_end_row;

    int mouse_selecting;
    int button;
};

struct _TrackerClass
{
    GtkWidgetClass parent_class;

    void (*patpos)(Tracker *t, int patpos, int patlen, int disprows);
    void (*xpanning)(Tracker *t, int leftchan, int numchans, int dispchans);
    void (*mainmenu_blockmark_set)(Tracker *t, int state);
};

extern const char * const notenames[4][96];

guint		tracker_get_type            (void);
GtkWidget*     	tracker_new                 (void);

void           	tracker_set_num_channels    (Tracker *t, int);

void           	tracker_set_backing_store   (Tracker *t, int on);
gboolean       	tracker_set_font            (Tracker *t, const gchar *fontname);

void           	tracker_reset               (Tracker *t);
void           	tracker_redraw              (Tracker *t);

void           	tracker_redraw_row          (Tracker *t, int row);
void           	tracker_redraw_current_row  (Tracker *t);

/* These are the navigation functions. */
void           	tracker_step_cursor_item    (Tracker *t, int direction);
void           	tracker_step_cursor_channel (Tracker *t, int direction);
void            tracker_set_cursor_channel  (Tracker *t, int channel);
void            tracker_set_cursor_item     (Tracker *t, int item);
void           	tracker_set_patpos          (Tracker *t, int row);
void           	tracker_step_cursor_row     (Tracker *t, int direction);

void           	tracker_set_pattern         (Tracker *t, XMPattern *pattern);
void           	tracker_set_xpanning        (Tracker *t, int left_channel);

/* Set, get, clear selection markers */
void		tracker_mark_selection	     (Tracker *t, gboolean enable);
void            tracker_clear_mark_selection (Tracker *t);
void		tracker_get_selection_rect   (Tracker *t, int *chStart, int *rowStart, int *nChannel, int *nRows);
gboolean	tracker_is_valid_selection   (Tracker *t);
gboolean        tracker_is_in_selection_mode (Tracker *t);

#endif /* _TRACKER_H */
