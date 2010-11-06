
/*
 * The Real SoundTracker - error handling functions
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

#include <unistd.h>
#include <string.h>

#include "audio.h"

static void
error_error_common (int level,
		  const char *text)
{
    // This uses the mixer backpipe (see decls in audio.h) to collect
    // all errors and warnings in one central place. Decls from
    // audio.h should probably be put into a more general place.
    int l;
    extern int pipeb[2];
    write(pipeb[1], &level, sizeof(level));
    l = strlen(text);
    write(pipeb[1], &l, sizeof(l));
    write(pipeb[1], text, l + 1);
}

void
error_error (const char *text)
{
    error_error_common(AUDIO_BACKPIPE_ERROR_MESSAGE, text);
}

void
error_warning (const char *text)
{
    error_error_common(AUDIO_BACKPIPE_WARNING_MESSAGE, text);
}

