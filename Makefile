#This script creates a debug-optimized binary by default. If you're on Linux, you'll get a faster binary from make.sh.

SRCDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST))))))

CFLAGS_gtk = -DFLIPS_GTK $(GTKFLAGS) $(GTKLIBS)
CFLAGS_windows := -DFLIPS_WINDOWS -mwindows -lgdi32 -lcomdlg32 -lcomctl32 -luser32 -lkernel32 -lshell32 -ladvapi32
CFLAGS_cli := -DFLIPS_CLI

CFLAGS_G = -fno-rtti -fno-exceptions -DNDEBUG -Wall

FNAME_gtk := flips
FNAME_windows := flips.exe
FNAME_cli := flips

CXX ?= g++
CFLAGS ?= -g

XFILES :=

SOURCES := $(SRCDIR)/*.cpp

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
DATAROOTDIR ?= $(PREFIX)/share
DATADIR ?= $(DATAROOTDIR)

ifeq ($(TARGET),win)
  override TARGET := windows
endif

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
    GTKFLAGS := $(shell pkg-config --cflags --libs gtk+-3.0)
  endif
  ifeq ($(GTKFLAGS),)
    $(warning pkg-config can't find gtk+-3.0, or pkg-config itself can't be found)
    $(warning if you have the needed files installed, specify their locations and names with `make GTKFLAGS='-I/usr/include' GTKLIBS='-L/usr/lib -lgtk'')
    $(warning if not, the package names under Debian and derivates are `pkg-config libgtk-3-dev'; for other distros, consult a search engine)
    TARGET := cli
    ifeq ($(TARGET),gtk)
      $(warning build will now fail)
    else
      $(warning switching to CLI build)
    endif
  endif
endif

all: $(FNAME_$(TARGET))
obj:
	mkdir obj
clean: | obj
	rm obj/* || true

ifeq ($(TARGET),windows)
  XFILES += obj/rc.o
obj/rc.o: flips.rc flips.h | obj
	windres flips.rc obj/rc.o
endif

MOREFLAGS := $(CFLAGS_$(TARGET))

DIVSUF := $(SRCDIR)/libdivsufsort-2.0.1
SOURCES += $(DIVSUF)/lib/divsufsort.c $(DIVSUF)/lib/sssort.c $(DIVSUF)/lib/trsort.c
MOREFLAGS += -I$(DIVSUF)/include -DHAVE_CONFIG_H -D__STDC_FORMAT_MACROS

ifeq ($(TARGET),gtk)
  CFLAGS_G += -fopenmp
endif

$(FNAME_$(TARGET)): $(SOURCES) $(XFILES)
	$(CXX) $^ -std=c++98 $(CFLAGS_G) $(MOREFLAGS) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) $(LFLAGS) -o$@
ifeq ($(CFLAGS),-g)
	echo 'Compiled Floating IPS in debug mode; for better performance, use ./make.sh instead'
endif

ifeq ($(TARGET),gtk)
install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(DATAROOTDIR)/applications
	mkdir -p $(DESTDIR)$(DATAROOTDIR)/icons/hicolor/scalable/apps
	mkdir -p $(DESTDIR)$(DATAROOTDIR)/icons/hicolor/symbolic/apps
	mkdir -p $(DESTDIR)$(DATAROOTDIR)/metainfo
	install -p -m755 $(FNAME_$(TARGET)) $(DESTDIR)$(BINDIR)
	install -p -m755 $(SRCDIR)/data/com.github.Alcaro.Flips.desktop $(DESTDIR)$(DATAROOTDIR)/applications
	install -p -m644 $(SRCDIR)/data/com.github.Alcaro.Flips.svg $(DESTDIR)$(DATAROOTDIR)/icons/hicolor/scalable/apps
	install -p -m644 $(SRCDIR)/data/com.github.Alcaro.Flips-symbolic.svg $(DESTDIR)$(DATAROOTDIR)/icons/hicolor/symbolic/apps
	install -p -m644 $(SRCDIR)/data/com.github.Alcaro.Flips.metainfo.xml $(DESTDIR)$(DATAROOTDIR)/metainfo

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(FNAME_$(TARGET))
	rm -f $(DESTDIR)$(DATAROOTDIR)/applications/com.github.Alcaro.Flips.desktop
	rm -f $(DESTDIR)$(DATAROOTDIR)/icons/hicolor/scalable/apps/com.github.Alcaro.Flips.svg
	rm -f $(DESTDIR)$(DATAROOTDIR)/icons/hicolor/symbolic/apps/com.github.Alcaro.Flips-symbolic.svg
	rm -f $(DESTDIR)$(DATAROOTDIR)/metainfo/com.github.Alcaro.Flips.metainfo.xml
endif
