#pragma once
#include "global.h"
#include "intwrap.h"
#include <stdint.h>

//This one defines:
//Macros END_LITTLE, END_BIG and ENDIAN; ENDIAN is equal to one of the other two. The test is borrowed from byuu's nall.
//end_swap() - Byteswaps an integer.
//end_nat_to_le(), end_le_to_nat(), end_nat_to_be(), end_be_to_nat() - Byteswaps an integer or returns it unmodified, depending on the host endianness.
//Class litend<> and bigend<> - Acts like the given integer type, but is stored by the named endianness internally. Safe to memcpy() and fwrite().

#define END_LITTLE 0x04030201
#define END_BIG 0x01020304
#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && __BYTE_ORDER == __LITTLE_ENDIAN) || defined(__LITTLE_ENDIAN__) || \
    defined(__i386__) || defined(__amd64__) || \
    defined(_M_IX86) || defined(_M_AMD64) || \
    defined(__ARM_EABI__) || defined(__arm__)
#define ENDIAN END_LITTLE
#define BIGEND_SWAP1(a)               a // pointless, but for consistency
#define BIGEND_SWAP2(a,b)             a b
#define BIGEND_SWAP3(a,b,c)           a b c
#define BIGEND_SWAP4(a,b,c,d)         a b c d
#define BIGEND_SWAP5(a,b,c,d,e)       a b c d e
#define BIGEND_SWAP6(a,b,c,d,e,f)     a b c d e f
#define BIGEND_SWAP7(a,b,c,d,e,f,g)   a b c d e f g
#define BIGEND_SWAP8(a,b,c,d,e,f,g,h) a b c d e f g h
#elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && __BYTE_ORDER == __BIG_ENDIAN) || defined(__BIG_ENDIAN__) || \
    defined(__powerpc__) || defined(_M_PPC)
#define ENDIAN END_BIG
#define BIGEND_SWAP1(a)               a
#define BIGEND_SWAP2(a,b)             b a
#define BIGEND_SWAP3(a,b,c)           c b a
#define BIGEND_SWAP4(a,b,c,d)         d c b a
#define BIGEND_SWAP5(a,b,c,d,e)       e d c b a
#define BIGEND_SWAP6(a,b,c,d,e,f)     f e d c b a
#define BIGEND_SWAP7(a,b,c,d,e,f,g)   g f e d c b a
#define BIGEND_SWAP8(a,b,c,d,e,f,g,h) h g f e d c b a
#else
#error please define your endianness
#endif

#if defined(__GNUC__)
//This one is mostly useless (GCC detects the pattern and optimizes it).
//However, MSVC doesn't, so I need the intrinsics. Might as well use both sets.
static inline uint8_t end_swap(uint8_t n) { return n; }
static inline uint16_t end_swap(uint16_t n) { return __builtin_bswap16(n); }
static inline uint32_t end_swap(uint32_t n) { return __builtin_bswap32(n); }
static inline uint64_t end_swap(uint64_t n) { return __builtin_bswap64(n); }
#elif defined(_MSC_VER)
static inline uint8_t end_swap(uint8_t n) { return n; }
static inline uint16_t end_swap(uint16_t n) { return _byteswap_ushort(n); }
static inline uint32_t end_swap(uint32_t n) { return _byteswap_ulong(n); }
static inline uint64_t end_swap(uint64_t n) { return _byteswap_uint64(n); }
#else
static inline uint8_t end_swap(uint8_t n) { return n; }
static inline uint16_t end_swap(uint16_t n) { return n>>8 | n<<8; }
static inline uint32_t end_swap(uint32_t n)
{
	n = n>>16 | n<<16;
	n = (n&0x00FF00FF)<<8 | (n&0xFF00FF00)>>8;
	return n;
}
static inline uint64_t end_swap(uint64_t n)
{
	n = n>>32 | n<<32;
	n = (n&0x0000FFFF0000FFFF)<<16 | (n&0xFFFF0000FFFF0000)>>16;
	n = (n&0x00FF00FF00FF00FF)<<8  | (n&0xFF00FF00FF00FF00)>>8 ;
	return n;
}
#endif

static inline int8_t  end_swap(int8_t  n) { return (int8_t )end_swap((uint8_t )n); }
static inline int16_t end_swap(int16_t n) { return (int16_t)end_swap((uint16_t)n); }
static inline int32_t end_swap(int32_t n) { return (int32_t)end_swap((uint32_t)n); }
static inline int64_t end_swap(int64_t n) { return (int64_t)end_swap((uint64_t)n); }

#ifdef ENDIAN
#if ENDIAN == END_LITTLE
template<typename T> static inline T end_nat_to_le(T val) { return val; }
template<typename T> static inline T end_nat_to_be(T val) { return end_swap(val); }
template<typename T> static inline T end_le_to_nat(T val) { return val; }
template<typename T> static inline T end_be_to_nat(T val) { return end_swap(val); }
#elif ENDIAN == END_BIG
template<typename T> static inline T end_nat_to_le(T val) { return end_swap(val); }
template<typename T> static inline T end_nat_to_be(T val) { return val; }
template<typename T> static inline T end_le_to_nat(T val) { return end_swap(val); }
template<typename T> static inline T end_be_to_nat(T val) { return val; }
#endif

template<typename T, bool little> class endian_core
{
	T val;
	
public:
	operator T()
	{
		if (little == (ENDIAN==END_LITTLE)) return val;
		else return end_swap(val);
	}
	
	void operator=(T newval)
	{
		if (little == (ENDIAN==END_LITTLE)) val = newval;
		else val = end_swap(newval);
	}
};

#else

//This one doesn't optimize properly. While it does get unrolled, it remains as four byte loads, and some shift/or.
template<typename T, bool little> class endian_core
{
	union {
		T align;
		uint8_t bytes[sizeof(T)];
	};
	
public:
	operator T()
	{
		if (little)
		{
			T ret=0;
			for (size_t i=0;i<sizeof(T);i++)
			{
				ret = (ret<<8) | bytes[i];
			}
			return ret;
		}
		else
		{
			T ret=0;
			for (size_t i=0;i<sizeof(T);i++)
			{
				ret = (ret<<8) | bytes[sizeof(T)-1-i];
			}
			return ret;
		}
	}
	
	void operator=(T newval)
	{
		if ((little && ENDIAN==END_LITTLE) || (!little && ENDIAN==END_BIG))
		{
			val = newval;
			return;
		}
		if (!little)
		{
			for (size_t i=0;i<sizeof(T);i++)
			{
				bytes[sizeof(T)-1-i]=(newval&0xFF);
				newval>>=8;
			}
		}
		else
		{
			for (size_t i=0;i<sizeof(T);i++)
			{
				bytes[i]=(newval&0xFF);
				newval>>=8;
			}
		}
	}
};
#endif

template<typename T> class bigend : public intwrap<endian_core<T, false>, T> {
public:
	bigend() {}
	bigend(T i) : intwrap<endian_core<T, false>, T>(i) {} // why does C++ need so much irritating cruft
};

template<typename T> class litend : public intwrap<endian_core<T, true>, T> {
public:
	litend() {}
	litend(T i) : intwrap<endian_core<T, true>, T>(i) {}
};
