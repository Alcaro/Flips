#pragma once

#ifdef _WIN32
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0600
#    define NTDDI_VERSION NTDDI_VISTA
#  elif _WIN32_WINNT < 0x0600
#    undef _WIN32_WINNT
#    define _WIN32_WINNT 0x0502//0x0501 excludes SetDllDirectory, so I need to put it at 0x0502
#    define NTDDI_VERSION NTDDI_WS03 // actually NTDDI_WINXPSP2, but MinGW sddkddkver.h gets angry about that
#  endif
#  define _WIN32_IE 0x0600
//the namespace pollution this causes is massive, but without it, there's a bunch of functions that
// just tail call kernel32.dll. With it, they can be inlined.
#  define WIN32_LEAN_AND_MEAN
#  ifndef NOMINMAX
#   define NOMINMAX
#  endif
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
#  ifdef _MSC_VER
#    define _CRT_NONSTDC_NO_DEPRECATE
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  include <windows.h>
#endif

#ifdef _MSC_VER
#pragma warning(disable:4800) // forcing value to bool 'true' or 'false' (performance warning)
#endif

#ifndef __has_include
#define __has_include(x) false
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE //strdup, realpath, asprintf
#endif
#define _strdup strdup //and windows is being windows as usual
#define __STDC_LIMIT_MACROS //how many of these stupid things exist
#define __STDC_FORMAT_MACROS//if I include a header, it's because I want to use its contents
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>
#include "function.h"
#include <utility>

#define byte uint8_t

typedef void(*funcptr)();

//Note to anyone interested in reusing these objects:
//Many, if not most, of them will likely change their interface, likely quite fundamentally, in the future.
//No attempt is made to keep any kind of backwards or forwards compatibility.

#define using(obj) for(bool FIRST=true;FIRST;FIRST=false)for(obj;FIRST;FIRST=false)
//in C++17, this becomes if(obj;true)

#define JOIN_(x, y) x ## y
#define JOIN(x, y) JOIN_(x, y)

#define STR_(x) #x
#define STR(x) STR_(x)

#ifdef __GNUC__
#define LIKELY(expr)    __builtin_expect(!!(expr), true)
#define UNLIKELY(expr)  __builtin_expect(!!(expr), false)
#else
#define LIKELY(expr)    (expr)
#define UNLIKELY(expr)  (expr)
#define __GNUC__ 0
#endif

//some magic stolen from http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx
//C++ can be so messy sometimes...
template<typename T, size_t N> char(&ARRAY_SIZE_CORE(T(&x)[N]))[N];
#define ARRAY_SIZE(x) (sizeof(ARRAY_SIZE_CORE(x)))

//yep, C++ is definitely a mess. based on https://github.com/swansontec/map-macro with some changes:
//- namespaced all child macros, renamed main one
//- merged https://github.com/swansontec/map-macro/pull/3
//- merged http://stackoverflow.com/questions/6707148/foreach-macro-on-macros-arguments#comment62878935_13459454, plus ifdef
#define PPFE_EVAL0(...) __VA_ARGS__
#define PPFE_EVAL1(...) PPFE_EVAL0 (PPFE_EVAL0 (PPFE_EVAL0 (__VA_ARGS__)))
#define PPFE_EVAL2(...) PPFE_EVAL1 (PPFE_EVAL1 (PPFE_EVAL1 (__VA_ARGS__)))
#define PPFE_EVAL3(...) PPFE_EVAL2 (PPFE_EVAL2 (PPFE_EVAL2 (__VA_ARGS__)))
#define PPFE_EVAL4(...) PPFE_EVAL3 (PPFE_EVAL3 (PPFE_EVAL3 (__VA_ARGS__)))
#define PPFE_EVAL(...)  PPFE_EVAL4 (PPFE_EVAL4 (PPFE_EVAL4 (__VA_ARGS__)))
#define PPFE_MAP_END(...)
#define PPFE_MAP_OUT
#define PPFE_MAP_GET_END2() 0, PPFE_MAP_END
#define PPFE_MAP_GET_END1(...) PPFE_MAP_GET_END2
#define PPFE_MAP_GET_END(...) PPFE_MAP_GET_END1
#define PPFE_MAP_NEXT0(test, next, ...) next PPFE_MAP_OUT
#ifdef _MSC_VER
//this version doesn't work on GCC, it makes PPFE_MAP0 not get expanded the second time and quite effectively stops everything.
//but completely unknown guy says it's required on MSVC, so I'll trust that and ifdef it
#define PPFE_MAP_NEXT1(test, next) PPFE_EVAL0(PPFE_MAP_NEXT0 (test, next, 0))
#else
#define PPFE_MAP_NEXT1(test, next) PPFE_MAP_NEXT0 (test, next, 0)
#endif
#define PPFE_MAP_NEXT(test, next)  PPFE_MAP_NEXT1 (PPFE_MAP_GET_END test, next)
#define PPFE_MAP0(f, x, peek, ...) f(x) PPFE_MAP_NEXT (peek, PPFE_MAP1) (f, peek, __VA_ARGS__)
#define PPFE_MAP1(f, x, peek, ...) f(x) PPFE_MAP_NEXT (peek, PPFE_MAP0) (f, peek, __VA_ARGS__)
#define PPFOREACH(f, ...) PPFE_EVAL (PPFE_MAP1 (f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))
//usage:
//#define STRING(x) char const *x##_string = #x;
//PPFOREACH(STRING, foo, bar, baz)
//limited to 365 entries, but that's enough.



//requirements:
//- static_assert(false) throws something at compile time
//- multiple static_assert(true) works
//- does not require unique names for each assertion
//- zero traces left in the object files, assuming no debug info
//- zero warnings under any compiler
//- static_assert(2+2 < 5); works at the global scope
//- static_assert(2+2 < 5); works as a class member
//- static_assert(2+2 < 5); works inside a function
//- static_assert(2+2 < 5); works in all of the above when templates are involved
//- works on all compilers
//optional:
//- (PASS) works in a template, even if the template isn't instantiated, if the condition isn't dependent on the types
//- (FAIL) works if compiled as C (tried to design an alternate implementation and ifdef it, but nothing works inside structs)
//- (PASS) can name assertions, if desired (only under C++11)
#ifdef __GNUC__
#define MAYBE_UNUSED __attribute__((__unused__)) // shut up, stupid warnings
#define TYPENAME_IF_GCC typename // gcc requires this. msvc rejects this.
#else
#define MAYBE_UNUSED
#define TYPENAME_IF_GCC
#endif

#if __cplusplus < 201999 // TODO: replace with real C++17
#if __cplusplus < 201103
template<bool x> struct static_assert_t;
template<> struct static_assert_t<true> { struct STATIC_ASSERTION_FAILED {}; };
template<> struct static_assert_t<false> {};
//#define static_assert(expr)
//	typedef TYPENAME_IF_NEEDED static_assert_t<(bool)(expr)>::STATIC_ASSERTION_FAILED
//	JOIN(static_assertion_, __COUNTER__) MAYBE_UNUSED;
#define static_assert_c(expr, name, ...) \
	enum { \
		JOIN(static_assertion_, __COUNTER__) = \
		sizeof(TYPENAME_IF_GCC static_assert_t<(bool)(expr)>::STATIC_ASSERTION_FAILED) \
	} MAYBE_UNUSED
#else
#define static_assert_c(expr, name, ...) static_assert(expr, name)
#endif

#define static_assert_name(x, ...) #x
#define static_assert(...) static_assert_c(__VA_ARGS__, static_assert_name(__VA_ARGS__))
#endif


//almost C version (fails inside structs)
//#define static_assert(expr) \
//	typedef char JOIN(static_assertion_, __COUNTER__)[(expr)?1:-1]



#ifdef __GNUC__
#define ALIGN(n) __attribute__((aligned(n)))
#endif
#ifdef _MSC_VER
#define ALIGN(n) __declspec(align(n))
#endif




#ifdef __cplusplus
class anyptr {
void* data;
public:
template<typename T> anyptr(T* data_) { data=(void*)data_; }
template<typename T> operator T*() { return (T*)data; }
template<typename T> operator const T*() const { return (const T*)data; }
};
#else
typedef void* anyptr;
#endif


#include <stdlib.h> // needed because otherwise I get errors from malloc_check being redeclared
anyptr malloc_check(size_t size);
anyptr try_malloc(size_t size);
#define malloc malloc_check
anyptr realloc_check(anyptr ptr, size_t size);
anyptr try_realloc(anyptr ptr, size_t size);
#define realloc realloc_check
anyptr calloc_check(size_t size, size_t count);
anyptr try_calloc(size_t size, size_t count);
#define calloc calloc_check
void malloc_assert(bool cond); // if the condition is false, the malloc failure handler is called


//if I cast it to void, that means I do not care, so shut the hell up about warn_unused_result
template<typename T> static inline void ignore(T t) {}

template<typename T> static T min(const T& a) { return a; }
template<typename T, typename... Args> static T min(const T& a, Args... args)
{
	const T& b = min(args...);
	if (a < b) return a;
	else return b;
}

template<typename T> static T max(const T& a) { return a; }
template<typename T, typename... Args> static T max(const T& a, Args... args)
{
	const T& b = max(args...);
	if (a < b) return b;
	else return a;
}



//too reliant on non-ancient compilers
////some SFINAE shenanigans to call T::create if it exists, otherwise new() - took an eternity to google up
////don't use this template yourself, use generic_create/destroy instead
//template<typename T> class generic_create_core {
//	template<int G> class int_eater {};
//public:
//	template<typename T2> static T* create(T2*, int_eater<sizeof(&T2::create)>*) { return T::create(); }
//	static T* create(T*, ...) { return new T(); }
//	
//	template<typename T2> static void destroy(T* obj, T2*, int_eater<sizeof(&T2::release)>*) { obj->release(); }
//	static void destroy(T* obj, T*, ...) { delete obj; }
//};
//template<typename T> T* generic_create() { return generic_create_core<T>::create((T*)NULL, NULL); }
//template<typename T> void generic_delete(T* obj) { generic_create_core<T>::destroy(obj, (T*)NULL, NULL); }
//
//template<typename T> T* generic_create() { return T::create(); }
//template<typename T> T* generic_new() { return new T; }
//template<typename T> void generic_delete(T* obj) { delete obj; }
//template<typename T> void generic_release(T* obj) { obj->release(); }
//
//template<typename T> void* generic_create_void() { return (void*)generic_create<T>(); }
//template<typename T> void* generic_new_void() { return (void*)generic_new<T>(); }
//template<typename T> void generic_delete_void(void* obj) { generic_delete((T*)obj); }
//template<typename T> void generic_release_void(void* obj) { generic_release((T*)obj); }



class empty {
#ifndef _MSC_VER     // error C2503: base classes cannot contain zero-sized arrays
	int __zero_size[]; // this base is used only by nocopy/nomove, and they're only used by
#endif               // nonzero objects which will optimize the empty base class anyways
};

class nocopy : empty {
protected:
	nocopy() {}
	~nocopy() {}
	nocopy(const nocopy&) = delete;
	const nocopy& operator=(const nocopy&) = delete;
#if !defined(_MSC_VER) || _MSC_VER >= 1900 // error C2610: is not a special member function which can be defaulted
                                           // defaulting the copies deletes the moves on gcc, but does nothing on msvc2013; known bug
	nocopy(nocopy&&) = default;
	nocopy& operator=(nocopy&&) = default;
#endif
};

class nomove : empty {
protected:
	nomove() {}
	~nomove() {}
	nomove(const nomove&) = delete;
	const nomove& operator=(const nomove&) = delete;
	nomove(nomove&&) = delete;
	nomove& operator=(nomove&&) = delete;
};

template<typename T>
class autoptr : nocopy {
	T* ptr;
public:
	autoptr() : ptr(NULL) {}
	autoptr(T* ptr) : ptr(ptr) {}
	autoptr(autoptr<T>&& other) { ptr=other.ptr; other.ptr=NULL; }
	autoptr<T>& operator=(T* ptr) { delete this->ptr; this->ptr=ptr; return *this; }
	autoptr<T>& operator=(autoptr<T>&& other) { delete this->ptr; ptr=other.ptr; other.ptr=NULL; return *this; }
	T* release() { T* ret = ptr; ptr = NULL; return ret; }
	T* operator->() { return ptr; }
	T& operator*() { return *ptr; }
	operator T*() { return ptr; }
	explicit operator bool() { return ptr; }
	~autoptr() { delete ptr; }
};

class null_t_impl {};
#define null_t null_t_impl* // random pointer type nobody will ever use




//#if defined(__linux__) || GCC_VERSION >= 40900
//#define asprintf(...) malloc_assert(asprintf(__VA_ARGS__) >= 0)
//#else
//void asprintf(char * * ptr, const char * fmt, ...);
//#endif

#ifdef _WIN32
void* memmem(const void * haystack, size_t haystacklen, const void * needle, size_t needlelen);
#endif


//msvc:
//typedef unsigned long uint32_t;
//typedef unsigned __int64 uint64_t;
//typedef unsigned int size_t;
template<typename T> static inline T bitround(T in)
{
	in--;
	in|=in>>1;
	in|=in>>2;
	in|=in>>4;
	in|=in>>8;
	if (sizeof(in)>2) in|=in>>8>>8; // double shift to shut up bitshift-out-of-range warnings
	if (sizeof(in)>4) in|=in>>8>>8>>8>>8;
	in++;
	return in;
}


//If any interface defines a callback, free() is banned while inside this callback; other functions
// are allowed, unless otherwise specified. Other instances of the same interface may be used and
// freed, and other interfaces may be called.
//If an interface defines a function to set some state, and a callback for when this state changes,
// calling that function will not trigger the state callback.
//Unless otherwise specified, an interface may only be used from its owner thread (the creator).
// However, it is safe for any thread to create an interface, including having different threads
// use multiple instances of the same interface.
//Don't depend on any pointer being unique; for example, the None interfaces are static. However,
// even if they are (potentially) non-unique, following the instructed method to free them is safe;
// either they're owned by the one one giving them to you, or their free() handlers are empty, or
// they could even be refcounted.
//If a pointer is valid until anything from a set of functions is called (including if the set
// contains only one listed function), free() will also invalidate that pointer.
//"Implementation" means the implementation of the interfaces; the blocks of code that are called
// when a function is called on an interface.
//"User" means the one using the interfaces. Some interface implementations are users of other
// interfaces; for example, an implementation of libretro is also the user of a dylib.
//An implementation may, at its sole discretion, choose to define any implementation of undefined
// behaviour. After all, any result, including something well defined, is a valid interpretation of
// undefined behaviour. The user may, of course, not rely on that.
//Any function that starts with an underscore may only be called by the module that implements that
// function. ("Module" is defined as "anything whose compilation is controlled by the same #ifdef,
// or the file implementing an interface, whichever makes sense"; for example, window-win32-* is the
// same module.) The arguments and return values of these private functions may change meaning
// between modules, and the functions are not guaranteed to exist at all, or closely correspond to
// their name. For example, _window_init_misc on GTK+ instead initializes a component needed by the
// listboxes.

//This file, and many other parts of Arlib, uses a weird mix between Windows- and Linux-style
// filenames and paths. This is intentional; the author prefers Linux-style paths and directory
// structures, but Windows file extensions. .exe is less ambigous than no extension, and Windows'
// insistence on overloading the escape character is irritating. Since this excludes following
// any single OS, the rest is personal preference.
