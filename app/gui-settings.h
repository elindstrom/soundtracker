
/*
 * The Real SoundTracker - GUI configuration dialog (header)
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

#ifndef _ST_GUI_SETTINGS_H
#define _ST_GUI_SETTINGS_H

#include <gtk/gtk.h>

typedef struct gui_prefs {
    gchar tracker_line_format[10];
    gboolean tracker_hexmode;
    gboolean tracker_upcase;
    gboolean highlight_rows;
    int highlight_rows_n;
    int highlight_rows_minor_n;

    gboolean advance_cursor_in_fx_columns;
    gboolean asynchronous_editing;

    gboolean tempo_bpm_update;
    gboolean auto_switch;

    gboolean gui_display_scopes;
    gboolean gui_disable_splash;
    gboolean gui_use_backing_store;

    gboolean save_geometry;
    gboolean save_settings_on_exit;

    gboolean sharp;
    gboolean bh;

    int tracker_update_freq;
    int scopes_update_freq;
    int scopes_buffer_size;

    int st_window_x;
    int st_window_y;
    int st_window_w;
    int st_window_h;
    
    gboolean store_perm;
    guint32 permanent_channels;

    gchar loadmod_path[128];
    gchar savemod_path[128];
    gchar savemodaswav_path[128];
    gchar savesongasxm_path[128];
    gchar loadsmpl_path[128];
    gchar savesmpl_path[128];
    gchar loadinstr_path[128];
    gchar saveinstr_path[128];
    gchar loadpat_path[128];
    gchar savepat_path[128];

    gchar rm_path[128];
    gchar unzip_path[128];
    gchar lha_path[128];
    gchar gz_path[128];
    gchar bz2_path[128];

} gui_prefs;

extern gui_prefs gui_settings;

void        gui_settings_dialog                   (void);

void        gui_settings_load_config              (void);
void        gui_settings_save_config              (void);
void        gui_settings_save_config_always       (void);

void	    gui_settings_highlight_rows_changed       (GtkSpinButton *spin);
void	    gui_settings_highlight_rows_minor_changed (GtkSpinButton *spin);
#define ASYNCEDIT (gui_settings.asynchronous_editing)

#endif /* _ST_GUI_SETTINGS_H */
