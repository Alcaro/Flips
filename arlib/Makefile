all: $(PROGRAM)_dummy

ifeq ($(OS),Windows_NT)
  OS = windows
  #$(error objdump something and check which sections can be nuked)
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

SPACE :=
SPACE +=

ARGUI ?= 0
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
OBJSUFFIX =
CCXXFLAGS = -fvisibility=hidden -fno-exceptions -Wall -Wno-comment

EXESUFFIX =
EXTRAOBJ ?=
CONF_CXXFLAGS += $(CONF_CFLAGS)

ifeq ($(OS),linux)
  CONF_LFLAGS += -ldl
  ifeq ($(ARTHREADS),1)
    CONF_CFLAGS += -pthread
    CONF_LFLAGS += -pthread
  endif
  OBJSUFFIX = -linux
  TESTRUNNER = valgrind
endif

ifeq ($(OS),windows)
  EXESUFFIX = .exe
#  EXTRAOBJ = obj/resource$(OBJSUFFIX).o
#  RC = windres
#  RCFLAGS =
#obj/resource$(OBJSUFFIX).o: ico/*
#	$(RC) $(RCFLAGS) ico/minir.rc obj/resource$(OBJSUFFIX).o
  OBJSUFFIX = -windows
endif

ifneq (,$(findstring test,$(MAKECMDGOALS)))
  SELFTEST = 1
endif
ifneq (,$(findstring check,$(MAKECMDGOALS)))
  SELFTEST = 1
endif

OPTFLAGS := -Os -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden
OPTFLAGS += -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables
OPTFLAGS += -ffunction-sections -fdata-sections
OPTFLAGS += -Werror

ifeq ($(OPT),1)
  CFLAGS += $(OPTFLAGS)
  LFLAGS += -Wl,--gc-sections -s
  DEBUG = 0
  OBJSUFFIX += -opt
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

ifneq ($(SELFTEST),)
  CONF_CFLAGS += -DARLIB_TEST -Dmain=not_quite_main
  OBJSUFFIX += -test
endif

#stolen from http://stackoverflow.com/questions/22586084/makefile-with-multiple-rules-sharing-same-recipe-with-patternrules
define ADDDIR_CORE
$(eval OBJPREFIX := obj/_arlib_$(subst /,_,$(1))_)
OBJS += $(patsubst $(1)/%.cpp,$(OBJPREFIX)%$(OBJSUFFIX).o,$(wildcard $(1)/*.cpp))
$(OBJPREFIX)%$(OBJSUFFIX).o: $(1)/%.cpp | obj
	$$(CXX) $$(TRUE_CXXFLAGS) -c $$< -o $$@
endef
define ADDDIR
  $(eval $(call ADDDIR_CORE,$(1)))
endef

OBJSUFFIX := $(subst $(SPACE),,$(OBJSUFFIX))
OBJS := $(patsubst %.cpp,obj/%$(OBJSUFFIX).o,$(wildcard *.cpp)) $(EXTRAOBJ)
# obj/miniz$(OBJSUFFIX).o

$(call ADDDIR,arlib)

ifeq ($(ARGUI),1)
  $(call ADDDIR,arlib/gui)
  ifeq ($(OS),windows)
    CONF_CFLAGS += -DARGUI_WINDOWS
    CONF_LFLAGS += -lgdi32 -lcomctl32 -lcomdlg32 -ldinput8 -ldxguid -lopengl32
  endif
  ifeq ($(OS),linux)
    CONF_CFLAGS += $(shell pkg-config --cflags gtk+-3.0) -DARGUI_GTK3 -DARGUIPROT_X11
    CONF_LFLAGS += -ldl -lX11 -lGL -lXi -lXext $(shell pkg-config --libs gtk+-3.0)
  endif
else
  CONF_CFLAGS += -DARGUI_NONE
endif

ifeq ($(ARTHREAD),1)
  $(call ADDDIR,arlib/thread)
  CONF_CFLAGS += -DARLIB_THREAD
  ifeq ($(OS),linux)
    CONF_CFLAGS += -pthread
    CONF_LFLAGS += -pthread
  endif
endif

ifeq ($(ARSANDBOX),1)
  $(call ADDDIR,arlib/sandbox)
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
  $(call ADDDIR,arlib/wutf)
  CONF_CFLAGS += -DARLIB_WUTF
endif

ifeq ($(ARSOCKET),1)
  $(call ADDDIR,arlib/socket)
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
  else ifeq ($(ARSOCKET_SSL),wolfssl)
    WOLFSSL_DIR = arlib/socket/wolfssl-3.9.0
    CONF_CFLAGS += -DARLIB_SSL_WOLFSSL -I$(WOLFSSL_DIR)
    OBJS += obj/_arlib_sp_wolfssl$(OBJSUFFIX).o
    #CFLAGS += $(OPTFLAGS)
  else ifeq ($(ARSOCKET_SSL),openssl)
    CONF_CFLAGS += -DARLIB_SSL_OPENSSL
    CONF_LFLAGS += -lssl -lcrypto
  else ifeq ($(ARSOCKET_SSL),tlse)
    CONF_CFLAGS += -DARLIB_SSL_TLSE
    OBJS += obj/_arlib_sp_tlse$(OBJSUFFIX).o
  else
    $(error unknown SSL library)
  endif
endif

TRUE_CFLAGS = -std=c99 $(CCXXFLAGS) $(CFLAGS) $(CONF_CFLAGS)
TRUE_CXXFLAGS =-std=c++11 -fno-rtti $(CCXXFLAGS) $(CXXFLAGS) $(CONF_CXXFLAGS)
TRUE_LFLAGS = $(LFLAGS) -fvisibility=hidden $(CONF_LFLAGS)

#double gcc bug combo:
#(1) GCC hates this pattern:
#//define foo(a,b,c) \
#//  bar(a) \
#//  bar(b) \
#//  bar(c)
# to my knowledge unreported
#(2) '#pragma GCC diagnostic ignored "-Wcomment"' does nothing
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431
TRUE_CFLAGS += -Wno-comment
TRUE_CXXFLAGS += -Wno-comment

#On Windows, cleaning up the object directory is expected to be done with 'del /q obj\*' in a batch script.
clean:
	rm obj/* || true

clean-prof:
	rm obj/*.o || true

obj:
	mkdir obj

obj/miniz$(OBJSUFFIX).o: miniz.c | obj
	$(CC) $(TRUE_CFLAGS) -c $< -o $@

obj/_arlib_sp_wolfssl$(OBJSUFFIX).o: arlib/socket/wolfssl-lib.c | obj
	$(CC) -DARLIB_SSL_WOLFSSL_SP $(TRUE_CFLAGS) $(OPTFLAGS) -c $< -o $@

obj/_arlib_sp_tlse$(OBJSUFFIX).o: arlib/socket/tlse.c | obj
	$(CC) -DTLSE_IMPL -DTLS_AMALGAMATION -D_XOPEN_SOURCE=600 $(TRUE_CFLAGS)  -w -c $< -o $@

obj/%-c$(OBJSUFFIX).o: %.c | obj
	$(CC) $(TRUE_CFLAGS) -c $< -o $@

obj/%$(OBJSUFFIX).o: %.cpp | obj
	$(CXX) $(TRUE_CXXFLAGS) -c $< -o $@

$(OUTNAME): $(OBJS)
	$(LD) $+ $(TRUE_LFLAGS) -o $@ -lm

$(PROGRAM)_dummy: $(OUTNAME)



ifneq ($(SELFTEST),)
obj/arlibtest$(EXESUFFIX): $(OBJS)
	$(LD) $+ $(TRUE_LFLAGS) -o $@ -lm

test: obj/arlibtest$(EXESUFFIX)
	$(TESTRUNNER) obj/arlibtest$(EXESUFFIX)
check: test
endif

#todo: replace with ./test shell script (make test/check should still exist)
