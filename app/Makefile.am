SUBDIRS = drivers mixers

bin_PROGRAMS = soundtracker

soundtracker_SOURCES = \
	audio.c audio.h \
	audioconfig.c audioconfig.h \
	cheat-sheet.c cheat-sheet.h \
	clavier.c clavier.h \
	driver.h driver-in.h driver-out.h \
	endian-conv.c endian-conv.h \
	envelope-box.c envelope-box.h \
	errors.c errors.h \
	event-waiter.c event-waiter.h \
	extspinbutton.c extspinbutton.h \
	file-operations.c file-operations.h \
	gui-settings.c gui-settings.h \
	gui-subs.c gui-subs.h \
	gui.c gui.h \
	i18n.h \
	instrument-editor.c instrument-editor.h \
	keys.c keys.h \
	main.c main.h \
	menubar.c menubar.h \
	mixer.h \
	module-info.c module-info.h \
	playlist.c playlist.h \
	poll.c poll.h \
	preferences.c preferences.h \
	recode.c recode.h \
	sample-display.c sample-display.h \
	sample-editor.c sample-editor.h \
	scope-group.c scope-group.h \
	st-subs.c st-subs.h \
	time-buffer.c time-buffer.h \
	tips-dialog.c tips-dialog.h \
	track-editor.c track-editor.h \
	tracker.c tracker.h \
	tracker-settings.c tracker-settings.h \
	transposition.c transposition.h \
	xm.c xm.h \
	xm-player.c xm-player.h

if !NO_GDK_PIXBUF
  soundtracker_SOURCES += scalablepic.c scalablepic.h
endif

if DRIVER_ALSA_050
  soundtracker_SOURCES += midi-050.c midi-utils-050.c midi-settings-050.c \
	midi.h midi-settings.h midi-utils.h
endif

if DRIVER_ALSA_09x
  soundtracker_SOURCES += midi-09x.c midi-utils-09x.c midi-settings-09x.c \
	midi.h midi-settings.h midi-utils.h
endif

soundtracker_LDADD = drivers/libdrivers.a mixers/libmixers.a ${ST_S_JACK_LIBS}

install-exec-local:
	case `uname` in \
	  OpenBSD) \
	    chown root.wheel $(bindir)/soundtracker; \
	    ;; \
	  NetBSD) \
	    chown root.wheel $(bindir)/soundtracker; \
	    ;; \
	  Darwin) \
	    chown root:admin $(bindir)/soundtracker; \
	    ;; \
	  *) \
	    chown root.root $(bindir)/soundtracker; \
	    ;; \
	esac
	chmod +s $(bindir)/soundtracker
	@echo ""
	@echo "***"
	@echo "*** Installing SoundTracker suid root."
	@echo "*** This improves sound responsivity, but may be a security risk"
	@echo "*** if this is a multi-user machine. See file INSTALL."
	@echo "***"
	@echo ""


stdir = $(datadir)/soundtracker

#INCLUDES = -DDATADIR=\"$(stdir)\" \
#	-DLOCALEDIR=\"$(datadir)/locale\"
INCLUDES = -DLOCALEDIR=\"$(datadir)/locale\"
