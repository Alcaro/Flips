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
		string a = "12345678";
		a += a;
		a += a;
		cstring b = a; // ensure this takes a proper reference, rather than piggybacking the original string
		a = "";
		assert_eq(b, "12345678123456781234567812345678");
	}
	
	return true;
}
