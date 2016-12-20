#pragma once
#include "global.h"
#include "string.h"
#include <stdio.h>

inline string tostring(string s) { return s; }
inline string tostring(cstring s) { return s; }
//I'd use int123_t, but the set {8, 16, 32, 64} is smaller than {char, short, int, long, long long}, so one disappears
//if this one shows up (for example time_t = long on Windows), error
//printf has PRIi32, but the native ones are defined in terms of int/long
inline string tostring(  signed char val)      { char ret[32]; sprintf(ret, "%i",   val); return ret; } // the C++ standard says
inline string tostring(unsigned char val)      { char ret[32]; sprintf(ret, "%u",   val); return ret; } // (un)signed char/short are
//signless char isn't integral, so not here
inline string tostring(  signed short val)     { char ret[32]; sprintf(ret, "%i",   val); return ret; } // promoted to (un)signed int
inline string tostring(unsigned short val)     { char ret[32]; sprintf(ret, "%u",   val); return ret; } // in ellipsis
inline string tostring(  signed int val)       { char ret[32]; sprintf(ret, "%i",   val); return ret; }
inline string tostring(unsigned int val)       { char ret[32]; sprintf(ret, "%u",   val); return ret; }
inline string tostring(  signed long val)      { char ret[32]; sprintf(ret, "%li",  val); return ret; }
inline string tostring(unsigned long val)      { char ret[32]; sprintf(ret, "%lu",  val); return ret; }
#ifdef _WIN32
# ifdef __GNUC__ // my GCC doesn't recognize I64
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat"
# endif
inline string tostring(  signed long long val) { char ret[32]; sprintf(ret, "%I64i", val); return ret; }
inline string tostring(unsigned long long val) { char ret[32]; sprintf(ret, "%I64u", val); return ret; }
# ifdef __GNUC__
#  pragma GCC diagnostic pop
# endif
#else
inline string tostring(  signed long long val) { char ret[32]; sprintf(ret, "%lli", val); return ret; }
inline string tostring(unsigned long long val) { char ret[32]; sprintf(ret, "%llu", val); return ret; }
#endif
inline string tostring(float val)              { char ret[64]; sprintf(ret, "%f",   val); return ret; } // increase buffer sizes
inline string tostring(double val)           { char ret[1024]; sprintf(ret, "%f",   val); return ret; } // http://stackoverflow.com/q/7235456
inline string tostring(bool val) { return val ? "true" : "false"; }
//inline string tostring(char val); // not sure if this one makes sense

inline string tostring(const char * s) { return s; } // only exists as tostring, fromstring would be a memory leak


inline bool fromstring(cstring s, string& out) { out=s; return true; }
inline bool fromstring(cstring s, cstring& out) { out=s; return true; }
bool fromstring(cstring s, signed char & out);
bool fromstring(cstring s, unsigned char & out);
bool fromstring(cstring s, signed short & out);
bool fromstring(cstring s, unsigned short & out);
bool fromstring(cstring s, signed int & out);
bool fromstring(cstring s, unsigned int & out);
bool fromstring(cstring s, signed long & out);
bool fromstring(cstring s, unsigned long & out);
bool fromstring(cstring s, signed long long & out);
bool fromstring(cstring s, unsigned long long & out);
bool fromstring(cstring s, float& out);
bool fromstring(cstring s, double& out);
bool fromstring(cstring s, bool& out);


#define ALLSTRINGABLE(x) \
	x(string) \
	x(cstring) \
	x(signed char) \
	x(unsigned char) \
	x(signed short) \
	x(unsigned short) \
	x(signed int) \
	x(unsigned int) \
	x(signed long) \
	x(unsigned long) \
	x(signed long long) \
	x(unsigned long long) \
	x(float) \
	x(double) \
	x(bool)
