
/*
 * The Real SoundTracker - XM player (header)
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

#ifndef _ST_XMPLAYER_H
#define _ST_XMPLAYER_H

#include <glib.h>

#include "xm.h"

extern int player_songpos, player_patpos;
extern int player_tempo, player_bpm;
extern gboolean player_looped;

void        xmplayer_init_module       (void);
gboolean    xmplayer_init_play_song    (int songpos, int patpos);
gboolean    xmplayer_init_play_pattern (int pattern, int patpos, int only1row);
gboolean    xmplayer_play_note         (int channel, int note, int instrument);
gboolean    xmplayer_play_note_full    (int channel,
					int note,
					STSample *sample,
					guint32 offset,
					guint32 count);
void        xmplayer_play_note_keyoff  (int channel);
double      xmplayer_play              (void);
void        xmplayer_stop              (void);
void        xmplayer_set_songpos       (int songpos);
void        xmplayer_set_pattern       (int pattern);
void        xmplayer_set_tempo         (int tempo);
void        xmplayer_set_bpm           (int bpm);

#endif /* _ST_XMPLAYER_H */
