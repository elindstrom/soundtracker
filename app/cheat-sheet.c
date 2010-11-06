
/*
 * The Real SoundTracker - XM effects cheat sheet
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

#define GTK_ENABLE_BROKEN
#include <config.h>

#include <gtk/gtktext.h>
#include <gtk/gtk.h>
#ifdef USE_GNOME
#include <gnome.h>
#endif

#include "i18n.h"

static GtkWidget *cheat_sheet_window = NULL;

static const char cheat_sheet_string[] =
"\n"
" Standard Effects Column            Volume Column\n"
" ---------------------------------------------------------------------\n"
"\n"
" 0      Arpeggio                     0       Do nothing\n"
" 1  (*) Porta up                   $10-$50   Set volume Value-$10\n"
" 2  (*) Porta down                   :          :        :\n"
" 3  (*) Tone porta                   :          :        :\n"
" 4  (*) Vibrato                    $60-$6f   Volume slide down\n"
" 5  (*) Tone porta+Volume slide    $70-$7f   Volume slide up\n"
" 6  (*) Vibrato+Volume slide       $80-$8f   Fine volume slide down\n"
" 7  (*) Tremolo                    $90-$9f   Fine volume slide up\n"
" 8      Set panning                $a0-$af   Set vibrato speed\n"
" 9      Sample offset              $b0-$bf   Vibrato\n"
" A  (*) Volume slide               $c0-$cf   Set panning\n"
" B      Position jump              $d0-$df   Panning slide left\n"
" C      Set volume                 $e0-$ef   Panning slide right\n"
" D      Pattern break              $f0-$ff   Tone porta\n"
" E1 (*) Fine porta up\n"
" E2 (*) Fine porta down            (*) = If the data byte is zero,\n"
" E3     Set gliss control                the last nonzero byte for the\n"
" E4     Set vibrato control              command should be used.\n"
" E5     Set finetune\n"
" E6     Set loop begin/loop\n"
" E7     Set tremolo control        ===================================\n"
" E9     Retrig note\n"
" EA (*) Fine volume slide up       Non-Standard XM effects - only\n"
" EB (*) Fine volume slide down     available in SoundTracker and\n"
" EC     Note cut                   OpenCP - don't use if you want to\n"
" ED     Note delay                 stay compatible with FastTracker.\n"
" EE     Pattern delay              -----------------------------------\n"
" F      Set tempo/BPM\n"
" G      Set global volume          Zxx  Set LP filter cutoff frequency\n"
" H  (*) Global volume slide        Qxx  Set LP filter resonance\n"
" K      Key off\n"
" L      Set envelope position      To switch off the filter, you must\n"
" P  (*) Panning slide              usw Q00 _and_ Zff!.\n"
" R  (*) Multi retrig note\n"
" T      Tremor\n"
" X1 (*) Extra fine porta up\n"
" X2 (*) Extra fine porta down";

static void
cheat_sheet_close_requested (void)
{
    gtk_widget_destroy(cheat_sheet_window);
    cheat_sheet_window = NULL;
}

void
cheat_sheet_dialog (void)
{
    GtkWidget *mainbox, *scrolled_window, *text, *hbox, *thing;
    GdkFont *font;

    if(cheat_sheet_window != NULL) {
	gdk_window_raise(cheat_sheet_window->window);
	return;
    }

#ifdef USE_GNOME
    cheat_sheet_window = gnome_app_new("SoundTracker", _("XM Effects Cheat Sheet"));
#else
    cheat_sheet_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(cheat_sheet_window), _("XM Effects Cheat Sheet"));
#endif
    g_signal_connect (GTK_OBJECT (cheat_sheet_window), "delete_event",
			GTK_SIGNAL_FUNC (cheat_sheet_close_requested), NULL);

    mainbox = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(mainbox), 4);
#ifdef USE_GNOME
    gnome_app_set_contents(GNOME_APP(cheat_sheet_window), mainbox);
#else
    gtk_container_add(GTK_CONTAINER(cheat_sheet_window), mainbox);
#endif
    gtk_widget_show(mainbox);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_box_pack_start (GTK_BOX (mainbox), scrolled_window, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				    GTK_POLICY_NEVER,
				    GTK_POLICY_ALWAYS);
    gtk_widget_show (scrolled_window);

    /* Close button */
    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    hbox = gtk_hbutton_box_new ();
    gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbox), 4);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
    gtk_box_pack_start (GTK_BOX (mainbox), hbox,
			FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    thing = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
    GTK_WIDGET_SET_FLAGS(thing, GTK_CAN_DEFAULT);
    gtk_window_set_default(GTK_WINDOW(cheat_sheet_window), thing);
    g_signal_connect (GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC (cheat_sheet_close_requested), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show (thing);

    text = gtk_text_new (NULL, NULL);
    gtk_text_set_editable (GTK_TEXT (text), FALSE);
    gtk_text_set_word_wrap (GTK_TEXT (text), FALSE);
    gtk_container_add (GTK_CONTAINER (scrolled_window), text);
    gtk_widget_grab_focus (text);
    gtk_widget_show (text);
    gtk_widget_set_usize(text, 42 * 12, 46 * 12);

    font = gdk_font_load ("-adobe-courier-medium-r-normal--*-120-*-*-*-*-*-*");

    gtk_text_insert(GTK_TEXT(text), font, NULL, NULL, cheat_sheet_string, -1);

    /* The Text widget will reference count the font, so we
     * unreference it here
     */
    gdk_font_unref (font);

    gtk_widget_show (cheat_sheet_window);
}
