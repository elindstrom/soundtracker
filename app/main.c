
/*
 * The Real SoundTracker - main program
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

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "i18n.h"
#include "gui.h"
#include "xm.h"
#include "audio.h"
#include "keys.h"
#include "gui-settings.h"
#include "audioconfig.h"
#include "tips-dialog.h"
#include "menubar.h"
#include "track-editor.h"
#include "midi.h"
#include "midi-settings.h"
#include "file-operations.h"

#include <glib.h>
#include <gtk/gtk.h>

XM *xm = NULL;
int pipea[2], pipeb[2];

static void
sigsegv_handler (int parameter)
{
    signal(SIGSEGV, SIG_DFL);

    if(xm != NULL) {
	int retval = XM_Save(xm, "crash-save.xm", FALSE);
	printf("*** SIGSEGV caught.\n*** Saved current XM to 'crash-save.xm' in current directory.\n    (status %d)\n", retval);
	exit(1);
    }
}

int
main (int argc,
      char *argv[])
{
    extern void
	driver_out_dummy, driver_in_dummy,
#ifdef DRIVER_OSS
	driver_out_oss, driver_in_oss,
#endif
#ifdef DRIVER_ALSA
	driver_out_alsa, driver_in_alsa,
#endif
#ifdef DRIVER_ALSA_050
	driver_out_alsa2, driver_in_alsa2,
#endif
#ifdef DRIVER_ESD
	driver_out_esd,
#endif
#ifdef DRIVER_SGI
	driver_out_irix,
#endif
#ifdef DRIVER_JACK
	driver_out_jack,
#endif
#ifdef DRIVER_SUN
	driver_out_sun, driver_in_sun,
#endif
#ifdef DRIVER_SDL
	driver_out_sdl,
#endif
#if USE_SNDFILE || !defined (NO_AUDIOFILE)
//	driver_out_file,
#endif
#if 0
	driver_out_test,
#endif
#if defined(_WIN32)
	driver_out_dsound,
#endif
	mixer_kbfloat,
	mixer_integer32;

    g_thread_init(NULL);

    if(pipe(pipea) || pipe(pipeb)) {
	fprintf(stderr, "Cränk. Can't pipe().\n");
	return 1;
    }

    if(!audio_init(pipea[0], pipeb[1])) {
	fprintf(stderr, "Can't init audio thread.\n");
	return 1;
    }

    /* In case we run setuid root, the main thread must not have root
       privileges -- it must be set back to the calling user ID!  The
       setresuid() stuff is for gtk+-1.2.10 on Linux. */
#ifdef HAVE_SETRESUID
    /* These aren't in the header files, so we prototype them here.
     */
    {
	int setresuid(uid_t ruid, uid_t euid, uid_t suid);
	int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
	setresuid(getuid(), getuid(), getuid());
	setresgid(getgid(), getgid(), getgid());
    }
#else
    seteuid(getuid());
    setegid(getgid());
#endif

#if ENABLE_NLS
    gtk_set_locale();
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    tips_dialog_load_settings();

    if(!gui_splash(argc, argv)) {
	fprintf(stderr, "GUI Initialization failed.\n");
	return 1;
    }

    audio_ctlpipe = pipea[1];
    audio_backpipe = pipeb[0];

    mixers = g_list_append(mixers,
			   &mixer_kbfloat);
    mixers = g_list_append(mixers,
			   &mixer_integer32);

#if 0
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_test);
#endif

#ifdef DRIVER_OSS
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_oss);
    drivers[DRIVER_INPUT] = g_list_append(drivers[DRIVER_INPUT],
					  &driver_in_oss);
#endif

#ifdef DRIVER_SGI
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_irix);
#endif

#ifdef DRIVER_JACK
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_jack);
#endif

#ifdef DRIVER_SUN
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_sun);
    drivers[DRIVER_INPUT] = g_list_append(drivers[DRIVER_INPUT],
					  &driver_in_sun);
#endif

#ifdef _WIN32
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_dsound);
#endif

#ifdef DRIVER_ALSA
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_alsa);
    drivers[DRIVER_INPUT] = g_list_append(drivers[DRIVER_INPUT],
					   &driver_in_alsa);
#endif

#ifdef DRIVER_ALSA_050
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_alsa2);
    drivers[DRIVER_INPUT] = g_list_append(drivers[DRIVER_INPUT],
					  &driver_in_alsa2); 
#endif

#ifdef DRIVER_ESD
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_esd);
#endif

#ifdef DRIVER_SDL
    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_sdl);
#endif

#if USE_SNDFILE || !defined (NO_AUDIOFILE)
/*    drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					   &driver_out_file);

					   This driver is not being added to the list because it's rather special
   in that it takes a filename argument each time it's called.. need to
   think about better integration perhaps.
*/
#endif

    if(g_list_length(drivers[DRIVER_OUTPUT]) == 0) {
	drivers[DRIVER_OUTPUT] = g_list_append(drivers[DRIVER_OUTPUT],
					       &driver_out_dummy);
    }
    if(g_list_length(drivers[DRIVER_INPUT]) == 0) {
	drivers[DRIVER_INPUT] = g_list_append(drivers[DRIVER_INPUT],
					      &driver_in_dummy);
    }

    g_assert(g_list_length(mixers) >= 1);

    fileops_tmpclean();
    gui_settings_load_config();
    audioconfig_load_mixer_config(); // in case gui_init already loads a module

    if(gui_final(argc, argv)) {
	audioconfig_load_config();
	track_editor_load_config();
#if defined(DRIVER_ALSA_050) || defined(DRIVER_ALSA_09x)
	midi_load_config();
	midi_init();
#endif

	signal(SIGSEGV, sigsegv_handler);

	gtk_main();

	gui_play_stop(); /* so that audio driver is shut down correctly. */

	menubar_write_accels();

	if(gui_settings.save_settings_on_exit) {
	    keys_save_config();
	    gui_settings_save_config();
	    audioconfig_save_config();
	}

	gui_settings_save_config_always();
	tips_dialog_save_settings();
	track_editor_save_config();
#if defined(DRIVER_ALSA_050) || defined(DRIVER_ALSA_09x)
	midi_save_config();
#endif
	fileops_tmpclean();
	return 0;
    } else {
	fprintf(stderr, "GUI Initialization failed.\n");
	return 1;
    }
}
