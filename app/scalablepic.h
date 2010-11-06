
/*
 * The Real SoundTracker - automatically scalable widget containing
 * an image (header)
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

#ifndef __SCALABLE_PIC_H
#define __SCALABLE_PIC_H

#include <gtk/gtk.h>
#include <gtk/gtkwidget.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCALABLE_PIC(obj) \
    GTK_CHECK_CAST (obj, scalable_pic_get_type (), ScalablePic)
#define SCALABLE_PIC_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, scalable_pic_get_type (), ScalablePicClass)
#define IS_SCALABLE_PIC(obj) \
    GTK_CHECK_TYPE (obj, scalable_pic_get_type ())

typedef struct _ScalablePic {
    GtkWidget widget;
    GdkPixbuf *pic, *copy;
    gint maxwidth, maxheight, former_width, former_height, pic_width, pic_height;
} ScalablePic;

typedef struct _ScalablePicClass {
    GtkWidgetClass parent_class;
} ScalablePicClass;

/* create a widget containing GdkPixbuf image */
GtkWidget *scalable_pic_new (GdkPixbuf *pic);
guint scalable_pic_get_type (void);

#ifdef __cplusplus
}
#endif

#endif /* __SCALABLE_PIC_H */
