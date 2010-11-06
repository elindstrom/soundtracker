
/*
 * The Real SoundTracker - Output driver module definitions
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

#ifndef _ST_DRIVER_OUT_H
#define _ST_DRIVER_OUT_H

#include <glib.h>
#include <gdk/gdktypes.h>

#include "driver.h"

typedef struct st_out_driver {
    st_driver common;

    // get time offset since first sound output
    double   (*get_play_time) (void *d);
} st_out_driver;

/* Install / remove poll() handlers, similar to gdk_input_add() */
gpointer audio_poll_add       (int fd,
			       GdkInputCondition cond,
			       GdkInputFunction func,
			       gpointer data);
void     audio_poll_remove    (gpointer poll);

/* Called by the driver to indicate that it accepts new data */
void     audio_play           (void);

/* Called by the playing driver to indicate that the samples belonging
   to the specified time are about to be played immediately. (This is
   used to synchronize the oscilloscopes) */
void     audio_time           (double);

/* Called by the sampling driver to deliver new data to the audio
   subsystem.  This API is going to be changed, though (the current
   one doesn't permit data other than 16bit/44.1kHz */
void     audio_sampled        (gint16 *data,
			       int count);

void     audio_mix            (void *dest,
			       guint32 count,
			       int mixfreq,
			       int mixformat);

#endif /* _ST_DRIVER_OUT_H */
