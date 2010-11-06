
/*
 * The Real SoundTracker - gtk+ Playlist widget
 *
 * Copyright (C) 1999-2003 Michael Krause
 * Copyright (C) 2003 Yury Aliaev
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

#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkinvisible.h>
#include <math.h>
#include <string.h>

#include "i18n.h"

#include "playlist.h"
#include "gui-subs.h"
#include "gui.h"
#include "extspinbutton.h"

enum {
    SIG_CURRENT_POSITION_CHANGED,
    SIG_RESTART_POSITION_CHANGED,
    SIG_SONG_LENGTH_CHANGED,
    SIG_ENTRY_CHANGED,
    LAST_SIGNAL
};

static guint playlist_signals[LAST_SIGNAL] = { 0 };

static gboolean
scroll_event (GtkWidget *w, GdkEventScroll *event, Playlist *p)
{
    int pos = p->current_position;

    switch(event->direction) {
    case GDK_SCROLL_UP:
	pos--;
	if((pos >= 0) && (pos) < p->length)
		gtk_adjustment_set_value(GTK_ADJUSTMENT(p->adj_songpos), pos);
	return TRUE;
    case GDK_SCROLL_DOWN:
	pos++;
	if((pos >= 0) && (pos) < p->length)
		gtk_adjustment_set_value(GTK_ADJUSTMENT(p->adj_songpos), pos);
	return TRUE;
    default:
	break;
    }
    return FALSE;
}

static gboolean
label_clicked (GtkWidget *w, GdkEventButton *event, Playlist *p)
{
    int i;

    int pos = p->current_position;

    if (event->button == 1) {
	// find what label is clicked
	for (i = -2; i <= 2; i++) {
	    if ((p->numlabels[i+2] == GTK_BIN(w)->child) ||
		(p->patlabels[i+2] == GTK_BIN(w)->child)) {
		pos += i;
		if ((pos >= 0) && (pos) < p->length)
		    gtk_adjustment_set_value(GTK_ADJUSTMENT(p->adj_songpos), pos);
		return (TRUE);
	    }
	}
    }
    return (FALSE);
}

static void
playlist_update_adjustment (GtkAdjustment *adj,
			    int min,
			    int max)
{
    gfloat p;

    p = adj->value;
    if(p < min)
	p = min;
    else if(p > max)
	p = max;
    
    adj->lower = min;
    adj->upper = max + 1;
    gtk_adjustment_set_value(adj, p);
    gtk_adjustment_changed(adj);
}

static void
playlist_draw_contents (Playlist *p)
{
    gint i;
    gchar *str;
    GtkWidget *thing;

    g_assert(IS_PLAYLIST(p));

    for (i = 0; i <= 4; i++) {
        if ((i >= 2 - p->current_position) &&
		 (i <= p->length - p->current_position + 1)) {
	    thing = p->numlabels[i];
	    gtk_label_set_text(GTK_LABEL(thing), str = g_strdup_printf("%.3u", i - 2 + p->current_position));
	    g_free(str);
	    if (i != 2) {
		thing = p->patlabels[i];
		gtk_label_set_text(GTK_LABEL(thing), str = g_strdup_printf("%.3u", p->patterns[i - 2 + p->current_position]));
		g_free(str);
	    }
	} else {
	    thing = p->numlabels[i];
	    gtk_label_set_text(GTK_LABEL(thing), "");
	    thing = p->patlabels[i];
	    gtk_label_set_text(GTK_LABEL(thing), "");
	}
    }
}

void
playlist_freeze (Playlist *p)
{
    p->frozen++;
}

void
playlist_thaw (Playlist *p)
{
    if(p->frozen)
	p->frozen--;
    if(!p->frozen)
	playlist_draw_contents(p);
}

void
playlist_enable (Playlist *p,
		 gboolean enable)
{
    gtk_widget_set_sensitive(p->spin_songlength, enable);
    gtk_widget_set_sensitive(p->spin_songpat, enable);
    gtk_widget_set_sensitive(p->spin_restartpos, enable);
    gtk_widget_set_sensitive(p->ibutton, enable);
    gtk_widget_set_sensitive(p->ifbutton, enable);
    gtk_widget_set_sensitive(p->icbutton, enable);
    gtk_widget_set_sensitive(p->dbutton, enable);
}

void
playlist_freeze_signals (Playlist *p)
{
    p->signals_disabled++;
}

void
playlist_thaw_signals (Playlist *p)
{
    g_assert(p->signals_disabled > 0);

    p->signals_disabled--;
}

void
playlist_set_length (Playlist *p, int length)
{
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songlength), length);
}

int
playlist_get_length (Playlist *p)
{
    return p->length;
}

void
playlist_set_nth_pattern (Playlist *p,
			  int pos,
			  int pat)
{
    g_assert(pos >= 0 && pos < p->length);
    g_assert(pat >= p->min_pattern && pat <= p->max_pattern);

    p->patterns[pos] = pat;

    if(pos == p->current_position) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat), pat);
    }

    if (!p->frozen)
	playlist_draw_contents(p);
}

int
playlist_get_nth_pattern (Playlist *p,
			  int pos)
{
    g_assert(pos >= 0 && pos < p->length);

    return p->patterns[pos];
}

void
playlist_set_position (Playlist *p,
		       int pos)
{
    g_assert(pos >= 0 && pos < p->length);

    gtk_adjustment_set_value(GTK_ADJUSTMENT(p->adj_songpos), pos);
}

int
playlist_get_position (Playlist *p)
{
    return p->current_position;
}

void
playlist_set_restart_position (Playlist *p,
			       int pos)
{
    g_assert(pos >= 0 && pos < p->length);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_restartpos), pos);
}

int
playlist_get_restart_position (Playlist *p)
{
    return p->restart_position;
}

static void
playlist_songpos_changed (GtkAdjustment *adj,
			  Playlist *p)
{
    int newpos;

    g_assert(IS_PLAYLIST(p));

    /* In gtk+-1.2.10, the GtkAdjustment loses this setting when the
       upper value is changed by the program. */
    adj->page_increment = 5.0;
    newpos = rint(adj->value);

    if(newpos == p->current_position)
	return;

    p->current_position = newpos;

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat), p->patterns[newpos]);

    if (!p->frozen)
	playlist_draw_contents (p);

    if(!p->signals_disabled)
	gtk_signal_emit(GTK_OBJECT(p),
			playlist_signals[SIG_CURRENT_POSITION_CHANGED],
			newpos);
}

static void
playlist_songlength_changed (GtkSpinButton *spin,
			     Playlist *p)
{
    int i, newlen;

    g_assert(IS_PLAYLIST(p));

    newlen = gtk_spin_button_get_value_as_int(spin);
    
    if(newlen == p->length)
	return;

    if(newlen > p->alloc_length) {
	p->patterns = g_renew(int, p->patterns, newlen);
	p->alloc_length = newlen;
    }

    if(newlen > p->length) {
	for(i = p->length; i < newlen; i++) {
	    p->patterns[i] = p->patterns[p->length - 1];
	}
    }

    p->length = newlen;

    playlist_update_adjustment(GTK_ADJUSTMENT(p->adj_songpos), 0, newlen - 1);
    gui_update_spin_adjustment(GTK_SPIN_BUTTON(p->spin_restartpos), 0, newlen - 1);
    if(!p->frozen)
	playlist_draw_contents(p);
    if(!p->signals_disabled)
	gtk_signal_emit(GTK_OBJECT(p),
			playlist_signals[SIG_SONG_LENGTH_CHANGED],
			p->length);
}

static void
playlist_songpat_changed (GtkSpinButton *spin,
			  Playlist *p)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    g_assert(IS_PLAYLIST(p));

    if(p->patterns[p->current_position] == n)
	return;

    p->patterns[p->current_position] = n;

    if(!p->signals_disabled)
	gtk_signal_emit(GTK_OBJECT(p),
			playlist_signals[SIG_ENTRY_CHANGED],
			p->current_position, n);
}

static void
playlist_restartpos_changed (GtkSpinButton *spin,
			     Playlist *p)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    g_assert(IS_PLAYLIST(p));

    /* In gtk+-1.2.10, the GtkAdjustment loses this setting when the
       upper value is changed by the program. */
    spin->adjustment->page_increment = 5.0;

    if(p->restart_position == n)
	return;

    p->restart_position = n;

    if(!p->signals_disabled)
	gtk_signal_emit(GTK_OBJECT(p),
			playlist_signals[SIG_RESTART_POSITION_CHANGED],
			n);
}

void
playlist_insert_pattern (Playlist *p,
			 int pos,
			 int pat)
{
    int current_songpos;

    g_assert(pos >= 0 && pos <= p->length);
    g_assert(pat >= p->min_pattern && pat <= p->max_pattern);

    if(p->length == p->max_length)
	return;

    current_songpos = rint(GTK_ADJUSTMENT(p->adj_songpos)->value);

    playlist_freeze(p);
    playlist_freeze_signals(p);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songlength), p->length + 1);
    playlist_thaw_signals(p);

    memmove(&p->patterns[pos + 1], &p->patterns[pos], (p->length - 1 - pos) * sizeof(int));
    p->patterns[pos] = pat;
    if(pos == current_songpos) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat), p->patterns[pos]);
    }

    /* This also redraws the playlist */
    playlist_thaw(p);

    if(!p->signals_disabled) {
	/* This also makes gui.c update the playlist copy which is
	 * held in the global XM structure. */
	gtk_signal_emit(GTK_OBJECT(p),
			playlist_signals[SIG_SONG_LENGTH_CHANGED],
			p->length);
    }
}

static void
playlist_insert_clicked (GtkWidget *w,
			 Playlist *p)
{
    int pos;

    g_assert(IS_PLAYLIST(p));

    if(p->length == p->max_length)
	return;

    pos = rint(GTK_ADJUSTMENT(p->adj_songpos)->value);

    playlist_insert_pattern(p, pos, gui_get_current_pattern());
}

static void
playlist_delete_clicked (GtkWidget *w,
			 Playlist *p)
{
    int pos;

    g_assert(IS_PLAYLIST(p));

    if(p->length == 1)
	return;

    pos = rint(GTK_ADJUSTMENT(p->adj_songpos)->value);
    memmove(&p->patterns[pos], &p->patterns[pos + 1], (p->length - pos - 1) * sizeof(int));

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songlength), p->length - 1);

    pos = rint(GTK_ADJUSTMENT(p->adj_songpos)->value);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat), p->patterns[pos]);
    if(!p->signals_disabled)
	gtk_signal_emit(GTK_OBJECT(p),
			playlist_signals[SIG_ENTRY_CHANGED],
			pos, p->patterns[pos]);
}

static void
is_realized(GtkWidget *algn, gpointer data)
{
    guint x, y;
    
    gtk_widget_realize(algn);
    x = (algn->allocation).width;
    y = (algn->allocation).height;
    gtk_widget_set_size_request(algn, x, y);
}

GtkWidget *
playlist_new (void)
{
    Playlist *p;
    GtkWidget *box, *thing, *thing1, *vbox, *frame, *box1, *evbox, *al;
    GtkObject *adj;
    gint i;

    p = gtk_type_new(playlist_get_type());
    GTK_BOX(p)->spacing = 2;
    GTK_BOX(p)->homogeneous = FALSE;

    vbox = GTK_WIDGET(p);
    
    box = gtk_hbox_new(FALSE, 2);
    thing = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(thing), GTK_SHADOW_IN);
    thing1 = gtk_table_new(2, 5, FALSE);
    
    /* pattern list view */
    
    for (i = 0; i <= 4; i++)
	if (i != 2) {
	    p->numlabels[i] = gtk_label_new ("");
	    gtk_widget_show(p->numlabels[i]);
	    evbox = gtk_event_box_new();
	    gtk_container_add(GTK_CONTAINER(evbox), p->numlabels[i]);
	    gtk_widget_show(evbox);
	    g_signal_connect(evbox, "button_press_event",
			       G_CALLBACK(label_clicked),
			       (gpointer)p);
	    box1 = gtk_hbox_new (FALSE, 0),
	    gtk_box_pack_start(GTK_BOX(box1), evbox, FALSE, FALSE, 3);
	    gtk_widget_show(box1);
	    gtk_table_attach_defaults (GTK_TABLE(thing1), box1, 0, 1, i, i+1);

	    p->patlabels[i] = gtk_label_new("");
	    gtk_widget_show(p->patlabels[i]);
	    evbox = gtk_event_box_new();
	    gtk_container_add(GTK_CONTAINER(evbox), p->patlabels[i]);
	    gtk_widget_show(evbox);
	    g_signal_connect(evbox, "button_press_event",
			      G_CALLBACK(label_clicked),
			       (gpointer)p);
	    box1 = gtk_hbox_new (FALSE, 0),
	    gtk_box_pack_start(GTK_BOX(box1), evbox, FALSE, FALSE, 0);
	    gtk_widget_show(box1);
	    gtk_table_attach_defaults(GTK_TABLE(thing1), box1, 1, 2, i, i+1);
	}
    /* central label */
    box1 = gtk_hbox_new(FALSE, 0);
    p->numlabels[2] = gtk_label_new("");
    gtk_widget_show(p->numlabels[2]);
    /* a bit trick to keep label size constant */
    al = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
    gtk_container_add(GTK_CONTAINER(al), p->numlabels[2]);
    g_signal_connect(al, "realize", G_CALLBACK(is_realized), NULL);
    gtk_widget_show(al);
    gtk_box_pack_start(GTK_BOX(box1), al, TRUE, TRUE, 0);
    gtk_widget_show(box1);

    /* current pattern */
    adj = gtk_adjustment_new((p->patterns)[p->current_position], p->min_pattern, p->max_pattern, 1.0, 5.0, 0.0);
    p->spin_songpat = extspinbutton_new(GTK_ADJUSTMENT (adj), 1.0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(p->spin_songpat));
    gtk_widget_show(p->spin_songpat);
    g_signal_connect(p->spin_songpat, "value-changed",
		    G_CALLBACK(playlist_songpat_changed),
		    (gpointer)p);
    gtk_box_pack_end(GTK_BOX(box1), p->spin_songpat, TRUE, TRUE, 0);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
    gtk_container_add(GTK_CONTAINER(frame), box1);
    gtk_widget_show(frame);

    gtk_table_attach_defaults(GTK_TABLE(thing1), frame, 0, 2, 2, 3);
    gtk_widget_show(thing1);
    
    evbox = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(evbox), thing1);
    gtk_widget_show(evbox);
    g_signal_connect(evbox, "button_press_event",
		       G_CALLBACK(label_clicked),
		       (gpointer)p);
    g_signal_connect(evbox, "scroll-event",
			G_CALLBACK(scroll_event),
			(gpointer)p);

    gtk_container_add(GTK_CONTAINER(thing), evbox);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);
    gtk_widget_show(box);

    /* scrollbar */
    p->adj_songpos = gtk_adjustment_new(p->current_position, 0.0, p->length, 1.0, 5.0, 1.0);
    thing = gtk_vscrollbar_new(GTK_ADJUSTMENT(p->adj_songpos));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);
    g_signal_connect(p->adj_songpos, "value_changed",
		    G_CALLBACK(playlist_songpos_changed),
		    (gpointer)p);

    /* buttons */
    thing = gtk_vbox_new(FALSE, 0);

    thing1 = p->ibutton = gtk_button_new_with_label(_("Insert"));
    gtk_widget_show(thing1);
    gui_hang_tooltip(thing1, _("Insert pattern that is being edited"));
    gtk_box_pack_start(GTK_BOX(thing), thing1, FALSE, FALSE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing1, "clicked",
		    G_CALLBACK(playlist_insert_clicked),
		    (gpointer)p);

    thing1 = p->dbutton = gtk_button_new_with_label(_("Delete"));
    gtk_widget_show(thing1);
    gui_hang_tooltip(thing1, _("Remove current playlist entry"));
    gtk_box_pack_start(GTK_BOX(thing), thing1, FALSE, FALSE, 0);
    g_signal_connect(thing1, "clicked",
		    G_CALLBACK(playlist_delete_clicked),
		    (gpointer)p);

    add_empty_vbox(thing);

    thing1 = p->icbutton = gtk_button_new_with_label(_("Add + Cpy"));
    gtk_widget_show(thing1);
    gui_hang_tooltip(thing1, _("Add a free pattern behind current position, and copy current pattern to it"));
    gtk_box_pack_start(GTK_BOX(thing), thing1, FALSE, FALSE, 0);

    thing1 = p->ifbutton = gtk_button_new_with_label(_("Add Free"));
    gtk_widget_show(thing1);
    gui_hang_tooltip(thing1, _("Add a free pattern behind current position"));
    gtk_box_pack_start(GTK_BOX(thing), thing1, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
    
    /* box with songlen and repstart spinbuttons */
    box = gtk_hbox_new(FALSE, 0);

    thing1 = gtk_label_new(_("Len"));
    gtk_widget_show(thing1);
    gtk_box_pack_start(GTK_BOX(box), thing1, TRUE, TRUE, 0);
    gtk_widget_show(box);

    thing1 = gtk_label_new(NULL);
    gtk_widget_set_usize(thing1, 2, 1);
    gtk_widget_show(thing1);
    gtk_box_pack_start(GTK_BOX(box), thing1, TRUE, TRUE, 0);

    adj = gtk_adjustment_new(p->length, 1.0, p->max_length, 1.0, 5.0, 0.0);
    thing1 = p->spin_songlength = extspinbutton_new(GTK_ADJUSTMENT(adj), 1.0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(thing1));
    gtk_widget_show(thing1);
    gui_hang_tooltip(thing1, _("Song length"));
    gtk_box_pack_start(GTK_BOX(box), thing1, TRUE, TRUE, 0);
    g_signal_connect(thing1, "value-changed",
			G_CALLBACK(playlist_songlength_changed),
			(gpointer)p);

    thing1 = gtk_label_new(_("Rstrt"));
    gtk_widget_show(thing1);
    gtk_box_pack_start(GTK_BOX(box), thing1, TRUE, TRUE, 0);

    thing1 = gtk_label_new(NULL);
    gtk_widget_set_usize(thing1, 2, 1);
    gtk_widget_show(thing1);
    gtk_box_pack_start(GTK_BOX(box), thing1, TRUE, TRUE, 0);

    adj = gtk_adjustment_new(p->current_position, 0.0, p->length - 1, 1.0, 5.0, 0.0);
    thing1 = p->spin_restartpos = extspinbutton_new(GTK_ADJUSTMENT(adj), 1.0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(thing1));
    gtk_widget_show(thing1);
    gui_hang_tooltip(thing1, _("Song restart position"));
    gtk_box_pack_start(GTK_BOX(box), thing1, TRUE, TRUE, 0);
    g_signal_connect(thing1, "value-changed",
			G_CALLBACK(playlist_restartpos_changed),
			(gpointer)p);

    gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
    
    playlist_draw_contents(p);

    return GTK_WIDGET(p);
}

/* --- gtk+ object initialization crap --- */

static void
playlist_class_init (PlaylistClass *class)
{
    GObjectClass *g_object_class;

    g_object_class = (GObjectClass*) class;

    playlist_signals[SIG_CURRENT_POSITION_CHANGED] =
	   g_signal_new ("current_position_changed",
			   G_TYPE_FROM_CLASS (g_object_class),
			   (GSignalFlags)G_SIGNAL_RUN_FIRST,
   			   G_STRUCT_OFFSET(PlaylistClass, current_position_changed),
			   NULL, NULL,
			   gtk_marshal_NONE__INT, G_TYPE_NONE, 1, G_TYPE_INT);
    playlist_signals[SIG_RESTART_POSITION_CHANGED] =
	    g_signal_new ("restart_position_changed",
			   G_TYPE_FROM_CLASS (g_object_class),
			   (GSignalFlags)G_SIGNAL_RUN_FIRST,
			   G_STRUCT_OFFSET(PlaylistClass, restart_position_changed),
			   NULL, NULL,
			   gtk_marshal_NONE__INT, G_TYPE_NONE, 1, G_TYPE_INT);
    playlist_signals[SIG_SONG_LENGTH_CHANGED] =
	    g_signal_new ("song_length_changed",
			   G_TYPE_FROM_CLASS (g_object_class),
			   (GSignalFlags)G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET(PlaylistClass, song_length_changed),
			   NULL, NULL,
			gtk_marshal_NONE__INT, G_TYPE_NONE, 1, G_TYPE_INT);
    playlist_signals[SIG_ENTRY_CHANGED] =
	    g_signal_new ("entry_changed",
			   G_TYPE_FROM_CLASS (g_object_class),
			   (GSignalFlags)G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET(PlaylistClass, entry_changed),
			   NULL, NULL,
			gtk_marshal_NONE__INT_INT, G_TYPE_NONE, 2,
			G_TYPE_INT, G_TYPE_INT);

    class->current_position_changed = NULL;
    class->restart_position_changed = NULL;
    class->song_length_changed = NULL;
    class->entry_changed = NULL;
}

static void
playlist_init (Playlist *p)
{
    // These presets should probably be configurable via the interface
    p->max_length = 256;
    p->min_pattern = 0;
    p->max_pattern = 255;

    // A reasonable default playlist
    p->length = 1;
    p->alloc_length = 1;
    p->patterns = g_new0(int, p->alloc_length);
    p->current_position = 0;
    p->restart_position = 0;
    p->signals_disabled = 0;
    p->frozen = FALSE;
}

guint
playlist_get_type()
{
    static guint playlist_type = 0;
    
    if (!playlist_type) {
	GTypeInfo playlist_info =
	{
	    sizeof(PlaylistClass),
	    (GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
	    (GClassInitFunc) playlist_class_init,
	    (GClassFinalizeFunc) NULL,
	    NULL,
	    sizeof(Playlist),
	    0,
	    (GInstanceInitFunc) playlist_init,
	};
	
	playlist_type = g_type_register_static(gtk_vbox_get_type (),
	    "Playlist", &playlist_info, (GTypeFlags)0);
    }

    return playlist_type;
}
