#include "global.h"
#undef malloc
#undef realloc
#undef calloc
#include <stdlib.h>

static void debug(void* ptr)
{
//static unsigned int g=0;
//printf("%i: %p\n",g++,ptr);
//if(g==1000)abort();
}

static void malloc_fail(size_t size)
{
	if (size > 0) printf("malloc failed, size %" PRIuPTR "\n", size);
	else puts("malloc failed, size unknown");
	abort();
}

anyptr malloc_check(size_t size)
{
	void* ret=malloc(size);
	debug(ret);
	if (size && !ret) malloc_fail(size);
	return ret;
}

anyptr try_malloc(size_t size)
{
	return malloc(size);
}

anyptr realloc_check(anyptr ptr, size_t size)
{
	void* ret=realloc(ptr, size);
	debug(ret);
	if (size && !ret) malloc_fail(size);
	return ret;
}

anyptr try_realloc(anyptr ptr, size_t size)
{
	return realloc(ptr, size);
}

anyptr calloc_check(size_t size, size_t count)
{
	void* ret=calloc(size, count);
	debug(ret);
	if (size && count && !ret) malloc_fail(size);
	return ret;
}

anyptr try_calloc(size_t size, size_t count)
{
	return calloc(size, count);
}

void malloc_assert(bool cond)
{
	if (!cond) malloc_fail(0);
}

void* operator new(size_t n) { return malloc_check(n); }
void* operator new[](size_t n) { return malloc_check(n); }
void operator delete(void * p) { free(p); }
extern "C" void __cxa_pure_virtual() { puts("__cxa_pure_virtual"); abort(); }
