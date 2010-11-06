/*
 * Copyright (C) 2000 Luc Tanguay <luc.tanguay@bell.ca>
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

#ifndef _MIDI_SETTINGS_H
#define _MIDI_SETTINGS_H

#include <config.h>

#if defined(DRIVER_ALSA_050) || defined(DRIVER_ALSA_09x)

#include <glib.h>

/* Macro to check if debug is activated */

#define IS_MIDI_DEBUG_ON (midi_settings.misc.debug_level > (MIDI_DEBUG_OFF))

/*** Structures ***/

/*
 * MIDI settings in configuration file .soundtracker/midi.
 * Split into different set of variables: one set for input parameters,
 * one for output (future development) and one for miscellaneous parms.
 */

/* Debug level */

enum {
  MIDI_DEBUG_OFF = 0,
  MIDI_DEBUG_BASIC = 1,
  MIDI_DEBUG_HIGHEST
};

/* Input preferences (mostly ALSA specifics) */

typedef struct {
  gboolean auto_connect;
  gint client;
  gint port;
  gboolean channel_enabled;
  gboolean volume_enabled;
} midi_input_prefs;

/* Output preferences (mostly ALSA specifics) */

typedef struct {
  gint client;
  gint port;
} midi_output_prefs;

/* Misc. preferences */

typedef struct {
  gint debug_level; /* one of the MIDI_DEBUG_... enum value */
} midi_misc_prefs;

typedef struct {
  midi_input_prefs input;
  midi_output_prefs output;
  midi_misc_prefs misc;
} midi_prefs;

/* Exported variables */

extern midi_prefs midi_settings;

/* Function prototypes */

void midi_load_config(void);
void midi_save_config(void);

void midi_settings_dialog(void);

#else

static inline void midi_settings_dialog(void)
{
}

#endif

#endif /* _MIDI_SETTINGS_H */
