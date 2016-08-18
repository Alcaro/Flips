#include "string.h"
#include <stdio.h>

inline string tostring(string s) { return s; }
inline string tostring(cstring s) { return s; }
inline string tostring(const char * s) { return s; }
inline string tostring(int val) { char ret[16]; sprintf(ret, "%i", val); return ret; }

template<typename T> inline T fromstring(cstring s);
template<> inline string fromstring<string>(cstring s) { return s; }
template<> inline cstring fromstring<cstring>(cstring s) { return s; }
//no const char *, their lifetime is unknowable

template<> inline int fromstring<int>(cstring s) { return strtol(s, NULL, 0); }
template<> inline long int fromstring<long int>(cstring s) { return strtol(s, NULL, 0); }
template<> inline unsigned int fromstring<unsigned int>(cstring s) { return strtoul(s, NULL, 0); }
template<> inline float fromstring<float>(cstring s) { return strtod(s, NULL); }

template<> inline char fromstring<char>(cstring s) { return s[0]; }
template<> inline bool fromstring<bool>(cstring s) { return s=="true"; }
