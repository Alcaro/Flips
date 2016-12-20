#pragma once
#include "global.h"

//error==0 means value exists and is valid, anything else means it doesn't
template<typename T> class maybe {
public:
	int error;
	union {
		char dummy; // don't demand a default-initialization on the value
		T value;
	};
	
	maybe(T&& data)
	{
		error = 0;
		new(&value) T(data);
	}
	maybe(null_t) : error(1) {}
	maybe(null_t, int error) : error(error) {}
	
	maybe(const maybe<T>& other)
	{
		error = other.value;
		if (error==0)
		{
			new(&value) T(other.value);
		}
	}
	maybe(maybe<T>&& other)
	{
		memcpy(this, &other, sizeof(*this));
		other.error=1;
	}
	~maybe()
	{
		if (error==0) value.~T();
	}
	
	explicit operator bool() { return !error; }
	bool operator!() { return !!error; }
};

template<> class maybe<void> {
public:
	int error;
	
	maybe(null_t) : error(1) {}
	maybe(null_t, int error) : error(error) {}
	
	explicit operator bool() { return !error; }
	bool operator!() { return !!error; }
};
