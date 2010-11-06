
/*
 * The Real SoundTracker - automatically scalable widget containing
 * an image
 *
 * Copyright (C) 2002 Yury Aliaev <mutabor@catholic.org>
 * Copyright (C) 1998-2002 Michael Krause
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

#include "scalablepic.h"

static void scalable_pic_class_init (ScalablePicClass *klass);
static void scalable_pic_init (ScalablePic *sp);
static void scalable_pic_realize (GtkWidget *widget);
static void scalable_pic_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void scalable_pic_send_configure (ScalablePic *sp);
static void scalable_pic_draw (GtkWidget *widget, GdkRectangle *area);
static gint scalable_pic_event (GtkWidget *widget, GdkEvent *event);
static void scalable_pic_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void scalable_pic_destroy (GtkObject *object);

guint scalable_pic_get_type (void)
{
    static guint scalable_pic_type = 0;
    
    if (!scalable_pic_type) {
	static const GtkTypeInfo scalable_pic_info = {
	    "ScalablePic",
	    sizeof (ScalablePic),
	    sizeof (ScalablePicClass),
	    (GtkClassInitFunc)scalable_pic_class_init,
	    (GtkObjectInitFunc)scalable_pic_init,
	    NULL,
	    NULL,
	    (GtkClassInitFunc)NULL
	};
	scalable_pic_type = gtk_type_unique (gtk_widget_get_type (),
		&scalable_pic_info);
    }
    return (scalable_pic_type);
}

GtkWidget *scalable_pic_new (GdkPixbuf *pic)
{
    ScalablePic *sp;
    
    GtkWidget *widget = GTK_WIDGET (gtk_type_new (scalable_pic_get_type ()));
    sp = SCALABLE_PIC (widget);

    sp->pic = pic;
    if(pic != NULL) {
	sp->maxwidth = 1.414 * (gfloat)(sp->pic_width = gdk_pixbuf_get_width(sp->pic));
	sp->maxheight = 1.414 * (gfloat)(sp->pic_height = gdk_pixbuf_get_height(sp->pic));
	sp->copy = NULL;
    }
    
    return (widget);    
}

static void scalable_pic_class_init (ScalablePicClass *klass)
{
    GtkWidgetClass *widget_class;
    GtkObjectClass *object_class;
    
    widget_class = (GtkWidgetClass *)klass;
    object_class = (GtkObjectClass *)klass;
    
    widget_class->realize = scalable_pic_realize;
    widget_class->event = scalable_pic_event;
    widget_class->size_request = scalable_pic_size_request;
    widget_class->size_allocate = scalable_pic_size_allocate;
    
    object_class->destroy = scalable_pic_destroy;
}

static void scalable_pic_init (ScalablePic *sp)
{
}

static void scalable_pic_realize (GtkWidget *widget)
{
    ScalablePic *sp;
    GdkWindowAttr attributes;
    guint attributes_mask;
    
    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_SCALABLE_PIC (widget));
    
    sp = SCALABLE_PIC (widget);
    
    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual (widget);
    attributes.colormap = gtk_widget_get_colormap (widget);
    attributes.event_mask = gtk_widget_get_events (widget);
    attributes.event_mask |= GDK_EXPOSURE_MASK;
    
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
	    &attributes, attributes_mask);

    gdk_window_set_user_data (widget->window, widget);
    
    widget->style = gtk_style_attach (widget->style, widget->window);
    gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
    
    scalable_pic_send_configure (SCALABLE_PIC (widget));
}

static void scalable_pic_destroy (GtkObject *object)
{
    ScalablePic *sp;
    ScalablePicClass *klass;
    
    g_return_if_fail (object != NULL);
    g_return_if_fail (IS_SCALABLE_PIC (object));
    
    sp = SCALABLE_PIC (object);
    if (sp->copy != NULL) gdk_pixbuf_unref (sp->copy);
    
    klass = gtk_type_class (gtk_widget_get_type ());
    if (GTK_OBJECT_CLASS (klass)->destroy)
	(* GTK_OBJECT_CLASS (klass)->destroy)(object);
}

static void scalable_pic_draw (GtkWidget *widget, GdkRectangle *area)
{
    gboolean need_resize = FALSE;    

    ScalablePic *sp;
    
    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_SCALABLE_PIC (widget));
    
    sp = SCALABLE_PIC (widget);

    if(sp->pic == NULL)
	return;

    if((widget->allocation.width != sp->former_width) ||
	    (widget->allocation.height != sp->former_height) ||
	    (sp->copy == NULL)) {
	if (widget->allocation.width > sp->maxwidth) {
	    sp->maxwidth = widget->allocation.width;
	    need_resize = TRUE;
	}
	if (widget->allocation.height > sp->maxheight) {
	    sp->maxheight = widget->allocation.height;
	    need_resize = TRUE;
	}

	if (need_resize || (sp->copy == NULL))	
	    sp->copy = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (sp->pic),
			       gdk_pixbuf_get_has_alpha (sp->pic),
			       gdk_pixbuf_get_bits_per_sample (sp->pic),
			       sp->maxwidth,sp->maxheight);
			       
	gdk_pixbuf_scale (sp->pic, sp->copy, 0, 0,
			    widget->allocation.width, widget->allocation.height,
			    0.0, 0.0,
			    (double)widget->allocation.width/(double)sp->pic_width,
			    (double)widget->allocation.height/(double)sp->pic_height,
			    GDK_INTERP_BILINEAR);

	sp->former_width = widget->allocation.width;
	sp->former_height = widget->allocation.height;
    }
    gdk_pixbuf_render_to_drawable (sp->copy, widget->window,
	    widget->style->black_gc, area->x, area->y, area->x, area->y,
	    area->width, area->height, GDK_RGB_DITHER_NORMAL, 0, 0);
}

static void scalable_pic_size_request (GtkWidget *widget,
			 GtkRequisition *requisition)
{
    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_SCALABLE_PIC (widget));

 /* widget then adjust its dimensions */
    requisition->width = 0;
    requisition->height = 0;
}

static void scalable_pic_size_allocate (GtkWidget *widget,
			GtkAllocation *allocation)
{
    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_SCALABLE_PIC (widget));
    g_return_if_fail (allocation != NULL);

    widget->allocation.x = allocation->x;
    widget->allocation.y = allocation->y;
    widget->allocation.width = allocation->width;
    widget->allocation.height = allocation->height;

    if (GTK_WIDGET_REALIZED (widget)) {
	gdk_window_move_resize (widget->window, allocation->x, allocation->y,
		allocation->width, allocation->height);
	scalable_pic_send_configure (SCALABLE_PIC (widget));
    }
}

static gint scalable_pic_event (GtkWidget *widget, GdkEvent *event)
{
    GdkEventExpose *evnt;
    
    evnt = (GdkEventExpose *)event;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_SCALABLE_PIC (widget), FALSE);
    
    if (event->type == GDK_EXPOSE)
	scalable_pic_draw (widget, &(evnt->area));
    else return (FALSE);
    return (TRUE);
}

static void scalable_pic_send_configure (ScalablePic *sp)
{
    GtkWidget *widget;
    GdkEventConfigure event;

    widget = GTK_WIDGET (sp);

    event.type = GDK_CONFIGURE;
    event.window = widget->window;
    event.x = widget->allocation.x;
    event.y = widget->allocation.y;
    event.width = widget->allocation.width;
    event.height = widget->allocation.height;

    gtk_widget_event (widget, (GdkEvent *)&event);
}
