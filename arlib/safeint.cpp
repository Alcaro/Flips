#define safeint_SELFTEST
#include "safeint.h"
#include "test.h"

#if 0 // apparently running 65536 iterations of anything under Valgrind takes a while
#define TEST(t,op) \
	for (int a=0;a<256;a++) \
	for (int b=0;b<256;b++) \
	{ \
		int au = (t)a; \
		int bu = (t)b; \
		safeint<t> as = au; \
		safeint<t> bs = bu; \
		if (as.valid() && bs.valid()) \
		{ \
			safeint<t> cs1 = (1 op 8 == 256/*if op == <<*/ && b>7 ? 999 : au op bu); \
			safeint<t> cs2 = as op bs; \
			if (cs1.val() != cs2.val()) \
			{ \
				assert_msg(false, tostring(au)+#op+tostring(bu)+": expected "+tostring(cs1.val())+", got "+tostring(cs2.val())); \
			} \
		} \
	}

test()
{
	TEST(uint8_t, +)
	TEST(uint8_t, -)
	TEST(uint8_t, *)
	TEST(uint8_t, <<)
	TEST( int8_t, +)
	TEST( int8_t, -)
	TEST( int8_t, *)
	TEST( int8_t, <<)
}
#endif
