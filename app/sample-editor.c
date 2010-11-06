
/*
 * The Real SoundTracker - sample editor
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
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <math.h>

#if USE_SNDFILE
#include <sndfile.h>
#elif !defined (NO_AUDIOFILE)
#include <audiofile.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#ifdef USE_GNOME
#include <gnome.h>
#endif

#include "i18n.h"
#include "sample-editor.h"
#include "xm.h"
#include "st-subs.h"
#include "gui.h"
#include "gui-subs.h"
#include "instrument-editor.h"
#include "sample-display.h"
#include "endian-conv.h"
#include "keys.h"
#include "track-editor.h"
#include "errors.h"
#include "time-buffer.h"
#include "audio.h"
#include "mixer.h"
#include "module-info.h"
#include "file-operations.h"
#include "gui-settings.h"
#include "xm.h"

// == GUI variables

static STSample *current_sample = NULL;

static GtkWidget *spin_volume, *spin_panning, *spin_finetune, *spin_relnote;
static GtkWidget *savebutton, *savebutton_rgn;
static SampleDisplay *sampledisplay;
static GtkWidget *sample_editor_hscrollbar;
static GtkWidget *loopradio[3], *resolution_radio[2];
static GtkWidget *spin_loopstart, *spin_loopend;
static GtkWidget *trimdialog = NULL;

static struct SampleEditor { // simplifies future encapsulation of this into a Gtk+ widget
    GtkWidget *label_selection;
    GtkWidget *label_length;
    gchar label_selection_new_text[64];
    gchar label_length_new_text[64];
    guint idle_handler;
    GtkWidget *vertical_boxes[3];
} sample_editor;
static struct SampleEditor * const se = &sample_editor;

// = Volume ramping dialog

static GtkWidget *volrampwindow = NULL;
static GtkWidget *sample_editor_volramp_spin_w[2];
static int sample_editor_volramp_last_values[2] = { 100, 100 };

// = Trim dialog

static gboolean trimbeg = TRUE;
static gboolean trimend = TRUE;
static gfloat threshold = -50.0;

// = Load sample dialog

#if USE_SNDFILE || !defined (NO_AUDIOFILE)
enum {
    MODE_MONO = 0,
    MODE_STEREO_LEFT,
    MODE_STEREO_RIGHT,
    MODE_STEREO_MIX
};

static gboolean wavload_through_library;

static GtkWidget *wavload_dialog;
static const char *wavload_samplename;

#if USE_SNDFILE
static SNDFILE *wavload_file;
static SF_INFO wavinfo;
static sf_count_t wavload_frameCount;
#else
static AFfilehandle wavload_file;
static AFframecount wavload_frameCount;
#endif

static int wavload_sampleWidth, wavload_channelCount, wavload_endianness, wavload_unsignedwords;
static long wavload_rate;

static const gchar *wavload_filename;
static GtkWidget *wavload_raw_resolution_w[2];
static GtkWidget *wavload_raw_channels_w[2];
static GtkWidget *wavload_raw_signed_w[2];
static GtkWidget *wavload_raw_endian_w[2];
static GtkWidget *wavload_raw_rate;

static gboolean libaf2 = TRUE;
#endif

// = Sampler variables

static SampleDisplay *monitorscope;
static GtkWidget *cancelbutton, *okbutton, *startsamplingbutton;

st_in_driver *sampling_driver = NULL;
void *sampling_driver_object = NULL;

static GtkWidget *samplingwindow = NULL;

struct recordbuf {
    struct recordbuf *next;
    int length;
    gint16 data[0];
};

static struct recordbuf *recordbufs, *current;
static const int recordbuflen = 10000;
static int currentoffs;
static int recordedlen, sampling;

// = Editing operations variables

static void *copybuffer = NULL;
static int copybufferlen;
static STSample copybuffer_sampleinfo;

// = Realtime stuff

static int update_freq = 50;
static int gtktimer = -1;

static void sample_editor_ok_clicked(void);
static void sample_editor_start_sampling_clicked(void);

static void sample_editor_spin_volume_changed(GtkSpinButton *spin);
static void sample_editor_spin_panning_changed(GtkSpinButton *spin);
static void sample_editor_spin_finetune_changed(GtkSpinButton *spin);
static void sample_editor_spin_relnote_changed(GtkSpinButton *spin);
static void sample_editor_selection_to_loop_clicked (void);
static void sample_editor_loop_changed(void);
static void sample_editor_display_loop_changed(SampleDisplay *, int start, int end);
static void sample_editor_display_selection_changed(SampleDisplay *, int start, int end);
static void sample_editor_display_window_changed(SampleDisplay *, int start, int end);
static void sample_editor_select_none_clicked(void);
static void sample_editor_select_all_clicked(void);
static void sample_editor_clear_clicked(void);
static void sample_editor_crop_clicked(void);
static void sample_editor_show_all_clicked(void);
static void sample_editor_zoom_in_clicked(void);
static void sample_editor_zoom_out_clicked(void);
static void sample_editor_loopradio_changed(void);
static void sample_editor_resolution_changed(void);
static void sample_editor_monitor_clicked(void);
static void sample_editor_cut_clicked(void);
static void sample_editor_remove_clicked(void);
static void sample_editor_copy_clicked(void);
void sample_editor_paste_clicked(void);
static void sample_editor_zoom_to_selection_clicked(void);

#if USE_SNDFILE || !defined (NO_AUDIOFILE)
static void sample_editor_load_wav(void);
static void sample_editor_save_wav(void);
static void sample_editor_save_region_wav(void);
#endif

static void sample_editor_open_volume_ramp_dialog(void);
static void sample_editor_close_volume_ramp_dialog(void);
static void sample_editor_perform_ramp(GtkWidget *w, gpointer data);

static void sample_editor_reverse_clicked(void);

static void sample_editor_trim_dialog(void);
static void sample_editor_trim(gboolean beg, gboolean end, gfloat threshold);
static void sample_editor_crop(void);
static void sample_editor_delete(STSample *sample,int start, int end);
    
static void
sample_editor_lock_sample (void)
{
    g_mutex_lock(current_sample->sample.lock);
}

static void
sample_editor_unlock_sample (void)
{
    if(gui_playing_mode) {
	mixer->updatesample(&current_sample->sample);
    }
    g_mutex_unlock(current_sample->sample.lock);
}

void
sample_editor_page_create (GtkNotebook *nb)
{
    GtkWidget *box, *thing, *hbox, *vbox, *vbox2, *frame, *box2;
    static const char *looplabels[] = {
	N_("No loop"),
	N_("Amiga"),
	N_("PingPong"),
	NULL
    };
    static const char *resolutionlabels[] = {
	N_("8 bits"),
	N_("16 bits"),
	NULL
    };

    box = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(box), 10);
    gtk_notebook_append_page(nb, box, gtk_label_new(_("Sample Editor")));
    gtk_widget_show(box);

    thing = sample_display_new(TRUE);
    gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "loop_changed",
		       GTK_SIGNAL_FUNC(sample_editor_display_loop_changed), NULL);
    gtk_signal_connect(GTK_OBJECT(thing), "selection_changed",
		       GTK_SIGNAL_FUNC(sample_editor_display_selection_changed), NULL);
    gtk_signal_connect(GTK_OBJECT(thing), "window_changed",
		       GTK_SIGNAL_FUNC(sample_editor_display_window_changed), NULL);
    sampledisplay = SAMPLE_DISPLAY(thing);
    sample_display_enable_zero_line(SAMPLE_DISPLAY(thing), TRUE);

    sample_editor_hscrollbar = gtk_hscrollbar_new(NULL);
    gtk_widget_show(sample_editor_hscrollbar);
    gtk_box_pack_start(GTK_BOX(box), sample_editor_hscrollbar, FALSE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    se->vertical_boxes[0] = vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);

    make_radio_group((const char**)looplabels, vbox, loopradio, FALSE, FALSE, sample_editor_loopradio_changed);
    gui_put_labelled_spin_button(vbox, _("Start"), 0, 0, &spin_loopstart, sample_editor_loop_changed, NULL);
    gui_put_labelled_spin_button(vbox, _("End"), 0, 0, &spin_loopend, sample_editor_loop_changed, NULL);
    sample_editor_loopradio_changed();

    thing = gtk_vseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

    se->vertical_boxes[1] = vbox = gtk_vbox_new(TRUE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);

    gui_put_labelled_spin_button(vbox, _("Volume"), 0, 64, &spin_volume, sample_editor_spin_volume_changed, NULL);
    gui_put_labelled_spin_button(vbox, _("Panning"), -128, 127, &spin_panning, sample_editor_spin_panning_changed, NULL);
    gui_put_labelled_spin_button(vbox, _("Finetune"), -128, 127, &spin_finetune, sample_editor_spin_finetune_changed, NULL);
    make_radio_group((const char**)resolutionlabels, vbox, resolution_radio, FALSE, FALSE, sample_editor_resolution_changed);

    thing = gtk_vseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

    se->vertical_boxes[2] = vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);

    /* Sample selection */
    vbox2 = gtk_vbox_new(TRUE, 2);
    gtk_widget_show(vbox2);
    gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, TRUE, 0);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(vbox2), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Selection:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);

    thing = gtk_button_new_with_label(_("None"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_select_none_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("All"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_select_all_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX(vbox2), frame, FALSE, TRUE, 0);
    gtk_widget_show (frame);

    se->label_selection = gtk_label_new ("");
    gtk_misc_set_alignment (GTK_MISC (se->label_selection), 0.5, 0.5);
    gtk_container_add (GTK_CONTAINER (frame), se->label_selection);
    gtk_widget_show (se->label_selection);

    /* Sample length */
    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(vbox2), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Length:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX(box2), frame, TRUE, TRUE, 0);
    gtk_widget_show (frame);

    se->label_length = gtk_label_new ("");
    gtk_misc_set_alignment (GTK_MISC (se->label_length), 0.5, 0.5);
    gtk_container_add (GTK_CONTAINER (frame), se->label_length);
    gtk_widget_show (se->label_length);

    add_empty_vbox(vbox);

    thing = gtk_button_new_with_label(_("Set as loop"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC( sample_editor_selection_to_loop_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    add_empty_vbox(vbox);

    gui_put_labelled_spin_button(vbox, _("RelNote"), -128, 127, &spin_relnote, sample_editor_spin_relnote_changed, NULL);

    thing = gtk_vseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

#if USE_SNDFILE || !defined (NO_AUDIOFILE)
    fileops_dialogs[DIALOG_LOAD_SAMPLE] = file_selection_create(_("Load Sample..."), sample_editor_load_wav);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_LOAD_SAMPLE]), gui_settings.loadsmpl_path);
    fileops_dialogs[DIALOG_SAVE_SAMPLE] = file_selection_create(_("Save WAV..."), sample_editor_save_wav);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_SAMPLE]), gui_settings.savesmpl_path);
    fileops_dialogs[DIALOG_SAVE_RGN_SAMPLE] = file_selection_create(_("Save region as WAV..."), sample_editor_save_region_wav);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_RGN_SAMPLE]), gui_settings.savesmpl_path);
#endif

    thing = gtk_button_new_with_label(_("Load Sample"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(fileops_open_dialog), (void*)DIALOG_LOAD_SAMPLE);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
#if USE_SNDFILE == 0 && defined (NO_AUDIOFILE)
    gtk_widget_set_sensitive(thing, 0);
#endif

    thing = gtk_button_new_with_label(_("Save WAV"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(fileops_open_dialog), (void*)DIALOG_SAVE_SAMPLE);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    savebutton = thing;
#if USE_SNDFILE == 0 && defined (NO_AUDIOFILE)
    gtk_widget_set_sensitive(thing, 0);
#endif

    thing = gtk_button_new_with_label(_("Save Region"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(fileops_open_dialog), (void*)DIALOG_SAVE_RGN_SAMPLE);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    savebutton_rgn = thing;
#if USE_SNDFILE == 0 && defined (NO_AUDIOFILE)
      gtk_widget_set_sensitive(thing, 0);
#endif

    
    thing = gtk_button_new_with_label(_("Monitor"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_monitor_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Volume Ramp"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_open_volume_ramp_dialog), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Trim"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_trim_dialog), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);


    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    thing = gtk_button_new_with_label(_("Zoom to selection"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_zoom_to_selection_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Show all"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_show_all_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Zoom in (+50%)"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_zoom_in_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Zoom out (-50%)"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_zoom_out_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Reverse"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_reverse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    thing = gtk_button_new_with_label(_("Cut"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_cut_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Remove"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_remove_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Copy"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_copy_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Paste"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_paste_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Clear Sample"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_clear_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Crop"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_crop_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

#if (USE_SNDFILE || !defined (NO_AUDIOFILE)) && HAVE_DLFCN_H
    { // hack, hack
	void *handle, *function;
#ifdef DL_LAZY
	handle = dlopen(NULL, DL_LAZY);
#else
	handle = dlopen(NULL, RTLD_NOW);
#endif
	function = dlsym(handle, "afSetVirtualPCMMapping");
	if(function == NULL) {
	    libaf2 = FALSE;
	}
    }
#endif

}

static void
sample_editor_block_loop_spins (int block)
{
    void (*func) (GtkObject*, GtkSignalFunc, gpointer);

    func = block ? gtk_signal_handler_block_by_func : gtk_signal_handler_unblock_by_func;

    func(GTK_OBJECT(spin_loopstart), sample_editor_loop_changed, NULL);
    func(GTK_OBJECT(spin_loopend), sample_editor_loop_changed, NULL);
}

static void
sample_editor_blocked_set_loop_spins (int start,
				      int end)
{
    sample_editor_block_loop_spins(1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_loopstart), start);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_loopend), end);
    sample_editor_block_loop_spins(0);
}

static gint
sample_editor_idle_draw_function (struct SampleEditor *se)
{
    gtk_label_set(GTK_LABEL(se->label_selection), se->label_selection_new_text);
    gtk_label_set(GTK_LABEL(se->label_length), se->label_length_new_text);
    gtk_idle_remove(se->idle_handler);
    se->idle_handler = 0;
    return TRUE;
}

static void
sample_editor_set_selection_label (int start,
				   int end)
{
    STSample *sts = current_sample;

    if(start > -1) {
	sprintf(se->label_selection_new_text, ("%d - %d"), start, end);
	sprintf(se->label_length_new_text, ("%d"), end-start);
    } else {
	strcpy(se->label_selection_new_text, _("(no selection)"));
	sprintf(se->label_length_new_text, ("%d"), sts->sample.length);
    }

    /* Somewhere on the way to gtk+-1.2.10, gtk_label_set_text() has
       become incredibly slow. So slow that my computer is overwhelmed
       with the frequent calls to this function when one does a
       selection with the mouse -- with the consequence that the idle
       function updating the visual selection in the sample display
       widget is called only very seldom.

       That's why I'm doing the gtk_label_set_text() only if the
       computer is idle. */

    if(!se->idle_handler) {
	se->idle_handler = gtk_idle_add((GtkFunction)sample_editor_idle_draw_function,
						  (gpointer)se);
	g_assert(se->idle_handler != 0);
    }
}

static void
sample_editor_blocked_set_display_loop (int start,
					int end)
{
    gtk_signal_handler_block_by_func(GTK_OBJECT(sampledisplay), GTK_SIGNAL_FUNC(sample_editor_display_loop_changed), NULL);
    sample_display_set_loop(sampledisplay, start, end);
    gtk_signal_handler_unblock_by_func(GTK_OBJECT(sampledisplay), GTK_SIGNAL_FUNC(sample_editor_display_loop_changed), NULL);
}

void
sample_editor_update (void)
{
    STSample *sts = current_sample;
    st_mixer_sample_info *s;
    char buf[20];
    int m = xm_get_modified();

    sample_display_set_data_16(sampledisplay, NULL, 0, FALSE);

    if(!sts || !sts->sample.data) {
	gtk_widget_set_sensitive(se->vertical_boxes[0], FALSE);
	gtk_widget_set_sensitive(se->vertical_boxes[1], FALSE);
	gtk_widget_set_sensitive(se->vertical_boxes[2], FALSE);
	gtk_widget_set_sensitive(savebutton, 0);
	gtk_widget_set_sensitive(savebutton_rgn, 0);
    }

    if(!sts) {
	return;
    }

    gtk_entry_set_text(GTK_ENTRY(gui_cursmpl_name), sts->name);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_volume), sts->volume);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_panning), sts->panning - 128);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_finetune), sts->finetune);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_relnote), sts->relnote);

    s = &sts->sample;

    sprintf(buf, ("%d"), s->length);
    gtk_label_set(GTK_LABEL(se->label_length), buf);

    sample_editor_block_loop_spins(1);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(loopradio[s->looptype]), TRUE);
    gui_update_spin_adjustment(GTK_SPIN_BUTTON(spin_loopstart), 0, s->length - 1);
    gui_update_spin_adjustment(GTK_SPIN_BUTTON(spin_loopend), 1, s->length);
    sample_editor_block_loop_spins(0);
    sample_editor_blocked_set_loop_spins(s->loopstart, s->loopend);
    
    sample_editor_set_selection_label(-1, 0);

    if(s->data) {
	sample_display_set_data_16(sampledisplay, s->data, s->length, FALSE);

	if(s->looptype != ST_MIXER_SAMPLE_LOOPTYPE_NONE) {
	    sample_editor_blocked_set_display_loop(s->loopstart, s->loopend);
	}

	gtk_widget_set_sensitive(se->vertical_boxes[0], TRUE);
	gtk_widget_set_sensitive(se->vertical_boxes[1], TRUE);
	gtk_widget_set_sensitive(se->vertical_boxes[2], TRUE);

#if USE_SNDFILE || !defined (NO_AUDIOFILE)
	gtk_widget_set_sensitive(savebutton, 1);
	gtk_widget_set_sensitive(savebutton_rgn, 1);
#endif
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(resolution_radio[sts->treat_as_8bit ? 0 : 1]), TRUE);
    }

    xm_set_modified(m);
}

gboolean
sample_editor_handle_keys (int shift,
			   int ctrl,
			   int alt,
			   guint32 keyval,
			   gboolean pressed)
{
    int i;
    int s = sampledisplay->sel_start, e = sampledisplay->sel_end;

    if(!pressed)
	return FALSE;

    if(s == -1) {
	s = 0;
	e = current_sample->sample.length;
    }

    i = keys_get_key_meaning(keyval, ENCODE_MODIFIERS(shift, ctrl, alt));
    if(i != -1 && KEYS_MEANING_TYPE(i) == KEYS_MEANING_NOTE) {
	i += 12 * gui_get_current_octave_value() + 1;
	if(i < 96 && current_sample != NULL) {
	    gui_play_note_full(tracker->cursor_ch, i, current_sample, s, e - s);
	}
	return TRUE;
    }

    return FALSE;
}

void
sample_editor_set_sample (STSample *s)
{
    current_sample = s;
    sample_editor_update();
}

static void
sample_editor_update_mixer_position (double songtime)
{
    audio_mixer_position *p;
    int i;

    if(songtime >= 0.0 && current_sample && (p = time_buffer_get(audio_mixer_position_tb, songtime))) {
	for(i = 0; i < sizeof(p->dump) / sizeof(p->dump[0]); i++) {
	    if(p->dump[i].current_sample == &current_sample->sample) {
		sample_display_set_mixer_position(sampledisplay, p->dump[i].current_position);
		return;
	    }
	}
    }

    sample_display_set_mixer_position(sampledisplay, -1);
}

static gint
sample_editor_update_timeout (gpointer data)
{
    double display_songtime = current_driver->get_play_time(current_driver_object);

    sample_editor_update_mixer_position(display_songtime);

    // Not quite the right place for this, but anyway...
    gui_clipping_indicator_update(display_songtime);

    return TRUE;
}

void
sample_editor_start_updating (void)
{
    if(gtktimer != -1)
	return;

    gtktimer = gtk_timeout_add(1000 / update_freq, sample_editor_update_timeout, NULL);
}

void
sample_editor_stop_updating (void)
{
    if(gtktimer == -1)
	return;

    gtk_timeout_remove(gtktimer);
    gtktimer = -1;
    gui_clipping_indicator_update(-1.0);
    sample_editor_update_mixer_position(-1.0);
}

static void
sample_editor_spin_volume_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->volume = gtk_spin_button_get_value_as_int(spin);
    xm_set_modified(1);
}

static void
sample_editor_spin_panning_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->panning = gtk_spin_button_get_value_as_int(spin) + 128;
    xm_set_modified(1);
}

static void
sample_editor_spin_finetune_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->finetune = gtk_spin_button_get_value_as_int(spin);
    xm_set_modified(1);
}

static void
sample_editor_spin_relnote_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->relnote = gtk_spin_button_get_value_as_int(spin);
    xm_set_modified(1);
}

static void
sample_editor_selection_to_loop_clicked (void)
{
    int s = sampledisplay->sel_start, e = sampledisplay->sel_end;

    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->sample.data != NULL);

    if(s == -1) {
	return;
    }

    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(loopradio[0]), TRUE);
    
    sample_editor_lock_sample();
    current_sample->sample.loopend = e;
    current_sample->sample.loopstart = s;
    sample_editor_unlock_sample();

    sample_editor_blocked_set_loop_spins(s, e);

    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(loopradio[1]), TRUE);
    xm_set_modified(1);
}


static void
sample_editor_loop_changed ()
{
    int s = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_loopstart)),
	e = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_loopend));

    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->sample.data != NULL);
    g_return_if_fail(current_sample->sample.looptype != ST_MIXER_SAMPLE_LOOPTYPE_NONE);

    if(s != current_sample->sample.loopstart || e != current_sample->sample.loopend) {
	if(e <= s) {
	    e = s + 1;
	    sample_editor_blocked_set_loop_spins(s, e);
	}

	sample_editor_lock_sample();
	current_sample->sample.loopend = e;
	current_sample->sample.loopstart = s;
	sample_editor_unlock_sample();

	sample_editor_blocked_set_display_loop(s, e);
    }

    xm_set_modified(1);
}

static void
sample_editor_display_loop_changed (SampleDisplay *sample_display,
				    int start,
				    int end)
{
    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->sample.data != NULL);
    g_return_if_fail(current_sample->sample.looptype != ST_MIXER_SAMPLE_LOOPTYPE_NONE);
    g_return_if_fail(start < end);

    if(start != current_sample->sample.loopstart || end != current_sample->sample.loopend) {
	sample_editor_blocked_set_loop_spins(start, end);

	sample_editor_lock_sample();
	current_sample->sample.loopend = end;
	current_sample->sample.loopstart = start;
	sample_editor_unlock_sample();
    }

    xm_set_modified(1);
}

static void
sample_editor_select_none_clicked (void)
{
    sample_display_set_selection(sampledisplay, -1, 1);
}

static void
sample_editor_select_all_clicked (void)
{
    g_return_if_fail(current_sample != NULL);

    sample_display_set_selection(sampledisplay,
				 0, current_sample->sample.length);
}

static void
sample_editor_display_selection_changed (SampleDisplay *sample_display,
					 int start,
					 int end)
{
    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->sample.data != NULL);
    g_return_if_fail(start < end);

    sample_editor_set_selection_label(start, end);
}

static void
sample_editor_hscrollbar_changed (GtkAdjustment *adj)
{
    sample_display_set_window(sampledisplay,
			      adj->value,
			      adj->value + sampledisplay->win_length);
}

static void
sample_editor_display_window_changed (SampleDisplay *sample_display,
				      int start,
				      int end)
{
    if(current_sample == NULL)
	return;

    gui_update_range_adjustment(GTK_RANGE(sample_editor_hscrollbar),
				start,
				current_sample->sample.length,
				end - start,
				sample_editor_hscrollbar_changed);
}

static void
sample_editor_loopradio_changed (void)
{
    int n = find_current_toggle(loopradio, 3);

    gtk_widget_set_sensitive(spin_loopstart, n != 0);
    gtk_widget_set_sensitive(spin_loopend, n != 0);
    
    if(current_sample != NULL) {
	if(current_sample->sample.looptype != n) {
	    sample_editor_lock_sample();
	    current_sample->sample.looptype = n;
	    sample_editor_unlock_sample();
	}

	if(n != ST_MIXER_SAMPLE_LOOPTYPE_NONE) {
	    sample_editor_blocked_set_display_loop(current_sample->sample.loopstart, current_sample->sample.loopend);
	} else {
	    sample_editor_blocked_set_display_loop(-1, 1);
	}
    }

    xm_set_modified(1);
}

static void
sample_editor_resolution_changed (void)
{
    STSample *sts = current_sample;
    st_mixer_sample_info *s;
    int n = find_current_toggle(resolution_radio, 2);

    if(!sts)
	return;

    s = &sts->sample;
    if(n == 0 && !sts->treat_as_8bit) {
	st_sample_cutoff_lowest_8_bits(s->data, s->length);
    }

    sts->treat_as_8bit = (n == 0);

    xm_set_modified(1);
}

static void
sample_editor_clear_clicked (void)
{
    STInstrument *instr;

    sample_editor_lock_sample();

    st_clean_sample(current_sample, NULL);

    instr = instrument_editor_get_instrument();
    if(st_instrument_num_samples(instr) == 0) {
	instrument_editor_clear_current_instrument();
    } else {
	instrument_editor_update();
	sample_editor_update();
	modinfo_update_all();
    }

    sample_editor_unlock_sample();

    xm_set_modified(1);
}

static void
sample_editor_crop_clicked (void)
{
    sample_editor_crop();
}

static void
sample_editor_show_all_clicked (void)
{
    if(current_sample == NULL || current_sample->sample.data == NULL)
	return;
    sample_display_set_window(sampledisplay, 0, current_sample->sample.length);
}

static void
sample_editor_zoom_in_clicked (void)
{
    int ns = sampledisplay->win_start,
	ne = sampledisplay->win_start + sampledisplay->win_length;
    int l;
    
    if(current_sample == NULL || current_sample->sample.data == NULL)
	return;

    l = sampledisplay->win_length / 4;

    ns += l;
    ne -= l;

    if(ne <= ns)
	ne = ns + 1;

    sample_display_set_window(sampledisplay, ns, ne);
}

static void
sample_editor_zoom_out_clicked (void)
{
    int ns = sampledisplay->win_start,
	ne = sampledisplay->win_start + sampledisplay->win_length;
    int l;
    
    if(current_sample == NULL || current_sample->sample.data == NULL)
	return;

    l = sampledisplay->win_length / 2;

    if(ns > l)
	ns -= l;
    else
	ns = 0;

    if(ne <= current_sample->sample.length - l)
	ne += l;
    else
	ne = current_sample->sample.length;

    sample_display_set_window(sampledisplay, ns, ne);
}

static void
sample_editor_zoom_to_selection_clicked (void)
{
    if(current_sample == NULL || current_sample->sample.data == NULL || sampledisplay->sel_start == -1)
	return;
    sample_display_set_window(sampledisplay, sampledisplay->sel_start, sampledisplay->sel_end);
}

void
sample_editor_copy_cut_common (gboolean copy,
			       gboolean spliceout)
{
    int cutlen, newlen;
    gint16 *newsample;
    STSample *oldsample = current_sample;
    int ss = sampledisplay->sel_start, se;
    
    if(oldsample == NULL || ss == -1)
	return;
    
    se = sampledisplay->sel_end;

    cutlen = se - ss;
    newlen = oldsample->sample.length - cutlen;

    if(copy) {
	if(copybuffer) {
	    free(copybuffer);
	    copybuffer = NULL;
	}
	copybufferlen = cutlen;
	copybuffer = malloc(copybufferlen * 2);
	if(!copybuffer) {
	    error_error(_("Out of memory for copybuffer.\n"));
	} else {
	    memcpy(copybuffer,
		   oldsample->sample.data + ss,
		   cutlen * 2);
	}
	memcpy(&copybuffer_sampleinfo, oldsample, sizeof(STSample));
    }

    if(!spliceout)
	return;

    if(newlen == 0) {
	sample_editor_clear_clicked();
	return;
    }

    newsample = malloc(newlen * 2);
    if(!newsample)
	return;

    sample_editor_lock_sample();

    memcpy(newsample,
	   oldsample->sample.data,
	   ss * 2);
    memcpy(newsample + ss,
	   oldsample->sample.data + se,
	   (oldsample->sample.length - se) * 2);

    free(oldsample->sample.data);

    oldsample->sample.data = newsample;
    oldsample->sample.length = newlen;

    /* Move loop start and end along with splice */
    if(oldsample->sample.loopstart > ss &&
       oldsample->sample.loopend < se) {
	/* loop was wholly within selection -- remove it */
	oldsample->sample.looptype = ST_MIXER_SAMPLE_LOOPTYPE_NONE;
	oldsample->sample.loopstart = 0;
	oldsample->sample.loopend = 1;
    } else {
	if(oldsample->sample.loopstart > se) {
	    /* loopstart was after selection */
	    oldsample->sample.loopstart -= (se-ss);
	} else if(oldsample->sample.loopstart > ss) {
	    /* loopstart was within selection */
	    oldsample->sample.loopstart = ss;
	}
	
	if(oldsample->sample.loopend > se) {
	    /* loopend was after selection */
	    oldsample->sample.loopend -= (se-ss);
	} else if(oldsample->sample.loopend > ss) {
	    /* loopend was within selection */
	    oldsample->sample.loopend = ss;
	}
    }

    st_sample_fix_loop(oldsample);
    sample_editor_unlock_sample();
    sample_editor_set_sample(oldsample);
    xm_set_modified(1);
}

static void
sample_editor_cut_clicked (void)
{
    sample_editor_copy_cut_common(TRUE, TRUE);
}

static void
sample_editor_remove_clicked (void)
{
    sample_editor_copy_cut_common(FALSE, TRUE);
}

static void
sample_editor_copy_clicked (void)
{
    sample_editor_copy_cut_common(TRUE, FALSE);
}

static void
sample_editor_init_sample (const char *samplename)
{
    STInstrument *instr;

    st_clean_sample(current_sample, NULL);
    
    instr = instrument_editor_get_instrument();
    if(st_instrument_num_samples(instr) == 0) {
	st_clean_instrument(instr, samplename);
	memset(instr->samplemap, gui_get_current_sample(), sizeof(instr->samplemap));
    }
	
    st_clean_sample(current_sample, samplename);
    
    current_sample->volume = 64;
    current_sample->finetune = 0;
    current_sample->panning = 128;
    current_sample->relnote = 0;
}

void
sample_editor_paste_clicked (void)
{
    gint16 *newsample;
    STSample *oldsample = current_sample;
    int ss = sampledisplay->sel_start, newlen;
    int update_ie = 0;

    if(oldsample == NULL || copybuffer == NULL)
	return;

    if(!oldsample->sample.data) {
	/* pasting into empty sample */
	sample_editor_lock_sample();
	sample_editor_init_sample(_("<just pasted>"));
	oldsample->treat_as_8bit = copybuffer_sampleinfo.treat_as_8bit;
	oldsample->volume = copybuffer_sampleinfo.volume;
	oldsample->finetune = copybuffer_sampleinfo.finetune;
	oldsample->panning = copybuffer_sampleinfo.panning;
	oldsample->relnote = copybuffer_sampleinfo.relnote;
	sample_editor_unlock_sample();
	ss = 0;
	update_ie = 1;
    } else {
	if(ss == -1)
	    return;
    }

    newlen = oldsample->sample.length + copybufferlen;

    newsample = malloc(newlen * 2);
    if(!newsample)
	return;

    sample_editor_lock_sample();

    memcpy(newsample,
	   oldsample->sample.data,
	   ss * 2);
    st_convert_sample(copybuffer,
		      newsample + ss,
		      16,
		      16,
		      copybufferlen);
    if(oldsample->treat_as_8bit) {
	st_sample_cutoff_lowest_8_bits(newsample + ss,
				       copybufferlen);
    }
    memcpy(newsample + (ss + copybufferlen),
	   oldsample->sample.data + ss,
	   (oldsample->sample.length - ss) * 2);

    free(oldsample->sample.data);

    oldsample->sample.data = newsample;
    oldsample->sample.length = newlen;

    sample_editor_unlock_sample();
    sample_editor_update();
    if(update_ie)
	instrument_editor_update();
    xm_set_modified(1);
}


static void
sample_editor_reverse_clicked (void)
{
    int ss = sampledisplay->sel_start, se = sampledisplay->sel_end;
    int i;
    gint16 *p, *q;

    if(!current_sample || ss == -1) {
	return;
    }

    sample_editor_lock_sample();

    p = q = current_sample->sample.data;
    p += ss;
    q += se;

    for(i = 0; i < (se - ss)/2; i++) {
	gint16 t = *p;
	*p++ = *--q;
	*q = t;
    }

    xm_set_modified(1);
    sample_editor_unlock_sample();
    sample_editor_update();
    sample_display_set_selection(sampledisplay, ss, se);
}

#if USE_SNDFILE || !defined (NO_AUDIOFILE)

static void
sample_editor_load_wav_main (int mode)
{ 
    /* Initialized global variables:

       wavload_through_library (TRUE or FALSE)
       wavload_samplename     (the name the sample is going to get in the XM)
       wavload_frameCount     (length of the file /stereo /16bits)

       with audiofile:
	 wavload_file;
	 wavload_sampleWidth, wavload_channelCount;
       without audiofile:
         wavload_filename;
	 wavload_sampleWidth, wavload_channelCount;
	 wavload_endianness, wavload_unsignedwords;
    */

    void *sbuf, *sbuf_loadto;
#if USE_SNDFILE
    sf_count_t len;
#else
    int len;
#endif
    int i, count;
    float rate;

    statusbar_update(STATUS_LOADING_SAMPLE, TRUE);
    
    len = 2 * wavload_frameCount * wavload_channelCount;
    if(!(sbuf = malloc(len))) {
	error_error(_("Out of memory for sample data."));
	goto errnobuf;
    }

    if(wavload_sampleWidth == 16) {
	sbuf_loadto = sbuf;
    } else {
	sbuf_loadto = sbuf + len / 2;
    }

    if(wavload_through_library) {
#if USE_SNDFILE
	if(wavload_frameCount != sf_readf_short(wavload_file, sbuf_loadto, wavload_frameCount)) {
#else
	if(wavload_frameCount != afReadFrames(wavload_file, AF_DEFAULT_TRACK, sbuf_loadto, wavload_frameCount)) {
#endif
	    error_error(_("Read error."));
	    goto errnodata;
	}
    } else {
	FILE *f = fopen(wavload_filename, "r");
	if(!f)
	    goto errnodata;
	if(wavload_frameCount != fread(sbuf_loadto,
				       wavload_channelCount * wavload_sampleWidth / 8,
				       wavload_frameCount,
				       f)) {
	    fclose(f);
	    error_error(_("Read error."));
	    goto errnodata;
	}
	fclose(f);
    }

    sample_editor_lock_sample();
    sample_editor_init_sample(wavload_samplename);
    current_sample->sample.data = sbuf;
    current_sample->treat_as_8bit = (wavload_sampleWidth == 8);
    current_sample->sample.length = wavload_frameCount;

    // Initialize relnote and finetune such that sample is played in original speed
    if(wavload_through_library) {
#if USE_SNDFILE
	current_sample->treat_as_8bit = ((wavinfo.format & (SF_FORMAT_PCM_S8 | SF_FORMAT_PCM_U8)) != 0);
	rate = wavinfo.samplerate;
#else
	rate = afGetRate(wavload_file, AF_DEFAULT_TRACK);
#endif
    } else {
	rate = wavload_rate;
    }
    xm_freq_note_to_relnote_finetune(rate,
				     4 * 12 + 1, // at C-4
				     &current_sample->relnote,
				     &current_sample->finetune);

    if(wavload_sampleWidth == 8) {
	if(wavload_through_library || wavload_unsignedwords) {
	    st_sample_8bit_signed_unsigned(sbuf_loadto, len / 2);
	}
	st_convert_sample(sbuf_loadto,
			  sbuf,
			  8,
			  16,
			  len / 2);
    } else {
	if(wavload_through_library) {
	    // I think that is what the virtualByteOrder stuff is for.
	    // le_16_array_to_host_order(sbuf, len / 2);
	} else {
#ifdef WORDS_BIGENDIAN
	    if(wavload_endianness == 0) {
#else
	    if(wavload_endianness == 1) {
#endif
		byteswap_16_array(sbuf, len / 2);
	    }
	    if(wavload_unsignedwords) {
		st_sample_16bit_signed_unsigned(sbuf_loadto, len / 2);
	    }
	}
    }

    if(mode != MODE_MONO) {
	gint16 *a = sbuf, *b = sbuf;

	count = wavload_frameCount;

	/* Code could probably be made shorter. But this is readable. */
	switch(mode) {
	case MODE_STEREO_MIX:
	    for(i = 0; i < count; i++, a+=2, b+=1)
		*b = (a[0] + a[1]) / 2;
	    break;
	case MODE_STEREO_LEFT:
	    for(i = 0; i < count; i++, a+=2, b+=1)
		*b = a[0];
	    break;
	case MODE_STEREO_RIGHT:
	    for(i = 0; i < count; i++, a+=2, b+=1)
		*b = a[1];
	    break;
	default:
	    g_assert_not_reached();
	    break;
	}
    }

    sample_editor_unlock_sample();

    instrument_editor_update();
    sample_editor_update();
    xm_set_modified(1);
    statusbar_update(STATUS_SAMPLE_LOADED, FALSE);
    goto errnobuf;

  errnodata:
    statusbar_update(STATUS_IDLE, FALSE);
    free(sbuf);
  errnobuf:
    if(wavload_through_library) {
#if USE_SNDFILE
	sf_close (wavload_file);
#else
	afCloseFile(wavload_file);
#endif
	wavload_file = NULL;
    }
}

static void
sample_editor_wavload_dialog_hide (GtkWidget *widget)
{
    gtk_widget_destroy(wavload_dialog);
}

static void
sample_editor_wavload_dialog_left (GtkWidget *widget)
{
    gtk_widget_destroy(wavload_dialog);
    sample_editor_load_wav_main(MODE_STEREO_LEFT);
}

static void
sample_editor_wavload_dialog_mix (GtkWidget *widget)
{
    gtk_widget_destroy(wavload_dialog);
    sample_editor_load_wav_main(MODE_STEREO_MIX);
}

static void
sample_editor_wavload_dialog_right (GtkWidget *widget)
{
    gtk_widget_destroy(wavload_dialog);
    sample_editor_load_wav_main(MODE_STEREO_RIGHT);
}

static void
sample_editor_open_stereowav_dialog (void)
{
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *box1;
    GtkWidget *box2;
    GtkWidget *separator;
    GtkWidget *label;
         
    window = gtk_window_new (GTK_WINDOW_DIALOG);
   
    wavload_dialog=window;

    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			GTK_SIGNAL_FUNC (sample_editor_wavload_dialog_hide), NULL); 

    gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW(window), _("Load stereo sample"));
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(mainwindow));
    
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
       
    box1 = gtk_vbox_new (FALSE, 2);
    
    label = gtk_label_new (_("You have selected a stereo sample!\n(SoundTracker can only handle mono samples!)\n\nPlease choose which channel to load:"));
    gtk_label_set_justify (GTK_LABEL (label),GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (box1), label, FALSE, TRUE, 0);
    gtk_widget_show (label);
	
    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 5);
    gtk_widget_show (separator);
	
    box2 = gtk_hbox_new(TRUE, 4);
    
    button = gtk_button_new_with_label (_("Left"));
    gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			GTK_SIGNAL_FUNC (sample_editor_wavload_dialog_left), NULL);
    gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
    gtk_widget_show (button);
 
    button = gtk_button_new_with_label (_("Mix"));
    gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			GTK_SIGNAL_FUNC (sample_editor_wavload_dialog_mix), NULL);
    gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
    gtk_widget_show (button);
 
    button = gtk_button_new_with_label (_("Right"));
    gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			GTK_SIGNAL_FUNC (sample_editor_wavload_dialog_right), NULL);
    gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
    gtk_widget_show (button);

    gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, FALSE, 0);
    gtk_widget_show (box2);
	
    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 5);
    gtk_widget_show (separator);
     
    button = gtk_button_new_with_label (_("Cancel"));
    gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			GTK_SIGNAL_FUNC (sample_editor_wavload_dialog_hide), NULL);   
    gtk_box_pack_start (GTK_BOX (box1), button, FALSE, FALSE, 0);
    gtk_widget_show (button);
    
    gtk_container_add (GTK_CONTAINER (window), box1);
    
    gtk_widget_show (box1);
    gtk_widget_show (window);
}

static void
sample_editor_raw_sample_dialog_hide (GtkWidget *widget)
{
    gtk_widget_destroy(wavload_dialog);
}

static void
sample_editor_raw_sample_dialog_ok (GtkWidget *widget)
{
    wavload_sampleWidth = find_current_toggle(wavload_raw_resolution_w, 2) == 1 ? 16 : 8;
    wavload_endianness = find_current_toggle(wavload_raw_endian_w, 2);
    wavload_channelCount = find_current_toggle(wavload_raw_channels_w, 2) == 1 ? 2 : 1;
    wavload_unsignedwords = find_current_toggle(wavload_raw_signed_w, 2);
    wavload_rate = atoi (gtk_entry_get_text(GTK_ENTRY(wavload_raw_rate)));

    gtk_widget_destroy(wavload_dialog);

    if(wavload_sampleWidth == 16) {
	wavload_frameCount /= 2;
    }

    if(wavload_channelCount == 2) {
	wavload_frameCount /= 2;
	sample_editor_open_stereowav_dialog();
    } else {
	sample_editor_load_wav_main(MODE_MONO);
    }
}

static void
sample_editor_open_raw_sample_dialog (const gchar *filename)
{
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *box1;
    GtkWidget *box2;
    GtkWidget *separator;
    GtkWidget *label;
    GtkWidget *thing;
    GtkWidget *combo;
    GList *combo_items = NULL;
    
    static const char *resolutionlabels[] = { N_("8 bits"), N_("16 bits"), NULL };
    static const char *signedlabels[] = { N_("Signed"), N_("Unsigned"), NULL };
    static const char *endianlabels[] = { N_("Little-Endian"), N_("Big-Endian"), NULL };
    static const char *channelslabels[] = { N_("Mono"), N_("Stereo"), NULL };

    wavload_filename = filename; // store for later usage

    window = gtk_window_new (GTK_WINDOW_DIALOG);
   
    wavload_dialog = window;

    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			GTK_SIGNAL_FUNC (sample_editor_raw_sample_dialog_hide), NULL); 

    gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW(window), _("Load raw sample"));
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(mainwindow));
    
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
       
    box1 = gtk_vbox_new (FALSE, 2);
    
    label = gtk_label_new (_("You have selected a sample that is not\nin a known format. You can load the raw data now.\n\nPlease choose a format:"));
    gtk_label_set_justify (GTK_LABEL (label),GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (box1), label, FALSE, TRUE, 0);
    gtk_widget_show (label);
	
    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 5);
    gtk_widget_show (separator);
	
    // The toggles
    
    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Resolution:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);
    make_radio_group_full(resolutionlabels, box2, wavload_raw_resolution_w,
			  FALSE, TRUE, NULL, NULL);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Word format:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);
    make_radio_group_full(signedlabels, box2, wavload_raw_signed_w,
			  FALSE, TRUE, NULL, NULL);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

    add_empty_hbox(box2);
    make_radio_group_full(endianlabels, box2, wavload_raw_endian_w,
			  FALSE, TRUE, NULL, NULL);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Channels:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);
    make_radio_group_full(channelslabels, box2, wavload_raw_channels_w,
			  FALSE, TRUE, NULL, NULL);

    // Standard sampling rates

    combo_items = g_list_append (combo_items, "8000");
    combo_items = g_list_append (combo_items, "8363");
    combo_items = g_list_append (combo_items, "11025");
    combo_items = g_list_append (combo_items, "12000");
    combo_items = g_list_append (combo_items, "16000");
    combo_items = g_list_append (combo_items, "16726");
    combo_items = g_list_append (combo_items, "22050");
    combo_items = g_list_append (combo_items, "24000");
    combo_items = g_list_append (combo_items, "32000");
    combo_items = g_list_append (combo_items, "33452");
    combo_items = g_list_append (combo_items, "44100");
    combo_items = g_list_append (combo_items, "48000");
    
    // Rate selection combo

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Sampling Rate:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);

    combo = gtk_combo_new ();
    gtk_combo_set_popdown_strings (GTK_COMBO (combo), combo_items);
    g_list_free (combo_items);  
    gtk_box_pack_start(GTK_BOX(box2), combo, FALSE, TRUE, 0);
    gtk_widget_show (combo);    
    wavload_raw_rate = GTK_COMBO (combo)->entry; 
    gtk_entry_set_text (GTK_ENTRY (wavload_raw_rate), _("8363")); // default
    
    // The bottom of the box

    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 5);
    gtk_widget_show (separator);
     
    button = gtk_button_new_with_label (_("OK"));
    gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			GTK_SIGNAL_FUNC (sample_editor_raw_sample_dialog_ok), NULL);
    gtk_box_pack_start (GTK_BOX (box1), button, FALSE, FALSE, 0);
    gtk_widget_show (button);
    
    button = gtk_button_new_with_label (_("Cancel"));
    gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			GTK_SIGNAL_FUNC (sample_editor_raw_sample_dialog_hide), NULL);
    gtk_box_pack_start (GTK_BOX (box1), button, FALSE, FALSE, 0);
    gtk_widget_show (button);
    
    gtk_container_add (GTK_CONTAINER (window), box1);
    
    gtk_widget_show (box1);
    gtk_widget_show (window);
}

static void
sample_editor_load_wav (void)
{
    const gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_LOAD_SAMPLE]));
#if USE_SNDFILE != 1
    int sampleFormat;
#endif

    gtk_widget_hide(fileops_dialogs[DIALOG_LOAD_SAMPLE]);

    if(!file_selection_is_valid(fn)) {
	/* No file was actually selected. */
	gnome_error_dialog(_("No file selected."));
	return;
    }

    g_return_if_fail(current_sample != NULL);

    file_selection_save_path(fn, gui_settings.loadsmpl_path);

    wavload_samplename = strrchr(fn, '/');
    if(!wavload_samplename)
	wavload_samplename = fn;
    else
	wavload_samplename++;

#if USE_SNDFILE
    wavinfo.format = 0;
    wavload_file = sf_open(fn, SFM_READ, &wavinfo);
#else
    wavload_file = afOpenFile(fn, "r", NULL);
#endif
    if(!wavload_file) {
	FILE *f = fopen(fn, "r");
	if(f) {
	    fseek(f, 0, SEEK_END);
	    wavload_frameCount = ftell(f);
	    fclose(f);
	    wavload_through_library = FALSE;
	    sample_editor_open_raw_sample_dialog(fn);
	} else {
	    error_error(_("Can't read sample"));
	}
	return;
    }

    wavload_through_library = TRUE;

#if USE_SNDFILE
    wavload_frameCount = wavinfo.frames;
#else
    wavload_frameCount = afGetFrameCount(wavload_file, AF_DEFAULT_TRACK);
#endif
    if(wavload_frameCount > mixer->max_sample_length) {
	error_warning(_("Sample is too long for current mixer module. Loading anyway."));
    }

#if USE_SNDFILE

    wavload_channelCount = wavinfo.channels;
    wavload_sampleWidth = 16;
    
#else

    wavload_channelCount = afGetChannels(wavload_file, AF_DEFAULT_TRACK);
    afGetSampleFormat(wavload_file, AF_DEFAULT_TRACK, &sampleFormat, &wavload_sampleWidth);

    /* I think audiofile-0.1.7 does this automatically, but I'm not sure */
#ifdef WORDS_BIGENDIAN
    afSetVirtualByteOrder(wavload_file, AF_DEFAULT_TRACK, AF_BYTEORDER_BIGENDIAN);
#else
    afSetVirtualByteOrder(wavload_file, AF_DEFAULT_TRACK, AF_BYTEORDER_LITTLEENDIAN);
#endif

#endif


    if((wavload_sampleWidth != 16 && wavload_sampleWidth != 8) || wavload_channelCount > 2) {
	error_error(_("Can only handle 8 and 16 bit samples with up to 2 channels"));
	goto errwrongformat;
    }

    if(wavload_channelCount == 1) {
	sample_editor_load_wav_main(MODE_MONO);
    } else {
	sample_editor_open_stereowav_dialog();
    }

    return;

  errwrongformat:
#if USE_SNDFILE
    sf_close(wavload_file);
#else
    afCloseFile(wavload_file);
#endif
    wavload_file = NULL;
    return;
}


static void
sample_editor_save_wav_main (const gchar *fn,
			     guint32 offset,
			     guint32 length)
{
#if USE_SNDFILE
    SNDFILE *outfile;
    SF_INFO sfinfo;
    int rate = 44100;
#else
    AFfilehandle outfile;
    AFfilesetup outfilesetup;
    double rate = 44100.0;
#endif

    statusbar_update(STATUS_SAVING_SAMPLE, TRUE);

    file_selection_save_path(fn, gui_settings.savesmpl_path);

#if USE_SNDFILE
    if(current_sample->treat_as_8bit) {
	sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_U8;
    } else {
	sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    }
    sfinfo.channels = 1;
    sfinfo.samplerate = rate;
    outfile = sf_open (fn, SFM_WRITE, &sfinfo);
#else
    outfilesetup = afNewFileSetup();
    afInitFileFormat(outfilesetup, AF_FILE_WAVE);
    afInitChannels(outfilesetup, AF_DEFAULT_TRACK, 1);
    if(current_sample->treat_as_8bit) {
	if(!libaf2) {
	    // for audiofile-0.1.x
	    afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 8);
	} else {
	    // for audiofile-0.2.x
	    afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_UNSIGNED, 8);
	}
    } else {
	afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    }
    afInitRate(outfilesetup, AF_DEFAULT_TRACK, rate);
    outfile = afOpenFile(fn, "w", outfilesetup);
    afFreeFileSetup(outfilesetup);
#endif

    if(!outfile) {
	error_error(_("Can't open file for writing."));
	return;
    }

#if USE_SNDFILE
    sf_writef_short (outfile, 
		     current_sample->sample.data + offset,
		     length);
    sf_close(outfile);
#else
    if(current_sample->treat_as_8bit) {
	void *buf = malloc(1 * length);
	g_assert(buf);
	st_convert_sample(current_sample->sample.data + offset,
			  buf,
			  16,
			  8,
			  length);
	st_sample_8bit_signed_unsigned(buf,
				       length);

	afWriteFrames(outfile, AF_DEFAULT_TRACK,
		      buf,
		      length);
	free(buf);
    } else {
	afWriteFrames(outfile, AF_DEFAULT_TRACK,
		      current_sample->sample.data + offset,
		      length);
    }
    afCloseFile(outfile);
#endif

    statusbar_update(STATUS_SAMPLE_SAVED, FALSE);
}

static void
sample_editor_save_wav_callback (gint reply,
		   gpointer data)
{
    if(reply == 0) {
		// save entire sample
		sample_editor_save_wav_main((gchar*)data, 0, current_sample->sample.length);
    }
}

static void
sample_editor_save_wav (void)
{
    gchar *fn;
	FILE *f;

    fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_SAMPLE]));

    gtk_widget_hide(fileops_dialogs[DIALOG_SAVE_SAMPLE]);

    if(!file_selection_is_valid(fn)) {
	/* No file was actually selected. */
	gnome_error_dialog(_("No file selected."));
	return;
    }

    g_return_if_fail(current_sample != NULL);
    if(current_sample->sample.data == NULL)
	return;

	f = fopen(fn, "r");

	if(f != NULL) {
	    fclose(f);
	    gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
				      _("Are you sure you want to overwrite the file?"),
				      sample_editor_save_wav_callback, fn);
	} else {
		sample_editor_save_wav_callback(0, fn);
    }
}

static void
sample_editor_save_region_callback (gint reply,
		   gpointer data)
{
    int rss = sampledisplay->sel_start, rse = sampledisplay->sel_end;

    if(reply == 0) {
        // save region
	sample_editor_save_wav_main((gchar*)data, rss, rse - rss);
    }
}

static void
sample_editor_save_region_wav (void)
{
    gchar *fn;
    FILE *f;

    fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileops_dialogs[DIALOG_SAVE_RGN_SAMPLE]));

    gtk_widget_hide(fileops_dialogs[DIALOG_SAVE_RGN_SAMPLE]);

    if(!file_selection_is_valid(fn)) {
	/* No file was actually selected. */
	gnome_error_dialog(_("No file selected."));
	return;
    }

    g_return_if_fail(current_sample != NULL);
    if(current_sample->sample.data == NULL)
	return;

    if(sampledisplay->sel_start == -1) {
        error_error(_("Please select region first."));
	return;
    }

	f = fopen(fn, "r");

	if(f != NULL) {
	    fclose(f);
	    gnome_app_ok_cancel_modal(GNOME_APP(mainwindow),
				      _("Are you sure you want to overwrite the file?"),
				      sample_editor_save_region_callback, fn);
	} else {
	    sample_editor_save_region_callback(0, fn);
    }
}

#endif /* USE_SNDFILE || !defined (NO_AUDIOFILE) */

/* ============================ Sampling functions coming up -------- */

GtkWidget*
sample_editor_create_sampling_widgets (void)
{
    GtkWidget *box, *thing, *box2;

    box = gtk_vbox_new(FALSE, 2);

    thing = sample_display_new(FALSE);
    gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    monitorscope = SAMPLE_DISPLAY(thing);
    
    box2 = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(box), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    thing = gtk_button_new_with_label(_("OK"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_ok_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_set_sensitive(thing, 0);
    gtk_widget_show(thing);
    okbutton = thing;

    thing = gtk_button_new_with_label(_("Start sampling"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_start_sampling_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    startsamplingbutton = thing;

    thing = gtk_button_new_with_label(_("Cancel"));
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_stop_sampling), NULL);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    cancelbutton = thing;

    return box;
}

static void
sampler_page_enable_widgets (int enable)
{
    gtk_widget_set_sensitive(okbutton, !enable);
    gtk_widget_set_sensitive(startsamplingbutton, enable);
}

static void
sample_editor_monitor_clicked (void)
{
    GtkWidget *mainbox, *thing;

    if(!sampling_driver || !sampling_driver_object) {
	error_error(_("No sampling driver available"));
	return;
    }

    if(samplingwindow != NULL) {
	gdk_window_raise(samplingwindow->window);
	return;
    }
    
#ifdef USE_GNOME
    samplingwindow = gnome_app_new("SoundTracker", _("Sampling Window"));
#else
    samplingwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(samplingwindow), _("Sampling Window"));
#endif
    gtk_signal_connect (GTK_OBJECT (samplingwindow), "delete_event",
			GTK_SIGNAL_FUNC (sample_editor_stop_sampling), NULL);

    mainbox = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(mainbox), 4);
#ifdef USE_GNOME
    gnome_app_set_contents(GNOME_APP(samplingwindow), mainbox);
#else
    gtk_container_add(GTK_CONTAINER(samplingwindow), mainbox);
#endif
    gtk_widget_show(mainbox);

    thing = sample_editor_create_sampling_widgets();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    sampler_page_enable_widgets(TRUE);

    recordbufs = NULL;
    sampling = 0;
    recordedlen = 0;
    currentoffs = recordbuflen;
    current = NULL;

    gtk_widget_show (samplingwindow);

    if(!sampling_driver->common.open(sampling_driver_object)) {
	sample_editor_stop_sampling();
    }
}

void
sample_editor_sampled (void *dest,
		       guint32 count,
		       int mixfreq,
		       int mixformat)
{
    int x;

    g_assert(mixformat == ST_MIXER_FORMAT_S16_LE);

    sample_display_set_data_16(monitorscope, dest, count, FALSE);

    while(sampling && count > 0) {
	if(currentoffs == recordbuflen) {
	    struct recordbuf *newbuf = malloc(sizeof(struct recordbuf) + recordbuflen * 2);
	    if(!newbuf) {
		error_error(_("Out of memory while sampling!"));
		sampling = 0;
		break;
	    }
	    newbuf->next = NULL;
	    newbuf->length = 0;
	    currentoffs = 0;
	    if(!recordbufs)
		recordbufs = newbuf;
	    else
		current->next = newbuf;
	    current = newbuf;
	}

	x = MIN(count, recordbuflen - currentoffs);
	memcpy(current->data + currentoffs, dest, x * 2);
	dest += x * 2;
	count -= x;
	current->length += x;
	currentoffs += x;
	recordedlen += x;
    }
}

void
sample_editor_stop_sampling (void)
{
    struct recordbuf *r, *r2;

    if(!samplingwindow) {
	return;
    }

    sampling_driver->common.release(sampling_driver_object);

    gtk_widget_destroy(samplingwindow);
    samplingwindow = NULL;

    /* clear the recorded sample */
    for(r = recordbufs; r; r = r2) {
	r2 = r->next;
	free(r);
    }
}

static void
sample_editor_ok_clicked (void)
{
    STInstrument *instr;
    struct recordbuf *r, *r2;
    gint16 *sbuf;
    char *samplename = _("<just sampled>");
    double rate = 44100.0;

    sampling_driver->common.release(sampling_driver_object);

    gtk_widget_destroy(samplingwindow);
    samplingwindow = NULL;

    g_return_if_fail(current_sample != NULL);

    sample_editor_lock_sample();

    st_clean_sample(current_sample, NULL);

    instr = instrument_editor_get_instrument();
    if(st_instrument_num_samples(instr) == 0)
	st_clean_instrument(instr, samplename);

    st_clean_sample(current_sample, samplename);
    
    sbuf = malloc(recordedlen * 2);
    current_sample->sample.data = sbuf;
    
    for(r = recordbufs; r; r = r2) {
	r2 = r->next;
	memcpy(sbuf, r->data, r->length * 2);
	sbuf += r->length;
	free(r);
    }

    if(recordedlen > mixer->max_sample_length) {
	error_warning(_("Recorded sample is too long for current mixer module. Using it anyway."));
    }

    current_sample->sample.length = recordedlen;
    current_sample->volume = 64;
    current_sample->panning = 128;
    current_sample->treat_as_8bit = FALSE;

    xm_freq_note_to_relnote_finetune(rate,
				     4 * 12 + 1, // at C-4
				     &current_sample->relnote,
				     &current_sample->finetune);

    sample_editor_unlock_sample();

    instrument_editor_update();
    sample_editor_update();
    xm_set_modified(1);
}

static void
sample_editor_start_sampling_clicked (void)
{
    sampler_page_enable_widgets(FALSE);
    sampling = 1;
}

/* ==================== VOLUME RAMPING DIALOG =================== */

static void
sample_editor_lrvol (GtkWidget *widget,
                     gpointer data)
{
    int mode = GPOINTER_TO_INT(data);

    switch(mode)
    {
    case 0: case 2:
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[mode/2]), 50);
        break;
    case 4: case 8:
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[mode/8]), 200);
        break;
    }
}


static void
sample_editor_open_volume_ramp_dialog (void)
{
    GtkWidget *mainbox, *box1, *thing;
    gint i;
    const char *labels1[] = {
	_("Normalize"),
	_("Execute"),
	_("Close")
    };

    if(volrampwindow != NULL) {
	gdk_window_raise(volrampwindow->window);
	return;
    }
    
#ifdef USE_GNOME
    volrampwindow = gnome_app_new("SoundTracker", _("Volume Ramping"));
#else
    volrampwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(volrampwindow), _("Volume Ramping"));
#endif
    gtk_signal_connect (GTK_OBJECT (volrampwindow), "delete_event",
			GTK_SIGNAL_FUNC (sample_editor_close_volume_ramp_dialog), NULL);

    gtk_window_set_transient_for(GTK_WINDOW(volrampwindow), GTK_WINDOW(mainwindow));

    mainbox = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(mainbox), 4);
#ifdef USE_GNOME
    gnome_app_set_contents(GNOME_APP(volrampwindow), mainbox);
#else
    gtk_container_add(GTK_CONTAINER(volrampwindow), mainbox);
#endif
    gtk_widget_show(mainbox);

    thing = gtk_label_new(_("Perform linear volume fade on Selection"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box1);
    gtk_box_pack_start(GTK_BOX(mainbox), box1, FALSE, TRUE, 0);

    gui_put_labelled_spin_button(box1, _("Left [%]:"), -1000, 1000, &sample_editor_volramp_spin_w[0], NULL, NULL);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[0]), sample_editor_volramp_last_values[0]);

    thing = gtk_button_new_with_label(_("H"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
                       GTK_SIGNAL_FUNC(sample_editor_lrvol), (gpointer)0);

    thing = gtk_button_new_with_label(_("D"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
                       GTK_SIGNAL_FUNC(sample_editor_lrvol), (gpointer)4);

    add_empty_hbox(box1);

    gui_put_labelled_spin_button(box1, _("Right [%]:"), -1000, 1000, &sample_editor_volramp_spin_w[1], NULL, NULL);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[1]), sample_editor_volramp_last_values[1]);

    thing = gtk_button_new_with_label(_("H"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
                       GTK_SIGNAL_FUNC(sample_editor_lrvol), (gpointer)2);

    thing = gtk_button_new_with_label(_("D"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
                       GTK_SIGNAL_FUNC(sample_editor_lrvol), (gpointer)8);

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box1);
    gtk_box_pack_start(GTK_BOX(mainbox), box1, FALSE, TRUE, 0);

    for(i = 0; i < 3; i++) {
	thing = gtk_button_new_with_label(labels1[i]);
	gtk_widget_show(thing);
	gtk_box_pack_start(GTK_BOX(box1), thing, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(thing), "clicked",
			   GTK_SIGNAL_FUNC(sample_editor_perform_ramp),
			   GINT_TO_POINTER(i));
    }

    gtk_widget_show (volrampwindow);
}

static void
sample_editor_close_volume_ramp_dialog (void)
{
    sample_editor_volramp_last_values[0] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[0]));
    sample_editor_volramp_last_values[1] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[1]));

    gtk_widget_destroy(volrampwindow);
    volrampwindow = NULL;
}

static void
sample_editor_perform_ramp (GtkWidget *w,
			    gpointer data)
{
    int action = GPOINTER_TO_INT(data);
    double left, right;
    const int ss = sampledisplay->sel_start, se = sampledisplay->sel_end;
    int i;
    gint16 *p;
 
    if(action == 2 || !current_sample || ss == -1) {
	sample_editor_close_volume_ramp_dialog();
	return;
    }

    if(action == 0) {
	// Find maximum amplitude
	int m;
	int q;
	p = current_sample->sample.data;
	p += ss;
	for(i = 0, m = 0; i < se - ss; i++) {
	    q = *p++;
	    q = ABS(q);
	    if(q > m)
		m = q;
	}
	left = right = (double)0x7fff / m;
    } else {
	left = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[0])) / 100;
	right = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[1])) / 100;
    }

    // Now perform the actual operation
    sample_editor_lock_sample();

    p = current_sample->sample.data;
    p += ss;
    for(i = 0; i < se - ss; i++) {
	double q = *p;
	q *= left + i * (right - left) / (se - ss);
	*p++ = CLAMP((int)q, -32768, +32767);
    }

    if(current_sample->treat_as_8bit) {
	st_sample_cutoff_lowest_8_bits(current_sample->sample.data + ss,
				       se - ss);
    }
    
    sample_editor_unlock_sample();
    xm_set_modified(1);
    sample_editor_update();
    sample_display_set_selection(sampledisplay, ss, se);
}

/* =================== TRIM AND CROP FUNCTIONS ================== */
static void
togglebutton_toggled (GtkToggleButton *widget, gboolean *state_var)
{
    *state_var = gtk_toggle_button_get_active(widget);
}

static void
adjustment_value_changed (GtkAdjustment *adj, gfloat *value)
{
    *value = adj->value;
}

#ifndef USE_GNOME
static void
trim_dialog_close_requested ()
{
    gtk_widget_hide(trimdialog);
}
#endif

static void
trim_dialog_clicked (GtkWidget *widget, gint button)
{
    if(button == 0)
	sample_editor_trim(trimbeg, trimend, threshold);
}

static void
sample_editor_trim_dialog (void)
{
    GtkObject *adj;
    GtkWidget *mainbox, *thing;
#ifndef USE_GNOME
    GtkWidget *button, *vbox;
#endif

    if(trimdialog != NULL) {
	gtk_widget_show(trimdialog);
	return;
    }
    
#ifdef USE_GNOME
    trimdialog = gnome_dialog_new(_("Trim parameters"),
			GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
    gnome_dialog_close_hides(GNOME_DIALOG(trimdialog), TRUE);
    gnome_dialog_set_close(GNOME_DIALOG(trimdialog), TRUE);
    gnome_dialog_set_default(GNOME_DIALOG(trimdialog), 0);
    gtk_signal_connect(GTK_OBJECT(trimdialog), "clicked",
			GTK_SIGNAL_FUNC (trim_dialog_clicked), NULL);
    mainbox = GNOME_DIALOG(trimdialog)->vbox;
#else
/* stolen from Gnome UI code. With Gnome life seemed so easy... (yaliaev) */
    trimdialog = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_window_set_title(GTK_WINDOW(trimdialog), _("Trim parameters"));
    gtk_container_border_width(GTK_CONTAINER(trimdialog), 4);
    
    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
    gtk_container_add(GTK_CONTAINER(trimdialog), vbox);
    gtk_widget_show(vbox);

    gtk_window_set_policy (GTK_WINDOW (trimdialog), FALSE, 
			 FALSE, FALSE);

    mainbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start (GTK_BOX (vbox), mainbox, 
		      TRUE, TRUE, 4);
		      
    thing = gtk_hbutton_box_new ();

    gtk_button_box_set_spacing (GTK_BUTTON_BOX (thing), 8);
    
    button = gtk_button_new_with_label(_("Ok"));
    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (thing), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC (trim_dialog_clicked), (gpointer)0);
    gtk_widget_grab_default (button);
    gtk_widget_show (button);

    button = gtk_button_new_with_label(_("Cancel"));
    gtk_box_pack_start (GTK_BOX (thing), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC (trim_dialog_clicked), (gpointer)1);
    gtk_widget_show (button);

    gtk_box_pack_end (GTK_BOX (vbox), thing, 
		    FALSE, TRUE, 0);
    gtk_widget_show (thing);

    thing = gtk_hseparator_new ();
    gtk_box_pack_end (GTK_BOX (vbox), thing, 
		      FALSE, TRUE, 4);
    gtk_widget_show (thing);
		      
    gtk_signal_connect(GTK_OBJECT (trimdialog), "delete_event",
			GTK_SIGNAL_FUNC (trim_dialog_close_requested), NULL);
#endif
    thing = gtk_check_button_new_with_label(_("Trim at the beginning"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), trimbeg);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(togglebutton_toggled), &trimbeg);
    gtk_widget_show(thing);

    thing = gtk_check_button_new_with_label(_("Trim at the end"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), trimend);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(togglebutton_toggled), &trimend);
    gtk_widget_show(thing);

    thing = gtk_label_new(_("Threshold (dB)"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    thing = gtk_hscale_new(GTK_ADJUSTMENT(adj = gtk_adjustment_new(threshold, -80.0, -20.0, 1.0, 5.0, 0.0)));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		       GTK_SIGNAL_FUNC(adjustment_value_changed), &threshold);
    gtk_widget_show(thing);

    gtk_widget_show_all(trimdialog);
}			
			
static void 
sample_editor_trim(gboolean trbeg, gboolean trend, gfloat thrshld)
{
    int start = sampledisplay->sel_start, end = sampledisplay->sel_end;
    int i, c, ofs;
    int amp = 0, val, bval = 0, maxamp, ground;
    int on, off;
    double avg;
    int reselect = 1; 
    gint16 *data;

    if(current_sample == NULL) return;
    if(!trbeg && !trend) return;
   
    /* if there's no selection, we operate on the entire sample */
    if(start == -1) {
        start = 0;
        end = current_sample->sample.length;
        reselect = 0;
    }
    
    data = current_sample->sample.data;
    /* Finding the maximum amplitude */
    for(i = 0, maxamp = 0; i < end - start; i++) {
	val = *(data + i);
	val = ABS(val);
	if(val > maxamp)
	    maxamp = val;
    }
  
    if (maxamp == 0) return;
   
    ground = rint((gfloat)maxamp * pow(10.0, thrshld/20));
    
    /* Computing the beginning average level until we reach the ground level */
    for(c = 0, ofs = start, amp = 0, avg = 0; ofs < end && amp < ground ; ofs++) {
	val = *(data + ofs);
	if (ofs == start) {
    	    bval = - val;
    	    amp = ABS(val);
	}
	if ((val < 0 && bval >= 0) || (val >= 0 && bval < 0)) {
    	    avg += amp;
    	    c++;
    	    amp = 0;
	} else {   
    	    if (ABS(val) > amp) amp = ABS(val);
	}   
	    bval = val;
    }
    avg = avg / c;
    
    /* Locating when sounds turns on.
       That is : when we *last* got higher than the average level. */
    for(amp = maxamp; ofs > start && amp > avg; --ofs) {
        //fprintf(stderr,"reverse\n");
        amp = 0;
        for(val = 1; ofs > start && val > 0; --ofs) {
            val = *(data + ofs);
            if (val > amp) amp = val;
        }
        for(; ofs > start && val <= 0; --ofs) {
            val = *(data + ofs);
            if (-val > amp) amp = -val;
        }
    }
    on = ofs;
   
    /* Computing the ending average level until we reach the ground level */
    for(ofs = end - 1, avg = 0, amp = 0, c = 0; ofs > on && amp < ground ; ofs--) {
        val = *(data + ofs);
        if (ofs == end -1) {
            bval = -val;
            amp = ABS(val);
        }
        if((val < 0 && bval >= 0) || (val >= 0 && bval < 0)) {
            avg += amp;
            c++;
            amp = 0;
        } else {   
            if (ABS(val) > amp) amp = ABS(val);
        }   
        bval = val;
    }
    avg = avg / c;
    
    /* Locating when sounds turns off.
       That is : when we *last* got higher than the average level. */
    for (amp = maxamp; ofs < end && amp > avg; ++ofs) {
        amp = 0;
        for (val = 1; ofs < end && val > 0; ++ofs) {
            val = *(data + ofs);
            if (val > amp) amp = val;
        }
        for (;ofs < end && val <= 0; ++ofs) {
            val = *(data + ofs);
            if (-val > amp) amp = -val;
        }
    }
    off = ofs;
    
    // Wiping blanks out
    if (on < start) on = start;
    if (off > end) off = end;
    sample_editor_lock_sample();
    if (trbeg) {
	sample_editor_delete(current_sample, start, on);
	off -= on - start;
	end -= on - start;
    }
    if (trend)
	sample_editor_delete(current_sample, off, end);
    st_sample_fix_loop(current_sample);
    sample_editor_unlock_sample();
    
    sample_editor_set_sample(current_sample);
    xm_set_modified(1);
    
    if (reselect == 1 && off > on) 
	sample_display_set_selection(sampledisplay, start, start + off - on);
}

static void 
sample_editor_crop()
{
    int start = sampledisplay->sel_start, end = sampledisplay->sel_end;

    if(current_sample == NULL || start == -1) 
    return;

    int l = current_sample->sample.length;
    
    sample_editor_lock_sample();
    sample_editor_delete(current_sample, 0, start);
    sample_editor_delete(current_sample, end - start, l - start);
    sample_editor_unlock_sample();
    
    sample_editor_set_sample(current_sample);
    xm_set_modified(1);
    
}

/* deletes the portion of *sample data from start to end-1 */
void
sample_editor_delete (STSample *sample,int start, int end)
{
    int newlen;
    gint16 *newdata;
    
    if(sample == NULL || start == -1 || start >= end)
	return;
    
    newlen = sample->sample.length - end + start;

    newdata = malloc(newlen * 2);
    if(!newdata)
	return;

    memcpy(newdata, sample->sample.data, start * 2);
    memcpy(newdata + start, sample->sample.data + end, (sample->sample.length - end) * 2);

    free(sample->sample.data);

    sample->sample.data = newdata;
    sample->sample.length = newlen;

    /* Move loop start and end along with splice */
    if(sample->sample.loopstart > start &&
       sample->sample.loopend < end) {
	/* loop was wholly within selection -- remove it */
	sample->sample.looptype = ST_MIXER_SAMPLE_LOOPTYPE_NONE;
	sample->sample.loopstart = 0;
	sample->sample.loopend = 1;
    } else {
	if(sample->sample.loopstart > end) {
	    /* loopstart was after selection */
	    sample->sample.loopstart -= (end-start);
	} else if(sample->sample.loopstart > start) {
	    /* loopstart was within selection */
	    sample->sample.loopstart = start;
	}
	
	if(sample->sample.loopend > end) {
	    /* loopend was after selection */
	    sample->sample.loopend -= (end-start);
	} else if(sample->sample.loopend > start) {
	    /* loopend was within selection */
	    sample->sample.loopend = start;
	}
    }

    st_sample_fix_loop(sample);

}
