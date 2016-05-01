//Module name: Floating IPS, global header
//Author: Alcaro
//Date: June 18, 2015
//Licence: GPL v3.0 or higher

#ifndef struct_mem
#define struct_mem

//the standard library can be assumed to exist
#include <stddef.h>//size_t, SIZE_MAX
#include <stdint.h>//uint8_t

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

struct mem {
	uint8_t * ptr;
	size_t len;
};

#if defined(FLIPS_WINDOWS)
#define LPCWSTR const wchar_t *
#else
#define LPCWSTR const char *
#endif

//used by both Flips core/GUI and the BPS creator
class file {
public:
	static file* create(LPCWSTR filename);
	
	virtual size_t len() = 0;
	virtual bool read(uint8_t* target, size_t start, size_t len) = 0;
	
	static struct mem read(LPCWSTR filename); // provided by Flips core
	struct mem read(); // provided by Flips core
	
	virtual ~file() {}
};

class filewrite {
public:
	static filewrite* create(LPCWSTR filename);
	
	virtual bool append(const uint8_t* data, size_t len) = 0;
	
	static bool write(LPCWSTR filename, struct mem data); // provided by Flips core
	
	virtual ~filewrite() {}
};

#endif
