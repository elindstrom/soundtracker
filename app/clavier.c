/* clavier.c - GTK+ "Clavier" Widget -- based on clavier-0.1.3
 * Copyright (C) 1998 Simon Kågedal
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
 * GNU General Public License in the file COPYING for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * created 1998-04-18 Simon Kågedal
 */

#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkkeysyms.h>
#include "clavier.h"

#define XFONTNAME "5x8"

static const int default_colors[] = {
    255, 0, 0,
    255, 255, 0,
};

static void init_colors(GtkWidget *widget)
{
    int n;
    const int *p;
    GdkColor *c;

    for (n = 0, p = default_colors, c = CLAVIER(widget)->colors; n < CLAVIERCOL_LAST; n++, c++) {
	c->red = *p++ * 65535 / 255;
	c->green = *p++ * 65535 / 255;
	c->blue = *p++ * 65535 / 255;
        c->pixel = (gulong)((c->red & 0xff00)*256 + (c->green & 0xff00) + (c->blue & 0xff00)/256);
        gdk_color_alloc(gtk_widget_get_colormap(widget), c);
    }
}

static GtkDrawingAreaClass *parent_class = NULL;

/* Signals */
enum
{
  CLAVIERKEY_PRESS,
  CLAVIERKEY_RELEASE,
  CLAVIERKEY_ENTER,
  CLAVIERKEY_LEAVE,
  LAST_SIGNAL
};

static gint clavier_signals[LAST_SIGNAL] = {0};

typedef void (*ClavierSignal1) (GtkObject *object,
				gint arg1,
				gpointer data);

/* Clavier Methods */
static void clavier_class_init (ClavierClass *class);
static void clavier_init (Clavier *clavier);

/* GtkObject Methods */
static void clavier_destroy (GtkObject *object);

/* GtkWidget Methods */
static void clavier_realize (GtkWidget *widget);
static gint clavier_expose (GtkWidget *widget, GdkEventExpose *event);
static gint clavier_button_press (GtkWidget *widget, GdkEventButton *event);
static gint clavier_button_release (GtkWidget *widget, GdkEventButton *event);
static gint clavier_motion_notify (GtkWidget *widget, GdkEventMotion *event);
static gint clavier_leave_notify (GtkWidget *widget, GdkEventCrossing *event);

/* Helpers */
static void draw_key_hint (Clavier *, gint, GdkGC *);

GtkType
clavier_get_type (void)
{
  static GtkType clavier_type = 0;

  if (!clavier_type)
    {
      GTypeInfo clavier_info = {
	sizeof (ClavierClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) clavier_class_init,
	(GClassFinalizeFunc) NULL,
	NULL,
	sizeof (Clavier),
	0,
	(GInstanceInitFunc) clavier_init,	
      };

      clavier_type = g_type_register_static(gtk_drawing_area_get_type (), 
			 "Clavier", &clavier_info, (GTypeFlags)0);
    }

  return clavier_type;
}

static void
clavier_class_init (ClavierClass *class)
{
  GObjectClass *object_class;
  GtkObjectClass *gtkobject_class;
  GtkWidgetClass *widget_class;

  object_class = (GObjectClass *) class;
  gtkobject_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  parent_class = gtk_type_class (gtk_drawing_area_get_type ());

  clavier_signals[CLAVIERKEY_PRESS] = 
    g_signal_new ("clavierkey_press", G_TYPE_FROM_CLASS (object_class),
		   (GSignalFlags) G_SIGNAL_RUN_FIRST,
		    G_STRUCT_OFFSET (ClavierClass, clavierkey_press),
		    NULL, NULL,
		    gtk_marshal_NONE__INT, G_TYPE_NONE, 1, 
		    G_TYPE_INT);

  clavier_signals[CLAVIERKEY_RELEASE] = 
    g_signal_new ("clavierkey_release", G_TYPE_FROM_CLASS (object_class),
		   (GSignalFlags) G_SIGNAL_RUN_FIRST,
		    G_STRUCT_OFFSET (ClavierClass, clavierkey_release),
		    NULL, NULL,
		    gtk_marshal_NONE__INT, G_TYPE_NONE, 1,
		    G_TYPE_INT);

  clavier_signals[CLAVIERKEY_ENTER] = 
    g_signal_new ("clavierkey_enter", G_TYPE_FROM_CLASS (object_class),
		   (GSignalFlags) G_SIGNAL_RUN_FIRST,
		    G_STRUCT_OFFSET (ClavierClass, clavierkey_enter),
		    NULL, NULL,
		    gtk_marshal_NONE__INT, G_TYPE_NONE, 1,
		    G_TYPE_INT);

  clavier_signals[CLAVIERKEY_LEAVE] = 
    g_signal_new ("clavierkey_leave", G_TYPE_FROM_CLASS (object_class),
		   (GSignalFlags) G_SIGNAL_RUN_FIRST,
		    G_STRUCT_OFFSET (ClavierClass, clavierkey_leave),
		    NULL, NULL,
		    gtk_marshal_NONE__INT, G_TYPE_NONE, 1,
		    G_TYPE_INT);

  gtkobject_class->destroy = clavier_destroy;

  widget_class->realize = clavier_realize;
  widget_class->expose_event = clavier_expose;

  widget_class->button_press_event = clavier_button_press;
  widget_class->button_release_event = clavier_button_release;
  widget_class->motion_notify_event = clavier_motion_notify;
  widget_class->leave_notify_event = clavier_leave_notify;
}

static void
clavier_init (Clavier *clavier)
{
  clavier->font = gdk_font_load(XFONTNAME);
  if(!clavier->font) {
      fprintf(stderr, "Hmpf. Strange X installation. You don't have the %s font?\n", XFONTNAME);
      clavier->font = gdk_font_load("fixed");
  }
  g_assert(clavier->font != NULL);

  clavier->fonth = clavier->font->ascent + clavier->font->descent;
  clavier->fontw = gdk_string_width(clavier->font, "X"); /* let's just hope this is a non-proportional font */
  clavier->keylabels = NULL;

  clavier->type = CLAVIER_TYPE_SEQUENCER;
  clavier->dir = CLAVIER_DIR_HORIZONTAL;
  clavier->key_start = 36;
  clavier->key_end = 96;
  clavier->black_key_height = 0.6;
  clavier->black_key_width = 0.54;

  clavier->is_pressed = FALSE;
  clavier->key_pressed = 0;

  clavier->key_info = NULL;
  clavier->key_info_size = 0;

  clavier->show_middle_c = TRUE;
}

static void
clavier_destroy (GtkObject *object)
{
  Clavier *clavier;

  g_return_if_fail (object != NULL);
  g_return_if_fail (IS_CLAVIER (object));

  clavier = CLAVIER (object);

  /* eventually free memory allocated for key info
   */

  if (clavier->key_info)
    {
      g_free (clavier->key_info);
    }

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* checks if the given key is a black one
 */

static gboolean
is_black_key (gint key)
{
  switch (key % 12)
    {
    case 1:
    case 3:
    case 6:
    case 8:
    case 10:
      return (TRUE);

    default:
      return (FALSE);
    }
}

/* calculates how many white and black keys there are in a certain
 * interval on the keyboard 
 */

static void
calc_keys (gint start, gint end, gint *whites, gint *blacks)
{
  gint i;

  *blacks = 0;
  *whites = 0;

  if (end >= start)
    {
      for (i=start; i<=end; i++)
	{
	  if (is_black_key(i))
	    (*blacks)++;
	}
      
      *whites = end - start + 1 - *blacks;
    }
}

/* swaps x and y values if clavier->dir == CLAVIER_DIR_VERTICAL
 */

static void
dir_swap (Clavier *clavier, gint *x1, gint *y1)
{
  if (clavier->dir == CLAVIER_DIR_VERTICAL)
    {
      gint x;
      x = *x1;
      *x1 = *y1;
      *y1 = x;
    }
}

/* use this swapping routine on all points, like on a line
 */

static void
dir_swap_point (Clavier *clavier, gint *x1, gint *y1)
{
  if (clavier->dir == CLAVIER_DIR_VERTICAL)
    {
      gint x;
      x = *x1;
      *x1 = *y1;
      *y1 = GTK_WIDGET(clavier)->allocation.height - x - 1;
    }
}

/* the opposite of dir_swap_point
 */

static void
dir_swap_point_reverse (Clavier *clavier, gint *x1, gint *y1)
{
  if (clavier->dir == CLAVIER_DIR_VERTICAL)
    {
      gint y;
      y = *y1;
      *y1 = *x1;
      *x1 = GTK_WIDGET(clavier)->allocation.height - y;
    }
}

/* use this swapping routine on all rectangles
 */

static void
dir_swap_rectangle (Clavier *clavier, gint *x1, gint *y1, 
		    gint *width, gint *height)
{
  if (clavier->dir == CLAVIER_DIR_VERTICAL)
    {
      gint x;
      x = *x1;
      *x1 = *y1;
      *y1 = GTK_WIDGET(clavier)->allocation.height - x - *width;
      
      x = *width;
      *width = *height;
      *height = x;
    }
}

/* this drawing routine lets each key, whether white or black, take
 * up the same width
 */

static void
calc_key_sequencer (Clavier *clavier, gint key, ClavierKeyInfo *key_info)
{
  GtkWidget *widget;
  gint width, height;

  widget = GTK_WIDGET (clavier);

  width = widget->allocation.width;
  height = widget->allocation.height;

  dir_swap (clavier, &width, &height);

  switch (key % 12)
    {
    case 1: case 3: case 6: case 8: case 10:

      /* black key */

      key_info->is_black = TRUE;
      key_info->upper_right_x = 
	(key - clavier->key_start + 1) * clavier->keywidth;

      break;

    case 0: case 2: case 5: case 7: case 9:

      /* short right side */

      key_info->is_black = FALSE;
      key_info->upper_right_x =
	((gfloat)(key - clavier->key_start) + 1) * clavier->keywidth;
      key_info->lower_right_x =
	((gfloat)(key - clavier->key_start) + 1.5) * clavier->keywidth;

      break;

    case 4: case 11:

      /* long right side */

      key_info->is_black = FALSE;
      key_info->upper_right_x =
	key_info->lower_right_x =
	(key - clavier->key_start + 1) * clavier->keywidth;

      break;
    }

  /* we need to do some fixing for the rightmost key */

  if (key == clavier->key_end)
    {
      key_info->upper_right_x =
	key_info->lower_right_x =
	width - 1;
    }
}

/* this draws a key like on a real keyboard, y'know, all white keys have
 * the same width and the black keys are a bit narrower.
 */

static void
calc_key_normal (Clavier *clavier, gint key, ClavierKeyInfo *key_info)
{
  GtkWidget *widget;
  gint blacks, whites, width, height;

  widget = GTK_WIDGET (clavier);

  width = widget->allocation.width;
  height = widget->allocation.height;

  dir_swap (clavier, &width, &height);

  calc_keys (clavier->key_start, key-1, &whites, &blacks);

  switch (key % 12)
    {
    case 1: case 3: case 6: case 8: case 10:

      /* black key */

      key_info->is_black = TRUE;
      key_info->upper_right_x = 
	((gfloat)whites + 
	 (clavier->black_key_width * 0.5)) * clavier->keywidth;

      break;

    case 0: case 2: case 5: case 7: case 9:

      /* the (whites):th white key from left or bottom, with short
       * right side
       */

      key_info->is_black = FALSE;
      key_info->upper_right_x =
	(whites + 1) * clavier->keywidth -
	(clavier->black_key_width * 0.5 * clavier->keywidth);
      key_info->lower_right_x = (whites + 1) * clavier->keywidth;

      break;

    case 4: case 11:

      /* the (whites):th white key from left or bottom, with long
       * right side
       */

      key_info->is_black = FALSE;
      key_info->upper_right_x =
	key_info->lower_right_x =
	(whites + 1) * clavier->keywidth;

      break;

    }

  /* we need to do some fixing for the rightmost key */

  if (key == clavier->key_end)
    {
      key_info->upper_right_x =
	key_info->lower_right_x =
	width - 1;
    }
}

static void
calc_key_info (Clavier *clavier)
{
  gint keys = clavier->key_end - clavier->key_start + 1;
  gint i;

  if (!clavier->key_info)
    {
      clavier->key_info = g_malloc (sizeof (ClavierKeyInfo) * keys);
    }
  else if (clavier->key_info_size != keys)
    {
      clavier->key_info = 
	g_realloc (clavier->key_info, sizeof (ClavierKeyInfo) * keys);
    }

  for (i=0; i<keys; i++)
    {
      if (clavier->type == CLAVIER_TYPE_NORMAL)
	{
	  calc_key_normal (clavier, 
			   clavier->key_start + i, &clavier->key_info[i]);
	}
      else
	{
	  calc_key_sequencer (clavier, 
			      clavier->key_start + i, &clavier->key_info[i]);
	}

      /* fix so draw_key draws lower rectangle correctly */

      if (i > 0 &&
	  (clavier->key_info[i].is_black))
	{
	  clavier->key_info[i].lower_right_x = 
	    clavier->key_info[i - 1].lower_right_x;
	}
    }
}

static void
clavier_draw_label (Clavier *clavier,
		    int keynum)
{
    ClavierKeyInfo *this = &(clavier->key_info[keynum]);
    GtkWidget *widget = GTK_WIDGET(clavier);
    gchar string[10] = "";

    if(clavier->keylabels) {
	sprintf(string, "%x", clavier->keylabels[keynum]);
    }

    if(clavier->key_info[keynum].is_black) {
	gdk_draw_string(widget->window, clavier->font, clavier->fontbgc, this->upper_right_x - 6, 10, string);
    } else {
	gdk_draw_string(widget->window, clavier->font, clavier->fontwgc, this->upper_right_x - 6, 20, string);
    }
}

/* main drawing function
 */

static gint
clavier_expose (GtkWidget *widget, GdkEventExpose *event)
{
  Clavier *clavier;
  int i;
  gint keys, whitekeys, blackkeys;
  gint width, height;
  ClavierKeyInfo first = {0, 0}, *prev;
  static gint last_transpose = -1;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (IS_CLAVIER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  clavier = CLAVIER(widget);

  width = widget->allocation.width;
  height = widget->allocation.height;

  dir_swap (clavier, &width, &height);

  keys = clavier->key_end - clavier->key_start + 1;

  gdk_draw_rectangle(widget->window, widget->style->bg_gc[0], TRUE,
		     event->area.x, event->area.y, event->area.width, event->area.height);

  calc_keys (clavier->key_start, clavier->key_end, &whitekeys, &blackkeys);
  
  switch (clavier->type)
    {
    case CLAVIER_TYPE_SEQUENCER:
      clavier->keywidth = (gfloat)width / (gfloat)(keys);
      break;

    case CLAVIER_TYPE_NORMAL:
      clavier->keywidth = (gfloat)width / (gfloat)whitekeys;
      break;
    }

  calc_key_info (clavier);

  /*  first.upper_right_x = first.lower_right_x = 0;
   */
  prev = &first;

  for (i=0; i<keys; i++)
    {
      gint x1, y1, x2, y2;

      if (clavier->key_info[i].is_black)
	{
	  x1 = prev->upper_right_x + 1;
	  y1 = 1;

	  x2 = clavier->key_info[i].upper_right_x - prev->upper_right_x - 1;
	  y2 = height * clavier->black_key_height;

	  dir_swap_rectangle (clavier, &x1, &y1, &x2, &y2);

	  gdk_draw_rectangle (widget->window,
			      widget->style->black_gc,
			      TRUE,
			      x1, y1, x2, y2);
	}
      else
	{
	  if (i != (keys-1))
	    {
	      x1 = clavier->key_info[i].lower_right_x;
	      y1 = 1;
	      x2 = x1;
	      y2 = height;
	      
	      dir_swap_point(clavier, &x1, &y1);
	      dir_swap_point(clavier, &x2, &y2);

	      gdk_draw_line (widget->window,
			     widget->style->black_gc,
			     x1, y1, x2, y2);
	    }
	}

      prev = &clavier->key_info[i];

      clavier_draw_label(clavier, i);
    }

  /* show middle C */

  if (clavier->show_middle_c)
    {
      draw_key_hint (clavier, 60, 
		     widget->style->black_gc);
    }

  /* show transpose base */
  if (clavier->show_transpose)
    {
      gint new_transpose= CLAVIER (clavier)->transpose_base;

      /* delete old */
      if (last_transpose != -1 && new_transpose != last_transpose)
	{
	  draw_key_hint (clavier, last_transpose,
			 last_transpose == 60 
			 ? widget->style->black_gc
			 : widget->style->bg_gc[0]);
	}

      draw_key_hint (clavier, new_transpose,
		     widget->style->light_gc[3]);

      last_transpose = new_transpose;
    }

  return FALSE;
}

/* Draw a key pressed or unpressed
 */

static void
draw_key (Clavier *clavier, gint key, gboolean pressed)
{
  ClavierKeyInfo first = {0, 0}, *prev, *this;
  gint keynum;
  gint x1, y1, x2, y2, width, height;
  GtkWidget *widget;
  GdkGC *gc;

  widget = GTK_WIDGET (clavier);

  width = widget->allocation.width;
  height = widget->allocation.height;

  dir_swap (clavier, &width, &height);

  keynum = key - clavier->key_start;

  this = &(clavier->key_info[keynum]);
  prev = (keynum > 0) ? &(clavier->key_info[keynum-1]) : &first;

  if (pressed)
    {
      /* if anyone could teach me how colours in gdk actually work
       * i'd be oh so happy :) 
       */

      gc = this->is_black ?
	/*	widget->style->fg_gc[4] :*/
	/*	widget->style->bg_gc[1];*/
	widget->style->light_gc[3] :
	widget->style->fg_gc[3];
	/*	widget->style->bg_gc[3] : 
	 *	widget->style->fg_gc[4];
	 */
    }
  else
    {
      gc = this->is_black ? widget->style->black_gc :
	widget->style->bg_gc[0];
    }

  /* draw upper part */

  x1 = prev->upper_right_x + 1;
  y1 = 1;
  x2 = this->upper_right_x - prev->upper_right_x - 1;
  y2 = height * clavier->black_key_height;

  dir_swap_rectangle (clavier, &x1, &y1, &x2, &y2);

  gdk_draw_rectangle (widget->window,
		      gc,
		      TRUE,
		      x1, y1, x2, y2);

  if (! clavier->key_info[keynum].is_black)
    {
      /* draw lower part */

      x1 = prev->lower_right_x + 1;
      y1 = height * clavier->black_key_height + 1;
      x2 = this->lower_right_x - prev->lower_right_x - 1;
      y2 = height * (1.0 - clavier->black_key_height) - 1;

      dir_swap_rectangle (clavier, &x1, &y1, &x2, &y2);
      
      gdk_draw_rectangle (widget->window,
			  gc,
			  TRUE,
			  x1, y1, x2, y2);
    }

  clavier_draw_label(clavier, keynum);
}

/* Draw a key "hint" (a little line above the `key') with the 
 * `gc'. 
 */

static void
draw_key_hint (Clavier *clavier, gint key, GdkGC *gc)
{
  ClavierKeyInfo first = {0, 0}, *prev, *this;
  gint keynum;
  gint x1, y1, x2, y2;
  GtkWidget *widget;

  widget = GTK_WIDGET (clavier);

  if (key < clavier->key_start || key > clavier->key_end)
    return;

  keynum = key - clavier->key_start;

  this = &(clavier->key_info[keynum]);
  prev = (keynum > 0) ? &(clavier->key_info[keynum-1]) : &first;

  x1 = prev->upper_right_x + 1;
  y1 = 0;
  x2 = this->upper_right_x;
  y2 = y1;

  dir_swap_point (clavier, &x1, &y1);
  dir_swap_point (clavier, &x2, &y2);

  gdk_draw_line (widget->window,
		 gc,
		 x1, y1, x2, y2);
}

/* See which key is drawn at x, y
 */

static gint
which_key (Clavier *clavier, gint x, gint y)
{
  gint i, keys, width, height;

  width = GTK_WIDGET(clavier)->allocation.width;
  height = GTK_WIDGET(clavier)->allocation.height;

  dir_swap_point_reverse (clavier, &x, &y);
  /*  dir_swap (clavier, &x, &y);*/

  dir_swap (clavier, &width, &height);

  keys = clavier->key_end - clavier->key_start + 1;

  if (y > (height * clavier->black_key_height))
    {
      /* check lower part */

      for (i=0; i<keys; i++)
	{
	  if (x < clavier->key_info[i].lower_right_x)
	    {
	      return clavier->key_start + i;
	    }
	}
    }
  else
    {
      /* check upper part */

      for (i=0; i<keys; i++)
	{
	  if (x < clavier->key_info[i].upper_right_x)
	    {
	      return (clavier->key_start + i);
	    }
	}
    }

  /* so it must be the rightmost key */

  return (clavier->key_end);
}

static void
press_key (Clavier *clavier, gint key)
{
  clavier->is_pressed = TRUE;
  clavier->key_pressed = key;

  gtk_signal_emit (GTK_OBJECT (clavier), 
		   clavier_signals[CLAVIERKEY_PRESS], key);

  /*  printf("press: %i\n", key); */
}

static void
release_key (Clavier *clavier)
{
  clavier->is_pressed = FALSE;

  gtk_signal_emit (GTK_OBJECT (clavier), clavier_signals[CLAVIERKEY_RELEASE], 
		   clavier->key_pressed);

  /*  printf ("release: %i\n", clavier->key_pressed); */
}

/* events
 */

static gint
clavier_button_press (GtkWidget *widget, GdkEventButton *event)
{
  Clavier *clavier;
  gint key;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (IS_CLAVIER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  clavier = CLAVIER(widget);

  key = which_key (clavier, (gint)event->x, (gint)event->y);

  press_key (clavier, key);

  /* add pointer grab - hmm, doesn't seem to work the way i'd like :( */

  gtk_grab_add (widget);
  /*  gdk_pointer_grab (widget->window, TRUE,
		    GDK_BUTTON_PRESS_MASK |
		    GDK_BUTTON_RELEASE_MASK |
		    GDK_BUTTON_MOTION_MASK |
		    GDK_POINTER_MOTION_HINT_MASK,
		    NULL, NULL, 0);*/

  return FALSE;
}

static gint
clavier_button_release (GtkWidget *widget, GdkEventButton *event)
{
  gint key;
  Clavier *clavier;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (IS_CLAVIER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  clavier = CLAVIER (widget);

  key = which_key (clavier, (gint)event->x, (gint)event->y);

  release_key (clavier);

  gtk_grab_remove (widget);
  /*  gdk_pointer_ungrab (0);*/

  return FALSE;
}

static gint
clavier_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
    Clavier *clavier;
    gint key;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_CLAVIER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    clavier = CLAVIER (widget);

    key = which_key (clavier, (gint)event->x, (gint)event->y);

    if(key != clavier->key_entered) {
	if(clavier->key_entered != -1) {
	    gtk_signal_emit (GTK_OBJECT (clavier), 
			     clavier_signals[CLAVIERKEY_LEAVE], clavier->key_entered);
	}
	clavier->key_entered = key;
	gtk_signal_emit (GTK_OBJECT (clavier), 
			 clavier_signals[CLAVIERKEY_ENTER], key);
    }

    if (clavier->is_pressed) {
	if (key != clavier->key_pressed) {
	    release_key (clavier);
	    press_key (clavier, key);
	}
    }

    return FALSE;
}

static gint
clavier_leave_notify (GtkWidget *widget,
		      GdkEventCrossing *event)
{
    Clavier *clavier = CLAVIER (widget);

    if(clavier->key_entered != -1) {
	gtk_signal_emit (GTK_OBJECT (clavier), 
			 clavier_signals[CLAVIERKEY_LEAVE], clavier->key_entered);
	clavier->key_entered = -1;
    }
    
    return FALSE;
}

GtkWidget*
clavier_new (void)
{
  Clavier *clavier;

  clavier = gtk_type_new (clavier_get_type ());

  /*  old_mask = gtk_widget_get_events (GTK_WIDGET (clavier)); */
  /*  gtk_widget_set_events (GTK_WIDGET (clavier), old_mask |  */

  return GTK_WIDGET (clavier);
}

static void
clavier_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  Clavier *clavier;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (IS_CLAVIER (widget));

  clavier = CLAVIER (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget) |
    GDK_EXPOSURE_MASK | 
    GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK | 
    GDK_POINTER_MOTION_MASK |
    GDK_LEAVE_NOTIFY_MASK;
  /*    GDK_POINTER_MOTION_HINT_MASK*/

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), 
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, clavier);

  init_colors(widget);

  clavier->fontwgc = gdk_gc_new(widget->window);
  clavier->fontbgc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(clavier->fontwgc, &clavier->colors[CLAVIERCOL_FONT_ON_WHITE]);
  gdk_gc_set_foreground(clavier->fontbgc, &clavier->colors[CLAVIERCOL_FONT_ON_BLACK]); 

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

/* these routines should most often be mapped to the press and release
 * signals, but not all applications would want it that way
 */

void
clavier_press (Clavier *clavier, gint key)
{
  /* TODO: should keep track of what keys are pressed so that the
   * whole widget can be correctly redrawn
   */

  if (key < clavier->key_start || key > clavier->key_end)
    return;

  draw_key (clavier, key, TRUE);
}

void 
clavier_release (Clavier *clavier, gint key)
{
  if (key < clavier->key_start || key > clavier->key_end)
    return;

  draw_key (clavier, key, FALSE);
}

/* attribute setting 
 */

void
clavier_set_range (Clavier *clavier, gint start, gint end)
{
  g_return_if_fail (start >= 0 && start <= 127);
  g_return_if_fail (start >= 0 && start <= 127);
  g_return_if_fail (end >= start);

  clavier->key_start = start;
  clavier->key_end = end;
}

void
clavier_set_clavier_type (Clavier *clavier, ClavierType type)
{
  clavier->type = type;
  /* should redraw here? */
}

void
clavier_set_clavier_dir (Clavier *clavier, ClavierDir dir)
{
  clavier->dir = dir;
  /* should redraw here? */
}

void
clavier_set_show_middle_c (Clavier *clavier, gboolean b)
{
  clavier->show_middle_c = b;
  /* should redraw here? */
}

void
clavier_set_show_transpose (Clavier *clavier, gboolean b)
{
  clavier->show_transpose = b;
  /* should redraw here? */
}

void
clavier_set_transpose_base (Clavier *clavier, gint key)
{
  clavier->transpose_base = key;

  if (GTK_WIDGET_DRAWABLE (clavier))
    gtk_widget_queue_draw (GTK_WIDGET (clavier));
}

void
clavier_set_key_labels (Clavier *clavier,
			gint8 *labels)
{
    clavier->keylabels = labels;
    gtk_widget_queue_draw(GTK_WIDGET(clavier));
}
