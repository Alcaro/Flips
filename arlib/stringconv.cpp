#include "stringconv.h"
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "test.h"

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

//specification: if the input is a hex number, return something strtoul accepts
//otherwise, return something that strtoul rejects
//this means drop the 0x
static const char * drop0x(const char * in)
{
	if (in[0]=='0' && in[1]!='0') return in+1;
	else return in;
}

#define FROMFUNCHEX(t,frt,f) \
	FROMFUNC(t,frt,f) \
	\
	bool fromstringhex(cstring s, t& out) \
	{ \
		const char * in = drop0x(s); \
		out = 0; \
		char * tmp; /* odd that this one isn't overloaded, like strchr */ \
		frt ret = f(in, &tmp, 16); \
		if (*tmp || (t)ret != (frt)ret) return false; \
		out = ret; \
		return true; \
	}

FROMFUNC(   signed char,    long,          strtol)
FROMFUNCHEX(unsigned char,  unsigned long, strtoul)
FROMFUNC(   signed short,   long,          strtol)
FROMFUNCHEX(unsigned short, unsigned long, strtoul)
FROMFUNC(   signed int,     long,          strtol)
FROMFUNCHEX(unsigned int,   unsigned long, strtoul)
FROMFUNC(   signed long,    long,          strtol)
FROMFUNCHEX(unsigned long,  unsigned long, strtoul)
FROMFUNC(   signed long long,   long long,          strtoll)
FROMFUNCHEX(unsigned long long, unsigned long long, strtoull)

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

template<typename T> void testunhex(const char * S, unsigned long long V)
{
	T a;
	assert_eq(fromstringhex(S, a), true);
	assert_eq(a, V);
}

test()
{
	testcall(testunhex<unsigned char     >("aa", 0xaa));
	testcall(testunhex<unsigned char     >("AA", 0xAA));
	testcall(testunhex<unsigned short    >("aaaa", 0xaaaa));
	testcall(testunhex<unsigned short    >("AAAA", 0xAAAA));
	testcall(testunhex<unsigned int      >("aaaaaaaa", 0xaaaaaaaa));
	testcall(testunhex<unsigned int      >("AAAAAAAA", 0xAAAAAAAA));
	testcall(testunhex<unsigned long     >("aaaaaaaa", 0xaaaaaaaa)); // this is sometimes 64bit, but good enough
	testcall(testunhex<unsigned long     >("AAAAAAAA", 0xAAAAAAAA));
	testcall(testunhex<unsigned long long>("aaaaaaaaaaaaaaaa", 0xaaaaaaaaaaaaaaaa));
	testcall(testunhex<unsigned long long>("AAAAAAAAAAAAAAAA", 0xAAAAAAAAAAAAAAAA));
}
