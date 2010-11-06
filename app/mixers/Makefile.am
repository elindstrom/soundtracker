EXTRA_DIST = kb-x86-asm.S kb-x86-asmIntel.a \
	integer32.c integer32-asm.S integer32-asm.h

noinst_LIBRARIES = libmixers.a

if NO_ASM
MIXERSOURCES = \
	integer32.c \
	kb-x86.c kbfloat-mix.c kb-x86-asm.h
else
MIXERSOURCES = \
	integer32.c integer32-asm.S integer32-asm.h \
	kb-x86.c kb-x86-asm.h kb-x86-asm.S
endif

libmixers_a_SOURCES = $(MIXERSOURCES)

INCLUDES = -I..
