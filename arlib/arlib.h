#pragma once
#include "containers.h"
#include "endian.h"
#include "file.h"
#include "function.h"
#include "intwrap.h"
#include "os.h"
#include "string.h"

//not in #ifdef, there's a check inside that header
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
