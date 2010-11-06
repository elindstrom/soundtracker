
/*
 * The Real SoundTracker - GTK+ Tracker widget
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

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <glib/gprintf.h>

#include "tracker.h"
#include "main.h"
#include "gui.h"
#include "gui-settings.h"

const char * const notenames[4][96] = {{
    "C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "H-0",
    "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "H-1",
    "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "H-2",
    "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "H-3",
    "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "H-4",
    "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "H-5",
    "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6", "G#6", "A-6", "A#6", "H-6",
    "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "A-7", "A#7", "H-7",
},{
    "C-0", "Db0", "D-0", "Eb0", "E-0", "F-0", "Gb0", "G-0", "Ab0", "A-0", "B-0", "H-0",
    "C-1", "Db1", "D-1", "Eb1", "E-1", "F-1", "Gb1", "G-1", "Ab1", "A-1", "B-1", "H-1",
    "C-2", "Db2", "D-2", "Eb2", "E-2", "F-2", "Gb2", "G-2", "Ab2", "A-2", "B-2", "H-2",
    "C-3", "Db3", "D-3", "Eb3", "E-3", "F-3", "Gb3", "G-3", "Ab3", "A-3", "B-3", "H-3",
    "C-4", "Db4", "D-4", "Eb4", "E-4", "F-4", "Gb4", "G-4", "Ab4", "A-4", "B-4", "H-4",
    "C-5", "Db5", "D-5", "Eb5", "E-5", "F-5", "Gb5", "G-5", "Ab5", "A-5", "B-5", "H-5",
    "C-6", "Db6", "D-6", "Eb6", "E-6", "F-6", "Gb6", "G-6", "Ab6", "A-6", "B-6", "H-6",
    "C-7", "Db7", "D-7", "Eb7", "E-7", "F-7", "Gb7", "G-7", "Ab7", "A-7", "B-7", "H-7",
},{
    "C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "B-0",
    "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "B-1",
    "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "B-2",
    "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3",
    "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4",
    "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5",
    "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6", "G#6", "A-6", "A#6", "B-6",
    "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "A-7", "A#7", "B-7",
},{
    "C-0", "Db0", "D-0", "Eb0", "E-0", "F-0", "Gb0", "G-0", "Ab0", "A-0", "Bb0", "B-0",
    "C-1", "Db1", "D-1", "Eb1", "E-1", "F-1", "Gb1", "G-1", "Ab1", "A-1", "Bb1", "B-1",
    "C-2", "Db2", "D-2", "Eb2", "E-2", "F-2", "Gb2", "G-2", "Ab2", "A-2", "Bb2", "B-2",
    "C-3", "Db3", "D-3", "Eb3", "E-3", "F-3", "Gb3", "G-3", "Ab3", "A-3", "Bb3", "B-3",
    "C-4", "Db4", "D-4", "Eb4", "E-4", "F-4", "Gb4", "G-4", "Ab4", "A-4", "Bb4", "B-4",
    "C-5", "Db5", "D-5", "Eb5", "E-5", "F-5", "Gb5", "G-5", "Ab5", "A-5", "Bb5", "B-5",
    "C-6", "Db6", "D-6", "Eb6", "E-6", "F-6", "Gb6", "G-6", "Ab6", "A-6", "Bb6", "B-6",
    "C-7", "Db7", "D-7", "Eb7", "E-7", "F-7", "Gb7", "G-7", "Ab7", "A-7", "Bb7", "B-7",
}};

static void init_display(Tracker *t, int width, int height);

static const int default_colors[] = {
    10, 20, 30,
    100, 100, 100,
    70, 70, 70,
    50, 50, 50,
    50, 60, 70,
    230, 230, 230,
    170, 170, 200,
    230, 200, 0,
    250, 100, 50,
    250, 200, 50,
};

enum {
    SIG_PATPOS,
    SIG_XPANNING,
    SIG_MAINMENU_BLOCKMARK_SET,
    LAST_SIGNAL
};

#define CLEAR(win, x, y, w, h) \
do { gdk_draw_rectangle(win, t->bg_gc, TRUE, x, y, w, h); } while(0)

static guint tracker_signals[LAST_SIGNAL] = { 0 };

static gint tracker_idle_draw_function (Tracker *t);

static void
tracker_idle_draw (Tracker *t)
{
    if(!t->idle_handler) {
	t->idle_handler = gtk_idle_add((GtkFunction)tracker_idle_draw_function,
				       (gpointer)t);
	g_assert(t->idle_handler != 0);
    }
}

void
tracker_set_num_channels (Tracker *t,
			  int n)
{
    GtkWidget *widget = GTK_WIDGET(t);

    t->num_channels = n;
    if(GTK_WIDGET_REALIZED(widget)) {
	init_display(t, widget->allocation.width, widget->allocation.height);
	gtk_widget_queue_draw(widget);
    }
    gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_XPANNING], t->leftchan, t->num_channels, t->disp_numchans);
}

void
tracker_set_patpos (Tracker *t,
		    int row)
{
    g_return_if_fail(t != NULL);
    g_return_if_fail((t->curpattern == NULL && row == 0) || (row < t->curpattern->length));

    if(t->patpos != row) {
	t->patpos = row;
	if(t->curpattern != NULL) {
	    if(t->inSelMode) {
		/* Force re-draw of patterns in block selection mode */
		gtk_widget_queue_draw(GTK_WIDGET(t));
	    } else {
		tracker_idle_draw(t);
	    }
	    gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_PATPOS], row, t->curpattern->length, t->disp_rows);
	}
    }
}

void
tracker_redraw (Tracker *t)
{
    gtk_widget_queue_draw(GTK_WIDGET(t));
}

void
tracker_redraw_row (Tracker *t,
		    int row)
{
    // This is yet to be optimized :-)
    tracker_redraw(t);
}

void
tracker_redraw_current_row (Tracker *t)
{
    tracker_redraw_row(t, t->patpos);
}

void
tracker_set_pattern (Tracker *t,
		     XMPattern *pattern)
{
    g_return_if_fail(t != NULL);

    if(t->curpattern != pattern) {
	t->curpattern = pattern;
	if(pattern != NULL) {
	    if(t->patpos >= pattern->length)
		t->patpos = pattern->length - 1;
	    gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_PATPOS], t->patpos, pattern->length, t->disp_rows);
	}
	gtk_widget_queue_draw(GTK_WIDGET(t));
    }
}

void
tracker_set_backing_store (Tracker *t,
			   int on)
{
    GtkWidget *widget;

    g_return_if_fail(t != NULL);

    if(on == t->enable_backing_store)
	return;

    t->enable_backing_store = on;

    widget = GTK_WIDGET(t);
    if(GTK_WIDGET_REALIZED(widget)) {
	if(on) {
	    t->pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
	    CLEAR(t->pixmap, 0, 0, widget->allocation.width, widget->allocation.height);
	} else {
	    gdk_pixmap_unref(t->pixmap);
	    t->pixmap = NULL;
	}

	gdk_gc_set_exposures(t->bg_gc, !on);
	gtk_widget_queue_draw(GTK_WIDGET(t));
    }
}

void
tracker_set_xpanning (Tracker *t,
		      int left_channel)
{
    GtkWidget *widget;

    g_return_if_fail(t != NULL);
	
    if(t->leftchan != left_channel) {
	widget = GTK_WIDGET(t);
	if(GTK_WIDGET_REALIZED(widget)) {
	    g_return_if_fail(left_channel + t->disp_numchans <= t->num_channels);

	    t->leftchan = left_channel;
	    gtk_widget_queue_draw(GTK_WIDGET(t));
	    
	    if(t->cursor_ch < t->leftchan)
		t->cursor_ch = t->leftchan;
	    else if(t->cursor_ch >= t->leftchan + t->disp_numchans)
		t->cursor_ch = t->leftchan + t->disp_numchans - 1;
	}
	gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_XPANNING], t->leftchan, t->num_channels, t->disp_numchans);
    }
}

static void
adjust_xpanning (Tracker *t)
{
    if(t->cursor_ch < t->leftchan)
	tracker_set_xpanning(t, t->cursor_ch);
    else if(t->cursor_ch >= t->leftchan + t->disp_numchans)
	tracker_set_xpanning(t, t->cursor_ch - t->disp_numchans + 1);
    else if(t->leftchan + t->disp_numchans > t->num_channels)
	tracker_set_xpanning(t, t->num_channels - t->disp_numchans);
}

void
tracker_step_cursor_item (Tracker *t,
			  int direction)
{
    g_return_if_fail(direction == -1 || direction == 1);
    
    t->cursor_item += direction;
    if(t->cursor_item & (~7)) {
	t->cursor_item &= 7;
	tracker_step_cursor_channel(t, direction);
    } else {
	adjust_xpanning(t);
	gtk_widget_queue_draw(GTK_WIDGET(t));
    }
}

void
tracker_set_cursor_item (Tracker *t,
                         int item)
{
    if (item < 0) item = 0;
    if (item > 7) item = 7;

    t->cursor_item = item;
    adjust_xpanning(t);
    gtk_widget_queue_draw(GTK_WIDGET(t));
}
 
void
tracker_step_cursor_channel (Tracker *t,
			     int direction)
{
    t->cursor_ch += direction;
    
    if(t->cursor_ch < 0)
	t->cursor_ch = t->num_channels - 1;
    else if(t->cursor_ch >= t->num_channels)
	t->cursor_ch = 0;

    adjust_xpanning(t);

    if(t->inSelMode) {
       /* Force re-draw of patterns in block selection mode */
       gtk_widget_queue_draw(GTK_WIDGET(t));
    } else {
       tracker_idle_draw(t);
    }
}

void
tracker_set_cursor_channel (Tracker *t,
                            int channel)
{
    t->cursor_ch = channel;

    if(t->cursor_ch < 0)
       t->cursor_ch = 0;
    else if(t->cursor_ch >= t->num_channels)
       t->cursor_ch = t->num_channels - 1;

    adjust_xpanning(t);

    if(t->inSelMode) {
	/* Force re-draw of patterns in block selection mode */
	gtk_widget_queue_draw(GTK_WIDGET(t));
    } else {
	tracker_idle_draw(t);
    }
}

void
tracker_step_cursor_row (Tracker *t,
			 int direction)
{
    int newpos = t->patpos + direction;

    while(newpos < 0)
	newpos += t->curpattern->length;
    newpos %= t->curpattern->length;

    tracker_set_patpos(t, newpos);
}

void
tracker_mark_selection (Tracker *t,
			gboolean enable)
{
    if(!enable) {
	t->sel_end_ch = t->cursor_ch;
	t->sel_end_row = t->patpos;
	t->inSelMode = FALSE;
    } else {
	t->sel_start_ch = t->sel_end_ch = t->cursor_ch;
    	t->sel_start_row = t->sel_end_row = t->patpos;
        t->inSelMode = TRUE;
	tracker_redraw(t);		
    }
}

void
tracker_clear_mark_selection (Tracker *t)
{
    if(t->sel_start_ch != -1) {
	t->sel_start_ch = t->sel_end_ch = -1;
	t->sel_start_row = t->sel_end_row = -1;
	t->inSelMode = FALSE;
	tracker_redraw(t);
    }
}

gboolean
tracker_is_in_selection_mode(Tracker *t)
{
    return t->inSelMode;
}

void
tracker_get_selection_rect(Tracker *t,
			   int *chStart,
			   int *rowStart,
			   int *nChannel,
			   int *nRows)
{
    if(!t->inSelMode) {
	if(t->sel_start_ch <= t->sel_end_ch) {
	    *nChannel = t->sel_end_ch - t->sel_start_ch + 1;
	    *chStart = t->sel_start_ch;
	} else {
	    *nChannel = t->sel_start_ch - t->sel_end_ch + 1;
	    *chStart = t->sel_end_ch;         	
	}
	if(t->sel_start_row <= t->sel_end_row) {
	    *nRows = t->sel_end_row - t->sel_start_row + 1;
	    *rowStart = t->sel_start_row;
	} else {
	    *nRows = t->sel_start_row - t->sel_end_row + 1;
	    *rowStart = t->sel_end_row;
	}	
    } else {
	if(t->sel_start_ch <= t->cursor_ch) {
	    *nChannel = t->cursor_ch - t->sel_start_ch + 1;
	    *chStart = t->sel_start_ch;
	} else {
	    *nChannel = t->sel_start_ch - t->cursor_ch + 1;
	    *chStart = t->cursor_ch;         	
	}
	if(t->sel_start_row <= t->patpos) {
	    *nRows = t->patpos - t->sel_start_row + 1;
	    *rowStart = t->sel_start_row;
	} else {
	    *nRows = t->sel_start_row - t->patpos + 1;
	    *rowStart = t->patpos;
	}	
    }
}

gboolean
tracker_is_valid_selection(Tracker *t)
{
    return (t->sel_start_ch >= 0 && t->sel_start_ch < xm->num_channels &&
	    t->sel_end_ch >= 0 && t->sel_end_ch < xm->num_channels &&
	    t->sel_start_row >= 0 && t->sel_start_row < t->curpattern->length &&
	    t->sel_end_row >= 0 && t->sel_end_row < t->curpattern->length);
}

static void
note2string (XMNote *note,
	     char *buf)
{
    static const char hexmapU[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
    };
    static const char hexmapL[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
        'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
    };

    const char *hexmap = gui_settings.tracker_upcase ? hexmapU : hexmapL;

    if(note->note > 0 && note->note < 97) {
	int index = (gui_settings.sharp ? 0 : 1) + (gui_settings.bh ? 2 : 0);
        buf[0] = notenames[index][note->note-1][0];
        buf[1] = notenames[index][note->note-1][1];
        buf[2] = notenames[index][note->note-1][2];
    } else if(note->note == 97) {
        buf[0] = '[';
        buf[1] = '-';
        buf[2] = ']';
    } else {
        buf[0] = gui_settings.tracker_line_format[0];
        buf[1] = gui_settings.tracker_line_format[1];
        buf[2] = gui_settings.tracker_line_format[2];
    }

    // Instrument number always displayed in Dec, because the spin
    // buttons use Dec as well.
    if(note->instrument >= 100) {
	buf[3] = '1';
    } else {
	buf[3] = ' ';
    }
    buf[4] = ((note->instrument / 10) % 10) + '0';
    buf[5] = (note->instrument % 10) + '0';
    
    if(!note->instrument) {
	buf[4] = gui_settings.tracker_line_format[3];
	buf[5] = gui_settings.tracker_line_format[4];
    }
    
    buf[6] = ' ';

    if(note->volume) {
	buf[7] = hexmap[note->volume / 16];
	buf[8] = hexmap[note->volume & 15];
    } else {
	buf[7] = gui_settings.tracker_line_format[5];
	buf[8] = gui_settings.tracker_line_format[6];
    }
    
    buf[9] = ' ';

    if(!note->fxtype && !note->fxparam) {
	buf[10] = gui_settings.tracker_line_format[7];
	buf[11] = gui_settings.tracker_line_format[8];
	buf[12] = gui_settings.tracker_line_format[9];
    } else {
	buf[10] = hexmap[note->fxtype];
	buf[11] = hexmap[note->fxparam / 16];
	buf[12] = hexmap[note->fxparam & 15];
    }
    
    buf[13] = ' ';
    buf[14] = ' ';
    buf[15] = 0;
}

static void
tracker_clear_notes_line (GtkWidget *widget,
			  GdkDrawable *win,
			  int y,
			  int pattern_row)
{
    Tracker *t = TRACKER(widget);
    GdkGC *gc;

    gc = t->bg_gc;

    if(pattern_row == t->patpos) {
	gc = t->bg_cursor_gc;      // cursor line
    } else if(gui_settings.highlight_rows) {
	if (pattern_row % gui_settings.highlight_rows_n == 0) {
	    gc = t->bg_majhigh_gc; // highlighted line
	} else if(pattern_row % gui_settings.highlight_rows_minor_n == 0) {
	    gc = t->bg_minhigh_gc; // minor highlighted line
	}
    }

    gdk_draw_rectangle(win, gc, TRUE, 0, y, widget->allocation.width, t->fonth);
}

static void
print_notes_line (GtkWidget *widget,
		  GdkDrawable *win,
		  int y,
		  int ch,
		  int numch,
		  int row)
{
    Tracker *t = TRACKER(widget);
    char buf[32*15];
    char *bufpt;
    int xBlock, BlockWidth, rowBlockStart, rowBlockEnd, chBlockStart, chBlockEnd;

    g_return_if_fail(ch + numch <= t->num_channels);

    tracker_clear_notes_line(widget, win, y, row);

    /* -- Draw selection highlighting if necessary -- */
    /* Calc starting and ending rows */
    if(t->inSelMode) {	
	if(t->sel_start_row < t->patpos) {
	    rowBlockStart = t->sel_start_row;
	    rowBlockEnd = t->patpos;
	} else {
	    rowBlockEnd = t->sel_start_row;
	    rowBlockStart = t->patpos;			
	}
    } else if(t->sel_start_row < t->sel_end_row) {
    	rowBlockStart = t->sel_start_row;
	rowBlockEnd = t->sel_end_row;
    } else {
	rowBlockEnd = t->sel_start_row;
	rowBlockStart = t->sel_end_row; 		
    }

    if(row >= rowBlockStart && row <= rowBlockEnd) {
	/* Calc bar origin and size */
	if(t->inSelMode) {
	    if(t->sel_start_ch <= t->cursor_ch)	{
		chBlockStart = t->sel_start_ch;
		chBlockEnd = t->cursor_ch;
	    } else {
		chBlockStart = t->cursor_ch;
		chBlockEnd = t->sel_start_ch;
	    }
	} else if(t->sel_start_ch <= t->sel_end_ch) {
	    chBlockStart = t->sel_start_ch;
	    chBlockEnd = t->sel_end_ch;
	} else	{
	    chBlockStart = t->sel_end_ch;
	    chBlockEnd = t->sel_start_ch;
	}
    	
	xBlock = t->disp_startx + (chBlockStart - ch) * t->disp_chanwidth;
	if(xBlock < 0 && chBlockEnd >= ch) {
	    xBlock = t->disp_startx;
	}
	BlockWidth = (chBlockEnd - (ch > chBlockStart ? ch : chBlockStart) + 1) * t->disp_chanwidth;
	if(BlockWidth > numch * t->disp_chanwidth) {
	    BlockWidth = (numch - (chBlockStart > ch ? chBlockStart - ch : 0)) * t->disp_chanwidth;
	}

	/* Draw only if in bounds */
        if(xBlock >= t->disp_startx && xBlock < t->disp_startx + numch * t->disp_chanwidth) {
	    gdk_gc_set_foreground(t->misc_gc, &t->colors[TRACKERCOL_BG_SELECTION]);
	    gdk_draw_rectangle(win, t->misc_gc, TRUE, xBlock, y,  BlockWidth, t->fonth);
	}
    }

    /* -- Draw the actual row contents -- */
    y += t->font->ascent + t->baselineskip;

    /* The row number */
    if(gui_settings.tracker_hexmode) {
	if(gui_settings.tracker_upcase) g_sprintf(buf, "%02X", row);
        else g_sprintf(buf, "%02x", row);
    } else {
	g_sprintf(buf, "%03d", row);
    }

    gdk_draw_string(win, t->font, t->notes_gc, 5, y, buf);
    
    /* The notes */
    for(numch += ch, bufpt = buf; ch < numch; ch++, bufpt += 14) {
	note2string(&t->curpattern->channels[ch][row], bufpt);
    }	

    gdk_draw_string(win, t->font, t->notes_gc, t->disp_startx, y, buf);
}

static void
print_notes_and_bars (GtkWidget *widget,
		      GdkDrawable *win,
		      int x,
		      int y,
		      int w,
		      int h,
		      int cursor_row)
{
    int scry;
    int n, n1, n2;
    int my;
    Tracker *t = TRACKER(widget);
    int i, x1, y1;

    /* Limit y and h to the actually used window portion */
    my = y - t->disp_starty;
    if(my < 0) {
	my = 0;
	h += my;                                                                                                           	
    }
    if(my + h > t->fonth * t->disp_rows) {
	h = t->fonth * t->disp_rows - my;
    }

    /* Calculate first and last line to be redrawn */
    n1 = my / t->fonth;
    n2 = (my + h - 1) / t->fonth;

    /* Print the notes */
    scry = t->disp_starty + n1 * t->fonth;
    for(i = n1; i <= n2; i++, scry += t->fonth) {
	n = i + cursor_row - t->disp_cursor;
	if(n >= 0 && n < t->curpattern->length) {
	    print_notes_line(widget, win, scry, t->leftchan, t->disp_numchans, n);
	} else {
	    CLEAR(win, 0, scry, widget->allocation.width, t->fonth);
	}
    }

    /* Draw the separation bars */
    gdk_gc_set_foreground(t->misc_gc, &t->colors[TRACKERCOL_BARS]);
    x1 = t->disp_startx - 2;
    y1 = t->disp_starty + n1 * t->fonth - t->fonth;
    h = (n2 - n1 + 2) * t->fonth;
    for(i = 0; i <= t->disp_numchans; i++, x1 += t->disp_chanwidth) {
	gdk_draw_line(win, t->misc_gc, x1, y1, x1, y1+h);
    }
}

static void
print_channel_numbers (GtkWidget *widget,
		       GdkDrawable *win)
{
    int x, y, i;
    char buf[4];
    Tracker *t = TRACKER(widget);

    if(0 /* !gui_settings.channel_numbering */) {
	return;
    }

    gdk_gc_set_foreground(t->misc_gc, &t->colors[TRACKERCOL_CHANNUMS]);

    x = t->disp_startx + (t->disp_chanwidth - (2 * t->fontw)) / 2; 
    y = t->disp_starty + t->font->ascent + t->baselineskip - t->fonth;
    for(i = 1; i <= t->disp_numchans; i++, x += t->disp_chanwidth) {
	g_sprintf(buf, "%2d", i + t->leftchan);
	gdk_draw_rectangle(win, t->bg_gc, TRUE, x, t->disp_starty - t->fonth, 2*t->fontw, t->fonth);
	gdk_draw_string(win, t->font, t->misc_gc, x, y, buf);
    }
}

static void
print_cursor (GtkWidget *widget,
	      GdkDrawable *win)
{
    int x, y, width;
    Tracker *t = TRACKER(widget);

    g_return_if_fail(t->cursor_ch >= t->leftchan && t->cursor_ch < t->leftchan + t->disp_numchans);
    g_return_if_fail((unsigned)t->cursor_item <= 7);

    width = 1;
    x = 0;
    y = t->disp_starty + t->disp_cursor * t->fonth;

    switch(t->cursor_item) {
    case 0: /* note */
	width = 3;
	break;
    case 1: /* instrument 0 */
	x = 4;
	break;
    case 2: /* instrument 1 */
	x = 5;
	break;
    case 3: /* volume 0 */
	x = 7;
	break;
    case 4: /* volume 1 */
	x = 8;
	break;
    case 5: /* effect 0 */
	x = 10;
	break;
    case 6: /* effect 1 */
	x = 11;
	break;
    case 7: /* effect 2 */
	x = 12;
	break;
    default:
	g_assert_not_reached();
	break;
    }

    x = x * t->fontw + t->disp_startx + (t->cursor_ch - t->leftchan) * t->disp_chanwidth - 1; 


	if (GTK_TOGGLE_BUTTON(editing_toggle)->active)
	    gdk_gc_set_foreground(t->misc_gc, &t->colors[TRACKERCOL_CURSOR_EDIT]);
	else
		gdk_gc_set_foreground(t->misc_gc, &t->colors[TRACKERCOL_CURSOR]);

    gdk_draw_rectangle(win, t->misc_gc, FALSE, x, y, width * t->fontw, t->fonth - 1);
}

static void
tracker_draw_clever (GtkWidget *widget,
		     GdkRectangle *area)
{
    Tracker *t;
    int dist, absdist;
    int y, redrawcnt;
    GdkWindow *win;
    int fonth;

    g_return_if_fail(widget != NULL);

    if(!GTK_WIDGET_VISIBLE(widget))
	return;

    t = TRACKER(widget);
    g_return_if_fail(t->curpattern != NULL);

    win = t->enable_backing_store ? (GdkWindow*)t->pixmap : widget->window;
    fonth = t->fonth;

    dist = t->patpos - t->oldpos;
    absdist = ABS(dist);
    t->oldpos = t->patpos;

    redrawcnt = t->disp_rows;
    y = t->disp_starty;

    if(absdist <= t->disp_cursor) {
	/* Before scrolling, redraw cursor line in the old picture;
	   better than scrolling first and then redrawing the old
	   cursor line (prevents flickering) */
	print_notes_and_bars(widget, win,
			     0, (t->disp_cursor) * fonth + t->disp_starty,
			     widget->allocation.width, fonth, t->oldpos - dist);
    }

    if(absdist < t->disp_rows) {
	/* this is interesting. we don't have to redraw the whole area, we can optimize
	   by scrolling around instead. */
	if(dist > 0) {
	    /* go down in pattern -- scroll up */
	    redrawcnt = absdist;
	    gdk_draw_drawable(win, t->bg_gc, win,
			      0, y + (absdist * fonth), 0, y,
			      widget->allocation.width, (t->disp_rows - absdist) * fonth);
	    y += (t->disp_rows - absdist) * fonth;
	} else if(dist < 0) {
	    /* go up in pattern -- scroll down */
	    if(1 /* gui_settings.channel_numbering */) {
		/* Redraw line displaying the channel numbers before scrolling down */
		print_notes_and_bars(widget, win,
				     0, t->disp_starty,
				     widget->allocation.width, fonth, t->oldpos - dist);
	    }
	    redrawcnt = absdist;
	    gdk_draw_drawable(win, t->bg_gc, win,
			      0, y, 0, y + (absdist * fonth),
			      widget->allocation.width, (t->disp_rows - absdist) * fonth);
	}
    }

    if(dist != 0) {
	print_notes_and_bars(widget, win, 0, y, widget->allocation.width, redrawcnt * fonth, t->oldpos);
	print_channel_numbers(widget, win);
    }

    /* update the cursor */
    print_notes_and_bars(widget, win,
			 0, t->disp_cursor * fonth + t->disp_starty,
			 widget->allocation.width, fonth, t->oldpos);
    print_cursor(widget, win);

    if(t->enable_backing_store) {
	gdk_draw_pixmap(widget->window, t->bg_gc, t->pixmap,
			area->x, area->y,
			area->x, area->y,
			area->width, area->height);
    }
}

static void
tracker_draw_stupid (GtkWidget *widget,
		     GdkRectangle *area)
{
    Tracker *t = TRACKER(widget);

    t->oldpos = -666;
    tracker_draw_clever(widget, area);
}

static gint
tracker_expose (GtkWidget *widget,
		GdkEventExpose *event)
{
    tracker_draw_stupid(widget, &event->area);
    return FALSE;
}

static gint
tracker_idle_draw_function (Tracker *t)
{
    GtkWidget *widget = GTK_WIDGET(t);
    GdkRectangle area = { 0, 0, widget->allocation.width, widget->allocation.height };

    if(GTK_WIDGET_MAPPED(GTK_WIDGET(t))) {
	tracker_draw_clever(GTK_WIDGET(t), &area);
    }

    gtk_idle_remove(t->idle_handler);
    t->idle_handler = 0;
    return TRUE;
}

static void
tracker_size_request (GtkWidget *widget,
		      GtkRequisition *requisition)
{
    Tracker *t = TRACKER(widget);
    requisition->width = 14 * t->fontw + 3*t->fontw + 10;
    requisition->height = 11*t->fonth;
}

static void
init_display (Tracker *t,
	      int width,
	      int height)
{
    GtkWidget *widget = GTK_WIDGET(t);
    int u;
    int line_numbers_space = 3 * t->fontw;

    if(!gui_settings.tracker_hexmode) {
	line_numbers_space += 1 * t->fontw; // Line numbers take up more space in decimal mode
    }

    t->disp_rows = height / t->fonth;
    if(!(t->disp_rows % 2))
	t->disp_rows--;
    t->disp_cursor = t->disp_rows / 2;
    t->disp_starty = (height - t->fonth * t->disp_rows) / 2 + t->fonth;
	t->disp_rows--;

    t->disp_chanwidth = 14 * t->fontw;
    u = width - line_numbers_space - 10;
    t->disp_numchans = u / t->disp_chanwidth;
    
    g_return_if_fail(xm != NULL);
    if(t->disp_numchans > t->num_channels)
	t->disp_numchans = t->num_channels;

    t->disp_startx = (u - t->disp_numchans * t->disp_chanwidth) / 2 + line_numbers_space + 5;
    adjust_xpanning(t);

    if(t->curpattern) {
	gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_PATPOS], t->patpos, t->curpattern->length, t->disp_rows);
	gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_XPANNING], t->leftchan, t->num_channels, t->disp_numchans);
    }
    
    if(t->enable_backing_store) {
	if(t->pixmap) {
	    gdk_pixmap_unref(t->pixmap);
	}
	t->pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
	CLEAR(t->pixmap, 0, 0, widget->allocation.width, widget->allocation.height);
    }
}

static void
tracker_size_allocate (GtkWidget *widget,
		       GtkAllocation *allocation)
{
    Tracker *t;
    
    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_TRACKER (widget));
    g_return_if_fail (allocation != NULL);

    widget->allocation = *allocation;
    if (GTK_WIDGET_REALIZED (widget)) {
	t = TRACKER (widget);
	
	gdk_window_move_resize (widget->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);

	init_display(t, allocation->width, allocation->height);
    }
}

void
tracker_reset (Tracker *t)
{
    GtkWidget *widget;

    g_return_if_fail(t != NULL);

    widget = GTK_WIDGET(t);
    if(GTK_WIDGET_REALIZED(widget)) {
	init_display(t, widget->allocation.width, widget->allocation.height);
	t->patpos = 0;
	t->cursor_ch = 0;
	t->cursor_item = 0;
	t->leftchan = 0;
	adjust_xpanning(t);
	gtk_widget_queue_draw(GTK_WIDGET(t));
    }
}

static void
init_colors (GtkWidget *widget)
{
    int n;
    const int *p;
    GdkColor *c;

    for (n = 0, p = default_colors, c = TRACKER(widget)->colors; n < TRACKERCOL_LAST; n++, c++) {
	c->red = *p++ * 65535 / 255;
	c->green = *p++ * 65535 / 255;
	c->blue = *p++ * 65535 / 255;
        c->pixel = (gulong)((c->red & 0xff00)*256 + (c->green & 0xff00) + (c->blue & 0xff00)/256);
        gdk_color_alloc(gtk_widget_get_colormap(widget), c);
    }
}

static void
tracker_realize (GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;
    Tracker *t;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_TRACKER (widget));
    
    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    t = TRACKER(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget) |
			GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
	 		GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

    attributes.visual = gtk_widget_get_visual (widget);
    attributes.colormap = gtk_widget_get_colormap (widget);
    
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);

    widget->style = gtk_style_attach (widget->style, widget->window);

    init_colors(widget);

    t->bg_gc = gdk_gc_new(widget->window);
    t->bg_cursor_gc = gdk_gc_new(widget->window);
    t->bg_majhigh_gc = gdk_gc_new(widget->window);
    t->bg_minhigh_gc = gdk_gc_new(widget->window);
    t->notes_gc = gdk_gc_new(widget->window);
    t->misc_gc = gdk_gc_new(widget->window);
    gdk_gc_set_foreground(t->bg_gc, &t->colors[TRACKERCOL_BG]);
    gdk_gc_set_foreground(t->bg_cursor_gc, &t->colors[TRACKERCOL_BG_CURSOR]);
    gdk_gc_set_foreground(t->bg_majhigh_gc, &t->colors[TRACKERCOL_BG_MAJHIGH]);
    gdk_gc_set_foreground(t->bg_minhigh_gc, &t->colors[TRACKERCOL_BG_MINHIGH]);
    gdk_gc_set_foreground(t->notes_gc, &t->colors[TRACKERCOL_NOTES]);

    if(!t->enable_backing_store)
	gdk_gc_set_exposures (t->bg_gc, 1); /* man XCopyArea, grep exposures */

    init_display(t, attributes.width, attributes.height);

    gdk_window_set_user_data (widget->window, widget);
    gdk_window_set_background(widget->window, &t->colors[TRACKERCOL_BG]);
}

gboolean
tracker_set_font (Tracker *t,
		  const gchar *fontname)
{
    GdkFont *font;
    int fonth, fontw;

    if((font = gdk_font_load(fontname))) {
	fonth = font->ascent + font->descent + t->baselineskip;
	fontw = gdk_string_width(font, "X"); /* let's just hope this is a non-proportional font */

	/* Some fonts have width 0, for example 'clearlyu' */
	if(fonth >= 1 && fontw >= 1) {
	    gdk_font_unref(t->font);
	    t->font = font;
	    t->fontw = fontw;
	    t->fonth = fonth;
	    tracker_reset(t);
	    return TRUE;
	}
    }

    return FALSE;
}

typedef void  (*___Sig1) (Tracker *, gint , gint , gint , gpointer);

static void
___marshal_Sig1 (GClosure *closure,
	GValue *return_value,
	guint n_param_values,
	const GValue *param_values,
	gpointer invocation_hint,
	gpointer marshal_data)
{
	register ___Sig1 callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2;

	g_return_if_fail (n_param_values == 4);

	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	} else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}

	callback = (___Sig1) (marshal_data != NULL ? marshal_data : cc->callback);

	callback ((Tracker *)data1,
		(gint ) g_value_get_int (param_values + 1),
		(gint ) g_value_get_int (param_values + 2),
		(gint ) g_value_get_int (param_values + 3),
		data2);
}

typedef void  (*___Sig2) (Tracker *, gint , gpointer);

static void
___marshal_Sig2 (GClosure *closure,
	GValue *return_value,
	guint n_param_values,
	const GValue *param_values,
	gpointer invocation_hint,
	gpointer marshal_data)
{
	register ___Sig2 callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2;

	g_return_if_fail (n_param_values == 2);

	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	} else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}

	callback = (___Sig2) (marshal_data != NULL ? marshal_data : cc->callback);

	callback ((Tracker *)data1,
		(gint ) g_value_get_int (param_values + 1),
		data2);
}

/* If selecting, mouse is used to select in pattern */
static void
tracker_mouse_to_cursor_pos (Tracker *t,
			     int x,
			     int y,
			     int *cursor_ch,
			     int *cursor_item,
			     int *patpos)
{
    int HPatHalf;

    /* Calc the column (channel and pos in channel) */
    if(x < t->disp_startx) {
	if(t->leftchan)
	    *cursor_ch = t->leftchan - 1;
	else
	    *cursor_ch = t->leftchan;
	*cursor_item = 0;
    } else if(x > t->disp_startx + t->disp_numchans * t->disp_chanwidth) {
	if(t->leftchan + t->disp_numchans < t->num_channels) {
	    *cursor_ch = t->leftchan + t->disp_numchans;
	    *cursor_item = 0;
	} else {
	    *cursor_ch = t->num_channels - 1;
	    *cursor_item = 7;			
	}
    } else {
	*cursor_ch = t->leftchan + ((x - t->disp_startx) / t->disp_chanwidth);
	*cursor_item = (x - (t->disp_startx + (*cursor_ch - t->leftchan) * t->disp_chanwidth)) / t->fontw;
	if(*cursor_item < 4)
	    *cursor_item = 0;
	else if(*cursor_item == 4)
	    *cursor_item = 1;
	else if(*cursor_item == 5 || *cursor_item == 6)
	    *cursor_item = 2;
	else if(*cursor_item == 7)
	    *cursor_item = 3;
	else if(*cursor_item == 8 || *cursor_item == 9)
	    *cursor_item = 4;
	else if(*cursor_item == 10)
	    *cursor_item = 5;
	else if(*cursor_item == 11)
	    *cursor_item = 6;
	else if(*cursor_item >= 12)
	    *cursor_item = 7;			
    }

    /* Calc the row */
    HPatHalf = t->disp_rows / 2;
    if(y < t->disp_starty)
	*patpos = t->patpos - HPatHalf - 1;
    else if(y > t->disp_rows * t->fonth)
	*patpos = t->patpos + HPatHalf + 1;
    else {
	*patpos = (y - t->disp_starty) / t->fonth;
	if(t->patpos <= *patpos)
	    *patpos = t->patpos + *patpos - HPatHalf;
	else
	    *patpos = t->patpos - (HPatHalf - *patpos);
    }
    if(*patpos < 0)
	*patpos = 0;
    else if(*patpos >= t->curpattern->length)
	*patpos = t->curpattern->length - 1;
}

static gboolean
tracker_scroll (GtkWidget      *widget,
		GdkEventScroll *event)
{
    Tracker	*t;
    t = TRACKER(widget);

    switch(event->direction) {
    case GDK_SCROLL_UP:
	if(event->state & GDK_SHIFT_MASK) {
	    if(t->leftchan > 0) tracker_set_xpanning(t, t->leftchan - 1);
	} else
	    if(t->patpos > 0) tracker_step_cursor_row(t, -1);
	return TRUE;
    case GDK_SCROLL_DOWN:
	if(event->state & GDK_SHIFT_MASK) {
	    if(t->leftchan + t->disp_numchans < t->num_channels) tracker_set_xpanning(t, t->leftchan + 1);
	} else
	    if(t->patpos < t->curpattern->length - 1) tracker_step_cursor_row(t, 1);
	return TRUE;
    case GDK_SCROLL_LEFT:/* For happy folks with 2-scroller mice :-) */
	if(t->leftchan > 0) tracker_set_xpanning(t, t->leftchan - 1);
	return TRUE;
    case GDK_SCROLL_RIGHT:
	if(t->patpos < t->curpattern->length - 1) tracker_step_cursor_row(t, 1);
	return TRUE;
    default:
	break;
    }
    return FALSE;
}

static gint
tracker_button_press (GtkWidget      *widget,
		      GdkEventButton *event)
{
    Tracker *t;
    int x, y, cursor_ch, cursor_item, patpos, redraw = 0;
    GdkModifierType state;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_TRACKER(widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    t = TRACKER(widget);
    gdk_window_get_pointer (event->window, &x, &y, &state);

    if(t->mouse_selecting && event->button != 1) {
	t->mouse_selecting = 0;
    } else if(!t->mouse_selecting) {
	t->button = event->button;		
	gdk_window_get_pointer (event->window, &x, &y, &state);
	if(t->button == 1) {
	    /* Start selecting block */
	    gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_MAINMENU_BLOCKMARK_SET], 1);
	    t->inSelMode = FALSE;
	    tracker_mouse_to_cursor_pos(t, x, y, &t->sel_start_ch, &cursor_item, &t->sel_start_row);
	    t->sel_end_row = t->sel_start_row;
	    t->sel_end_ch = t->sel_start_ch;
	    t->mouse_selecting = 1;
	    tracker_redraw(t);
	} else if(t->button == 2) {
	    /* Tracker cursor posititioning and clear block mark if any */
	    if(t->inSelMode || t->sel_start_ch != -1) {
		gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_MAINMENU_BLOCKMARK_SET], 0);
		t->sel_start_ch = t->sel_end_ch = -1;
		t->sel_start_row = t->sel_end_row = -1;
		t->inSelMode = FALSE;			
		tracker_redraw(t);
	    }
	    tracker_mouse_to_cursor_pos(t, x, y, &cursor_ch, &cursor_item, &patpos);
	    /* Redraw only if needed */
	    if(cursor_ch != t->cursor_ch || cursor_item != t->cursor_item) {
		t->cursor_ch = cursor_ch;
		t->cursor_item = cursor_item;
		adjust_xpanning(t);
		redraw = 1;
	    }
	    if(patpos != t->patpos) {
		tracker_set_patpos(t, patpos);
		redraw = 0;
	    }
	    if(redraw) {
		gtk_widget_queue_draw(GTK_WIDGET(t));
	    }
	}
    }
    return TRUE;
}

static gint
tracker_motion_notify (GtkWidget *widget,
		       GdkEventMotion *event)
{
    Tracker *t;
    int x, y, cursor_item;
    GdkModifierType state;

    t = TRACKER(widget);

    if(!t->mouse_selecting)
	return TRUE;

    if (event->is_hint) {
	gdk_window_get_pointer (event->window, &x, &y, &state);
    } else {
	x = event->x;
	y = event->y;
	state = event->state;
    }

    if((state & GDK_BUTTON1_MASK) && t->mouse_selecting) {
        tracker_mouse_to_cursor_pos(t, x, y, &t->sel_end_ch, &cursor_item, &t->sel_end_row);

	if(x > t->disp_startx + t->disp_numchans*t->disp_chanwidth && t->leftchan + t->disp_numchans < t->num_channels)	{
	    t->sel_end_ch++;
	    tracker_set_xpanning(t, t->leftchan + 1);
	} else if(x < t->disp_startx && t->leftchan > 0) {
	    t->sel_end_ch--;
	    tracker_set_xpanning(t, t->leftchan - 1);
	}	
	if((t->sel_end_row > t->patpos + (t->disp_rows/2)) || (y > t->widget.allocation.height && t->patpos < t->sel_end_row))
	    tracker_set_patpos(t, t->patpos+1);
	else if((t->sel_end_row < t->patpos - (t->disp_rows/2)) || (y <= 0 && t->patpos > t->sel_end_row))
	    tracker_set_patpos(t, t->patpos-1);
	tracker_redraw(t);
    }

    return TRUE;
}

static gint
tracker_button_release (GtkWidget      *widget,
			GdkEventButton *event)
{
    Tracker *t;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_TRACKER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    t = TRACKER(widget);

    if(t->mouse_selecting && event->button == 1) {
	t->mouse_selecting = 0;
	gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_MAINMENU_BLOCKMARK_SET], 0);
    }

    return TRUE;
}

static void
tracker_class_init (TrackerClass *class)
{
    GObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GObjectClass*) class;
    widget_class = (GtkWidgetClass*) class;
    
    widget_class->realize = tracker_realize;
    widget_class->expose_event = tracker_expose;
    widget_class->size_request = tracker_size_request;
    widget_class->size_allocate = tracker_size_allocate;
    widget_class->button_press_event = tracker_button_press;
    widget_class->button_release_event = tracker_button_release;
    widget_class->motion_notify_event = tracker_motion_notify;
    widget_class->scroll_event = tracker_scroll;


    tracker_signals[SIG_PATPOS] =
	   		g_signal_new ("patpos",
			G_TYPE_FROM_CLASS (object_class),
			(GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
			G_STRUCT_OFFSET (TrackerClass, patpos),
			NULL, NULL,
			___marshal_Sig1,
			G_TYPE_NONE, 3,
			G_TYPE_INT,
			G_TYPE_INT,
			G_TYPE_INT);

    tracker_signals[SIG_XPANNING] = g_signal_new ("xpanning",
		    	G_TYPE_FROM_CLASS (object_class),
			(GSignalFlags)G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET(TrackerClass, xpanning),
			NULL, NULL,
			___marshal_Sig1,
			G_TYPE_NONE, 3,
			G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
    tracker_signals[SIG_MAINMENU_BLOCKMARK_SET] = g_signal_new(
		    "mainmenu_blockmark_set",
		    	G_TYPE_FROM_CLASS (object_class),
			(GSignalFlags)G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET(TrackerClass, mainmenu_blockmark_set),
			NULL, NULL,
			___marshal_Sig2,
			G_TYPE_NONE, 1, G_TYPE_INT);

    
    class->patpos = NULL;
    class->xpanning = NULL;
    class->mainmenu_blockmark_set = NULL;
}

static void
tracker_init (Tracker *t)
{
    t->baselineskip = 0;
    t->font = gdk_font_load("fixed");
    t->fonth = t->font->ascent + t->font->descent + t->baselineskip;
    t->fontw = gdk_string_width(t->font, "X"); /* let's just hope this is a non-proportional font */
    t->oldpos = -1;
    t->curpattern = NULL;
    t->enable_backing_store = 0;
    t->pixmap = NULL;
    t->patpos = 0;
    t->cursor_ch = 0;
    t->cursor_item = 0;

    t->inSelMode = FALSE;
    t->sel_end_row = t->sel_end_ch = t->sel_start_row = t->sel_start_ch = -1;
    t->mouse_selecting = 0;
    t->button = -1;
}

guint
tracker_get_type (void)
{
    static guint tracker_type = 0;
    
    if (!tracker_type) {
	GTypeInfo tracker_info =
	{
	    sizeof(TrackerClass),
	    (GBaseInitFunc) NULL,
	    (GBaseFinalizeFunc) NULL,
	    (GClassInitFunc) tracker_class_init,
	    (GClassFinalizeFunc) NULL,
	    NULL,
	    sizeof(Tracker),
	    0,
	    (GInstanceInitFunc) tracker_init,
	};
	
	tracker_type = g_type_register_static(gtk_widget_get_type (),
	    "Tracker", &tracker_info, (GTypeFlags)0);
    }
    
    return tracker_type;
}

GtkWidget*
tracker_new (void)
{
    return GTK_WIDGET(gtk_type_new(tracker_get_type()));
}
