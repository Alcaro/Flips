#pragma once
#include "bml.h"
#include "containers.h"
#include "endian.h"
#include "file.h"
#include "function.h"
#include "intwrap.h"
#include "os.h"
#include "serialize.h"
#include "string.h"
#include "stringconv.h"

//not in #ifdef, it contains some dummy implementations if threads are disabled
#include "thread/thread.h"

#if !defined(ARGUI_NONE) && !defined(ARGUI_WIN32) && !defined(ARGUI_GTK3)
#define ARGUI_NONE
#endif
#ifndef ARGUI_NONE
#include "gui/window.h"
#endif
#ifdef ARLIB_WUTF
#include "wutf.h"
#endif
#ifdef ARLIB_SANDBOX
#include "sandbox.h"
#endif
#ifdef ARLIB_SOCKET
#include "socket.h"
#endif
