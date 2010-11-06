
/*
 * The Real SoundTracker - GTK+ envelope editor box
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

#include <string.h>

#include <gtk/gtk.h>

#include "i18n.h"
#include "gui-subs.h"
#include "envelope-box.h"
#include "xm.h"
#include "gui-settings.h"

static STEnvelope dummy_envelope = {
    { { 0, 0 } },
    1,
    0,
    0,
    0,
    0
};

static void
spin_length_changed (GtkSpinButton *spin,
		     EnvelopeBox *e);

static gboolean
envelope_box_clip_point_movement (EnvelopeBox *e,
				  int p,
				  int *dx,
				  int *dy)
{
    gboolean corrected = FALSE;
    int bound;
    int curx = e->current->points[p].pos;
    int cury = e->current->points[p].val;

    if(dx != NULL) {
	if(p == 0 && *dx != 0) {
	    *dx = 0;
	    corrected = TRUE;
	} else if(*dx < 0) {
	    if(p > 0)
		bound = e->current->points[p-1].pos + 1;
	    else
		bound = 0;
	    if(*dx < bound - curx) {
		*dx = bound - curx;
		corrected = TRUE;
	    }
	} else {
	    if(p < e->current->num_points - 1)
		bound = e->current->points[p+1].pos - 1;
	    else
		bound = 65535;
	    if(*dx > bound - curx) {
		*dx = bound - curx;
		corrected = TRUE;
	    }
	}
    }

    if(dy != NULL) {
	if(*dy < 0) {
	    bound = 0;
	    if(*dy < bound - cury) {
		*dy = bound - cury;
		corrected = TRUE;
	    }
	} else {
	    bound = 64;
	    if(*dy > bound - cury) {
		*dy = bound - cury;
		corrected = TRUE;
	    }
	}
    }

    return corrected;
}

#ifdef USE_GNOME
static gint
envelope_box_point_event (GnomeCanvasItem *item,
			  GdkEvent *event,
			  gpointer data);


static void
envelope_box_canvas_size_allocate (GnomeCanvas *c,
				   void *dummy,
				   EnvelopeBox *e)
{
    double newval = (double)GTK_WIDGET(c)->allocation.height / (64 + 10);
    if(newval != e->zoomfactor_base) {
	gnome_canvas_set_pixels_per_unit(c, newval * e->zoomfactor_mult);
	e->zoomfactor_base = newval;
    }
}

static void
envelope_box_canvas_set_max_x (EnvelopeBox *e,
			       int x)
{
    e->canvas_max_x = x;
    gnome_canvas_set_scroll_region(e->canvas, -2 - 10 - 10, -2, x + 2 + 10, 66);
}

static void
envelope_box_canvas_paint_grid (EnvelopeBox *e)
{
    GnomeCanvasPoints *points;
    GnomeCanvasItem *item;
    GnomeCanvasGroup *group;
    int lines[] = {
	0, 0, 0, 64,
	-6, 0, 0, 0,
	-4, 16, 0, 16,
	-6, 32, 0, 32,
	-4, 48, 0, 48,
	-6, 64, 0, 64,
	2, 0, 16384, 0,
	2, 64, 16384, 64,
    };
    int i;

    group = GNOME_CANVAS_GROUP (gnome_canvas_item_new (gnome_canvas_root(e->canvas),
						       gnome_canvas_group_get_type (),
						       "x", 0.0,
						       "y", 0.0,
						       NULL));

    points = gnome_canvas_points_new(2);
    for(i = 0; i < sizeof(lines) / 4 / sizeof(int); i++) {
	points->coords[0] = lines[4*i+0];
	points->coords[1] = lines[4*i+1];
	points->coords[2] = lines[4*i+2];
	points->coords[3] = lines[4*i+3];
	item = gnome_canvas_item_new (group,
				      gnome_canvas_line_get_type (),
				      "points", points,
				      "fill_color", "#000088",
				      "width_units", 0.5,
				      NULL);
	gnome_canvas_item_lower_to_bottom(item);
    }
    gnome_canvas_points_free(points);

    // UGH! Find a better way of making the background white...
    item = gnome_canvas_item_new (group,
				  gnome_canvas_rect_get_type (),
				  "x1", (double)-500,
				  "y1", (double)-100,
				  "x2", (double)+70000,
				  "y2", (double)+100,
				  "fill_color", "#ffffff",
				  "outline_color", "#ffffff",
				  "width_pixels", 0,
				  NULL);
    gnome_canvas_item_lower_to_bottom(item);
}

static void
envelope_box_canvas_add_point (EnvelopeBox *e,
			       int n)
{
    // Create new point
    e->points[n] = gnome_canvas_item_new (e->group,
					  gnome_canvas_rect_get_type (),
					  "x1", (double)e->current->points[n].pos - 1.5,
					  "y1", (double)(64-e->current->points[n].val) - 1.5,
					  "x2", (double)e->current->points[n].pos + 1.5,
					  "y2", (double)(64-e->current->points[n].val) + 1.5,
					  "fill_color", "#ff0000",
					  "outline_color", "#ff0000",
					  "width_pixels", 0,
					  NULL);
    gtk_signal_connect (GTK_OBJECT (e->points[n]), "event",
			(GtkSignalFunc) envelope_box_point_event,
			e);

    // Adjust / Create line connecting to the previous point
    if(n > 0) {
	if(e->lines[n - 1]) {
	    GNOME_CANVAS_LINE(e->lines[n - 1])->coords[2] = (double)e->current->points[n].pos;
	    GNOME_CANVAS_LINE(e->lines[n - 1])->coords[3] = (double)(64 - e->current->points[n].val);
	    gnome_canvas_item_request_update(e->lines[n - 1]);
	} else {
	    GnomeCanvasPoints *points = gnome_canvas_points_new(2);
	    points->coords[0] = (double)e->current->points[n-1].pos;
	    points->coords[1] = (double)(64 - e->current->points[n-1].val);
	    points->coords[2] = (double)e->current->points[n].pos;
	    points->coords[3] = (double)(64 - e->current->points[n].val);

	    e->lines[n-1] = gnome_canvas_item_new (e->group,
						   gnome_canvas_line_get_type (),
						   "points", points,
						   "fill_color", "black",
						   "width_units", 1.0,
						   NULL);
	    gnome_canvas_item_lower_to_bottom(e->lines[n-1]);
	    gnome_canvas_points_free(points);
	}
    }

    // Adjust / Create line connecting to the next point
    if(n < e->current->num_points - 1) {
	if(e->lines[n]) {
	    printf("muh.\n");
	} else {
	    GnomeCanvasPoints *points = gnome_canvas_points_new(2);
	    points->coords[0] = (double)e->current->points[n].pos;
	    points->coords[1] = (double)(64 - e->current->points[n].val);
	    points->coords[2] = (double)e->current->points[n + 1].pos;
	    points->coords[3] = (double)(64 - e->current->points[n + 1].val);

	    e->lines[n] = gnome_canvas_item_new (e->group,
						 gnome_canvas_line_get_type (),
						 "points", points,
						 "fill_color", "black",
						 "width_units", 1.0,
						 NULL);
	    gnome_canvas_item_lower_to_bottom(e->lines[n]);
	    gnome_canvas_points_free(points);
	}
    }

    gtk_widget_queue_draw(GTK_WIDGET(e->canvas)); // Is needed for anti-aliased canvas
}
		       
#endif

static void
envelope_box_block_loop_spins (EnvelopeBox *e,
			       int block)
{
    void (*func) (GtkObject*, gpointer);

    func = block ? gtk_signal_handler_block_by_data : gtk_signal_handler_unblock_by_data;

    func(GTK_OBJECT(e->spin_length), e);
    func(GTK_OBJECT(e->spin_pos), e);
    func(GTK_OBJECT(e->spin_offset), e);
    func(GTK_OBJECT(e->spin_value), e);
    func(GTK_OBJECT(e->spin_sustain), e);
    func(GTK_OBJECT(e->spin_loop_start), e);
    func(GTK_OBJECT(e->spin_loop_end), e);
}

static int
envelope_box_insert_point (EnvelopeBox *e,
			   int before,
			   int pos,
			   int val)
{
    /* Check if there's enough room */
    if(e->current->num_points == ST_MAX_ENVELOPE_POINTS)
	return FALSE;
    if(!(before >= 1 && before <= e->current->num_points))
	return FALSE;
    if(pos <= e->current->points[before - 1].pos)
	return FALSE;
    if(before < e->current->num_points && pos >= e->current->points[before].pos)
	return FALSE;

    // Update envelope structure
    memmove(&e->current->points[before + 1], &e->current->points[before],
	    (ST_MAX_ENVELOPE_POINTS - 1 - before) * sizeof(e->current->points[0]));
    e->current->points[before].pos = pos;
    e->current->points[before].val = val;
    e->current->num_points++;

    // Update GUI
    gtk_spin_button_set_value(e->spin_length, e->current->num_points);
    spin_length_changed(e->spin_length, e);
    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_pos, before);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[before].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[before].val);
    envelope_box_block_loop_spins(e, FALSE);
    xm_set_modified(1);

#ifdef USE_GNOME
    // Update Canvas
    memmove(&e->points[before + 1], &e->points[before], (ST_MAX_ENVELOPE_POINTS - 1 - before) * sizeof(e->points[0]));
    memmove(&e->lines[before + 1], &e->lines[before], (ST_MAX_ENVELOPE_POINTS - 1 - before) * sizeof(e->lines[0]));
    e->lines[before] = NULL;
    envelope_box_canvas_add_point(e, before);
#endif

    return TRUE;
}

static void
envelope_box_delete_point (EnvelopeBox *e,
			   int n)
{
    if(!(n >= 1 && n < e->current->num_points))
	return;

    // Update envelope structure
    memmove(&e->current->points[n], &e->current->points[n + 1],
	    (ST_MAX_ENVELOPE_POINTS - 1 - n) * sizeof(e->current->points[0]));
    e->current->num_points--;

    // Update GUI
    gtk_spin_button_set_value(e->spin_length, e->current->num_points);
    spin_length_changed(e->spin_length, e);
    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_pos, n);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[n].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[n].val);
    envelope_box_block_loop_spins(e, FALSE);
    xm_set_modified(1);

#ifdef USE_GNOME
    // Update Canvas
    gtk_object_destroy(GTK_OBJECT(e->points[n]));
    gtk_object_destroy(GTK_OBJECT(e->lines[n-1]));
    memmove(&e->points[n], &e->points[n + 1], (ST_MAX_ENVELOPE_POINTS - 1 - n) * sizeof(e->points[0]));
    e->points[ST_MAX_ENVELOPE_POINTS - 1] = NULL;
    memmove(&e->lines[n - 1], &e->lines[n], (ST_MAX_ENVELOPE_POINTS - 1 - n) * sizeof(e->lines[0]));
    e->lines[ST_MAX_ENVELOPE_POINTS - 2] = NULL;
    if(e->lines[n-1]) {
	GNOME_CANVAS_LINE(e->lines[n - 1])->coords[0] = (double)e->current->points[n-1].pos;
	GNOME_CANVAS_LINE(e->lines[n - 1])->coords[1] = (double)(64 - e->current->points[n-1].val);
	gnome_canvas_item_request_update(e->lines[n - 1]);
    }
    gtk_widget_queue_draw(GTK_WIDGET(e->canvas)); // Is needed for anti-aliased canvas
    envelope_box_canvas_set_max_x(e, e->current->points[e->current->num_points - 1].pos);
#endif
}

/* We assume here that the movement is valid! */
static void
envelope_box_move_point (EnvelopeBox *e,
			 int n,
			 int dpos,
			 int dval)
{
    // Update envelope structure
    e->current->points[n].pos += dpos;
    e->current->points[n].val += dval;

    // Update GUI
    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[n].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[n].val);
    envelope_box_block_loop_spins(e, FALSE);
    xm_set_modified(1);

#ifdef USE_GNOME
    // Update Canvas
    gnome_canvas_item_move(e->points[n], dpos, -dval);
    if(n < e->current->num_points - 1) {
	GNOME_CANVAS_LINE(e->lines[n])->coords[0] += dpos;
	GNOME_CANVAS_LINE(e->lines[n])->coords[1] -= dval;
	gnome_canvas_item_request_update(e->lines[n]);
    }
    if(n > 0) {
	GNOME_CANVAS_LINE(e->lines[n-1])->coords[2] += dpos;
	GNOME_CANVAS_LINE(e->lines[n-1])->coords[3] -= dval;
	gnome_canvas_item_request_update(e->lines[n-1]);
    }
#endif
}

#ifdef USE_GNOME

/* This function returns world coordinates for a click on an item, if
   it is given, or else (if item == NULL), assumes the click was on
   the root canvas (event->button.x/y contain screen pixel coords in
   that case). A little confusing, I admit. */

static void
envelope_box_get_world_coords (GnomeCanvasItem *item,
			       GdkEvent *event,
			       EnvelopeBox *e,
			       double *worldx,
			       double *worldy)
{
    if(item == NULL) {
	gnome_canvas_window_to_world(e->canvas, event->button.x, event->button.y, worldx, worldy);
    } else {
	*worldx = event->button.x;
	*worldy = event->button.y;
    }
}

static gboolean
envelope_box_canvas_point_out_of_sight (EnvelopeBox *e,
					int xpos)
{
    double xposwindow;

    gnome_canvas_world_to_window(e->canvas, xpos, 0, &xposwindow, NULL);

    return xposwindow < 0 || xposwindow >= GTK_WIDGET(e->canvas)->allocation.width;
}

static void
envelope_box_initialize_point_dragging (EnvelopeBox *e,
					GnomeCanvasItem *eventitem,
					GdkEvent *event,
					GnomeCanvasItem *point)
{
    GdkCursor *cursor;
    int i;
    double x, y;

    envelope_box_get_world_coords(eventitem, event, e, &x, &y);

    cursor = gdk_cursor_new (GDK_FLEUR);
    gnome_canvas_item_grab (point,
			    GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
			    cursor,
			    event->button.time);
    gdk_cursor_destroy (cursor);

    e->dragfromx = x;
    e->dragfromy = y;

    gnome_canvas_get_scroll_offsets(e->canvas, &e->dragging_canvas_from_x, &e->dragging_canvas_from_y);

    e->dragpoint = -1;
    for(i = 0; i < 12; i++) {
	if(e->points[i] == point) {
	    e->dragpoint = i;
	    break;
	}
    }
    g_assert(e->dragpoint != -1);

    e->dragging_item_from_x = e->current->points[e->dragpoint].pos;
    e->dragging_item_from_y = e->current->points[e->dragpoint].val;

    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_pos, e->dragpoint);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[e->dragpoint].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[e->dragpoint].val);
    envelope_box_block_loop_spins(e, FALSE);
}

static void
envelope_box_stop_point_dragging (EnvelopeBox *e,
				  GdkEvent *event)
{
    if(e->dragpoint == -1)
	return;

    gnome_canvas_item_ungrab(e->points[e->dragpoint], event->button.time);

    /* Shrink scrolling region horizontally, if necessary */
    if(e->dragpoint == e->current->num_points - 1 && e->current->points[e->dragpoint].pos < e->canvas_max_x) {
	envelope_box_canvas_set_max_x(e, e->current->points[e->dragpoint].pos);
    }

    /* If new location is out of sight, jump there */
    if(envelope_box_canvas_point_out_of_sight(e, e->current->points[e->dragpoint].pos)) {
	int dx = e->current->points[e->dragpoint].pos - e->dragging_item_from_x;
	int dy = e->current->points[e->dragpoint].val - e->dragging_item_from_y;
	double zoom = e->zoomfactor_base * e->zoomfactor_mult;
	gnome_canvas_scroll_to(e->canvas,
			       e->dragging_canvas_from_x + dx * zoom,
			       e->dragging_canvas_from_y - dy * zoom);
    }
}

static gint
envelope_box_canvas_event (GnomeCanvas *canvas,
			   GdkEvent *event,
			   gpointer data)
{
    EnvelopeBox *e = ENVELOPE_BOX(data);
    double x, y;
    GnomeCanvasItem *item;
    GdkCursor *cursor;

    int i, insert_after = -1;

    switch (event->type) {
    case GDK_BUTTON_PRESS:
	if(event->button.button == 2) {
	    /* Middle button */
	    if(event->button.state & GDK_SHIFT_MASK) {
		/* Zoom in/out */
		cursor = gdk_cursor_new (GDK_SIZING);
		gnome_canvas_item_grab (GNOME_CANVAS_ITEM(e->group),
					GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
					cursor,
					event->button.time);
		gdk_cursor_destroy (cursor);

		e->zooming_canvas = TRUE;
		e->dragfromy = event->button.y;
		e->zooming_canvas_from_val = e->zoomfactor_mult;
	    } else {
		/* Drag canvas */
		cursor = gdk_cursor_new (GDK_FLEUR);
		gnome_canvas_item_grab (GNOME_CANVAS_ITEM(e->group),
					GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
					cursor,
					event->button.time);
		gdk_cursor_destroy (cursor);

		e->dragging_canvas = TRUE;
		e->dragfromx = event->button.x;
		e->dragfromy = event->button.y;
		gnome_canvas_get_scroll_offsets(e->canvas, &e->dragging_canvas_from_x, &e->dragging_canvas_from_y);
	    }
	    return TRUE;
	}

	if(event->button.button == 1) {
	    /* Find out where to insert new point */
	    gnome_canvas_window_to_world(canvas, event->button.x, event->button.y, &x, &y);
	    item = gnome_canvas_get_item_at(canvas, x, y);

	    for(i = 0, insert_after = -1; i < e->current->num_points && e->points[i]; i++) {
		if(e->points[i] == item) {
		    /* An already existing point has been selected. Will
		       be handled by envelope_box_point_event(). */
		    return FALSE;
		}
		if(e->current->points[i].pos < (int)x) {
		    insert_after = i;
		}
	    }

	    if(insert_after != -1 && y >= 0 && y < 64) {
		/* Insert new point and start dragging */
		envelope_box_insert_point(e, insert_after + 1, (int)x, 64 - y);
		envelope_box_initialize_point_dragging(e, NULL, event, e->points[insert_after + 1]);
		return TRUE;
	    }
	}
	break;

    case GDK_BUTTON_RELEASE:
	if(event->button.button == 2) {
	    gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM(e->group), event->button.time);
	    e->dragging_canvas = FALSE;
	    e->zooming_canvas = FALSE;
	    return TRUE;
	} else if(event->button.button == 1) {
	    envelope_box_stop_point_dragging(e, event);
	}
	break;

    case GDK_MOTION_NOTIFY:
	if(e->dragging_canvas) {
	    int dx = event->motion.x - e->dragfromx;
	    int dy = event->motion.y - e->dragfromy;
	    gnome_canvas_scroll_to(e->canvas, (e->dragging_canvas_from_x - dx), e->dragging_canvas_from_y - dy);
	} else if(e->zooming_canvas) {
	    int dy = event->motion.y - e->dragfromy;
	    e->zoomfactor_mult = e->zooming_canvas_from_val + (-(double)dy / 20);
	    if(e->zoomfactor_mult < 1.0) {
		e->zoomfactor_mult = 1.0;
	    }
	    gnome_canvas_set_pixels_per_unit(e->canvas, e->zoomfactor_base * e->zoomfactor_mult);
	}
	break;

    default:
	break;
    }

    return FALSE;
}

static gint
envelope_box_point_event (GnomeCanvasItem *item,
			  GdkEvent *event,
			  gpointer data)
{
    EnvelopeBox *e = ENVELOPE_BOX(data);

    switch (event->type) {
    case GDK_ENTER_NOTIFY:
	gnome_canvas_item_set(item, "fill_color", "#ffff00", NULL);
	break;
	
    case GDK_LEAVE_NOTIFY:
	if(!(event->crossing.state & GDK_BUTTON1_MASK)) {
	    gnome_canvas_item_set(item, "fill_color", "#ff0000", NULL);
	}
	break;
	
    case GDK_BUTTON_PRESS:
	if(event->button.button == 1) {
	    envelope_box_initialize_point_dragging(e, item, event, item);
	    return TRUE;
	}
	break;
	
    case GDK_BUTTON_RELEASE:
	if(event->button.button == 1) {
	    envelope_box_stop_point_dragging(e, event);
	    return TRUE;
	}
	break;

    case GDK_MOTION_NOTIFY:
	if(e->dragpoint != -1 && event->motion.state & GDK_BUTTON1_MASK) {
	    /* Snap movement to integer grid */
	    int dx, dy;

	    dx = event->motion.x - e->dragfromx;
	    dy = e->dragfromy - event->motion.y;

	    if(dx || dy) {
		envelope_box_clip_point_movement(e, e->dragpoint, &dx, &dy);

		envelope_box_move_point(e, e->dragpoint, dx, dy);

		e->dragfromx += dx;
		e->dragfromy -= dy;

		/* Expand scrolling region horizontally, if necessary */
		if(e->dragpoint == e->current->num_points - 1 && e->current->points[e->dragpoint].pos > e->canvas_max_x) {
		    envelope_box_canvas_set_max_x(e, e->current->points[e->dragpoint].pos);
		}
	    }
	}
	break;

    default:
	break;
    }
    
    return FALSE;
}
#endif

void envelope_box_set_envelope(EnvelopeBox *e, STEnvelope *env)
{
    int i;
    int m = xm_get_modified();

    g_return_if_fail(e != NULL);

    if(env == NULL) {
	env = &dummy_envelope;
    }

    e->current = env;

    // Some preliminary Paranoia...
    g_assert(env->num_points >= 1 && env->num_points <= ST_MAX_ENVELOPE_POINTS);
    for(i = 0; i < env->num_points; i++) {
	int h = env->points[i].val;
	g_assert(h >= 0 && h <= 64);
    }

    // Update GUI
    gtk_spin_button_set_value(e->spin_length, env->num_points);
    spin_length_changed(e->spin_length, e);
    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_pos, 0);
    gtk_spin_button_set_value(e->spin_offset, env->points[0].pos);
    gtk_spin_button_set_value(e->spin_value, env->points[0].val);
    gtk_spin_button_set_value(e->spin_sustain, env->sustain_point);
    gtk_spin_button_set_value(e->spin_loop_start, env->loop_start);
    gtk_spin_button_set_value(e->spin_loop_end, env->loop_end);
    envelope_box_block_loop_spins(e, FALSE);

    gtk_toggle_button_set_state(e->enable, env->flags & EF_ON);
    gtk_toggle_button_set_state(e->sustain, env->flags & EF_SUSTAIN);
    gtk_toggle_button_set_state(e->loop, env->flags & EF_LOOP);

#ifdef USE_GNOME
    // Update Canvas
    for(i = 0; i < (sizeof(e->points) / sizeof(e->points[0])) && e->points[i]; i++) {
	gtk_object_destroy(GTK_OBJECT(e->points[i]));
	e->points[i] = NULL;
    }
    for(i = 0; i < (sizeof(e->lines) / sizeof(e->lines[0])) && e->lines[i]; i++) {
	gtk_object_destroy(GTK_OBJECT(e->lines[i]));
	e->lines[i] = NULL;
    }
    for(i = 0; i < env->num_points; i++) {
	envelope_box_canvas_add_point(e, i);
    }
    envelope_box_canvas_set_max_x(e, env->points[env->num_points - 1].pos);
#endif

    xm_set_modified(m);
}

static void handle_toggle_button(GtkToggleButton *t, EnvelopeBox *e)
{
    int flag = 0;

    if(t == e->enable)
	flag = EF_ON;
    else if(t == e->sustain)
	flag = EF_SUSTAIN;
    else if(t == e->loop)
	flag = EF_LOOP;

    g_return_if_fail(flag != 0);

    if(t->active)
	e->current->flags |= flag;
    else
	e->current->flags &= ~flag;

    xm_set_modified(1);
}

static void handle_spin_button(GtkSpinButton *s, EnvelopeBox *e)
{
    unsigned char *p = NULL;

    if(s == e->spin_sustain)
	p = &e->current->sustain_point;
    else if(s == e->spin_loop_start)
	p = &e->current->loop_start;
    else if(s == e->spin_loop_end)
	p = &e->current->loop_end;

    g_return_if_fail(p != NULL);

    *p = gtk_spin_button_get_value_as_int(s);

    xm_set_modified(1);
}

static void
spin_length_changed (GtkSpinButton *spin,
		     EnvelopeBox *e)
{
    int newval = gtk_spin_button_get_value_as_int(spin);

    while(newval < e->current->num_points) {
	envelope_box_delete_point(e, e->current->num_points - 1);
    }

    while(newval > e->current->num_points) {
	envelope_box_insert_point(e, e->current->num_points,
				  e->current->points[e->current->num_points - 1].pos + 10,
				  e->current->points[e->current->num_points - 1].val);
    }

    envelope_box_block_loop_spins(e, TRUE);
    gui_update_spin_adjustment(e->spin_pos, 0, newval - 1);
    gui_update_spin_adjustment(e->spin_sustain, 0, newval - 1);
    gui_update_spin_adjustment(e->spin_loop_start, 0, newval - 1);
    gui_update_spin_adjustment(e->spin_loop_end, 0, newval - 1);
    envelope_box_block_loop_spins(e, FALSE);
}

static void
spin_pos_changed (GtkSpinButton *spin,
		  EnvelopeBox *e)
{
    int m = xm_get_modified();
    int p = gtk_spin_button_get_value_as_int(e->spin_pos);

    g_assert(p >= 0 && p < e->current->num_points);

    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[p].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[p].val);
    envelope_box_block_loop_spins(e, FALSE);
    xm_set_modified(m);
}

static void
spin_offset_changed (GtkSpinButton *spin,
		     EnvelopeBox *e)
{
    int p = gtk_spin_button_get_value_as_int(e->spin_pos);
    int dx;

    g_assert(p >= 0 && p < e->current->num_points);

    dx = gtk_spin_button_get_value_as_int(spin) - e->current->points[p].pos;

    envelope_box_clip_point_movement(e, p, &dx, NULL);
    envelope_box_move_point(e, p, dx, 0);

#ifdef USE_GNOME
    // Horizontal adjustment of scrolling region
    if(p == e->current->num_points - 1) {
	envelope_box_canvas_set_max_x(e, e->current->points[p].pos);
    }
#endif
}

static void
spin_value_changed (GtkSpinButton *spin,
		    EnvelopeBox *e)
{
    int p = gtk_spin_button_get_value_as_int(e->spin_pos);
    int dy;

    g_assert(p >= 0 && p < e->current->num_points);

    dy = gtk_spin_button_get_value_as_int(spin) - e->current->points[p].val;

    envelope_box_clip_point_movement(e, p, NULL, &dy);
    envelope_box_move_point(e, p, 0, dy);
}

static void
insert_clicked (GtkWidget *w,
		EnvelopeBox *e)
{
    int pos = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(e->spin_pos));

    envelope_box_insert_point(e, pos, e->current->points[pos].pos - 1, e->current->points[pos].val);
}

static void
delete_clicked (GtkWidget *w,
		EnvelopeBox *e)
{
    envelope_box_delete_point(e, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(e->spin_pos)));
}

GtkWidget* envelope_box_new(const gchar *label)
{
    EnvelopeBox *e;
    GtkWidget *box2, *thing, *box3, *box4, *frame;
#ifdef USE_GNOME
    GtkWidget *table, *canvas;
#endif

    e = gtk_type_new(envelope_box_get_type());
    GTK_BOX(e)->spacing = 2;
    GTK_BOX(e)->homogeneous = FALSE;

    box2 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(e), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    thing = gtk_check_button_new_with_label(label);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(handle_toggle_button), e);
    e->enable = GTK_TOGGLE_BUTTON(thing);

    add_empty_hbox(box2);

    box2 = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(e), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    /* Numerical list editing fields */
    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    gui_put_labelled_spin_button(box3, _("Length"), 1, 12, (GtkWidget**)&e->spin_length, spin_length_changed, e);
    gui_put_labelled_spin_button(box3, _("Current"), 0, 11, (GtkWidget**)&e->spin_pos, spin_pos_changed, e);
    gui_put_labelled_spin_button(box3, _("Offset"), 0, 65535, (GtkWidget**)&e->spin_offset, spin_offset_changed, e);
    gui_put_labelled_spin_button(box3, _("Value"), 0, 64, (GtkWidget**)&e->spin_value, spin_value_changed, e);

    box4 = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(box3), box4, FALSE, TRUE, 0);
    gtk_widget_show(box4);

    thing = gtk_button_new_with_label(_("Insert"));
    gtk_box_pack_start(GTK_BOX(box4), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(insert_clicked), e);
    
    thing = gtk_button_new_with_label(_("Delete"));
    gtk_box_pack_start(GTK_BOX(box4), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(delete_clicked), e);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    // Here comes the graphical stuff
#ifdef USE_GNOME
// gnome-libs-1.0 explicitly states that AA canvas is buggy
// gnome-libs-1.2 doesn't change anything
// gnome-libs-1.4 doesn't even work (program hangs in infinite loop)
//    if(gui_settings.gui_use_aa_canvas) { // gnome-libs-1.0.12 says aa canvas is buggy
//	gtk_widget_push_visual (gdk_rgb_get_visual ());
//	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
//	canvas = gnome_canvas_new_aa ();
//    } else {
	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	canvas = gnome_canvas_new ();
//    }
    e->canvas = GNOME_CANVAS(canvas);
    gtk_widget_pop_colormap ();
    gtk_widget_pop_visual ();

    memset(e->points, 0, sizeof(e->points));
    memset(e->lines, 0, sizeof(e->lines));
    e->zoomfactor_base = 0.0;
    e->zoomfactor_mult = 1.0;
    e->dragpoint = -1;

    envelope_box_canvas_paint_grid(e);

    e->group = GNOME_CANVAS_GROUP (gnome_canvas_item_new (gnome_canvas_root(e->canvas),
							  gnome_canvas_group_get_type (),
							  "x", 0.0,
							  "y", 0.0,
							  NULL));

    gtk_signal_connect_after (GTK_OBJECT (canvas), "event",
			      (GtkSignalFunc) envelope_box_canvas_event,
			      e);

    gtk_signal_connect_after(GTK_OBJECT(canvas), "size_allocate",
			     GTK_SIGNAL_FUNC(envelope_box_canvas_size_allocate), e);

    table = gtk_table_new (2, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE (table), 4);
    gtk_table_set_col_spacings (GTK_TABLE (table), 4);
    gtk_box_pack_start (GTK_BOX (box2), table, TRUE, TRUE, 0);
    gtk_widget_show (table);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_table_attach (GTK_TABLE (table), frame,
		      0, 1, 0, 1,
		      GTK_EXPAND | GTK_FILL | GTK_SHRINK,
		      GTK_EXPAND | GTK_FILL | GTK_SHRINK,
		      0, 0);
    gtk_widget_show (frame);

    gtk_widget_set_usize (canvas, 30, 64);
    gtk_container_add (GTK_CONTAINER (frame), canvas);
    gtk_widget_show (canvas);

    thing = gtk_hscrollbar_new (GTK_LAYOUT (canvas)->hadjustment);
    gtk_table_attach (GTK_TABLE (table), thing,
		      0, 1, 1, 2,
		      GTK_EXPAND | GTK_FILL | GTK_SHRINK,
		      GTK_FILL,
		      0, 0);
    gtk_widget_show (thing);

    thing = gtk_vscrollbar_new (GTK_LAYOUT (canvas)->vadjustment);
    gtk_table_attach (GTK_TABLE (table), thing,
		      1, 2, 0, 1,
		      GTK_FILL,
		      GTK_EXPAND | GTK_FILL | GTK_SHRINK,
		      0, 0);
    gtk_widget_show (thing);

#else /* !USE_GNOME */

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_widget_show (frame);

    gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);

    thing = gtk_label_new(_("Graphical\nEnvelope\nEditor\nonly in\nGNOME Version"));
    gtk_widget_show(thing);
    gtk_container_add (GTK_CONTAINER (frame), thing);

#endif /* defined(USE_GNOME) */

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    /* Sustain / Loop widgets */
    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    thing = gtk_check_button_new_with_label(_("Sustain"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(handle_toggle_button), e);
    e->sustain = GTK_TOGGLE_BUTTON(thing);

    gui_put_labelled_spin_button(box3, _("Point"), 0, 11, (GtkWidget**)&e->spin_sustain, handle_spin_button, e);

    thing = gtk_check_button_new_with_label(_("Loop"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(handle_toggle_button), e);
    e->loop = GTK_TOGGLE_BUTTON(thing);

    gui_put_labelled_spin_button(box3, _("Start"), 0, 11, (GtkWidget**)&e->spin_loop_start, handle_spin_button, e);
    gui_put_labelled_spin_button(box3, _("End"), 0, 11, (GtkWidget**)&e->spin_loop_end, handle_spin_button, e);

    return GTK_WIDGET(e);
}

guint envelope_box_get_type()
{
    static guint envelope_box_type = 0;
    
    if (!envelope_box_type) {
	GtkTypeInfo envelope_box_info =
	{
	    "EnvelopeBox",
	    sizeof(EnvelopeBox),
	    sizeof(EnvelopeBoxClass),
	    (GtkClassInitFunc) NULL,
	    (GtkObjectInitFunc) NULL,
	    (GtkArgSetFunc) NULL,
	    (GtkArgGetFunc) NULL,
	};
	
	envelope_box_type = gtk_type_unique(gtk_vbox_get_type (), &envelope_box_info);
    }
    
    return envelope_box_type;
}


