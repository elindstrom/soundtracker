
/*
 * The Real SoundTracker - Assembly routines for the mixer (header)
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

#ifndef _ST_MIXERASM_H
#define _ST_MIXERASM_H

#include <glib.h>

gint32 mixerasm_stereo_16_scopes (gint32 current,     // 8
				  gint32 increment,   // 12
				  gint16 *data,       // 16
				  gint32 *mixed,      // 20
				  gint16 *scopedata,   // 24
				  guint32 volume,     // 28
				  guint32 leftvol,    // 32
				  guint32 rightvol,   // 36
				  guint32 count);     // 40

gint32 mixerasm_mono_16_scopes (gint32 current,     // 8
				gint32 increment,   // 12
				gint16 *data,       // 16
				gint32 *mixed,      // 20
				gint16 *scopedata,   // 24
				guint32 volume,     // 28
				guint32 count);     // 32

gint32 mixerasm_stereo_16(gint32 current,     // 8
			  gint32 increment,   // 12
			  gint16 *data,       // 16
			  gint32 *mixed,      // 20
			  guint32 leftvol,    // 24
			  guint32 rightvol,   // 28
			  guint32 count);     // 32

gint32 mixerasm_mono_16(gint32 current,     // 8
			gint32 increment,   // 12
			gint16 *data,       // 16
			gint32 *mixed,      // 20
			guint32 volume,     // 24
			guint32 count);     // 28

#endif /* _ST_MIXERASM_H */
