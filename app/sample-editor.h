
/*
 * The Real SoundTracker - sample editor (header)
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

#ifndef _SAMPLE_EDITOR_H
#define _SAMPLE_EDITOR_H

#include <gtk/gtk.h>
#include "xm.h"
#include "driver-inout.h"

void         sample_editor_page_create               (GtkNotebook *nb);

gboolean     sample_editor_handle_keys               (int shift,
						      int ctrl,
						      int alt,
						      guint32 keyval,
						      gboolean pressed);

void         sample_editor_set_sample                (STSample *);
void         sample_editor_update                    (void);

void         sample_editor_stop_sampling             (void);

void         sample_editor_start_updating            (void);
void         sample_editor_stop_updating             (void);

void	     sample_editor_copy_cut_common	     (gboolean copy, gboolean spliceout);
void	     sample_editor_paste_clicked	     (void);

extern st_io_driver *sampling_driver;
extern void *sampling_driver_object;

#endif /* _SAMPLE_EDITOR_H */
