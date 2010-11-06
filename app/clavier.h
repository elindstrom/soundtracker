/* clavier.h - GTK+ "Clavier" Widget
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
 * created 1998-04-19
 */

#ifndef __CLAVIER_H__
#define __CLAVIER_H__

#include <gdk/gdk.h>
#include <gtk/gtkdrawingarea.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define CLAVIER(obj)	GTK_CHECK_CAST (obj, clavier_get_type (), Clavier)
#define CLAVIER_CLASS(klass) \
  GTK_CHECK_CLASS_CAST (klass, clavier_get_type, ClavierClass)
#define IS_CLAVIER(obj)	GTK_CHECK_TYPE (obj, clavier_get_type ())


typedef struct _Clavier 	Clavier;
typedef struct _ClavierClass 	ClavierClass;

typedef enum 
{
  CLAVIER_TYPE_SEQUENCER,
  CLAVIER_TYPE_NORMAL
} ClavierType;

typedef enum
{
  CLAVIER_DIR_VERTICAL,
  CLAVIER_DIR_HORIZONTAL
} ClavierDir;

struct _ClavierKeyInfo
{
  gboolean is_black;	       	/* is it a black key */

  /* these two contains the x value of the right side of the key 
   * (it's actually y if clavier->dir == CLAVIER_DIR_VERTICAL) 
   */  

  gint upper_right_x;
  gint lower_right_x;		/* not valid if is_black==TRUE */
};

typedef struct _ClavierKeyInfo ClavierKeyInfo;

enum {
    CLAVIERCOL_FONT_ON_WHITE,
    CLAVIERCOL_FONT_ON_BLACK,
    CLAVIERCOL_LAST,
};

struct _Clavier
{
  GtkDrawingArea drawingarea;

  GdkFont *font;
  int fonth, fontw;
  GdkGC *fontbgc, *fontwgc;
  GdkColor colors[CLAVIERCOL_LAST];

  gint8 *keylabels;              // Pointer to bytes, not characters...

  ClavierType type;
  ClavierDir dir;
  gboolean show_middle_c;
  gboolean show_transpose;	/* show where the current transpose base is */
  gint transpose_base;		/* so where is it? */
  gint key_start;	      /* key number start. like MIDI, middle C = 60 */
  gint key_end;		     	/* key number end. */
  gfloat black_key_height;    	/* percentage of the whole height */
  gfloat black_key_width;	/* when type==CLAVIER_TYPE_NORMAL */

  gboolean is_pressed;		/* is a key pressed? */
  gint key_pressed;		/* which key? */

  int key_entered;              /* the key the mouse is over currently (or -1) */

  /* stuff that is calculated */

  gfloat keywidth;	     	/* X if dir==CLAVIER_DIR_HORIZONTAL */

  ClavierKeyInfo *key_info;
  gint key_info_size;
};

struct _ClavierClass
{
  GtkDrawingAreaClass parent_class;

  void (* clavierkey_press) (Clavier *clavier, gint key);
  void (* clavierkey_release) (Clavier *clavier, gint key);
  void (* clavierkey_enter) (Clavier *clavier, gint key);
  void (* clavierkey_leave) (Clavier *clavier, gint key);
};

GtkType		clavier_get_type	(void);
GtkWidget*	clavier_new	(void);

void		clavier_set_range (Clavier *clavier, gint start, gint end);
void		clavier_set_clavier_type (Clavier *clavier, ClavierType type);
void		clavier_set_clavier_dir (Clavier *clavier, ClavierDir dir);
void		clavier_set_show_middle_c (Clavier *, gboolean);
void		clavier_set_show_transpose (Clavier *, gboolean);
void		clavier_set_transpose_base (Clavier *, gint);
void            clavier_set_key_labels (Clavier *, gint8 *);

void		clavier_press (Clavier *clavier, gint key);
void		clavier_release (Clavier *clavier, gint key);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CLAVIER_H__ */
