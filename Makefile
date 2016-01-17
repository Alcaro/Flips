CFLAGS_gtk = -DFLIPS_GTK $(GTKFLAGS) $(GTKLIBS)
CFLAGS_windows := -DFLIPS_WINDOWS -mwindows -lgdi32 -lcomdlg32 -lcomctl32 -luser32 -lkernel32 -lshell32 -ladvapi32
CFLAGS_win := $(CFLAGS_windows)
CFLAGS_cli := -DFLIPS_CLI

CFLAGS_G = -fno-rtti -fno-exceptions

FNAME_gtk := flips
FNAME_windows := flips.exe
FNAME_win := $(FNAME_windows)
FNAME_cli := flips

CXX = g++

XFILES :=

ifeq ($(TARGET),)
  targetmachine := $(shell $(CC) -dumpmachine)
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
    $(warning if you have the needed files installed, specify their locations and names with `make 'GTKFLAGS=-I/usr/include' 'GTKLIBS=-L/usr/lib -lgtk'')
    $(warning if not, the package name under Debian and derivates is `libgtk-3-dev'; on other distros, consult a search engine)
    $(warning switching to CLI build)
    TARGET := cli
  endif
endif

all: $(FNAME_$(TARGET))

ifeq ($(TARGET),windows)
  XFILES += rc.o
rc.o:
	windres flips.rc rc.o
endif

MOREFLAGS := $(CFLAGS_$(TARGET))


ifneq ($(DIVSUF),no)
  DIVSUF_EXIST := $(lastword $(wildcard libdivsufsort-2*/include/config.h))
  DIVSUF := $(subst /include/config.h,,$(DIVSUF_EXIST))
  ifneq ($(DIVSUF),)
    DIVSUF_SOURCES := $(filter-out %/utils.c,$(wildcard $(DIVSUF)/lib/*.c))
    ifeq ($(DIVSUF_OPENMP),)
      ifeq ($(TARGET),gtk)
        DIVSUF_OPENMP := -fopenmp
      endif
    endif
    DIVSUF_CFLAGS := -DUSE_DIVSUFSORT -I$(DIVSUF)/include -DHAVE_CONFIG_H -DNDEBUG -D__STDC_FORMAT_MACROS $(DIVSUF_OPENMP)
    DIVSUF_LFLAGS :=
    MOREFLAGS += $(DIVSUF_CFLAGS) $(DIVSUF_SOURCES) $(DIVSUF_LFLAGS)
  else
    $(warning no libdivsufsort-2.x detected; switching to fallback SA-IS)
    $(warning libdivsufsort is approximately twice as fast as SA-IS)
  endif
endif


$(FNAME_$(TARGET)): *.cpp $(XFILES)
	$(CXX) $^ -std=c++98 $(CFLAGS) $(LFLAGS) $(CFLAGS_G) $(MOREFLAGS) $(XFILES) -o$@
