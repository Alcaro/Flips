#include "serialize.h"
#include "test.h"

#ifdef ARLIB_TEST
struct ser1 {
	int a;
	int b;
	
	SERIALIZE(a, b);
};

struct ser2 {
	ser1 c;
	ser1 d;
	
	SERIALIZE(c, d);
};

struct ser3 {
	int a;
	int b;
	int c;
	int d;
	int e;
	int f;
	int g;
	int h;
	
	SERIALIZE(a, b, c, d, e, f, g, h);
};

struct ser4 {
	ser3 mem;
	int count = 0;
	template<typename T> void serialize(T& s) { mem.serialize(s); count++; }
};

test()
{
	{
		ser1 item;
		item.a = 1;
		item.b = 2;
		
		assert_eq(bmlserialize(item), "a=1\nb=2");
	}
	
	{
		ser2 item;
		item.c.a = 1;
		item.c.b = 2;
		item.d.a = 3;
		item.d.b = 4;
		assert_eq(bmlserialize(item), "c a=1 b=2\nd a=3 b=4");
	}
}

test()
{
	{
		ser1 item = bmlunserialize<ser1>("a=1\nb=2");
		assert_eq(item.a, 1);
		assert_eq(item.b, 2);
	}
	
	{
		ser2 item = bmlunserialize<ser2>("c a=1 b=2\nd a=3 b=4");
		assert_eq(item.c.a, 1);
		assert_eq(item.c.b, 2);
		assert_eq(item.d.a, 3);
		assert_eq(item.d.b, 4);
	}
	
	//the system should not be order-sensitive
	{
		ser2 item = bmlunserialize<ser2>("d b=4 a=3\nc a=1 b=2");
		assert_eq(item.c.a, 1);
		assert_eq(item.c.b, 2);
		assert_eq(item.d.a, 3);
		assert_eq(item.d.b, 4);
	}
	
	//in case of dupes, last one should win; extraneous nodes should be cleanly ignored
	{
		ser1 item = bmlunserialize<ser1>("a=1\nb=2\nq=0\na=3\na=4");
		assert_eq(item.a, 4);
		assert_eq(item.b, 2);
	}
	
	//the system is allowed to loop, but only if there's bogus or extraneous nodes
	//we want O(n) runtime for a clean document, so ensure no looping
	//this includes missing and duplicate elements, both of which are possible for serialized arrays
	{
		ser4 item = bmlunserialize<ser4>("a=1\nb=2\nd=4\ne=5\ne=5\nf=6");
		assert_eq(item.count, 1);
	}
}
#endif
