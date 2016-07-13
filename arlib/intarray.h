#include "global.h"

//Use only with the primitive types.
template<typename T>
class intarray {
public:
	//Could be optimized a lot harder, but I don't care, it's not used much.
	
	T* ptr;
	size_t len;
	
	static const size_t MIN_SIZE = 128/sizeof(T);         // minimum number of objects this one always holds
	static const size_t MIN_SIZE_SHRINK = 4096/sizeof(T); // don't shrink smaller than this size
	static const size_t MIN_SIZE_FACTOR = 4;              // only shrink by this factor or more (must be power of two)
	
private:
	size_t capacity;
	
	static size_t resize_size(size_t oldlen, size_t len)
	{
		len = bitround(len);
		
		if (len < MIN_SIZE) return MIN_SIZE;
		if (len > oldlen) return len;
		
		if (len < oldlen/MIN_SIZE_FACTOR && oldlen>MIN_SIZE_SHRINK)
		{
			if (oldlen/MIN_SIZE_FACTOR < MIN_SIZE_SHRINK) return MIN_SIZE_SHRINK;
			else return oldlen/MIN_SIZE_FACTOR;
		}
		
		return oldlen;
	}
	
	void resize(size_t newcap)
	{
		newcap = resize_size(capacity, newcap);
		if (newcap == capacity) return;
		
		capacity = newcap;
		ptr = realloc(ptr, sizeof(T)*capacity);
	}
	
public:
	intarray()
	{
		ptr = NULL;
		len = 0;
		capacity = 0;
	}
	
	intarray(const intarray<T>& other)
	{
		ptr = NULL;
		capacity = 0;
		resize(other.len);
		len = other.len;
		memcpy(ptr, other.ptr, sizeof(T)*len);
	}
	
	~intarray() { free(ptr); }
	
	//Optimization - call this to reserve a chunk of space of 'len' entries. Calling append() with the same data will avoid a copy.
	//The length to append() may be smaller than here. If you don't want to append anything at all, it's fine to not call append().
	T* append_try(size_t len)
	{
		resize(this->len + len);
		return ptr+len;
	}
	
	void append(const T* data, size_t len)
	{
		if (data == ptr+len)
		{
			this->len += len;
			return;
		}
		
		resize(this->len + len);
		memcpy(ptr + this->len, data, sizeof(T)*len);
		this->len += len;
	}
	
	void prepend(const T* data, size_t len)
	{
		resize(this->len + len);
		memmove(ptr+len, ptr, sizeof(T)*this->len);
		memcpy(ptr, data, sizeof(T)*len);
		this->len += len;
	}
	
	void drop(int count)
	{
		memmove(ptr, ptr+count, sizeof(T)*(len-count));
		len -= count;
		resize(len);
	}
};

using bytearray = intarray<uint8_t>;
