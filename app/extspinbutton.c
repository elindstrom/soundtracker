
/*
 * The Real SoundTracker - GTK+ Spinbutton extensions
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

#include <math.h>

#include "extspinbutton.h"
#include "gui.h"

/* These two defines have been taken from gtkspinbutton.c in gtk+-1.2.1
   Unfortunately there's no cleaner solution */

#define MIN_SPIN_BUTTON_WIDTH              30
#define ARROW_SIZE                         11

static GtkEntryClass *parent_class = NULL;

static int
extspinbutton_find_display_digits (GtkAdjustment *adjustment)
{
    int num_digits;

    num_digits = adjustment->lower < 0 || adjustment->upper < 0;
    num_digits += ceil(log10(MAX(1000, MAX(ABS(adjustment->lower), ABS(adjustment->upper)))));

    return num_digits;
}

static void
extspinbutton_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
    g_return_if_fail (widget != NULL);
    g_return_if_fail (requisition != NULL);
    g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

    GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

    if(EXTSPINBUTTON(widget)->size_hack) {
	requisition->width = MAX (MIN_SPIN_BUTTON_WIDTH,
				  extspinbutton_find_display_digits(GTK_SPIN_BUTTON(widget)->adjustment)
				  * gdk_string_width(gdk_font_from_description(widget->style->font_desc), "X"))
	    + ARROW_SIZE
	    + 2 * widget->style->xthickness;
    } else {
	// This is the normal size_request() from gtk+-1.2.8
	requisition->width = MIN_SPIN_BUTTON_WIDTH + ARROW_SIZE 
	    + 2 * widget->style->xthickness;
    }
}

static void
extspinbutton_value_changed (GtkSpinButton *spin)
{
    if(gtk_window_get_focus(GTK_WINDOW(mainwindow))) {
	// Should only do this if this widget is really in the main window.
	gtk_window_set_focus(GTK_WINDOW(mainwindow), NULL);
    }
}

GtkWidget *
extspinbutton_new (GtkAdjustment *adjustment,
		   gfloat climb_rate,
		   guint digits)
{
    ExtSpinButton *s;

    s = gtk_type_new(extspinbutton_get_type());
    s->size_hack = TRUE;
    gtk_spin_button_configure(GTK_SPIN_BUTTON(s), adjustment, climb_rate, digits);

    g_signal_connect(s, "value-changed",
		     G_CALLBACK(extspinbutton_value_changed), NULL);

    return GTK_WIDGET(s);
}

void
extspinbutton_disable_size_hack (ExtSpinButton *b)
{
    b->size_hack = FALSE;
}

static void
extspinbutton_init (ExtSpinButton *s)
{
}

static void
extspinbutton_class_init (ExtSpinButtonClass *class)
{
    GtkWidgetClass   *widget_class;

    widget_class = (GtkWidgetClass*)class;

    widget_class->size_request = extspinbutton_size_request;

    parent_class = gtk_type_class (GTK_TYPE_ENTRY);
}

guint
extspinbutton_get_type (void)
{
    static guint extspinbutton_type = 0;
    
    if (!extspinbutton_type) {
	GTypeInfo extspinbutton_info =
	{
	    sizeof(ExtSpinButtonClass),
	    (GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
	    (GClassInitFunc) extspinbutton_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,
	    sizeof(ExtSpinButton),
	    0,
	    (GInstanceInitFunc) extspinbutton_init,
	};
	
	extspinbutton_type = g_type_register_static(gtk_spin_button_get_type (),"ExtSpinButton", &extspinbutton_info,  (GTypeFlags)0);
    }
    
    return extspinbutton_type;
}
