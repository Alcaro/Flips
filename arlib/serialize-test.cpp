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

struct ser5 {
	array<int> data;
	SERIALIZE(data);
};

struct ser6 {
	array<ser1> data;
	SERIALIZE(data);
};

struct ser7 {
	ser5 par;
	SERIALIZE(par);
};

struct ser8 {
	//signed char a;
	//signed short b;
	//signed int c;
	//signed long d;
	//signed long long e;
	unsigned char f;
	unsigned short g;
	unsigned int h;
	unsigned long i;
	unsigned long long j;
	
	template<typename T>
	void serialize(T& s)
	{
		//s.hex("a", a);
		//s.hex("b", b);
		//s.hex("c", c);
		//s.hex("d", d);
		//s.hex("e", e);
		s.hex("f", f);
		s.hex("g", g);
		s.hex("h", h);
		s.hex("i", i);
		s.hex("j", j);
	}
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
	
	{
		ser5 item;
		item.data.append(1);
		item.data.append(2);
		item.data.append(3);
		assert_eq(bmlserialize(item), "data=1\ndata=2\ndata=3");
	}
	
	{
		ser6 item;
		item.data.append();
		item.data.append();
		item.data[0].a=1;
		item.data[0].b=2;
		item.data[1].a=3;
		item.data[1].b=4;
		assert_eq(bmlserialize(item), "data a=1 b=2\ndata a=3 b=4");
	}
	
	{
		ser7 item;
		item.par.data.append(1);
		item.par.data.append(2);
		item.par.data.append(3);
		assert_eq(bmlserialize(item), "par data=1 data=2 data=3");
	}
	
	{
		ser8 item;
		item.f = 0xAA;
		item.g = 0xAAAA;
		item.h = 0xAAAAAAAA;
		item.i = 0xAAAAAAAA;
		item.j = 0xAAAAAAAAAAAAAAAA;
		assert_eq(bmlserialize(item), "f=AA\ng=AAAA\nh=AAAAAAAA\ni=AAAAAAAA\nj=AAAAAAAAAAAAAAAA");
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
	
	{
		ser5 item = bmlunserialize<ser5>("data=1\ndata=2\ndata=3");
		assert_eq(item.data.size(), 3);
		assert_eq(item.data[0], 1);
		assert_eq(item.data[1], 2);
		assert_eq(item.data[2], 3);
	}
	
	{
		ser6 item = bmlunserialize<ser6>("data a=1 b=2\ndata a=3 b=4");
		assert_eq(item.data.size(), 2);
		assert_eq(item.data[0].a, 1);
		assert_eq(item.data[0].b, 2);
		assert_eq(item.data[1].a, 3);
		assert_eq(item.data[1].b, 4);
	}
	
	{
		ser7 item = bmlunserialize<ser7>("par data=1 data=2 data=3");
		assert_eq(item.par.data.size(), 3);
		assert_eq(item.par.data[0], 1);
		assert_eq(item.par.data[1], 2);
		assert_eq(item.par.data[2], 3);
	}
	
	{
		ser8 item = bmlunserialize<ser8>("f=AA\ng=AAAA\nh=AAAAAAAA\ni=AAAAAAAA\nj=AAAAAAAAAAAAAAAA");
		assert_eq(item.f, 0xAA);
		assert_eq(item.g, 0xAAAA);
		assert_eq(item.h, 0xAAAAAAAA);
		assert_eq(item.i, 0xAAAAAAAA);
		assert_eq(item.j, 0xAAAAAAAAAAAAAAAA);
	}
}
#endif
