#include "string.h"
#include "test.h"

uint8_t* string::alloc(uint8_t* prev, uint32_t prevsize, uint32_t newsize)
{
	if (prevsize==0)
	{
		uint8_t* ptr = malloc(bytes_for(newsize));
		*(int*)ptr = 1;
		return ptr+sizeof(int);
	}
	
	if (newsize==0)
	{
		int* refcount = (int*)(prev-sizeof(int));
		if (--*refcount == 0) free(refcount);
		return NULL;
	}
	
	prevsize = bytes_for(prevsize);
	newsize = bytes_for(newsize);
	if (prevsize==newsize) return prev;
	
	int* refcount = (int*)(prev-sizeof(int));
	if (*refcount == 1)
	{
		return (uint8_t*)realloc(refcount, newsize)+sizeof(int);
	}
	--*refcount;
	
	uint8_t* ptr = malloc(bytes_for(newsize));
	memcpy(ptr, prev-sizeof(int), min(prevsize, newsize));
	*(int*)ptr = 1;
	return ptr+sizeof(int);
}

void string::unshare() const
{
	if (inlined()) return;
	if (m_owning && *(int*)(m_data-sizeof(int))==1) return;
	
	uint8_t* prevdat = m_data;
	m_data = alloc(NULL,0, m_len);
	memcpy(m_data, prevdat, m_len);
	m_data[m_len] = '\0';
	
	if (m_owning) alloc(prevdat,m_len, 0);
	
	m_owning = true;
	m_nul = true;
}

void string::resize(uint32_t newlen)
{
	unshare();
	
	switch (!inlined()<<1 | (newlen>max_inline))
	{
	case 0: // small->small
		{
			m_inline[newlen] = '\0';
			m_inline_len = max_inline-newlen;
		}
		break;
	case 1: // small->big
		{
			uint8_t* newptr = alloc(NULL,0, newlen);
			memcpy(newptr, m_inline, max_inline);
			newptr[newlen] = '\0';
			m_data = newptr;
			m_len = newlen;
			m_owning = true;
			m_nul = true;
			
			m_inline_len = -1;
		}
		break;
	case 2: // big->small
		{
			uint8_t* oldptr = m_data;
			uint32_t oldlen = m_len;
			memcpy(m_inline, oldptr, newlen);
			alloc(oldptr,oldlen, 0);
			m_inline[newlen] = '\0';
			m_inline_len = max_inline-newlen;
		}
		break;
	case 3: // big->big
		{
			m_data = alloc(m_data,m_len, newlen);
			m_data[newlen] = '\0';
			m_len = newlen;
		}
		break;
	}
}

void string::init_from(arrayview<byte> data)
{
	const uint8_t * str = data.ptr();
	uint32_t len = data.size();
	
	if (len <= max_inline)
	{
		memcpy(m_inline, str, len);
		m_inline[len] = '\0';
		m_inline_len = max_inline-len;
	}
	else
	{
		m_inline_len = -1;
		
		m_data = alloc(NULL,0, len+1);
		memcpy(m_data, str, len);
		m_data[len]='\0';
		
		m_len = len;
		m_owning = true;
		m_nul = true;
	}
}

void string::init_from_nocopy(arrayview<byte> data)
{
	const uint8_t * str = data.ptr();
	uint32_t len = data.size();
	
	if (len <= max_inline)
	{
		memcpy(m_inline, str, len);
		m_inline[len] = '\0';
		m_inline_len = max_inline-len;
	}
	else
	{
		m_inline_len = -1;
		
		m_data = (uint8_t*)str; // if m_owning is false, we know to not modify this
		m_len = len;
		m_owning = false;
		m_nul = false;
	}
}

void string::replace(int32_t pos, int32_t len, const string& newdat)
{
	//if newdat is a cstring backed by this, then modifying this invalidates that string, so it's illegal
	//if newdat equals this, then the memmoves will mess things up
	if (this == &newdat)
	{
		string copy = newdat;
		replace(pos, len, copy);
		return;
	}
	
	uint32_t prevlength = length();
	uint32_t newlength = newdat.length();
	
	if (newlength < prevlength)
	{
		unshare();
		memmove(ptr()+pos+newlength, ptr()+pos+len, prevlength-len-pos);
		resize(prevlength - len + newlength);
	}
	if (newlength == prevlength)
	{
		unshare();
	}
	if (newlength > prevlength)
	{
		resize(prevlength - len + newlength);
		memmove(ptr()+pos+newlength, ptr()+pos+len, prevlength-len-pos);
	}
	
	memcpy(ptr()+pos, newdat.ptr(), newlength);
}

string string::replace(const string& in, const string& out)
{
	size_t outlen = length();
	
	if (in.length() != out.length())
	{
		uint8_t* haystack = ptr();
		uint8_t* haystackend = ptr()+length();
		while (true)
		{
			haystack = (uint8_t*)memmem(haystack, haystackend-haystack, in.ptr(), in.length());
			if (!haystack) break;
			
			haystack += in.length();
			outlen += out.length(); // outlen-inlen is type uint - bad idea
			outlen -= in.length();
		}
	}
	
	string ret;
	uint8_t* retptr = ret.construct(outlen).ptr();
	
	uint8_t* prev = ptr();
	uint8_t* myend = ptr()+length();
	while (true)
	{
		uint8_t* match = (uint8_t*)memmem(prev, myend-prev, in.ptr(), in.length());
		if (!match) break;
		
		memcpy(retptr, prev, match-prev);
		retptr += match-prev;
		prev = match + in.length();
		
		memcpy(retptr, out.ptr(), out.length());
		retptr += out.length();
	}
	memcpy(retptr, prev, myend-prev);
	
	return ret;
}

array<string> string::split(const string& sep, size_t limit) const
{
	array<string> ret;
	const uint8_t * data = ptr();
	const uint8_t * dataend = ptr()+length();
	
	while (ret.size() < limit)
	{
		const uint8_t * next = (uint8_t*)memmem(data, dataend-data, sep.ptr(), sep.length());
		if (!next) break;
		ret.append(arrayview<uint8_t>(data, next-data));
		data = next+sep.length();
	}
	ret.append(arrayview<uint8_t>(data, dataend-data));
	return ret;
}

array<string> string::rsplit(const string& sep, size_t limit) const
{
	array<string> ret;
	const uint8_t * datastart = ptr();
	const uint8_t * data = ptr()+length();
	
	const uint8_t * sepp = sep.ptr();
	size_t sepl = sep.length();
	
	while (ret.size() < limit)
	{
		if (datastart+sepl > data) break;
		const uint8_t * next = data-sepl;
		while (memcmp(next, sepp, sepl)!=0)
		{
			if (datastart==next) goto done;
			next--;
		}
		ret.insert(0, arrayview<uint8_t>(next+sepl, data-(next+sepl)));
		data = next;
	}
done:
	ret.insert(0, arrayview<uint8_t>(datastart, data-datastart));
	return ret;
}

string string::codepoint(uint32_t cp)
{
	string ret;
	if (cp<=0x7F)
	{
		ret[0] = cp;
	}
	else if (cp<=0x07FF)
	{
		ret[0] = (((cp>> 6)     )|0xC0);
		ret[1] = (((cp    )&0x3F)|0x80);
	}
	else if (cp>=0xD800 && cp<=0xDFFF) return "\xEF\xBF\xBD";
	else if (cp<=0xFFFF)
	{
		ret[0] = (((cp>>12)&0x0F)|0xE0);
		ret[1] = (((cp>>6 )&0x3F)|0x80);
		ret[2] = (((cp    )&0x3F)|0x80);
	}
	else if (cp<=0x10FFFF)
	{
		ret[0] = (((cp>>18)&0x07)|0xF0);
		ret[1] = (((cp>>12)&0x3F)|0x80);
		ret[2] = (((cp>>6 )&0x3F)|0x80);
		ret[3] = (((cp    )&0x3F)|0x80);
	}
	else return "\xEF\xBF\xBD";
	return ret;
}

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
		string a;
		a = "192.168.0.1";
		assert_eq(a.split(".").join("/"), "192/168/0/1");
		assert_eq(a.split<1>(".").join("/"), "192/168.0.1");
		assert_eq(a.rsplit(".").join("/"), "192/168/0/1");
		assert_eq(a.rsplit<1>(".").join("/"), "192.168.0/1");
		
		a = "baaaaaaaaaaaaaaa";
		assert_eq(a.split("a").join("."), "b...............");
		assert_eq(a.split("aa").join("."), "b.......a");
		assert_eq(a.split<1>("aa").join("."), "b.aaaaaaaaaaaaa");
		assert_eq(a.split<1>("x").join("."), "baaaaaaaaaaaaaaa");
		
		a = "aaaaaaaaaaaaaaab";
		assert_eq(a.split("a").join("."), "...............b");
		assert_eq(a.split("aa").join("."), ".......ab");
		assert_eq(a.split<1>("aa").join("."), ".aaaaaaaaaaaaab");
		assert_eq(a.split<1>("x").join("."), "aaaaaaaaaaaaaaab");
		
		a = "baaaaaaaaaaaaaaa";
		assert_eq(a.rsplit("a").join("."), "b...............");
		assert_eq(a.rsplit("aa").join("."), "ba.......");
		assert_eq(a.rsplit<1>("aa").join("."), "baaaaaaaaaaaaa.");
		assert_eq(a.rsplit<1>("x").join("."), "baaaaaaaaaaaaaaa");
		
		a = "aaaaaaaaaaaaaaab";
		assert_eq(a.rsplit("a").join("."), "...............b");
		assert_eq(a.rsplit("aa").join("."), "a.......b");
		assert_eq(a.rsplit<1>("aa").join("."), "aaaaaaaaaaaaa.b");
		assert_eq(a.rsplit<1>("x").join("."), "aaaaaaaaaaaaaaab");
	}
}
