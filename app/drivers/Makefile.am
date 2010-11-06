
noinst_LIBRARIES = libdrivers.a

libdrivers_a_SOURCES = dummy-drivers.c file-output.c

if DRIVER_OSS
  libdrivers_a_SOURCES += oss-output.c oss-input.c
endif

if DRIVER_ALSA
  libdrivers_a_SOURCES += alsa-output.c alsa-input.c
endif

if DRIVER_ALSA_050
  libdrivers_a_SOURCES += alsa2-output.c alsa2-input.c
endif

if DRIVER_ESD
  libdrivers_a_SOURCES += esd-output.c
endif

if DRIVER_JACK
  libdrivers_a_SOURCES += jack-output.c
endif

if DRIVER_IRIX
  libdrivers_a_SOURCES += irix-output.c
endif

if DRIVER_SUN
  libdrivers_a_SOURCES += sun-output.c sun-input.c
endif

if DRIVER_SDL
  libdrivers_a_SOURCES += sdl-output.c
endif

INCLUDES = -I.. ${ST_S_JACK_INCLUDES}

EXTRA_DIST = dsound-output.c
