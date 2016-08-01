#pragma once
#include "global.h"
#include <new>
#include <string.h>

//as much fun as it is to optimize the hell out of random stuff, I want to get things done as well

//size: two pointers
//this object does not own its storage, it's just a pointer wrapper
template<typename T> class arrayview {
protected:
	class null_only;
	
	T * items;
	size_t count;
	
	//void clone(const arrayview<T>& other)
	//{
	//	this->count=other.count;
	//	this->items=other.items;
	//}
public:
	const T& operator[](size_t n) const { return items[n]; }
	
	const T* ptr() const { return items; }
	size_t size() const { return count; }
	
	operator bool() { return count; }
	
	arrayview()
	{
		this->items=NULL;
		this->count=0;
	}
	
	arrayview(const null_only*)
	{
		this->items=NULL;
		this->count=0;
	}
	
	arrayview(const T * ptr, size_t count)
	{
		this->items = (T*)ptr;
		this->count = count;
	}
	
	template<size_t N> arrayview(const T (&ptr)[N])
	{
		this->items = (T*)ptr;
		this->count = N;
	}
	
	//arrayview(const arrayview<T>& other)
	//{
	//	clone(other);
	//}
	
	//arrayview<T> operator=(const arrayview<T>& other)
	//{
	//	clone(other);
	//	return *this;
	//}
};

//size: two pointers, plus one T per item
//this one owns its storage
template<typename T> class array : public arrayview<T> {
	//T * items;
	//size_t count;
	
	void clone(const array<T>& other)
	{
		this->count=other.count;
		this->items=malloc(sizeof(T)*bitround(this->count));
		for (size_t i=0;i<this->count;i++) new(&this->items[i]) T(other.items[i]);
	}
	
	void swap(array<T>& other)
	{
		T* newitems = other.items;
		size_t newcount = other.count;
		other.items = this->items;
		other.count = this->count;
		this->items = newitems;
		this->count = newcount;
	}
	
	void resize_grow(size_t count)
	{
		if (this->count >= count) return;
		size_t bufsize_pre=bitround(this->count);
		size_t bufsize_post=bitround(count);
		if (bufsize_pre != bufsize_post) this->items=realloc(this->items, sizeof(T)*bufsize_post);
		for (size_t i=this->count;i<count;i++)
		{
			new(&this->items[i]) T();
		}
		this->count=count;
	}
	
	void resize_shrink(size_t count)
	{
		if (this->count < count) return;
		for (size_t i=count;i<this->count;i++)
		{
			this->items[i].~T();
		}
		size_t bufsize_pre=bitround(this->count);
		size_t bufsize_post=bitround(count);
		if (bufsize_pre != bufsize_post) this->items=realloc(this->items, sizeof(T)*bufsize_post);
		this->count=count;
	}
	
	void resize_to(size_t count)
	{
		if (count > this->count) resize_grow(count);
		else resize_shrink(count);
	}
	
public:
	T& operator[](size_t n) { resize_grow(n+1); return this->items[n]; }
	const T& operator[](size_t n) const { return this->items[n]; }
	
	T* ptr() { return this->items; }
	void resize(size_t len) { resize_to(len); }
	
	T join() const
	{
		T out = this->items[0];
		for (size_t n=1;n<this->count;n++)
		{
			out += this->items[n];
		}
		return out;
	}
	
	T join(T between) const
	{
		T out = this->items[0];
		for (size_t n=1;n < this->count;n++)
		{
			out += between;
			out += this->items[n];
		}
		return out;
	}
	
	T join(char between) const
	{
		T out = this->items[0];
		for (size_t n=1;n<this->count;n++)
		{
			out += between;
			out += this->items[n];
		}
		return out;
	}
	
	void append(const T& item) { size_t pos = this->count; resize_grow(pos+1); this->items[pos] = item; }
	void reset() { resize_shrink(0); }
	
	arrayview<T> slice(size_t first, size_t count) { return arrayview<T>(this->items+first, this->count); }
	
	array()
	{
		this->items=NULL;
		this->count=0;
	}
	
	array(const array<T>& other)
	{
		clone(other);
	}
	
#ifdef HAVE_MOVE
	array(array<T>&& other)
	{
		swap(other);
	}
#endif
	
	array<T> operator=(array<T> other)
	{
		swap(other);
		return *this;
	}
	
	static array<T> create_from(T* ptr, size_t count)
	{
		array<T> ret;
		ret.items = ptr;
		ret.count = count;
		return ret;
	}
	
	~array()
	{
		for (size_t i=0;i<this->count;i++) this->items[i].~T();
		free(this->items);
	}
};


template<> class array<bool> {
protected:
	static const size_t n_inline = sizeof(uint8_t*)/sizeof(uint8_t)*8;
	
	union {
		uint8_t bits_inline[n_inline/8];
		uint8_t* bits_outline;
	};
	uint8_t* bits()
	{
		if (nbits <= n_inline) return bits_inline;
		else return bits_outline;
	}
	const uint8_t* bits() const
	{
		if (nbits <= n_inline) return bits_inline;
		else return bits_outline;
	}
	size_t nbits;
	
	class entry {
		array<bool>& parent;
		size_t index;
		
	public:
		operator bool() const { return parent.get(index); }
		entry& operator=(bool val) { parent.set(index, val); return *this; }
		
		entry(array<bool>& parent, size_t index) : parent(parent), index(index) {}
	};
	friend class entry;
	
	bool get(size_t n) const
	{
		if (n >= nbits) return false;
		return bits()[n/8]>>(n&7) & 1;
	}
	
	void set(size_t n, bool val)
	{
		if (n >= nbits)
		{
			resize(n+1);
		}
		uint8_t& byte = bits()[n/8];
		byte &=~ (1<<(n&7));
		byte |= (val<<(n&7));
	}
	
public:
	bool operator[](size_t n) const { return get(n); }
	entry operator[](size_t n) { return entry(*this, n); }
	
	size_t size() const { return nbits; }
	void reset()
	{
		if (nbits >= n_inline) free(this->bits_outline);
		this->nbits = 0;
	}
	
	void resize(size_t len)
	{
		switch ((this->nbits > n_inline)<<1 | (len > n_inline))
		{
		case 0: // small->small
			break;
		case 1: // small->big
			{
				size_t newbytes = bitround((len+7)/8);
				uint8_t* newbits = malloc(newbytes);
				memcpy(newbits, this->bits_inline, sizeof(this->bits_inline));
				memset(newbits+sizeof(this->bits_inline), 0, newbytes-sizeof(this->bits_inline));
				bits_outline = newbits;
			}
			break;
		case 2: // big->small
			{
				uint8_t* freethis = this->bits_outline;
				memcpy(this->bits_inline, this->bits_outline, sizeof(this->bits_inline));
				free(freethis);
			}
		case 3: // big->big
			{
				size_t prevbytes = bitround((this->nbits+7)/8);
				size_t newbytes = bitround((len+7)/8);
				if (newbytes > prevbytes)
				{
					bits_outline = realloc(this->bits_outline, newbytes);
					if (newbytes > prevbytes)
					{
						memset(this->bits_outline+prevbytes, 0, newbytes-prevbytes);
					}
				}
			}
			break;
		}
		
		this->nbits = len;
	}
	
	void append(bool item) { set(this->nbits, item); }
	
	array<bool> slice(size_t first, size_t count)
	{
		if ((first&7) == 0)
		{
			array<bool> ret;
			ret.resize(count);
			memcpy(ret.bits(), this->bits() + first/8, (count+7)/8);
			return ret;
		}
		else
		{
			array<bool> ret;
			ret.resize(count);
			for (size_t i=0;i<count;i++) ret.set(i, this->get(first+i));
			return ret;
		}
	}
	
	array()
	{
		this->nbits = 0;
		memset(this->bits_inline, 0, sizeof(this->bits_inline));
	}
	
	~array()
	{
		if (nbits >= n_inline) free(this->bits_outline);
	}
};
