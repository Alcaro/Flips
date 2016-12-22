//TODO:
//- cool down on string.h refcounting
//- SSO for array<>
//- make strings and arrays nullable, so failure vs empty answer can be determined
//- window.h: remove pointers
//- window.h: remove varargs
//- msvc compat: add some define that, if absent, enables every feature

//WARNING: Arlib comes with zero stability guarantees. It can and will change in arbitrary ways, for any reason and at any time.

#pragma once
#include "global.h"
#include <utility> // std::move
#include "bml.h"
#include "containers.h"
#include "crc32.h"
#include "endian.h"
#include "file.h"
#include "function.h"
#include "intsafe.h"
#include "intwrap.h"
#include "os.h"
#include "serialize.h"
#include "string.h"
#include "stringconv.h"
#include "test.h"
#include "zip.h"

//not in #ifdef, it contains some dummy implementations if threads are disabled
#include "thread/thread.h"

#if !defined(ARGUI_NONE) && !defined(ARGUI_WINDOWS) && !defined(ARGUI_GTK3)
#define ARGUI_NONE
#endif
#ifndef ARGUI_NONE
#include "gui/window.h"
#endif

#ifdef ARLIB_OPENGL
#include "opengl/aropengl.h"
#endif

#ifdef ARLIB_WUTF
#include "wutf/wutf.h"
#endif

#ifdef ARLIB_SANDBOX
#include "sandbox/sandbox.h"
#endif

#ifdef ARLIB_SOCKET
#include "socket/socket.h"
#endif
