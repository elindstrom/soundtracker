
/*
 * The Real SoundTracker - gtk+ Playlist widget (header)
 *
 * Copyright (C) 1999-2001 Michael Krause
 * Copyright (C) 2003 Yury Aliaev
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

#ifndef _PLAYLIST_H
#define _PLAYLIST_H

#include <gdk/gdk.h>
#include <gtk/gtkvbox.h>

#define PLAYLIST(obj)          GTK_CHECK_CAST (obj, playlist_get_type (), Playlist)
#define PLAYLIST_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, playlist_get_type (), PlaylistClass)
#define IS_PLAYLIST(obj)       GTK_CHECK_TYPE (obj, playlist_get_type ())

typedef struct _Playlist       Playlist;
typedef struct _PlaylistClass  PlaylistClass;

struct _Playlist
{
    GtkVBox widget;

    GtkObject *adj_songpos;
    GtkWidget *spin_songlength, *spin_songpat, *spin_restartpos;
    GtkWidget *ibutton, *icbutton, *ifbutton, *dbutton;
    
    GtkWidget *numlabels[5], *patlabels[5];

    int max_length;
    int min_pattern;
    int max_pattern;

    int length;
    int alloc_length;
    int *patterns;

    int current_position;
    int restart_position;
    int signals_disabled;
    int frozen;
};

struct _PlaylistClass
{
    GtkVBoxClass parent_class;

    void (*current_position_changed) (Playlist *p, int pos);
    void (*restart_position_changed) (Playlist *p, int pos);
    void (*song_length_changed) (Playlist *p, int length);
    void (*entry_changed) (Playlist *p, int pos, int pat);
};

guint          playlist_get_type               (void);
GtkWidget *    playlist_new                    (void);

void           playlist_freeze                 (Playlist *p);
void           playlist_thaw                   (Playlist *p);
void           playlist_enable                 (Playlist *p, gboolean enable);

void           playlist_freeze_signals         (Playlist *p);
void           playlist_thaw_signals           (Playlist *p);

void           playlist_set_length             (Playlist *p, int length);
int            playlist_get_length             (Playlist *p);

void           playlist_set_nth_pattern        (Playlist *p, int pos, int pat);
int            playlist_get_nth_pattern        (Playlist *p, int pos);

void           playlist_set_position           (Playlist *p, int pos);
int            playlist_get_position           (Playlist *p);

void           playlist_set_restart_position   (Playlist *p, int pos);
int            playlist_get_restart_position   (Playlist *p);

void	       playlist_insert_pattern	       (Playlist *p, int pos, int pat);

#endif /* _PLAYLIST_H */
