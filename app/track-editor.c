
/*
 * The Real SoundTracker - track editor
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "i18n.h"
#include "track-editor.h"
#include "gui.h"
#include "st-subs.h"
#include "keys.h"
#include "audio.h"
#include "xm-player.h"
#include "main.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "preferences.h"
#include "tracker-settings.h"
#include "menubar.h"
#include "scope-group.h"

Tracker *tracker;
GtkWidget *trackersettings;
GtkWidget *vscrollbar;
ScopeGroup *scopegroup;

static GtkWidget *hscrollbar;

static XMPattern *pattern_buffer = NULL;
static XMNote *track_buffer = NULL;
static int track_buffer_length;

/* Block stuff */
static XMPattern block_buffer;

/* this array contains -1 if the note is not running, or the channel number where
   it is being played. this is necessary to handle the key on/off situation. */
static int note_running[96];

static int update_freq = 30;
static int gtktimer = -1;

static guint track_editor_editmode_status_idle_handler = 0;
static gchar track_editor_editmode_status_ed_buf[128];

/* jazz edit stuff */
static GtkWidget *jazzbox;
static GtkWidget *jazztable;
static GtkToggleButton *jazztoggles[32];
static int jazz_numshown = 0;
static int jazz_enabled = FALSE;

static void vscrollbar_changed(GtkAdjustment *adj);
static void hscrollbar_changed(GtkAdjustment *adj);
static void update_vscrollbar(Tracker *t, int patpos, int patlen, int disprows);
static void update_hscrollbar(Tracker *t, int leftchan, int numchans, int dispchans);
static void update_mainmenu_blockmark(Tracker *t, int state);
static gboolean track_editor_handle_column_input(Tracker *t, int gdkkey);

/* Note recording stuff */
static struct {
    guint32 key;
    int chn;
    gboolean act;
} reckey[32];
static gboolean insert_noteoff;

static gint
track_editor_editmode_status_idle_function (void)
{
#ifdef USE_GNOME
    gnome_appbar_set_status(GNOME_APPBAR(status_bar), track_editor_editmode_status_ed_buf);
#else
    gtk_statusbar_pop(GTK_STATUSBAR(status_bar), statusbar_context_id);
    gtk_statusbar_push(GTK_STATUSBAR(status_bar), statusbar_context_id, track_editor_editmode_status_ed_buf);
#endif

    gtk_idle_remove(track_editor_editmode_status_idle_handler);
    track_editor_editmode_status_idle_handler = 0;
    return TRUE;
}

void
show_editmode_status(void)
{
    Tracker *t = tracker;
    XMNote *note = &t->curpattern->channels[t->cursor_ch][t->patpos];
    gchar tmp_buf[128];
    int cmd_p1, cmd_p2;
    
    static const gchar *fx_commands[]={
        N_("Arpeggio"),                    /* 0 */
        N_("Porta up"),                    /* 1 */
        N_("Porta down"),                  /* 2 */
        N_("Tone porta"),                  /* 3 */
        N_("Vibrato"),                     /* 4 */
        N_("Tone porta + Volume slide"),   /* 5 */
        N_("Vibrato + Volume slide"),      /* 6 */
        N_("Tremolo"),                     /* 7 */
        N_("Set panning"),                 /* 8 */
        N_("Position jump"),               /* 9 */
        N_("Set volume"),                  /* A */
        N_("Position jump"),               /* B */
        N_("Set volume"),                  /* C */
        N_("Pattern break"),               /* D */
        NULL,                              /* E */
        N_("Set tempo/bpm"),               /* F */
        N_("Set global volume"),           /* G */
        N_("Global volume slide"),         /* H */
        NULL,                              /* I */
        NULL,                              /* J */
        N_("Key off"),                     /* K */
        N_("Set envelop position"),        /* L */
        NULL,                              /* M */
        NULL,                              /* N */
        NULL,                              /* O */
        N_("Panning slide"),               /* P */
        N_("LP filter resonance"),         /* Q */
        N_("Multi retrig note"),           /* R */
        NULL,                              /* S */
        N_("Tremor"),                      /* T */
        NULL,                              /* U */
        NULL,                              /* V */
        NULL,                              /* W */
        NULL,                              /* X */
        NULL,                              /* Y */
        N_("LP filter cutoff"),            /* Z */
    };

    static const gchar *e_fx_commands[]={
        NULL,                            /* 0 */
        N_("Fine porta up"),             /* 1 */
        N_("Fine porta down"),           /* 2 */
        N_("Set gliss control"),         /* 3 */
        N_("Set vibrato control"),       /* 4 */
        N_("Set finetune"),              /* 5 */
        N_("Set loop begin/loop"),       /* 6 */
        N_("Set tremolo control"),       /* 7 */
        NULL,                            /* 8 */
        N_("Retrig note"),               /* 9 */
        N_("Fine volume slide up"),      /* A */
        N_("Fine volume slide down"),    /* B */
        N_("Note cut"),                  /* C */
        N_("Note delay"),                /* D */
        N_("Pattern delay"),             /* E */
        NULL,                            /* F */
    };

    static const gchar *vol_fx_commands[]={
        N_("Volume slide down"),
        N_("Volume slide up"),
        N_("Fine volume slide down"),
        N_("Fine volume slide up"),
        N_("Set vibrato speed"),
        N_("Vibrato"),
        N_("Set panning"),
        N_("Panning slide left"),
        N_("Panning slide right"),
        N_("Tone porta"),
    };

    static const gchar *e47_fx_forms[]={
        N_("sine"),                      /* 0 */
        N_("ramp down"),                 /* 1 */
        N_("square"),                    /* 2 */
    };

    g_sprintf(track_editor_editmode_status_ed_buf, "[Chnn: %02d] [Pos: %03d] [Instr: %03d] ", t->cursor_ch+1,
                                                               t->patpos,
                                                               note->instrument);

    strcat(track_editor_editmode_status_ed_buf, "[Vol: ");
    
    switch(note->volume&0xf0)
    {
    case 0:
        strcat(track_editor_editmode_status_ed_buf, _("None"));
        break;
    case 0x10: case 0x20: case 0x30: case 0x40: case 0x50:
        strcat(track_editor_editmode_status_ed_buf, _("Set volume"));
        break;
    default:
        if(note->volume>0xf) {
            g_sprintf(tmp_buf,"%s", _(vol_fx_commands[((note->volume&0xf0)-0x60)>>4]));
            strcat(track_editor_editmode_status_ed_buf, tmp_buf);
        } else
            strcat(track_editor_editmode_status_ed_buf, _("None"));
        break;
    }

    if(note->volume&0xf0) {
        if(note->volume>=0x10 && note->volume<=0x50)
            g_sprintf(tmp_buf, " => %02d ] ", note->volume-0x10);
        else
            g_sprintf(tmp_buf, " => %02d ] ", note->volume&0xf);
    } else
         g_sprintf(tmp_buf, " ] ");
    strcat(track_editor_editmode_status_ed_buf, tmp_buf);
        
    memset(tmp_buf, 0, strlen(tmp_buf));
    strcat(track_editor_editmode_status_ed_buf, "[Cmd: ");

    /* enum values maybe for fx numbers ? */
    switch(note->fxtype)
    {
    case 0:
        if(note->fxparam)
            g_sprintf(tmp_buf, "%s", _(fx_commands[note->fxtype]));
        else
            g_sprintf(tmp_buf, _("None ]"));
        break;
    case 14:
        switch((note->fxparam&0xf0)>>4)
        {
        case 0: case 8: case 15:
            g_sprintf(tmp_buf, _("None ]"));
            break;
        default:
            g_sprintf(tmp_buf, "%s", _(e_fx_commands[(note->fxparam&0xf0)>>4]));
            break;
        }
        break;
    case 33:
        switch((note->fxparam&0xf0)>>4)
        {
        case 1:
            g_sprintf(tmp_buf, "Extra fine porta up");
            break;
        case 2:
            g_sprintf(tmp_buf, "Extra fine porta down");
            break;
        default:
            g_sprintf(tmp_buf, _("None ]"));
            break;
        }
        break;
    case 18: case 19: case 22: case 23: case 24:
    case 28: case 30: case 31: case 32: case 34:
        g_sprintf(tmp_buf, _("None ]"));
        break;

    default:
        g_sprintf(tmp_buf, "%s", _(fx_commands[note->fxtype]));
        break;
    }
    strcat(track_editor_editmode_status_ed_buf, tmp_buf);

    memset(tmp_buf, 0, strlen(tmp_buf));

    cmd_p1 = (note->fxparam&0xf0)>>4;
    cmd_p2 = note->fxparam&0xf;

    /* enum values maybe for fx numbers ? */
    switch(note->fxtype)
    {
    case 0:
        if(note->fxparam)
            g_sprintf(tmp_buf, " => %02d %02d ]", cmd_p1, cmd_p2);
        break;
    case 4:  case 7: case 10: case 17: case 25: case 27: case 29:
        g_sprintf(tmp_buf, " => %02d %02d ]", cmd_p1, cmd_p2);
        break;
    case 1: case 2: case 3: case 5: case 6: case 8: case 9:
    case 11: case 12: case 13: case 15: case 16: case 21: case 26: case 35:

        if(note->fxtype==15)
            if (note->fxparam<32)
                g_sprintf(tmp_buf, " => tempo: %02d ]", note->fxparam);
            else
                g_sprintf(tmp_buf, " => BPM: %03d ]", note->fxparam);
        else if(note->fxtype==9)
                g_sprintf(tmp_buf, " => offset: %d ]", note->fxparam<<8);
             else
                g_sprintf(tmp_buf, " => %03d ]", note->fxparam);

        break;
    case 14:
        if(cmd_p1!=0 && cmd_p1!=8 && cmd_p1!=15) {
            if((cmd_p1==4 || cmd_p1==7) && (cmd_p2<3))
                g_sprintf(tmp_buf, " => %02d (%s) ]", cmd_p2, e47_fx_forms[cmd_p2]);
            else
                g_sprintf(tmp_buf, " => %02d ]", cmd_p2);
        }
        break;
    case 33:
        if (cmd_p1==1 || cmd_p1==2)
            g_sprintf(tmp_buf, " => %02d ]", cmd_p2);
        break;
    default:
        g_sprintf(tmp_buf, "]");
        break;
    }
    strcat(track_editor_editmode_status_ed_buf, tmp_buf);

    if(!track_editor_editmode_status_idle_handler) {
	track_editor_editmode_status_idle_handler = gtk_idle_add(
	    (GtkFunction)track_editor_editmode_status_idle_function,
	    NULL);
	g_assert(track_editor_editmode_status_idle_handler != 0);
    }
}

void
track_editor_set_num_channels (int n)
{
    int i;

    tracker_set_num_channels(tracker, n);

    // Remove superfluous togglebuttons from table
    for(i = n; i < jazz_numshown; i++) {
	gtk_object_ref(GTK_OBJECT(jazztoggles[i]));
	gtk_container_remove(GTK_CONTAINER(jazztable), GTK_WIDGET(jazztoggles[i]));
    }

    // Resize table
    gtk_table_resize(GTK_TABLE(jazztable), 1, n);

    // Add new togglebuttons to table
    for(i = jazz_numshown; i < n; i++) {
	gtk_table_attach_defaults(GTK_TABLE(jazztable), GTK_WIDGET(jazztoggles[i]), i, i + 1, 0, 1);
    }

    jazz_numshown = n;
}

void
tracker_page_create (GtkNotebook *nb)
{
    GtkWidget *vbox, *hbox, *table, *thing;
    int i;

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);

    jazzbox = hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    thing = gtk_label_new(_("Jazz Edit:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    jazztable = table = gtk_table_new(1, 1, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);
    gtk_widget_show(table);

    for(i = 0; i < 32; i++) {
	char buf[10];
	g_sprintf(buf, "%02d", i+1);
	thing = gtk_toggle_button_new_with_label(buf);
	jazztoggles[i] = GTK_TOGGLE_BUTTON(thing);
	gtk_widget_show(thing);
    }

    table = gtk_table_new(2, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    gtk_widget_show(table);

    thing = tracker_new();
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 0, 1);
    gtk_widget_show(thing);
    g_signal_connect(thing, "patpos", G_CALLBACK(update_vscrollbar), NULL);
    g_signal_connect(thing, "xpanning", G_CALLBACK(update_hscrollbar), NULL);
    g_signal_connect(thing, "mainmenu_blockmark_set", G_CALLBACK(update_mainmenu_blockmark), NULL);
    tracker = TRACKER(thing);

    tracker_set_update_freq(gui_settings.tracker_update_freq);

    trackersettings = trackersettings_new();
    trackersettings_set_tracker_widget(TRACKERSETTINGS(trackersettings), tracker);

    hscrollbar = gtk_hscrollbar_new(NULL);
    gtk_table_attach(GTK_TABLE(table), hscrollbar, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
    gtk_widget_show(hscrollbar);

    vscrollbar = gtk_vscrollbar_new(NULL);
    gtk_table_attach(GTK_TABLE(table), vscrollbar, 1, 2, 0, 1, 0, GTK_FILL, 0, 0);
    gtk_widget_show(vscrollbar);

    for(i = 0; i < sizeof(note_running) / sizeof(note_running[0]); i++) {
        note_running[i] = -1;
    }

    gtk_notebook_append_page(nb, vbox, gtk_label_new(_("Tracker")));
    gtk_container_border_width(GTK_CONTAINER(vbox), 10);

    memset(&block_buffer, 0, sizeof(block_buffer));

#ifdef USE_GNOME
    /* Create popup menu */
    thing = gnome_popup_menu_new(track_editor_popup_menu);
    gnome_popup_menu_attach(thing, &tracker->widget, (gpointer)thing);
#endif
}

static void
update_vscrollbar (Tracker *t,
		   int patpos,
		   int patlen,
		   int disprows)
{
    gui_update_range_adjustment(GTK_RANGE(vscrollbar), patpos, patlen + disprows - 1,
				disprows, vscrollbar_changed);
}

static void
update_hscrollbar (Tracker *t,
		   int leftchan,
		   int numchans,
		   int dispchans)
{
    gui_update_range_adjustment(GTK_RANGE(hscrollbar), leftchan, numchans,
				dispchans, hscrollbar_changed);
}

static void
update_mainmenu_blockmark (Tracker *t,
			   int state)
{
    menubar_block_mode_set((gboolean)state);
}

static void
vscrollbar_changed (GtkAdjustment *adj)
{
    if(ASYNCEDIT || (gui_playing_mode != PLAYING_SONG && gui_playing_mode != PLAYING_PATTERN)) {
	gtk_signal_handler_block_by_func(GTK_OBJECT(tracker), GTK_SIGNAL_FUNC(update_vscrollbar), NULL);
	tracker_set_patpos(TRACKER(tracker), adj->value);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(tracker), GTK_SIGNAL_FUNC(update_vscrollbar), NULL);
    }
}

static void
hscrollbar_changed (GtkAdjustment *adj)
{
    tracker_set_xpanning(TRACKER(tracker), adj->value);
}

void
track_editor_toggle_jazz_edit (void)
{
    if(!jazz_enabled) {
	gtk_widget_show(jazzbox);
	jazz_enabled = TRUE;
    } else {
	gtk_widget_hide(jazzbox);
	jazz_enabled = FALSE;
    }
}

void
track_editor_toggle_insert_noteoff (void)
{
    if(!insert_noteoff)
	insert_noteoff = TRUE;
    else
	insert_noteoff = FALSE;
}



static gboolean
track_editor_is_channel_playing (int ch)
{
    /* Is there a note playing on channel `ch'? Used for jazz edit. */

    int i;

    for(i = 0; i < sizeof(note_running) / sizeof(note_running[0]); i++) {
	if(note_running[i] == ch) {
	    return TRUE;
	}
    }

    return FALSE;
}

static int
track_editor_find_next_jazz_channel (int current_channel,
				     int note)
{
    /* Jazz Edit Mode. The user has selected some channels that we can
       use. Go to the next one where no sample is currently
       playing. */

    int j, n;

    for(j = 0, n = (current_channel + 1) % xm->num_channels;
	j < xm->num_channels - 1;
	j++, n = (n+1) % xm->num_channels) {
	if(jazztoggles[n]->active && !track_editor_is_channel_playing(n)) {
	    return n;
	}
    }

    return current_channel;
}

void
track_editor_do_the_note_key (int notekeymeaning,
			      gboolean pressed,
			      guint32 xkeysym,
			      int modifiers)
{
    static int playchan = 0;

    int j, n;
    int note;

    note = notekeymeaning + 12 * gui_get_current_octave_value() + 1;

    if(!(note >= 0 && note < 96))
	return;

    if(pressed) {
	if(note_running[note] == -1) {
	    if(jazz_enabled) {
		if(GTK_TOGGLE_BUTTON(editing_toggle)->active) {
		    gui_play_note(tracker->cursor_ch, note);
		    note_running[note] = tracker->cursor_ch;
		    n = track_editor_find_next_jazz_channel(tracker->cursor_ch, note);
		    tracker_step_cursor_channel(tracker, n - tracker->cursor_ch);
		} else {
		    playchan = track_editor_find_next_jazz_channel(playchan, note);
		    gui_play_note(playchan, note);
		    note_running[note] = playchan;
		}
	    } else {
		gui_play_note(tracker->cursor_ch, note);
		note_running[note] = tracker->cursor_ch;
	    }
	}
    } else {
	if(note_running[note] != -1) {
	    if(keys_is_key_pressed(xkeysym, modifiers)) {
		/* this is just an auto-repeat fake keyoff. pooh.
		   in reality this key is still being held down */
		return;
	    }
	    for(j = 0, n = 0; j < sizeof(note_running) / sizeof(note_running[0]); j++) {
		n += (note_running[j] == note_running[note]);
	    }
	    if(n == 1) {
	        // only do the keyoff if all previous keys are off.
		gui_play_note_keyoff(note_running[note]);
	    }
	    note_running[note] = -1;
	}
    }
}

gboolean
track_editor_handle_keys (int shift,
			  int ctrl,
			  int alt,
			  guint32 keyval,
			  gboolean pressed)
{
    static int current_channel = -1;

    int c, i, m, tip;
    Tracker *t = tracker;
    gboolean handled = FALSE;
    
    m = i = keys_get_key_meaning(keyval, ENCODE_MODIFIERS(shift, ctrl, alt));
    tip = KEYS_MEANING_TYPE(i);

    if(t->cursor_item == 0 &&  tip <= KEYS_MEANING_KEYOFF) {
	if(i != -1) {
	    switch(tip) {
	    case KEYS_MEANING_NOTE:
		i += 12 * gui_get_current_octave_value() + 1;
		if(i < 96) {
		    
		    if(!GTK_TOGGLE_BUTTON(editing_toggle)->active)
			goto fin_note;
		    
		    if(!jazztoggles[tracker->cursor_ch]->active && jazz_enabled) {
			int n = track_editor_find_next_jazz_channel(tracker->cursor_ch, m + 12 * 
			gui_get_current_octave_value() + 1);
			tracker_step_cursor_channel(tracker, n - tracker->cursor_ch);
		    }
		    
                    if(!GUI_ENABLED && !ASYNCEDIT) { // Recording mode 
                        if(pressed){ // Insert note
			    
                            for(c = 0; c < 32; c++){ // Cleanup
                                if(!reckey[c].act) 
                                    continue;
                                if(reckey[c].key == keyval)
                                   goto fin_note; // Key is allready down
                                else if(reckey[c].chn == t->cursor_ch)
                                   reckey[c].act = 0; // There can be only one sound per channel
                            }
                            
                            // Find free reckey
                            for(c = 0; c < 32; c++)
                                if(!reckey[c].act)
                                    break;
                                                        
                            // Fill in the reckey
                            reckey[c].key = keyval;
                            reckey[c].chn = t->cursor_ch;
                            reckey[c].act = TRUE;
                            
                            XMNote *note = &t->curpattern->channels[t->cursor_ch][t->patpos];
                            note->note = i;
                            note->instrument = gui_get_current_instrument();
                            tracker_redraw_current_row(t);
                            xm->modified = 1;
			    
                        } else if(!keys_is_key_pressed(keyval, 
				  ENCODE_MODIFIERS(shift, ctrl, alt))) { // Release key
			    
                           // Find right reckey
                           for(c = 0; c < 32; c++)
                               if(reckey[c].act && reckey[c].key == keyval)
                                   break;
                           if(c == 32)
                               goto fin_note; // Key was released by other means
			   
                           reckey[c].act = FALSE;
                           
                           if (!insert_noteoff)
                               goto fin_note;
                           
                           XMNote *note = &t->curpattern->channels[reckey[c].chn][t->patpos];
                           note->note = 97;
                           note->instrument = 0;
                           tracker_redraw_current_row(t);
                           xm->modified = 1; 
                        }
                    } else if (pressed) {
			
			XMNote *note = &t->curpattern->channels[t->cursor_ch][t->patpos];
			note->note = i;
			note->instrument = gui_get_current_instrument();
			tracker_redraw_current_row(t);
			tracker_step_cursor_row(t, gui_get_current_jump_value());
			xm->modified = 1;
		    }
fin_note:
		    track_editor_do_the_note_key(m, pressed, keyval, ENCODE_MODIFIERS(shift, ctrl, alt));
		}
		break;
	    case KEYS_MEANING_KEYOFF:
		if(pressed && GTK_TOGGLE_BUTTON(editing_toggle)->active) {
		    XMNote *note = &t->curpattern->channels[t->cursor_ch][t->patpos];
		    note->note = 97;
		    note->instrument = 0;
		    tracker_redraw_current_row(t);
		    tracker_step_cursor_row(t, gui_get_current_jump_value());
		    xm->modified = 1;
		}
		break;
	    }
	    if (GTK_TOGGLE_BUTTON(editing_toggle)->active) show_editmode_status();
	    return TRUE;
	}
    }

    if(!pressed){
	if (tip == KEYS_MEANING_CH){
	    current_channel = -1;
	    return TRUE;
	}
	return FALSE;
    }

    switch (tip){
    case KEYS_MEANING_FUNC:
	m = gui_get_current_jump_value ();
	if ((m += KEYS_MEANING_VALUE(i) ? -1 : 1)  <= 16 &&
	     m >= 0) gui_set_jump_value (m);
	return TRUE;
    case KEYS_MEANING_CH:
	tracker_set_cursor_channel (t, current_channel = KEYS_MEANING_VALUE(i));
	if (GTK_TOGGLE_BUTTON(editing_toggle)->active) show_editmode_status();
	return TRUE;
    }

    switch (keyval) {
    case GDK_Up:
	if((GUI_ENABLED || ASYNCEDIT) && !ctrl && !shift && !alt) {
	    tracker_set_patpos(t, t->patpos > 0 ? t->patpos - 1 : t->curpattern->length - 1);
	    handled = TRUE;
	}
	break;
    case GDK_Down:
	if((GUI_ENABLED || ASYNCEDIT) && !ctrl && !shift && !alt) {
	    tracker_set_patpos(t, t->patpos < t->curpattern->length - 1 ? t->patpos + 1 : 0);
	    handled = TRUE;
	}
	break;
    case GDK_Page_Up:
	if(!GUI_ENABLED && !ASYNCEDIT)
	    break;
	if(t->patpos >= 16)
	    tracker_set_patpos(t, t->patpos - 16);
	else
	    tracker_set_patpos(t, 0);
	handled = TRUE;
	break;
    case GDK_Page_Down:
	if(!GUI_ENABLED && !ASYNCEDIT)
	    break;
	if(t->patpos < t->curpattern->length - 16)
	    tracker_set_patpos(t, t->patpos + 16);
	else
	    tracker_set_patpos(t, t->curpattern->length - 1);
	handled = TRUE;
	break;
    case GDK_F9:
    case GDK_Home:
	if(GUI_ENABLED || ASYNCEDIT) {
	    tracker_set_patpos(t, 0);
	    handled = TRUE;
	}
	break;
    case GDK_F10:
	if(GUI_ENABLED || ASYNCEDIT) {
	    tracker_set_patpos(t, t->curpattern->length/4);
	    handled = TRUE;
	}
	break;
    case GDK_F11:
	if(GUI_ENABLED || ASYNCEDIT) {
	    tracker_set_patpos(t, t->curpattern->length/2);
	    handled = TRUE;
	}
	break;
    case GDK_F12:
	if(GUI_ENABLED || ASYNCEDIT) {
	    tracker_set_patpos(t, 3*t->curpattern->length/4);
	    handled = TRUE;
	}
	break;
    case GDK_End:
	if(GUI_ENABLED || ASYNCEDIT) {
	    tracker_set_patpos(t, t->curpattern->length - 1);
	    handled = TRUE;
	}
	break;
    case GDK_Left:
	if(!shift && !ctrl && !alt) {
	    /* cursor left */
	    tracker_step_cursor_item(t, -1);
	    handled = TRUE;
	}
	break;
    case GDK_Right:
	if(!shift && !ctrl && !alt) {
	    /* cursor right */
	    tracker_step_cursor_item(t, 1);
	    handled = TRUE;
	}
	break;
    case GDK_Tab:
    case GDK_ISO_Left_Tab:
	tracker_step_cursor_channel(t, shift ? -1 : 1);
	handled = TRUE;
	break;
/*    case GDK_Shift_R:
	play_song();
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), TRUE);
	tracker_redraw(tracker);
	handled = TRUE;
        break;*/
    case GDK_Delete:
	if(GTK_TOGGLE_BUTTON(editing_toggle)->active) {
	    XMNote *note = &t->curpattern->channels[t->cursor_ch][t->patpos];

            if(shift) {
		note->note = 0;
		note->instrument = 0;
		note->volume = 0;
		note->fxtype = 0;
		note->fxparam = 0;
            } else if(ctrl) {
		note->volume = 0;
		note->fxtype = 0;
		note->fxparam = 0;
            } else if(alt) {
		note->fxtype = 0;
		note->fxparam = 0;
            } else {
                switch(t->cursor_item) {
                case 0: case 1: case 2:
                    note->note = 0;
                    note->instrument = 0;
                    break;
                case 3: case 4:
                    note->volume = 0;
                    break;
                case 5: case 6: case 7:
                    note->fxtype = 0;
                    note->fxparam = 0;
                    break;
                default:
                    g_assert_not_reached();
                    break;
                }
            }

	    tracker_redraw_current_row(t);
	    tracker_step_cursor_row(t, gui_get_current_jump_value());
	    xm->modified = 1;
	    handled = TRUE;
	}
	break;
    case GDK_Insert:
	if(GTK_TOGGLE_BUTTON(editing_toggle)->active && !shift && !alt && !ctrl) {
	    XMNote *note = &t->curpattern->channels[t->cursor_ch][t->patpos];

	    for(i = t->curpattern->length - 1; i>t->patpos; --i)
		t->curpattern->channels[t->cursor_ch][i] = t->curpattern->channels[t->cursor_ch][i-1];

	    note->note = 0;
	    note->instrument = 0;
	    note->volume = 0;
	    note->fxtype = 0;
	    note->fxparam = 0;

	    tracker_redraw_current_row(t);
	    xm->modified = 1;
	    handled = TRUE;
        }
        break;
    case GDK_BackSpace:
	if(GTK_TOGGLE_BUTTON(editing_toggle)->active) {
	    XMNote *note;

	    if(t->patpos) {
		--t->patpos;
		for(i = t->patpos; i<t->curpattern->length-1; i++)
		    t->curpattern->channels[t->cursor_ch][i] = t->curpattern->channels[t->cursor_ch][i+1];
		
		note = &t->curpattern->channels[t->cursor_ch][t->curpattern->length - 1];
		note->note = 0;
		note->instrument = 0;
		note->volume = 0;
		note->fxtype = 0;
		note->fxparam = 0;
		
		tracker_redraw_current_row(t);
		xm->modified = 1;
		handled = TRUE;
	    }
        }
        break;

    case GDK_Escape:
       if (shift){
           tracker_set_cursor_item (t, 0);
           handled = TRUE;
       }
       break;

    case ' ':
	if (current_channel >= 0){
	    if (current_channel > (t->num_channels - 1))
		current_channel = t->num_channels - 1;
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(scopegroup->scopebuttons[current_channel]),
					 !GTK_TOGGLE_BUTTON(scopegroup->scopebuttons[current_channel])->active);
	    handled = TRUE;
	}
	break;

    default:
	if(!ctrl && !alt) {
	    if(GTK_TOGGLE_BUTTON(editing_toggle)->active) {
		handled = track_editor_handle_column_input(t, keyval);
	    }
	}
	break;
    }

    if (GTK_TOGGLE_BUTTON(editing_toggle)->active && handled) show_editmode_status();

    return handled;
}

void
track_editor_copy_pattern (Tracker *t)
{
    XMPattern *p = t->curpattern;

    if(pattern_buffer) {
	st_free_pattern_channels(pattern_buffer);
	free(pattern_buffer);
    }
    pattern_buffer = st_dup_pattern(p);
    tracker_redraw(t);
}

void
track_editor_cut_pattern (Tracker *t)
{
    XMPattern *p = t->curpattern;

    if(pattern_buffer) {
	st_free_pattern_channels(pattern_buffer);
	free(pattern_buffer);
    }
    pattern_buffer = st_dup_pattern(p);
    st_clear_pattern(p);
    xm->modified = 1;
    tracker_redraw(t);
}

void
track_editor_paste_pattern (Tracker *t)
{
    XMPattern *p = t->curpattern;
    int i;

    if(!pattern_buffer)
	return;
    for(i = 0; i < 32; i++) {
	free(p->channels[i]);
	p->channels[i] = st_dup_track(pattern_buffer->channels[i], pattern_buffer->length);
    }
    p->alloc_length = pattern_buffer->length;
    if(p->length != pattern_buffer->length) {
	p->length = pattern_buffer->length;
	gui_update_pattern_data();
	tracker_reset(t);
    } else {
	tracker_redraw(t);
    }
    xm->modified = 1;
}

void
track_editor_copy_track (Tracker *t)
{
    int l = t->curpattern->length;
    XMNote *n = t->curpattern->channels[t->cursor_ch];

    if(track_buffer) {
	free(track_buffer);
    }
    track_buffer_length = l;
    track_buffer = st_dup_track(n, l);
    tracker_redraw(t);
}

void
track_editor_cut_track (Tracker *t)
{
    int l = t->curpattern->length;
    XMNote *n = t->curpattern->channels[t->cursor_ch];

    if(track_buffer) {
	free(track_buffer);
    }
    track_buffer_length = l;
    track_buffer = st_dup_track(n, l);
    st_clear_track(n, l);
    xm->modified = 1;
    tracker_redraw(t);
}

void
track_editor_paste_track (Tracker *t)
{
    int l = t->curpattern->length;
    XMNote *n = t->curpattern->channels[t->cursor_ch];
    int i;

    if(!track_buffer)
	return;
    i = track_buffer_length;
    if(l < i)
	i = l;
    while(i--)
	n[i] = track_buffer[i];
    xm->modified = 1;
    tracker_redraw(t);
}

void
track_editor_delete_track (Tracker *t)
{
    st_pattern_delete_track(t->curpattern, t->cursor_ch);
    xm->modified = 1;
    tracker_redraw(t);
}

void
track_editor_insert_track (Tracker *t)
{
    st_pattern_insert_track(t->curpattern, t->cursor_ch);
    xm->modified = 1;
    tracker_redraw(t);
}

void
track_editor_kill_notes_track (Tracker *t)
{
    int i;
    XMNote *note;

    for(i = t->patpos; i<t->curpattern->length; i++) {
       note = &t->curpattern->channels[t->cursor_ch][i];
       note->note = 0;
       note->instrument = 0;
       note->volume = 0;
       note->fxtype = 0;
       note->fxparam = 0;
    }

    xm->modified = 1;
    tracker_redraw(t);
}

void
track_editor_cmd_mvalue (Tracker *t, gboolean mode)
{
    XMNote *note;
    int nparam, tpos;

    tpos = t->cursor_item;

    if(tpos>=3) {
        note = &t->curpattern->channels[t->cursor_ch][(t->patpos - gui_get_current_jump_value()) % t->curpattern->length];

        if(tpos<5)
            nparam = note->volume & 0xf;
        else
            nparam = note->fxparam;

        if(mode==TRUE)
            nparam = (nparam + 1) & 0xff;
        else
            nparam = (nparam - 1) & 0xff;

        note = &t->curpattern->channels[t->cursor_ch][t->patpos];
        if(tpos<5)
            note->volume |= nparam & 0xf;
        else
            note->fxparam = nparam;

        tracker_step_cursor_row(t, gui_get_current_jump_value());
        xm->modified = 1;
        tracker_redraw(t);
    }
}

void
track_editor_mark_selection (Tracker *t)
{
    tracker_mark_selection(t, TRUE);
}

void
track_editor_clear_mark_selection (Tracker *t)
{
    tracker_clear_mark_selection(t);
    menubar_block_mode_set(FALSE);
}

static void
track_editor_copy_cut_selection_common (Tracker *t,
					gboolean cut)
{
    int i;
    int height, width, chStart, rowStart;

    if(!tracker_is_valid_selection(t))
	return;

    if(tracker_is_in_selection_mode(t))
	tracker_mark_selection(t, FALSE);

    tracker_get_selection_rect(t, &chStart, &rowStart, &width, &height);

    block_buffer.alloc_length = block_buffer.length = height;

    for(i = 0; i < 32; i++) {
	free(block_buffer.channels[i]);
	block_buffer.channels[i] = NULL;
    }

    for(i = 0; i < width; i++) {
	block_buffer.channels[i] = st_dup_track_wrap(t->curpattern->channels[(chStart + i) % xm->num_channels],
						     t->curpattern->length,
						     rowStart,
						     height);
	if(cut) {
	    st_clear_track_wrap(t->curpattern->channels[(chStart + i) % xm->num_channels],
				t->curpattern->length,
				rowStart,
				height);
	}
    }
}

void
track_editor_copy_selection (Tracker *t)
{
    track_editor_copy_cut_selection_common(t, FALSE);
    menubar_block_mode_set(FALSE);
}

void
track_editor_cut_selection (Tracker *t)
{
    track_editor_copy_cut_selection_common(t, TRUE);
    menubar_block_mode_set(FALSE);
    xm->modified = 1;
    tracker_redraw(t);
}

void
track_editor_paste_selection (Tracker *t)
{
    int i;

    if(block_buffer.length > t->curpattern->length)
		return;

    for(i = 0; i < 32; i++) {
		st_paste_track_into_track_wrap(block_buffer.channels[i],
				       t->curpattern->channels[(t->cursor_ch + i) % xm->num_channels],
				       t->curpattern->length,
				       t->patpos,
				       block_buffer.length);
    }

    xm->modified = 1;
 	/* I'm not sure if it's a good idea (Olivier GLORIEUX) */
    tracker_set_patpos(t, (t->patpos + block_buffer.length) % t->curpattern->length);
    tracker_redraw(t);
}

void
track_editor_interpolate_fx (Tracker *t)
{
    int height, width, chStart, rowStart;
    int xmnote_offset;
    guint8 xmnote_mask;
    XMNote *note_start, *note_end;
    int i;
    int dy;
    int start_value, start_char;

    if(!tracker_is_valid_selection(t))
	return;

    tracker_get_selection_rect(t, &chStart, &rowStart, &width, &height);
    if(width != 1 || t->cursor_ch != chStart)
	return;

    note_start = &t->curpattern->channels[t->cursor_ch][rowStart];
    note_end = &t->curpattern->channels[t->cursor_ch][rowStart + height - 1];

    if(t->cursor_item == 3 || t->cursor_item == 4) {
	// Interpolate volume column
	xmnote_offset = (void*)(&note_start->volume) - (void*)note_start;

	switch(note_start->volume & 0xf0) {
	case 0x10: case 0x20: case 0x30: case 0x40: case 0x50:
	    if((note_end->volume & 0xf0) < 0x10 || (note_end->volume & 0xf0) > 0x50)
		return;
	    xmnote_mask = 0xff;
	    break;
//	case 0xc0:
//	    if((note_end->volume & 0xf0) != 0xc0)
//		return;
//	    xmnote_mask = 0x0f;
//	    break;
	default:
	    if((note_end->volume & 0xf0) != (note_start->volume & 0xf0))
		return;
	    /* let's do at least _some_thing */
	    xmnote_mask = 0x0f;
	    break;
	}

    } else if(t->cursor_item >= 5) {
	// Interpolate effects column
	xmnote_offset = (void*)&note_start->fxparam - (void*)note_start;

	if(note_start->fxtype != note_end->fxtype)
	    return;

	switch(note_start->fxtype) {
	    // The Axx for example needs special treatment here
//	case 0xc: case 'Z'-'A'+10:
//	    xmnote_mask = 0xff;
//	    break;
	default:
	    /* let's do at least _some_thing */
	    xmnote_mask = 0xff;
	    break;
	}

	for(i = 1; i < height - 1; i++) {
	    // Skip lines that allready have effect on them
	    if((note_start + i)->fxtype)
		continue;

	    // Copy the effect type into all rows in between
	    (note_start + i)->fxtype = note_start->fxtype;
	}

    } else {
	return;
    }

    /* Bit-fiddling coming up... */

    dy = *((guint8*)(note_end) + xmnote_offset);
    dy &= xmnote_mask;
    start_char = *((guint8*)(note_start) + xmnote_offset);
    start_value = start_char & xmnote_mask;
    dy -= start_value;

    for(i = 1; i < height - 1; i++) {
	int new_value;

        // On effect interpolation, skip lines that allready contain different effects
        if(t->cursor_item >= 5 && (note_start + i)->fxtype != note_start->fxtype)
            continue;

	new_value = start_value + (int)((float)i * dy / (height - 1) + (dy >= 0 ? 1.0 : -1.0) * 0.5);
	new_value &= xmnote_mask;
	new_value |= (start_char & ~xmnote_mask);

	*((guint8*)(note_start + i) + xmnote_offset) = new_value;
    }

    tracker_redraw(t);
}

static void
track_editor_handle_semidec_column_input (Tracker *t,
					  int exp,
					  gint8 *modpt,
					  int n)
{
    switch(exp) {
    case 0:
	if(n < 0 || n > 9)
	    return;
	*modpt = (*modpt / 10) * 10 + n;
	break;
    case 1:
	if(n < 0 || n > 26)
	    return;
	*modpt = (*modpt % 10) + 10 * n;
	break;
    }

    tracker_redraw_current_row(t);
    if(!gui_settings.advance_cursor_in_fx_columns)
	tracker_step_cursor_row(t, gui_get_current_jump_value());
    else
	tracker_step_cursor_item(t, 1);
    xm->modified = 1;
}

static void
track_editor_handle_hex_column_input (Tracker *t,
				      int exp,
				      gint8 *modpt,
				      int n)
{
    int s;

    if(n < 0 || n > 15)
	return;

    exp *= 4;
    s = *modpt & (0xf0 >> exp);
    s |= n << exp;
    *modpt = s;
    tracker_redraw_current_row(t);

    if(!gui_settings.advance_cursor_in_fx_columns) {
	tracker_step_cursor_row(t, gui_get_current_jump_value());
    } else {
         if(exp) { /* not at the end */
	     tracker_step_cursor_item(t, 1);
         } else {
	     tracker_step_cursor_row(t, gui_get_current_jump_value());
	     tracker_step_cursor_item(t, -1);
	     if(t->cursor_item == 6) /* -2 in case of fx col */
		 tracker_step_cursor_item(t, -1);
	 }
    }

    xm->modified = 1;
}

static gboolean
track_editor_handle_column_input (Tracker *t,
				  int gdkkey)
{
    int n;
    XMNote *note = &t->curpattern->channels[t->cursor_ch][t->patpos];

    if(t->cursor_item == 5) {
	/* Effect column (not the parameter) */
	switch(gdkkey) {
	case '0' ... '9':
	    n = gdkkey - '0';
	    break;
	case 'a' ... 'z': case 'A' ... 'Z':
	    gdkkey = tolower(gdkkey);
	    n = gdkkey - 'a' + 10;
	    break;
	default:
	    return FALSE;
	}
	note->fxtype = n;
	tracker_redraw_current_row(t);
	if(!gui_settings.advance_cursor_in_fx_columns)
	    tracker_step_cursor_row(t, gui_get_current_jump_value());
	else
	    tracker_step_cursor_item(t, 1);
	xm->modified = 1;
	return TRUE;
    }

    gdkkey = tolower(gdkkey);
    n = gdkkey - '0' - (gdkkey >= 'a') * ('a' - '9' - 1);

    switch(t->cursor_item) {
    case 1: case 2: /* instrument column */
	track_editor_handle_semidec_column_input(t, 2 - t->cursor_item, &note->instrument, n);
	break;
    case 3: case 4: /* volume column */
	track_editor_handle_hex_column_input(t, 4 - t->cursor_item, &note->volume, n);
	break;
    case 6: case 7: /* effect parameter */
	track_editor_handle_hex_column_input(t, 7 - t->cursor_item, &note->fxparam, n);
	break;
    default:
	return FALSE;
    }

    return TRUE;
}

static gint
tracker_timeout (gpointer data)
{
    double display_songtime;
    audio_player_pos *p;

    if(current_driver_object == NULL) {
	/* Can happen when audio thread stops on its own. Note that
	 * tracker_stop_updating() is called in
	 * gui.c::read_mixer_pipe(). */
	return TRUE;
    }

    display_songtime = current_driver->get_play_time(current_driver_object);

    p = time_buffer_get(audio_playerpos_tb, display_songtime);
    if(p) {
	gui_update_player_pos(p);
    }
    return TRUE;
}

void
tracker_start_updating (void)
{
    if(gtktimer != -1)
	return;

    gtktimer = gtk_timeout_add(1000/update_freq, tracker_timeout, NULL);
}

void
tracker_stop_updating (void)
{
    if(gtktimer == -1)
	return;

    gtk_timeout_remove(gtktimer);
    gtktimer = -1;
}

void
tracker_set_update_freq (int freq)
{
    update_freq = freq;
    if(gtktimer != -1) {
	tracker_stop_updating();
	tracker_start_updating();
    }
}

void
track_editor_load_config (void)
{
    char buf[256];
    prefs_node *f;
    int i, j;

    f = prefs_open_read("jazz");
    if(f) {
	for(i = 0; i < 32; i++) {
	    g_sprintf(buf, "jazz-toggle-%d", i);
	    prefs_get_int(f, buf, &j);
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(jazztoggles[i]), j);
	}

	prefs_close(f);
    }
}

void
track_editor_save_config (void)
{
    char buf[256];
    prefs_node *f;
    int i;

    if(gui_settings.save_settings_on_exit) {
	trackersettings_write_settings();
    }

    f = prefs_open_write("jazz");
    if(!f)
	return;

    for(i = 0; i < 32; i++) {
	g_sprintf(buf, "jazz-toggle-%d", i);
	prefs_put_int(f, buf, GTK_TOGGLE_BUTTON(jazztoggles[i])->active);
    }

    prefs_close(f);
}

void
track_editor_toggle_permanentness (Tracker *t, gboolean all)
{
    int i = t->cursor_ch;

    if(!all)
	gui_settings.permanent_channels ^= 1 << i;
    else
	gui_settings.permanent_channels = gui_settings.permanent_channels ? 0 : 0xFFFFFFFF;
    
    tracker_redraw(t);
}
