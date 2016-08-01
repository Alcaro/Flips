#include "string.h"
#include <stdio.h>

inline string tostring(string s) { return s; }
inline string tostring(cstring s) { return s; }
inline string tostring(const char * s) { return s; }
inline string tostring(int val) { char ret[16]; sprintf(ret, "%i", val); return ret; }

template<typename T> inline T fromstring(string s);
template<> inline string fromstring<string>(string s) { return s; }
template<> inline cstring fromstring<cstring>(string s) { return s; }
//no const char *, their lifetime is unknowable
template<> inline int fromstring<int>(string s) { return atoi(s); }

