CFLAGS_gtk = -DFLIPS_GTK $(GTKFLAGS) $(GTKLIBS)
CFLAGS_windows := -DFLIPS_WINDOWS -mwindows -lgdi32 -lcomdlg32 -lcomctl32 -luser32 -lkernel32 -lshell32 -ladvapi32
CFLAGS_cli := -DFLIPS_CLI

CFLAGS_G = -fno-rtti -fno-exceptions -DNDEBUG

FNAME_gtk := flips
FNAME_windows := flips.exe
FNAME_cli := flips

CXX = g++

XFILES :=

SOURCES := *.cpp

ifeq ($(TARGET),)
  targetmachine := $(shell $(CXX) -dumpmachine)
  ifneq ($(findstring mingw,$(targetmachine)),)
    TARGET := windows
  else ifneq ($(findstring linux,$(targetmachine)),)
    TARGET := gtk
  else
    TARGET :=
  endif
endif

ifeq ($(TARGET),win)
  override TARGET := windows
endif

ifeq ($(TARGET),)
  uname := $(shell uname -a)
  ifeq ($(uname),)
    TARGET := windows
  else ifneq ($(findstring CYGWIN,$(uname)),)
    TARGET := windows
  else ifneq ($(findstring Darwin,$(uname)),)
    TARGET := cli
  else
    TARGET := gtk
  endif
endif

ifeq ($(TARGET),gtk)
  ifeq ($(GTKFLAGS),)
    GTKFLAGS := $(shell pkg-config --cflags --libs gtk+-3.0 2>/dev/null)
  endif
  ifeq ($(GTKFLAGS),)
    $(warning pkg-config can't find gtk+-3.0, or pkg-config itself can't be found)
    $(warning if you have the needed files installed, specify their locations and names with `make GTKFLAGS='-I/usr/include' GTKLIBS='-L/usr/lib -lgtk'')
    $(warning if not, the package name under Debian and derivates is `libgtk-3-dev'; for other distros, consult a search engine)
    $(warning switching to CLI build)
    TARGET := cli
  endif
endif

all: $(FNAME_$(TARGET))
obj:
	mkdir obj

ifeq ($(TARGET),windows)
  XFILES += obj/rc.o
obj/rc.o: flips.rc | obj
	windres flips.rc obj/rc.o
endif

MOREFLAGS := $(CFLAGS_$(TARGET))


ifneq ($(DIVSUF),no)
  DIVSUF_EXIST := $(lastword $(wildcard libdivsufsort-2*/include/config.h))
  DIVSUF := $(subst /include/config.h,,$(DIVSUF_EXIST))
  ifneq ($(DIVSUF),)
    DIVSUF_SOURCES := $(filter-out %/utils.c,$(wildcard $(DIVSUF)/lib/*.c))
    DIVSUF_CFLAGS := -I$(DIVSUF)/include -DHAVE_CONFIG_H -D__STDC_FORMAT_MACROS
    DIVSUF_LFLAGS :=
    MOREFLAGS += $(DIVSUF_CFLAGS) $(DIVSUF_SOURCES) $(DIVSUF_LFLAGS)
  else
    $(warning no libdivsufsort-2.x detected; switching to fallback libdivsufsort-lite)
    $(warning libdivsufsort is approximately 3% faster than lite)
    SOURCES += divsufsort.c
  endif
else
  SOURCES += divsufsort.c
endif

ifeq ($(TARGET),gtk)
  CFLAGS_G += -fopenmp
endif

$(FNAME_$(TARGET)): $(SOURCES) $(XFILES)
	$(CXX) $^ -std=c++98 $(CFLAGS) $(LFLAGS) $(CFLAGS_G) $(MOREFLAGS) $(XFILES) -o$@
