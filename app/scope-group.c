
/*
 * The Real SoundTracker - main window oscilloscope group
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtkprivate.h>

#ifndef NO_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "scalablepic.h"
#endif

#include "scope-group.h"
#include "sample-display.h"
#include "audio.h"
#include "gui-subs.h"
#include "gui-settings.h"

static void
button_toggled (GtkWidget *widget,
		int n)
{
    ScopeGroup *s;
    GtkWidget *w;

    int on = GTK_TOGGLE_BUTTON(widget)->active;

    player_mute_channels[n] = !on;

/* enjoy the hack!!! */
    if ((w = widget->parent) != NULL) {
	s = SCOPE_GROUP(w->parent); /* button<-table<-scope_group */
	s->on_mask = (s->on_mask & (~(1 << n))) | (on ? 1 << n : 0);/* set mask */

#ifndef NO_GDK_PIXBUF
	if (s->scopes_on) {
	    gtk_widget_hide (on ? s->mutedpic[n] : GTK_WIDGET(s->scopes[n]));
	    gtk_widget_show (on ? GTK_WIDGET(s->scopes[n]) : s->mutedpic[n]);
	} else {
	    (on ? gtk_widget_hide : gtk_widget_show)(GTK_WIDGET(s->mutedpic[n]));
	}
#endif
    }
}

void
scope_group_set_num_channels (ScopeGroup *s,
			      int num_channels)
{
    int i;

    // Remove superfluous scopes from table
    for(i = num_channels; i < s->numchan; i++) {
	gtk_container_remove(GTK_CONTAINER(s->table), s->scopebuttons[i]);
    }

    // Resize table
    gtk_table_resize(GTK_TABLE(s->table), 2, num_channels / 2);

    // Add new scopes to table
    for(i = s->numchan; i < num_channels; i++) {
	gtk_object_ref(GTK_OBJECT(s->scopebuttons[i]));
	gtk_table_attach_defaults(GTK_TABLE(s->table), s->scopebuttons[i], i / 2, i / 2 + 1, i % 2, i % 2 + 1);
    }

    // Reset all buttons (enable all channels)
    for(i = 0; i < 32; i++) {
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(s->scopebuttons[i]), 1);
#ifndef NO_GDK_PIXBUF
	gtk_widget_hide (s->mutedpic[i]);
	if (s->scopes_on) gtk_widget_show (GTK_WIDGET(s->scopes[i]));
#endif
    }

    s->numchan = num_channels;
    s->on_mask = 0xFFFFFFFF; /* all channels are on */
}

void
scope_group_enable_scopes (ScopeGroup *s,
			   int enable)
{
    int i;

    s->scopes_on = enable;

    for(i = 0; i < 32; i++) {
#ifdef NO_GDK_PIXBUF
	(enable ? gtk_widget_show : gtk_widget_hide)(GTK_WIDGET(s->scopes[i]));
#else
	if (GTK_TOGGLE_BUTTON(s->scopebuttons[i])->active)
	    (enable ? gtk_widget_show : gtk_widget_hide)(GTK_WIDGET(s->scopes[i]));
#endif
    }
}

gint
scope_group_timeout (ScopeGroup *s)
{
    double time1, time2;
    int i, l;
    static gint16 *buf = NULL;
    static int bufsize = 0;
    int o1, o2;

    if(!s->scopes_on || !scopebuf_ready || !current_driver)
	return TRUE;

    time1 = current_driver->get_play_time(current_driver_object);
    time2 = time1 + (double)1 / s->update_freq;

    for(i = 0; i < 2; i++) {
	if(time1 < scopebuf_start.time || time2 < scopebuf_start.time) {
	    // requesting too old samples -- scopebuf_length is too low
	    goto ende;
	}

	if(time1 >= scopebuf_end.time || time2 >= scopebuf_end.time) {
	    /* requesting samples which haven't been even rendered yet.
	       can happen with short driver latencies. */
	    time1 -= (double)1 / s->update_freq;
	    time2 -= (double)1 / s->update_freq;
	} else {
	    break;
	}
    }

    if(i == 2) {
	goto ende;
    }

    o1 = (time1 - scopebuf_start.time) * scopebuf_freq + scopebuf_start.offset;
    o2 = (time2 - scopebuf_start.time) * scopebuf_freq + scopebuf_start.offset;

    l = o2 - o1;
    if(bufsize < l) {
	free(buf);
	buf = malloc(2 * l);
	bufsize = l;
    }

    o1 %= scopebuf_length;
    o2 %= scopebuf_length;
    g_assert(o1 >= 0 && o1 <= scopebuf_length);
    g_assert(o2 >= 0 && o2 <= scopebuf_length);

    for(i = 0; i < s->numchan; i++) {
	if(o2 > o1) {
	    sample_display_set_data_16(s->scopes[i], scopebufs[i] + o1, l, TRUE);
	} else {
	    memcpy(buf, scopebufs[i] + o1, 2 * (scopebuf_length - o1));
	    memcpy(buf + scopebuf_length - o1, scopebufs[i], 2 * o2);
	    sample_display_set_data_16(s->scopes[i], buf, l, TRUE);
	}
    }

    return TRUE;

  ende:
    for(i = 0; i < s->numchan; i++) {
	sample_display_set_data_8(s->scopes[i], NULL, 0, FALSE);
    }
    return TRUE;
}

void
scope_group_start_updating (ScopeGroup *s)
{
    if(!s->scopes_on || s->gtktimer != -1)
	return;

    s->gtktimer = gtk_timeout_add(1000/s->update_freq, (gint(*)())scope_group_timeout, s);
}

void
scope_group_stop_updating (ScopeGroup *s)
{
    int i;

    if(!s->scopes_on || s->gtktimer == -1)
	return;

    gtk_timeout_remove(s->gtktimer);
    s->gtktimer = -1;

    for(i = 0; i < s->numchan; i++) {
	sample_display_set_data_8(s->scopes[i], NULL, 0, FALSE);
    }
}

void
scope_group_set_update_freq (ScopeGroup *s,
			     int freq)
{
    s->update_freq = freq;
    if(s->scopes_on && s->gtktimer != -1) {
	scope_group_stop_updating(s);
	scope_group_start_updating(s);
    }
}

static gint
scope_group_scope_event (GtkWidget *t,
			 GdkEvent *event,
			 ScopeGroup *sg)
{
    int i;

    switch (event->type) {
    case GDK_BUTTON_PRESS:
	if(event->button.button == 3) {
	    /* Turn all scopes on if some were muted, or make solo selected othervise */
	    if(sg->on_mask == 0xFFFFFFFF) {
		for (i = 0; i < sg->numchan; i++)
		    if(sg->scopebuttons[i] != t)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sg->scopebuttons[i]),FALSE);
		    else sg->on_mask = (1 << i) | (0xFFFFFFFF << sg->numchan);
	    } else {
		for (i = 0; i < sg->numchan; i++)
		    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sg->scopebuttons[i]),TRUE);
		sg->on_mask = 0xFFFFFFFF;
	    }
	    return TRUE;
	}
    default:
	break;
    }
    
    return FALSE;
}

#ifndef NO_GDK_PIXBUF
GtkWidget *
scope_group_new (GdkPixbuf *mutedpic)
#else
GtkWidget *
scope_group_new (void)
#endif
{
    ScopeGroup *s;
    GtkWidget *button, *box, *thing;
    gint i;
    char buf[5];

    s = gtk_type_new(scope_group_get_type());
    GTK_BOX(s)->spacing = 2;
    GTK_BOX(s)->homogeneous = FALSE;
    s->scopes_on = 0;
    s->update_freq = 40;
    s->gtktimer = -1;
    s->numchan = 2;
    s->on_mask = 0xFFFFFFFF;

    s->table = gtk_table_new(2, 1, TRUE);
    gtk_widget_show(s->table);
    gtk_box_pack_start(GTK_BOX(s), s->table, TRUE, TRUE, 0);

    for(i = 0; i < 32; i++) {
	button = gtk_toggle_button_new();
	s->scopebuttons[i] = button;
	g_signal_connect(button, "event",
			   G_CALLBACK(scope_group_scope_event), s);
	g_signal_connect(button, "toggled",
			   G_CALLBACK(button_toggled), GINT_TO_POINTER(i));
	gtk_widget_show(button);
	gtk_widget_ref(button);

	box = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(box);
	gtk_container_add(GTK_CONTAINER(button), box);


#ifndef NO_GDK_PIXBUF
	thing = scalable_pic_new (mutedpic);
	gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
	s->mutedpic[i] = thing;
#endif


	thing = sample_display_new(FALSE);
	gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
	s->scopes[i] = SAMPLE_DISPLAY(thing);

	g_sprintf(buf, "%02d", i+1);
	thing = gtk_label_new(buf);
	gtk_widget_show(thing);
	gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
    }

    gtk_table_attach_defaults(GTK_TABLE(s->table), s->scopebuttons[0], 0, 1, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(s->table), s->scopebuttons[1], 0, 1, 1, 2);

    scope_group_set_update_freq(s, gui_settings.scopes_update_freq);

    return GTK_WIDGET(s);
}

guint
scope_group_get_type (void)
{
    static guint scope_group_type = 0;
    
    if (!scope_group_type) {
	GTypeInfo scope_group_info =
	{
	    sizeof(ScopeGroupClass),
	    (GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
	    (GClassInitFunc) NULL,
	    (GClassFinalizeFunc) NULL,
	    NULL,
	    sizeof(ScopeGroup),
	    0,
	    (GInstanceInitFunc) NULL,
	};
	
	scope_group_type = g_type_register_static(gtk_hbox_get_type (),
	    "ScopeGroup", &scope_group_info,  (GTypeFlags)0);
    }
    
    return scope_group_type;
}
