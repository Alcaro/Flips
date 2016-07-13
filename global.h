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
#endif
