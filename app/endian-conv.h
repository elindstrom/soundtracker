
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

#ifndef _ENDIAN_CONV_H
#define _ENDIAN_CONV_H

#include <config.h>

#include <glib.h>

static inline guint32
get_le_32 (guint8 *src)
{
#if defined(__i386__)
    return *(guint32*)src;
#else
    return (src[0] << 0) + (src[1] << 8) + (src[2] << 16) + (src[3] << 24);
#endif
}

static inline void
put_le_32 (guint8 *dest, gint32 d)
{
#if defined(__i386__)
    *(guint32*)dest = d;
#else
    dest[0] = d >> 0; dest[1] = d >> 8; dest[2] = d >> 16; dest[3] = d >> 24;
#endif
}

static inline guint16
get_le_16 (guint8 *src)
{
#if defined(__i386__)
    return *(guint16*)src;
#else
    return (src[0] << 0) + (src[1] << 8);
#endif
}

static inline void
put_le_16 (guint8 *dest, gint16 d)
{
#if defined(__i386__)
    *(guint16*)dest = d;
#else
    dest[0] = d >> 0; dest[1] = d >> 8;
#endif
}

static inline guint16
get_be_16 (guint8 *src)
{
    return (src[0] << 8) + (src[1] << 0);
}

void
le_16_array_to_host_order (gint16 *data,
			   int count);

void
byteswap_16_array (gint16 *data,
		   int count);

#endif /* _ENDIAN_CONV_H */


