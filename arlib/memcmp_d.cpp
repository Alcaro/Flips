#include "global.h"

size_t memcmp_d(const void * a, const void * b, size_t len)
{
	const size_t alignmul = sizeof(uintptr_t);
	const size_t alignmask = alignmul-1;
	
	const uint8_t* ab = (uint8_t*)a;
	const uint8_t* bb = (uint8_t*)b;
	
	if (((uintptr_t)ab^(uintptr_t)bb)&alignmask || len<alignmul*2)
	{
		size_t i;
		for (i=0;i<len && ab[i]==bb[i];i++) {}
		return i;
	}
	size_t align = (-(uintptr_t)ab) & alignmask;
	for (size_t i=0;i<align;i++)
	{
		if (ab[i]!=bb[i]) return i;
	}
	
	size_t* aa = (size_t*)(ab+align);
	size_t* ba = (size_t*)(bb+align);
	size_t lena = (len-align)/alignmul;
	size_t i;
	for (i=0;i<lena;i++)
	{
		if (aa[i]!=ba[i]) break;
	}
	i *= alignmul;
	i += align;
	while (i<len && ab[i]==bb[i]) i++;
	return i;
}

//size_t memcmp_d(const void * a, const void * b, size_t len)
//{
	//const uint8_t* ab = (uint8_t*)a;
	//const uint8_t* bb = (uint8_t*)b;
	//
	//size_t i;
	//for (i=0;i<len && ab[i]==bb[i];i++) {}
	//return i;
//}

//size_t memcmp_d(const void * a, const void * b, size_t len)
//{
	//size_t r1 = memcmp_dfast(a, b, len);
	//size_t r2 = memcmp_dslow(a, b, len);
	//if (r1!=r2)
	//{
		//size_t r3 = memcmp_dfast(a, b, len);
		//printf("%zu:%zu:%zu\n",r1,r2,r3);
		//abort();
	//}
	//return r2;
//}
