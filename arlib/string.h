#pragma once
#include "global.h"

//A cstring does not own its memory; it only borrows it from someone else. A string does own its memory.

//Public members shall be named after their counterpart in std::string, if one exists; if not, look in .NET System.String.
//If neither have a good counterpart, any name is acceptable.

//Due to COW optimizations, strings are not thread safe. If you need to share strings across threads,
// call .unshare() after storing it.

//Strings are always NUL terminated. It is safe to overwrite the NUL on a string; that will extend the string.

class string {
private:
	static const int obj_size = 24; // maximum 32, or len_inline overflows
	static const int max_inline = obj_size-1-1; // -1 for length, -1 for NUL
	
	friend class cstring;
	friend class wstring;
	
	union {
		struct { // .inlined = 1 (checking .inlined is always allowed)
			//ensure .inlined is in the first byte; don't care if it's top or bottom bit
			//GCC orders bitfields according to <http://stackoverflow.com/a/1490135>
			// With GCC, big endian machines lay out the bits big end first and little endian machines lay out the bits little end first.
			//while MSVC follows <https://msdn.microsoft.com/en-us/library/ewwyfdbe.aspx>
			// Microsoft Specific: The ordering of data declared as bit fields is from low to high bit
			//so I need the low bit first on little endian, MSVC or both; and high bit first on bigend GCC.
			//Luckily, Windows doesn't operate on bigend, per <https://support.microsoft.com/en-us/kb/102025>
			// Windows NT was designed around Little Endian architecture and was not designed to be compatible with Big Endian
			//so I can ignore that combination, and swapping based on endian gives what I want.
			BIGEND_SWAP2(
			uint8_t inlined : 1;
			uint8_t owning : 1;
			mutable uint8_t wcache : 1;
			,
			uint8_t len_inline : 5;
			)
			//it would be possible to use the last byte of the inlined data as both length indicator and NUL terminator
			//(0x00 = length 23, other = outlined or shorter)
			//but the extra effort required makes it not worth it. 22 is a perfectly fine SSO length, I don't need 23.
			char data_inline[max_inline+1];
		};
		struct { // .inlined = 0
			BIGEND_SWAP2(
			uint32_t inlined32 : 1;
			uint32_t owning32 : 1;
			mutable uint32_t wcache32 : 1;
			,
			uint32_t len_outline : 29;
			)
			//char pad[4];
			char* data_outline; // if owning, there's also a int32 refcount before this pointer; if not owning, no such thing
			//char pad2[8];
		};
	};
	
	static size_t bytes_for(uint32_t len)
	{
		return bitround(sizeof(int)+len+1);
	}
	
	static char * clone_sized(const char * in, uint32_t len, uint32_t alloclen)
	{
		int* refcount = malloc(bytes_for(alloclen));
		*refcount = 1;
		char* ret = (char*)(refcount+1);
		memcpy(ret, in, len);
		ret[len] = '\0';
		return ret;
	}
	static char * clone(const char * in, uint32_t len)
	{
		return clone_sized(in, len, len);
	}
	static char * clone(const char * in)
	{
		return clone(in, strlen(in));
	}
	
	int* refcount() // yields garbage if not inlined or not owning
	{
		return (int*)(data_outline-sizeof(int));
	}
	void addref()
	{
		if (inlined) return;
		if (!owning) return;
		++*refcount();
	}
	void release()
	{
		if (inlined) return;
		if (!owning) return;
		if (--*refcount() == 0) free(data_outline - sizeof(int));
	}
public:
	//Detaches a string object from anything it's COWed with. Normally not needed, but if you need to
	// share a string across threads, it can be useful.
	void unshare()
	{
		wcache = 0;
		if (inlined) return;
		if (owning && *refcount() == 1) return;
		//use the string after releasing our reference - ugly, but we lose the old refcount if we change data_outline, and we're not thread safe anyways
		release();
		owning = 1;
		data_outline = clone(data_outline, len_outline);
	}
	
private:
	void init_from(const char * str)
	{
		uint32_t len = strlen(str);
		if (len <= max_inline)
		{
			inlined = 1;
			owning = 1;
			wcache = 0;
			len_inline = len;
			memcpy(data_inline, str, len+1);
		}
		else
		{
			inlined32 = 0;
			owning32 = 1;
			wcache32 = 0;
			len_outline = len;
			data_outline = clone(str, len_outline);
		}
	}
	
	void init_from(const string& other)
	{
		memcpy(this, &other, sizeof(string));
		if (!inlined)
		{
			if (owning) addref();
			else data_outline = clone(data_outline, len_outline);
		}
	}
	
	void resize(uint32_t newsize)
	{
		uint32_t oldsize = size();
		if (oldsize == newsize) return;
		unshare();
		
		if (newsize > max_inline)
		{
			if (inlined)
			{
				data_outline = clone_sized(data_inline, oldsize, newsize);
			}
			else if (bytes_for(oldsize) != bytes_for(newsize))
			{
				data_outline = realloc(data_outline-sizeof(int), bytes_for(newsize));
			}
			inlined32 = 0;
			owning32 = 1; // set this unconditionally, it allows the compiler to merge the writed
			wcache32 = 0;
			len_outline = newsize;
			data_outline[newsize] = '\0';
		}
		else
		{
			if (!inlined) memcpy(data_inline, data(), oldsize);
			data_inline[newsize] = '\0';
			inlined = 1;
			owning = 1;
			wcache = 0;
			len_inline = newsize;
		}
	}
	
	//Ignored if the new size is smaller.
	void resize_grow(uint32_t newsize)
	{
		uint32_t oldsize = size();
		if (oldsize >= newsize) return;
		resize(newsize);
	}
	
	//Ignored if the new size is larger.
	void resize_shrink(uint32_t newsize)
	{
		uint32_t oldsize = size();
		if (oldsize <= newsize) return;
		resize(newsize);
	}
	
	char getchar(uint32_t index) const { return data()[index]; }
	void setchar(uint32_t index, char val) { unshare(); resize_grow(index+1); data()[index] = val; }
	
	char * data() { return inlined ? data_inline : data_outline; }
	
	class noinit {};
	string(noinit) {}
	
public:
	string() { inlined=1; owning=1; wcache=0; len_inline=0; data_inline[0] = '\0'; }
	string(const string& other) { init_from(other); }
	string(const char * str) { init_from(str); }
	string& operator=(const string& other) { release(); init_from(other); return *this; }
	string& operator=(const char * str) { release(); init_from(str); return *this; }
	~string() { release(); }
	
	const char * data() const { return inlined ? data_inline : data_outline; }
	uint32_t size() const { return inlined ? len_inline : len_outline; }
	operator const char * () const { return data(); }
	
private:
	class charref {
		string* parent;
		uint32_t index;
		
	public:
		charref& operator=(char ch) { parent->setchar(index, ch); return *this; }
		operator char() { return parent->getchar(index); }
		
		charref(string* parent, uint32_t index) : parent(parent), index(index) {}
	};
	friend class charref;
	
public:
	charref operator[](uint32_t index) { return charref(this, index); }
	charref operator[](int index) { return charref(this, index); }
	char operator[](uint32_t index) const { return getchar(index); }
	char operator[](int index) const { return getchar(index); }
	
	void replace(uint32_t pos, uint32_t len, string newdat)
	{
		unshare();
		uint32_t newlen = newdat.size();
		if (newlen > len) resize(size()-len+newlen);
		uint32_t mylen = size();
		char* dat = data();
		if (newlen != len) memmove(dat+pos+newlen, dat+pos+len, mylen-len-pos);
		memcpy(dat+pos, newdat.data(), newlen);
		if (newlen < len) resize(mylen-len+newlen);
	}
};

class cstring : public string {
public:
	cstring() : string() {}
	cstring(const string& other) : string(other) {}
	cstring(const cstring& other) : string(noinit())
	{
		memcpy(this, &other, sizeof(cstring));
		owning = 0;
	}
	cstring(const char * str)
	{
		inlined32 = 0;
		owning32 = 0;
		wcache32 = 0;
		len_outline = strlen(str);
		data_outline = (char*)str;
	}
};

//TODO
class wstring : public string {
	mutable uint32_t pos_bytes;
	mutable uint32_t pos_chars;
	mutable uint32_t wsize;
	//char pad[4];
	const uint32_t WSIZE_UNKNOWN = -1;
	
	void clearcache() const
	{
		pos_bytes = 0;
		pos_chars = 0;
		wsize = WSIZE_UNKNOWN;
		wcache = 1;
	}
	
	void checkcache() const
	{
		if (!wcache) clearcache();
	}
	
	uint32_t findcp(uint32_t index) const
	{
		checkcache();
		
		if (pos_chars > index)
		{
			pos_bytes=0;
			pos_chars=0;
		}
		
		uint8_t* scan = (uint8_t*)data() + pos_bytes;
		uint32_t chars = pos_chars;
		while (chars != index)
		{
			if ((*scan&0xC0) != 0x80) chars++;
			scan++;
		}
		pos_bytes = scan - (uint8_t*)data();
		pos_chars = index;
		
		return pos_bytes;
	}
	
	uint32_t getcp(uint32_t index) const { return 42; }
	void setcp(uint32_t index, uint32_t val) { }
	
	class charref {
		wstring* parent;
		uint32_t index;
		
	public:
		charref& operator=(char ch) { parent->setcp(index, ch); return *this; }
		operator uint32_t() { return parent->getcp(index); }
		
		charref(wstring* parent, uint32_t index) : parent(parent), index(index) {}
	};
	friend class charref;
	
public:
	wstring() : string() { clearcache(); }
	wstring(const string& other) : string(other) { clearcache(); }
	wstring(const char * str) : string(str) { clearcache(); }
	
	charref operator[](uint32_t index) { return charref(this, index); }
	charref operator[](int index) { return charref(this, index); }
	uint32_t operator[](uint32_t index) const { return getcp(index); }
	uint32_t operator[](int index) const { return getcp(index); }
	
	uint32_t size() const
	{
		checkcache();
		if (wsize == WSIZE_UNKNOWN)
		{
			uint8_t* scan = (uint8_t*)data() + pos_bytes;
			uint32_t chars = pos_chars;
			while (*scan)
			{
				if ((*scan&0xC0) != 0x80) chars++;
				scan++;
			}
			wsize = chars;
		}
		return wsize;
	}
};
