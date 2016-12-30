#pragma once
#include "global.h"
#include <new>
#include <string.h>
#include <type_traits>

//size: two pointers
//this object does not own its storage, it's just a pointer wrapper
template<typename T> class arrayview {
protected:
	T * items; // not const, despite not necessarily being writable; this makes arrayvieww/array a lot simpler
	size_t count;
	
protected:
	static const bool trivial_cons = std::is_trivial<T>::value; // constructor is memset(0)
#if __GNUC__ >= 5
	static const bool trivial_copy = std::is_trivially_copyable<T>::value; // copy constructor is memcpy
#else
	static const bool trivial_copy = trivial_cons;
#endif
	//static const bool trivial_comp = std::has_unique_object_representations<T>::value;
	static const bool trivial_comp = std::is_integral<T>::value; // comparison operator is memcmp
	
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
	
	arrayview(null_t)
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
	
	arrayview<T> slice(size_t first, size_t count) const { return arrayview<T>(this->items+first, count); }
	
	T join() const
	{
		T out = this->items[0];
		for (size_t n=1;n<this->count;n++)
		{
			out += this->items[n];
		}
		return out;
	}
	
	template<typename T2> decltype(T() + T2()) join(T2 between) const
	{
		decltype(T() + T2()) out = this->items[0];
		for (size_t n=1;n < this->count;n++)
		{
			out += between;
			out += this->items[n];
		}
		return out;
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
	
	bool operator==(arrayview<T> other)
	{
		if (size() != other.size()) return false;
		if (this->trivial_comp)
		{
			return memcmp(ptr(), other.ptr(), sizeof(T)*size())==0;
		}
		else
		{
			for (size_t i=0;i<size();i++)
			{
				if (!(items[i]==other[i])) return false;
			}
			return true;
		}
	}
	
	bool operator!=(arrayview<T> other)
	{
		return !(*this == other);
	}
	
	const T* begin() { return this->items; }
	const T* end() { return this->items+this->count; }
};

//size: two pointers
//this one can write its storage, but doesn't own it
template<typename T> class arrayvieww : public arrayview<T> {
	//T * items;
	//size_t count;
public:
	
	T& operator[](size_t n) { return this->items[n]; }
	const T& operator[](size_t n) const { return this->items[n]; }
	
	T* ptr() { return this->items; }
	const T* ptr() const { return this->items; }
	
	arrayvieww()
	{
		this->items=NULL;
		this->count=0;
	}
	
	arrayvieww(null_t)
	{
		this->items=NULL;
		this->count=0;
	}
	
	arrayvieww(T * ptr, size_t count)
	{
		this->items = ptr;
		this->count = count;
	}
	
	template<size_t N> arrayvieww(T (&ptr)[N])
	{
		this->items = ptr;
		this->count = N;
	}
	
	arrayvieww(const arrayvieww<T>& other)
	{
		this->items = other.items;
		this->count = other.count;
	}
	
	arrayvieww<T> operator=(arrayvieww<T> other)
	{
		this->items = other.items;
		this->count = other.count;
		return *this;
	}
	
	arrayvieww<T> slice(size_t first, size_t count) { return arrayvieww<T>(this->items+first, count); }
	
	T* begin() { return this->items; }
	T* end() { return this->items+this->count; }
};

//size: two pointers, plus one T per item
//this one owns its storage, and manages its memory
template<typename T> class array : public arrayvieww<T> {
	//T * items;
	//size_t count;
	
	void clone(const arrayview<T>& other)
	{
		this->count = other.size(); // I can't access non-this instances of my base class, so let's just use the public interface.
		this->items = malloc(sizeof(T)*bitround(this->count));
		if (this->trivial_copy)
		{
			memcpy(this->items, other.ptr(), sizeof(T)*this->count);
		}
		else
		{
			for (size_t i=0;i<this->count;i++)
			{
				new(&this->items[i]) T(other.ptr()[i]);
			}
		}
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
	
	void resize_grow_noinit(size_t count)
	{
		if (this->count >= count) return;
		size_t bufsize_pre = bitround(this->count);
		size_t bufsize_post = bitround(count);
		if (bufsize_pre != bufsize_post) this->items=realloc(this->items, sizeof(T)*bufsize_post);
		this->count=count;
	}
	
	void resize_shrink_noinit(size_t count)
	{
		if (this->count <= count) return;
		size_t bufsize_pre = bitround(this->count);
		size_t bufsize_post = bitround(count);
		if (bufsize_pre != bufsize_post) this->items=realloc(this->items, sizeof(T)*bufsize_post);
		this->count=count;
	}
	
	void resize_grow(size_t count)
	{
		if (this->count >= count) return;
		size_t prevcount = this->count;
		resize_grow_noinit(count);
		if (this->trivial_cons)
		{
			memset(this->items+prevcount, 0, sizeof(T)*(count-prevcount));
		}
		else
		{
			for (size_t i=prevcount;i<count;i++)
			{
				new(&this->items[i]) T();
			}
		}
	}
	
	void resize_shrink(size_t count)
	{
		if (this->count <= count) return;
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
	
	void resize(size_t len) { resize_to(len); }
	void reserve(size_t len) { resize_grow(len); }
	void reserve_noinit(size_t len)
	{
		if (this->trivial_cons) resize_grow_noinit(len);
		else resize_grow(len);
	}
	
	void append(const T& item) { size_t pos = this->count; resize_grow(pos+1); this->items[pos] = item; }
	void reset() { resize_shrink(0); }
	
	void remove(size_t index)
	{
		this->items[index].~T();
		memmove(this->items+index, this->items+index+1, sizeof(T)*(this->count-1-index));
		this->count--;
	}
	
	array()
	{
		this->items=NULL;
		this->count=0;
	}
	
	array(null_t)
	{
		this->items=NULL;
		this->count=0;
	}
	
	array(const array<T>& other)
	{
		clone(other);
	}
	
	array(const arrayview<T>& other)
	{
		clone(other);
	}
	
	array(array<T>&& other)
	{
		swap(other);
	}
	
	array<T> operator=(array<T> other)
	{
		swap(other);
		return *this;
	}
	
	array<T> operator=(arrayview<T> other)
	{
		if (other.ptr() >= this->ptr() && other.ptr() < this->ptr()+this->size())
		{
			size_t start = other.ptr()-this->ptr();
			size_t len = other.size();
			
			for (size_t i=0;i<start;i++) this->items[i].~T();
			memmove(this->ptr(), this->ptr()+start, sizeof(T)*len);
			for (size_t i=start+len;i<this->count;i++) this->items[i].~T();
			
			resize_shrink_noinit(len);
		}
		else
		{
			for (size_t i=0;i<this->count;i++) this->items[i].~T();
			free(this->items);
			clone(other);
		}
		return *this;
	}
	
	array<T>& operator+=(arrayview<T> other)
	{
		size_t prevcount = this->count;
		size_t othercount = other.size();
		
		const T* src;
		T* dst;
		
		if (other.ptr() >= this->ptr() && other.ptr() < this->ptr()+this->size())
		{
			size_t start = other.ptr()-this->ptr();
			
			resize_grow_noinit(prevcount + othercount);
			src = this->items+start;
			dst = this->items+prevcount;
		}
		else
		{
			resize_grow_noinit(prevcount + othercount);
			src = other.ptr();
			dst = this->items+prevcount;
		}
		
		if (this->trivial_copy)
		{
			memcpy(dst, src, sizeof(T)*othercount);
		}
		else
		{
			for (size_t i=0;i<othercount;i++)
			{
				new(&dst[i]) T(src[i]);
			}
		}
		
		return *this;
	}
	
	~array()
	{
		for (size_t i=0;i<this->count;i++) this->items[i].~T();
		free(this->items);
	}
	
	//takes ownership of the given data
	static array<T> create_usurp(T* ptr, size_t count)
	{
		array<T> ret;
		ret.items = ptr;
		ret.count = 0;
		ret.resize_grow_noinit(count);
		return ret;
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
