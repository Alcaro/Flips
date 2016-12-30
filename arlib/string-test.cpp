#include "string.h"
#include "test.h"

test()
{
	{
		const char * g = "hi";
		
		string a = g;
		a[2]='!';
		string b = a;
		assert_eq(b, "hi!");
		a[3]='!';
		assert_eq(a, "hi!!");
		assert_eq(b, "hi!");
		a = b;
		assert_eq(a, "hi!");
		assert_eq(b, "hi!");
		
		
		a.replace(1,1, "ello");
		assert_eq(a, "hello!");
		assert_eq(a.substr(1,3), "el");
		a.replace(1,4, "i");
		assert_eq(a, "hi!");
		a.replace(1,2, "ey");
		assert_eq(a, "hey");
		
		assert_eq(a.substr(2,2), "");
	}
	
	{
		//ensure it works properly when going across the inline-outline border
		string a = "123456789012345";
		a += "678";
		assert_eq(a, "123456789012345678");
		a += (const char*)a;
		string b = a;
		assert_eq(a, "123456789012345678123456789012345678");
		assert_eq(a.substr(1,3), "23");
		assert_eq(b, "123456789012345678123456789012345678");
		assert_eq(a.substr(1,21), "23456789012345678123");
		assert_eq(a.substr(1,~1), "2345678901234567812345678901234567");
		assert_eq(a.substr(2,2), "");
		assert_eq(a.substr(22,22), "");
		a.replace(1,5, "-");
		assert_eq(a, "1-789012345678123456789012345678");
		a.replace(4,20, "-");
		assert_eq(a, "1-78-12345678");
	}
	
	{
		//ensure outline->outline also works
		string a = "123456789012345";
		a += "678";
		assert_eq(a, "123456789012345678");
		a += (const char*)a;
		string b = a;
		assert_eq(a, "123456789012345678123456789012345678");
		assert_eq(a.substr(1,3), "23");
		assert_eq(b, "123456789012345678123456789012345678");
		assert_eq(a.substr(1,21), "23456789012345678123");
		assert_eq(a.substr(1,~1), "2345678901234567812345678901234567");
		assert_eq(a.substr(2,2), "");
		assert_eq(a.substr(22,22), "");
		a.replace(1,5, "-");
		assert_eq(a, "1-789012345678123456789012345678");
		a.replace(4,20, "-");
		assert_eq(a, "1-78-12345678");
	}
	
	{
		string a = "12345678";
		a += a;
		a += a;
		cstring b = a; // ensure this takes a proper reference, rather than piggybacking the original string
		a = "";
		assert_eq(b, "12345678123456781234567812345678");
	}
	
	{
		string a = "1abc1de1fgh1";
		assert_eq(a.replace("1", ""), "abcdefgh");
		assert_eq(a.replace("1", "@"), "@abc@de@fgh@");
		assert_eq(a.replace("1", "@@"), "@@abc@@de@@fgh@@");
	}
	
	{
		//this has thrown valgrind errors due to derpy allocations
		string a = "abcdefghijklmnopqrstuvwxyz";
		string b = a; // needs an extra reference
		a += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		assert_eq(a, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
	}
	
	{
		//this has also crashed, due to unshare() not respecting m_owning=false
		cstring a = "aaaaaaaaaaaaaaaa";
		a[0] = 'b';
		assert_eq(a, "baaaaaaaaaaaaaaa");
	}
	
	{
		arrayview<byte> a((uint8_t*)"123", 3);
		string b = "["+string(a)+"]";
		string c = "["+cstring(a)+"]";
		assert_eq(b, "[123]");
		assert_eq(c, "[123]");
	}
	
	{
		string a = "baaaaaaaaaaaaaaa";
		array<string> b;
		
		b = a.split("a");
		assert_eq(b[0], "b");
		assert_eq(b[1], "");
		assert_eq(b[15], "");
		assert_eq(b.size(), 16);
		
		b = a.split("aa");
		assert_eq(b.size(), 8);
		assert_eq(b[0], "b");
		assert_eq(b[1], "");
		assert_eq(b[6], "");
		assert_eq(b[7], "a");
		
		b = a.split<1>("aa");
		assert_eq(b.size(), 2);
		assert_eq(b[0], "b");
		assert_eq(b[1], "aaaaaaaaaaaaa");
		
		b = a.split<1>("c");
		assert_eq(b.size(), 1);
		assert_eq(b[0], "baaaaaaaaaaaaaaa");
	}
}
