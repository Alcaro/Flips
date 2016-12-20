PROGRAM = flips
ARGUI = 1
ARWUTF = 1

EXTRAOBJ += obj/divsufsort-c$(OBJSUFFIX).o
SOURCES += patch/*.cpp

DOMAINS += LDSS
SOURCES_LDSS := libdivsufsort-2.0.1/lib/divsufsort.c libdivsufsort-2.0.1/lib/sssort.c libdivsufsort-2.0.1/lib/trsort.c
CFLAGS_LDSS := -Ilibdivsufsort-2.0.1/include -DHAVE_CONFIG_H
include arlib/Makefile
