
/*
 * The Real SoundTracker - error handling functions (header)
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

#ifndef _ERRORS_H
#define _ERRORS_H

/* Routines related to error messages and status bars -- these are
   thread-safe and non-blocking; dialogs are displayed within
   read_mixer_pipe(). More complicated dialogs (OK/Cancel questions
   etc.) have to be hacked up manually. */

void              error_error                        (const char *text);
void              error_warning                      (const char *text);

#endif /* _ERRORS_H */
