PROGRAM = flips
ARGUI = 1
ARWUTF = 1

EXTRAOBJ += obj/divsufsort-c$(OBJSUFFIX).o
EXTRAOBJ += patch/*.cpp
include arlib/Makefile
