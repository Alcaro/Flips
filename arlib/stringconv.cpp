#include "stringconv.h"
#include <stdio.h>
#include <math.h>
#include <float.h>

#define FROMFUNC(t,frt,f) \
	bool fromstring(cstring s, t& out) \
	{ \
		out = 0; \
		char * tmp; /* odd that this one isn't overloaded, like strchr */ \
		frt ret = f(s, &tmp, 10); \
		if (*tmp || (t)ret != (frt)ret) return false; \
		out = ret; \
		return true; \
	}

FROMFUNC(signed char,    long,          strtol)
FROMFUNC(unsigned char,  unsigned long, strtoul)
FROMFUNC(signed short,   long,          strtol)
FROMFUNC(unsigned short, unsigned long, strtoul)
FROMFUNC(signed int,     long,          strtol)
FROMFUNC(unsigned int,   unsigned long, strtoul)
FROMFUNC(signed long,    long,          strtol)
FROMFUNC(unsigned long,  unsigned long, strtoul)
FROMFUNC(signed long long,   long long,          strtoll)
FROMFUNC(unsigned long long, unsigned long long, strtoull)

bool fromstring(cstring s, double& out)
{
	out = 0;
	char * tmp;
	double ret = strtod(s, &tmp);
	if (*tmp || ret==HUGE_VAL || ret==-HUGE_VAL) return false;
	out = ret;
	return true;
}

//strtof exists in C99, but let's not use that
bool fromstring(cstring s, float& out)
{
	out = 0;
	double tmp;
	if (!fromstring(s, tmp)) return false;
	if (tmp < -FLT_MAX || tmp > FLT_MAX) return false;
	out = tmp;
	return true;
}

bool fromstring(cstring s, bool& out)
{
	if (s=="false" || s=="0")
	{
		out=false;
		return true;
	}
	
	if (s=="true" || s=="1")
	{
		out=true;
		return true;
	}
	
	out=false;
	return false;
}
