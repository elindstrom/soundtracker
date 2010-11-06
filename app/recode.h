
/*
 * The Real SoundTracker - DOS Charset recoder (header)
 *
 * Copyright (C) 1998-2001 Michael Krause
 *
 * The tables have been taken from recode-3.4.1/ibmpc.c:
 * Copyright (C) 1990, 1993, 1994 Free Software Foundation, Inc.
 * Francois Pinard <pinard@iro.umontreal.ca>, 1988.
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

#ifndef _RECODE_H
#define _RECODE_H

void       recode_ibmpc_to_latin1                (char *string, int len);
void       recode_latin1_to_ibmpc                (char *string, int len);

#endif /* _RECODE_H */
