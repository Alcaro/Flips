#pragma once
#include "global.h"
#include "array.h"
#include <string.h>

//A string is a mutable sequence of bytes. It usually represents UTF-8 text, but can be arbitrary binary data, including NULs.
//All functions taking or returning a char* assume/guarantee NUL termination. However, anything taking uint8_t* does not.

//cstring is a special case of string that's not guaranteed to own its storage; it lives and dies by whatever string or array it was created from.
//Creating a cstring from another cstring leaves no dependency on this cstring; they're both bound to their source.
//Modifying a cstring disconnects it from its source and allows the original string to be deleted.

class cstring;

class string {
	static const int obj_size = 16; // maximum 120, or the inline length overflows
	                                // (127 would fit, but that requires an extra alignment byte, which throws the sizeof assert)
	                                // minimum 16 (pointer + various members + alignment) (actually minimum 12 on 32bit, but who needs 32bit)
	static const int max_inline = obj_size-1;
	
	union {
		struct {
			uint8_t m_inline[max_inline];
			
			//this is how many bytes are unused by the raw string data
			//if all bytes are used, there are zero unused bytes - which also serves as the NUL
			//if not inlined, it's -1
			uint8_t m_inline_len;
		};
		struct {
			mutable uint8_t* m_data; // if owning, there's also a int refcount before this pointer; if not owning, no such thing
			uint32_t m_len;
			mutable bool m_owning;
			mutable bool m_nul; // whether the string is properly terminated (always true if owning)
			uint8_t reserved; // matches the last byte of the inline data; never ever access this
		};
	};
	
	bool inlined() const
	{
		static_assert(sizeof(string)==obj_size);
		
		return m_inline_len != (uint8_t)-1;
	}
	
	const uint8_t * ptr() const
	{
		if (inlined()) return m_inline;
		else return m_data;
	}
	
	uint8_t * ptr()
	{
		if (inlined()) return m_inline;
		else return m_data;
	}
	
	
	static size_t bytes_for(uint32_t len)
	{
		return bitround(sizeof(int)+len+1);
	}
	
	//the sizes can be 0 if you want to
	//sizes are how many characters fit in the string, excluding the NUL
	//always allocates, doesn't try to inline
	static uint8_t* alloc(uint8_t* prev, uint32_t prevsize, uint32_t newsize);
	
	void unshare() const;
	
	//does not initialize the new data
	void resize(uint32_t newlen);
	
	const char * ptr_withnul() const
	{
		if (!inlined() && !m_nul)
		{
			unshare();
		}
		return (char*)ptr();
	}
	
public:
	uint32_t length() const
	{
		if (inlined()) return max_inline-m_inline_len;
		else return m_len;
	}
	
	arrayview<byte> bytes() const
	{
		return arrayview<byte>(ptr(), length());
	}
	//If this is true, bytes()[bytes().length()] is '\0'. If false, it's undefined behavior.
	bool bytes_hasterm() const
	{
		return (inlined() || m_nul);
	}
	
private:
	//cstring uses the nocopy constructors
	friend class cstring;
	
	void init_from(const char * str)
	{
		init_from(arrayview<byte>((uint8_t*)str, strlen(str)));
	}
	void init_from(arrayview<byte> data);
	void init_from(const string& other)
	{
		memcpy(this, &other, sizeof(*this));
		if (!inlined())
		{
			if (m_owning) ++*(int*)(m_data-sizeof(int));
			else unshare();
		}
	}
	void init_from(string&& other)
	{
		memcpy(this, &other, sizeof(*this));
		other.m_inline_len = 0;
		if (!inlined() && !m_owning) unshare();
	}
	void init_from_nocopy(const char * str)
	{
		init_from_nocopy(arrayview<byte>((uint8_t*)str, strlen(str)));
		if (!inlined()) m_nul = true;
	}
	void init_from_nocopy(arrayview<byte> data);
	void init_from_nocopy(const string& other)
	{
		memcpy(this, &other, sizeof(*this));
		if (!inlined() && m_owning)
		{
			++*(int*)(m_data-sizeof(int));
		}
	}
	void init_from_nocopy(string&& other)
	{
		memcpy(this, &other, sizeof(*this));
		other.m_inline_len = 0;
	}
	void release()
	{
		if (!inlined() && m_owning)
		{
			alloc(m_data,m_len, 0);
		}
	}
	
	//constant for all string implementations, but used by the implementation, so let's keep it here
	int32_t realpos(int32_t pos) const
	{
		if (pos >= 0) return pos;
		else return length()-~pos;
	}
	
	char getchar(int32_t index) const
	{
		//this function is REALLY hot, use the strongest possible optimizations
		if (index >= 0)
		{
			if (inlined()) return m_inline[index];
			else if ((uint32_t)index < m_len) return m_data[index];
			else return '\0';
		}
		
		return getchar(realpos(index));
	}
	void setchar(int32_t index_, char val)
	{
		unshare();
		uint32_t index = realpos(index_);
		if (index == length())
		{
			resize(index+1);
		}
		ptr()[index] = val;
	}
	
	void append(const uint8_t * newdat, uint32_t newlength)
	{
		if (newdat >= (uint8_t*)ptr() && newdat < (uint8_t*)ptr()+length())
		{
			uint32_t offset = newdat-ptr();
			uint32_t oldlength = length();
			resize(oldlength+newlength);
			memcpy(ptr()+oldlength, ptr()+offset, newlength);
		}
		else
		{
			uint32_t oldlength = length();
			resize(oldlength+newlength);
			memcpy(ptr()+oldlength, newdat, newlength);
		}
	}
	
public:
	//Resizes the string to a suitable size, then allows the caller to fill it in with whatever. Initial contents are undefined.
	//The returned pointer may only be used until the first subsequent use of the string, including read-only operations.
	arrayvieww<byte> construct(uint32_t len)
	{
		resize(len);
		return arrayvieww<byte>(ptr(), len);
	}
	
	void replace(int32_t pos, int32_t len, const string& newdat); // const string& is ugly, but cstring isn't declared yet
	
	string replace(const string& in, const string& out);
	
	string& operator+=(const char * right)
	{
		append((uint8_t*)right, strlen(right));
		return *this;
	}
	
	string& operator+=(const string& right)
	{
		append(right.ptr(), right.length());
		return *this;
	}
	
	string& operator+=(char right)
	{
		uint8_t tmp = right;
		append(&tmp, 1);
		return *this;
	}
	
	//can't create csplit without things blowing up
	//limit is maximum number of cuts
	array<string> split(const string& sep, size_t limit) const;
	
	template<size_t limit = SIZE_MAX>
	array<string> split(const string& sep) const { return split(sep, limit); }
	
	array<string> rsplit(const string& sep, size_t limit) const;
	
	template<size_t limit = SIZE_MAX>
	array<string> rsplit(const string& sep) const { return rsplit(sep, limit); }
	
private:
	class noinit {};
	string(noinit) {}
	
public:
	string() { init_from(""); }
	string(const string& other) { init_from(other); }
	string(string&& other) { init_from(std::move(other)); }
	string(const char * str) { init_from(str); }
	//string(const uint8_t * str, uint32_t len) { init_from(str, len); }
	string(arrayview<uint8_t> bytes) { init_from(bytes); }
	string(arrayview<char> chars)
	{
		init_from_nocopy(arrayview<uint8_t>((uint8_t*)chars.ptr(), chars.size()));
	}
	string& operator=(const string& other) { release(); init_from(other); return *this; }
	string& operator=(const char * str) { release(); init_from(str); return *this; }
	~string() { release(); }
	
	operator bool() const { return length() != 0; }
	operator const char * () const { return ptr_withnul(); }
	
private:
	class charref : nocopy {
		friend class string;
		string* parent;
		uint32_t index;
		charref(string* parent, uint32_t index) : parent(parent), index(index) {}
		
	public:
		charref& operator=(char ch) { parent->setchar(index, ch); return *this; }
		operator char() { return parent->getchar(index); }
	};
	friend class charref;
	
public:
	//Reading the NUL terminator is fine. Writing extends the string. Poking outside the string is undefined.
	//charref operator[](uint32_t index) { return charref(this, index); }
	charref operator[](int index) { return charref(this, index); }
	//char operator[](uint32_t index) const { return getchar(index); }
	char operator[](int index) const { return getchar(index); }
	
	//static string create(arrayview<uint8_t> data) { string ret=noinit(); ret.init_from(data.ptr(), data.size()); return ret; }
	
	string substr(int32_t start, int32_t end) const
	{
		start = realpos(start);
		end = realpos(end);
		return string(arrayview<byte>(ptr()+start, end-start));
	}
	inline cstring csubstr(int32_t start, int32_t end) const;
	inline bool contains(cstring other) const;
	inline bool startswith(cstring other) const;
	inline bool endswith(cstring other) const;
	
	static string codepoint(uint32_t cp);
};

inline bool operator==(const string& left, const char * right ) { return left.bytes() == arrayview<byte>((uint8_t*)right,strlen(right)); }
inline bool operator==(const string& left, const string& right) { return left.bytes() == right.bytes(); }
inline bool operator==(const char * left,  const string& right) { return operator==(right, left); }
inline bool operator!=(const string& left, const char * right ) { return !operator==(left, right); }
inline bool operator!=(const string& left, const string& right) { return !operator==(left, right); }
inline bool operator!=(const char * left,  const string& right) { return !operator==(left, right); }

inline string operator+(const string& left, const string& right) { string ret=left; ret+=right; return ret; }
inline string operator+(string&& left, const char * right) { left+=right; return left; }
inline string operator+(const string& left, const char * right) { string ret=left; ret+=right; return ret; }
inline string operator+(string&& left, const string& right) { left+=right; return left; }
inline string operator+(const char * left, const string& right) { string ret=left; ret+=right; return ret; }

inline string operator+(string&& left, char right) { left+=right; return left; }
inline string operator+(const string& left, char right) { string ret=left; ret+=right; return ret; }
inline string operator+(char left, const string& right) { string ret; ret[0]=left; ret+=right; return ret; }

class cstring : public string {
	friend class string;
public:
	cstring() : string() {}
	cstring(const string& other) : string(noinit()) { init_from_nocopy(other); }
	cstring(const cstring& other) : string(noinit()) { init_from_nocopy(other); }
	cstring(string&& other) : string(noinit()) { init_from_nocopy(std::move(other)); }
	cstring(cstring&& other) : string(noinit()) { init_from_nocopy(std::move(other)); }
	cstring(const char * str) : string(noinit()) { init_from_nocopy(str); }
	//cstring(const uint8_t * str, uint32_t len) : string(noinit()) { init_from_nocopy(str, len); }
	cstring(arrayview<uint8_t> bytes) : string(noinit()) { init_from_nocopy(bytes); }
	cstring(arrayview<char> chars) : string(noinit())
	{
		init_from_nocopy(arrayview<uint8_t>((uint8_t*)chars.ptr(), chars.size()));
	}
private:
	//don't use arrayview, if (nul) then it uses len+1 bytes
	cstring(const uint8_t * str, uint32_t len, bool nul) : string(noinit())
	{
		init_from_nocopy(arrayview<byte>(str, len));
		if (!inlined()) m_nul=nul;
	}
public:
	
	cstring& operator=(const cstring& other) { release(); init_from_nocopy(other); return *this; }
};

inline cstring string::csubstr(int32_t start, int32_t end) const
{
	start = realpos(start);
	end = realpos(end);
	if (inlined()) return cstring(arrayview<byte>(ptr()+start, end-start));
	else return cstring(ptr()+start, end-start, (m_nul && (uint32_t)end == m_len));
}

inline bool string::contains(cstring other) const
{
	return memmem(this->ptr(), this->length(), other.ptr(), other.length()) != NULL;
}

inline bool string::startswith(cstring other) const
{
	if (other.length() > this->length()) return false;
	return (!memcmp(this->ptr(), other.ptr(), other.length()));
}

inline bool string::endswith(cstring other) const
{
	if (other.length() > this->length()) return false;
	return (!memcmp(this->ptr()+this->length()-other.length(), other.ptr(), other.length()));
}

//TODO
//class wstring : public string {
//	mutable uint32_t pos_bytes;
//	mutable uint32_t pos_chars;
//	mutable uint32_t wsize;
//	char pad[4];
//	const uint32_t WSIZE_UNKNOWN = -1;
//	
//	void clearcache() const
//	{
//		pos_bytes = 0;
//		pos_chars = 0;
//		wsize = WSIZE_UNKNOWN;
//		wcache(true);
//	}
//	
//	void checkcache() const
//	{
//		if (!wcache()) clearcache();
//	}
//	
//	uint32_t findcp(int32_t index) const
//	{
//		checkcache();
//		
//		if (pos_chars > (uint32_t)index)
//		{
//			pos_bytes=0;
//			pos_chars=0;
//		}
//		
//		uint8_t* scan = (uint8_t*)data() + pos_bytes;
//		uint32_t chars = pos_chars;
//		while (chars != (uint32_t)index)
//		{
//			if ((*scan&0xC0) != 0x80) chars++;
//			scan++;
//		}
//		pos_bytes = scan - (uint8_t*)data();
//		pos_chars = index;
//		
//		return pos_bytes;
//	}
//	
//	uint32_t getcp(int32_t index) const { return 42; }
//	void setcp(int32_t index, uint32_t val) { }
//	
//	class charref {
//		wstring* parent;
//		int32_t index;
//		
//	public:
//		charref& operator=(char ch) { parent->setcp(index, ch); return *this; }
//		operator uint32_t() { return parent->getcp(index); }
//		
//		charref(wstring* parent, int32_t index) : parent(parent), index(index) {}
//	};
//	friend class charref;
//	
//public:
//	wstring() : string() { clearcache(); }
//	wstring(const string& other) : string(other) { clearcache(); }
//	wstring(const char * str) : string(str) { clearcache(); }
//	
//	charref operator[](int32_t index) { return charref(this, index); }
//	uint32_t operator[](int32_t index) const { return getcp(index); }
//	
//	uint32_t size() const
//	{
//		checkcache();
//		if (wsize == WSIZE_UNKNOWN)
//		{
//			uint8_t* scan = (uint8_t*)data() + pos_bytes;
//			uint32_t chars = pos_chars;
//			while (*scan)
//			{
//				if ((*scan&0xC0) != 0x80) chars++;
//				scan++;
//			}
//			wsize = chars;
//		}
//		return wsize;
//	}
//};
