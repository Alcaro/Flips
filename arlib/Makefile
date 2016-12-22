#object filename structure:
#obj/RULE___OBJNAME___PATH.o
#RULE is the set of build flags for this file, nearly always DEFAULT
#OBJNAME is the OBJNAME= make parameter, or a default value
#PATH is the source filename, including extension, relative to project root, with slashes replaced with double underscore
#example: obj/DEFAULT___linux___arlib__file-posix.cpp.o

SPACE :=
SPACE +=

ifeq ($(OS),Windows_NT)
  OS = windows
else
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Linux)
    OS = linux
  else ifeq ($(UNAME_S),Darwin)
    OS = osx
  else
    $(error what weird OS is this?)
  endif
endif

ARGUI ?= 0
AROPENGL ?= 0
AROPENGL_D3DSYNC ?= 1
ARIMAGE ?= 0
ARTHREAD ?= 0
ARSANDBOX ?= 0
ARWUTF ?= 0
ARSOCKET ?= 0
ARSOCKET_SSL ?= openssl

DEBUG ?= 1
OPT ?= 0

CC = gcc
CFLAGS =
CXX = g++
CXXFLAGS = $(CFLAGS)
LD = g++
LFLAGS =
OBJNAME =
CCXXFLAGS = -fvisibility=hidden -Wall
ifneq ($(EXCEPTIONS),1)
  CCXXFLAGS += -fno-exceptions
endif

#double gcc bug combo:
#(1) GCC hates this pattern:
#//define foo(a,b,c) \
#//  bar(a) \
#//  bar(b) \
#//  bar(c)
# to my knowledge unreported
#(2) '#pragma GCC diagnostic ignored "-Wcomment"' does nothing
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431
CCXXFLAGS += -Wno-comment

EXESUFFIX =
EXTRAOBJ ?=
CONF_CXXFLAGS += $(CONF_CFLAGS)

ifeq ($(OS),linux)
  CONF_LFLAGS += -ldl
  ifeq ($(ARTHREADS),1)
    CONF_CFLAGS += -pthread
    CONF_LFLAGS += -pthread
  endif
  OBJNAME = linux
  TESTRUNNER = valgrind
endif

ifeq ($(OS),windows)
  EXESUFFIX = .exe
#  EXTRAOBJ = obj/resource$(OBJNAME).o
#  RC = windres
#  RCFLAGS =
#obj/resource$(OBJNAME).o: ico/*
#	$(RC) $(RCFLAGS) ico/minir.rc obj/resource$(OBJNAME).o
  OBJNAME = windows
endif

ifneq (,$(findstring test,$(MAKECMDGOALS)))
  SELFTEST = 1
endif
ifneq (,$(findstring check,$(MAKECMDGOALS)))
  SELFTEST = 1
endif

OPTFLAGS := -Os -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden
OPTFLAGS += -fno-unwind-tables -fno-asynchronous-unwind-tables
OPTFLAGS += -ffunction-sections -fdata-sections
OPTFLAGS += -Werror

ifeq ($(OPT),1)
  CFLAGS += $(OPTFLAGS)
  LFLAGS += -Wl,--gc-sections -s
  DEBUG = 0
  OBJNAME += -opt
endif
ifeq ($(DEBUG),1)
  CFLAGS += -g -DDEBUG
endif

ifeq ($(PROFILE),gen)
  CONF_CFLAGS += -fprofile-generate
  CONF_LFLAGS += -lgcov
endif
ifeq ($(PROFILE),use)
  CONF_CFLAGS += -fprofile-use -fprofile-correction
endif

OUTNAME = $(PROGRAM)$(EXESUFFIX)

#OBJMANGLE(rule,sources) - takes C/C++ source files and returns the mangled name under the specified rule
OBJMANGLE = $(patsubst %,obj/$1___$(OBJNAME)___%.o,$(subst /,__,$2))
#SOURCENAME(obj) - takes a .o file, returned from OBJMANGLE, and returns the corresponding source file
#does not handle multi-file inputs, use $(foreach)
SOURCENAME = $(strip $(subst __,/,$(lastword $(subst ___,$(SPACE),$(patsubst obj/%.o,%,$1)))))
#DOMAINNAME(obj) - takes a .o file, returned from OBJMANGLE, and returns the corresponding domain
DOMAINNAME = $(firstword $(subst ___,$(SPACE),$(patsubst obj/%.o,%,$1)))

CFLAGS_DEFAULT =
CFLAGS_NOWARN = -w

.SECONDEXPANSION:
obj/%.c.o: $$(call SOURCENAME,$$@) | obj
	$(CC) $(TRUE_CFLAGS) $(CFLAGS_$(call DOMAINNAME,$@)) -c $< -o $@
obj/%.cpp.o: $$(call SOURCENAME,$$@) | obj
	$(CXX) $(TRUE_CXXFLAGS) $(CFLAGS_$(call DOMAINNAME,$@)) -c $< -o $@

SOURCES += *.cpp
SOURCES_ARLIB := arlib/*.cpp

ifneq ($(SELFTEST),)
  ifeq ($(ARLIB_MAIN),)
    CONF_CFLAGS += -DARLIB_TEST -Dmain=not_quite_main
    CFLAGS_ARLIB += -UARLIB_TEST -DARLIB_TESTRUNNER
    DOMAINS :=
  else
    SOURCES :=
    CFLAGS_ARLIB += -DARLIB_TEST -DARLIB_TESTRUNNER -DARLIB_TEST_ARLIB
  endif
  OBJNAME += -test
endif

ifeq ($(ARGUI),1)
  SOURCES_ARLIB += arlib/gui/*.cpp
  ifeq ($(OS),windows)
    CONF_CFLAGS += -DARGUI_WINDOWS
    CONF_LFLAGS += -lgdi32 -lcomctl32 -lcomdlg32
  endif
  ifeq ($(OS),linux)
    CONF_CFLAGS += $(shell pkg-config --cflags gtk+-3.0) -DARGUI_GTK3 -DARGUIPROT_X11
    CONF_LFLAGS += -ldl -lX11 $(shell pkg-config --libs gtk+-3.0)
  endif
else
  CONF_CFLAGS += -DARGUI_NONE
endif

ifeq ($(AROPENGL),1)
  ifeq ($(ARGUI),0)
    $(error can't use OpenGL without the GUI)
  endif
  SOURCES_ARLIB += arlib/opengl/*.cpp
  CONF_CFLAGS += -DARLIB_OPENGL
  ifeq ($(OS),linux)
    CONF_LFLAGS += -ldl
  endif
  ifeq ($(AROPENGL_D3DSYNC),1)
    CONF_CFLAGS += -DAROPENGL_D3DSYNC
  endif
endif

ifeq ($(ARTHREAD),1)
  SOURCES_ARLIB += arlib/thread/*.cpp
  CONF_CFLAGS += -DARLIB_THREAD
  ifeq ($(OS),linux)
    CONF_CFLAGS += -pthread
    CONF_LFLAGS += -pthread
  endif
endif

ifeq ($(ARSANDBOX),1)
  SOURCES_ARLIB += arlib/sandbox/*.cpp
  CONF_CFLAGS += -DARLIB_SANDBOX
  #not true since the windows sandbox isn't a real sandbox
  #ifeq ($(OS),windows)
  #  #both sandbox and WuTF need to redirect functions; this implementation is in WuTF
  #  #if linux ends up needing it too, I'll split out the redirector to a top-level Arlib file
  #  #Arlib is designed for use with -Wl,--gc-sections anyways
  #  ARWUTF = 1
  #endif
endif

ifeq ($(ARWUTF),1)
  SOURCES_ARLIB += arlib/wutf/*.cpp
  CONF_CFLAGS += -DARLIB_WUTF
endif

ifeq ($(ARSOCKET),1)
  SOURCES_ARLIB += arlib/socket/*.cpp
  CONF_CFLAGS += -DARLIB_SOCKET
  
  ifeq ($(OS),windows)
    CONF_LFLAGS += -lws2_32
  endif
  
  ifeq ($(ARSOCKET_SSL),no)
    #no SSL
    #socketssl will still be available at compile time, but fails linking
  else ifeq ($(OS),windows)
    CONF_CFLAGS += -DARLIB_SSL_SCHANNEL
    CONF_LFLAGS += -lcrypt32 -lsecur32
  else ifeq ($(ARSOCKET_SSL),openssl)
    CONF_CFLAGS += -DARLIB_SSL_OPENSSL
    CONF_LFLAGS += -lssl -lcrypto
  else ifeq ($(ARSOCKET_SSL),gnutls)
    CONF_CFLAGS += -DARLIB_SSL_GNUTLS
    CONF_LFLAGS += -lgnutls
  else ifeq ($(ARSOCKET_SSL),tlse)
    CONF_CFLAGS += -DARLIB_SSL_TLSE
    SOURCES_NOWARN += arlib/deps/tlse.c
  else
    $(error unknown SSL library)
  endif
endif

SOURCES_ARLIB += arlib/deps/miniz.c

TRUE_CFLAGS = -std=c99 $(CCXXFLAGS) $(CFLAGS) $(CONF_CFLAGS)
TRUE_CXXFLAGS =-std=c++11 -fno-rtti $(CCXXFLAGS) $(CXXFLAGS) $(CONF_CXXFLAGS)
TRUE_LFLAGS = $(LFLAGS) -fvisibility=hidden $(CONF_LFLAGS)

ifeq ($(OS),windows)
  TRUE_LFLAGS += -static-libgcc 
endif

#On Windows, cleaning up the object directory is expected to be done with 'del /q obj\*' in a batch script.
clean:
	rm obj/* || true

clean-prof:
	rm obj/*.o || true

obj:
	mkdir obj


DOMAINS += DEFAULT ARLIB NOWARN

OBJNAME := $(subst $(SPACE),,$(OBJNAME))
SOURCES_DEFAULT := $(SOURCES)
OBJS += $(foreach domain,$(sort $(DOMAINS)),$(call OBJMANGLE,$(domain),$(wildcard $(SOURCES_$(domain)))))

.DEFAULT_GOAL := all
all: $(OUTNAME)
$(OUTNAME): $(OBJS)
	$(LD) $+ $(TRUE_LFLAGS) -o $@ -lm



ifneq ($(SELFTEST),)
obj/arlibtest$(EXESUFFIX): $(OBJS)
	$(LD) $+ $(TRUE_LFLAGS) -o $@ -lm

test: obj/arlibtest$(EXESUFFIX)
	$(TESTRUNNER) obj/arlibtest$(EXESUFFIX)
check: test
endif
