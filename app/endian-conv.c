
/*
 * The Real SoundTracker - Endianness conversion
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

#include <config.h>

#include "endian-conv.h"

void
le_16_array_to_host_order (gint16 *data,
			   int count)
{
#ifdef WORDS_BIGENDIAN
    for(; count; count--, data++) {
	gint8 *p = (gint8*)data;
	gint8 a = p[0];
	p[0] = p[1];
	p[1] = a;
    }
#endif
}

void
byteswap_16_array (gint16 *data,
		   int count)
{
    for(; count; count--, data++) {
	gint8 *p = (gint8*)data;
	gint8 a = p[0];
	p[0] = p[1];
	p[1] = a;
    }
}
