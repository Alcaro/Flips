//Module name: libips
//Author: Alcaro
//Date: July 11, 2013
//Licence: GPL v3.0 or higher

#ifndef __cplusplus
#include <stdbool.h> //bool; if this does not exist, remove it and uncomment the following three lines.
//#define bool int
//#define true 1
//#define false 0
#endif
#include <stdlib.h> //malloc, realloc, free
#include <string.h> //memcpy, memset

#include "libips.h"

typedef unsigned char byte;

#define min(a, b) ((a)<(b) ? (a) : (b))
#define max(a, b) ((a)>(b) ? (a) : (b))
#define clamp(a, b, c) max(a, min(b, c))

struct ipsstudy {
	enum ipserror error;
	unsigned int outlen_min;
	unsigned int outlen_max;
	unsigned int outlen_min_mem;
};

enum ipserror ips_study(struct mem patch, struct ipsstudy * study)
{
	study->error = ips_invalid;
	if (patch.len < 8) return ips_invalid;
	const unsigned char * patchat = patch.ptr;
	const unsigned char * patchend = patchat + patch.len;
#define read8() ((patchat < patchend) ? (*patchat++) : 0)
#define read16() ((patchat+1 < patchend) ? (patchat += 2,(                      (patchat[-2] << 8) | patchat[-1])) : 0)
#define read24() ((patchat+2 < patchend) ? (patchat += 3,((patchat[-3] << 16) | (patchat[-2] << 8) | patchat[-1])) : 0)
	if (read8() != 'P' ||
	    read8() != 'A' ||
	    read8() != 'T' ||
	    read8() != 'C' ||
	    read8() != 'H')
	{
		return ips_invalid;
	}
	
	unsigned int offset = read24();
	unsigned int outlen = 0;
	unsigned int thisout = 0;
	//unsigned int lastoffset = 0;
	bool w_scrambled = false;
	while (offset != 0x454F46) // 454F46=EOF
	{
		unsigned int size = read16();
		if (size == 0)
		{
			size = read16();
			if (!size) return ips_invalid; // don't know what this means (65536 bytes? something else?),
			thisout = offset + size;       //   better reject it until I find out
			read8();
		}
		else
		{
			thisout = offset + size;
			patchat += size;
		}
		//turns out this messes up manually created patches. https://github.com/Alcaro/Flips/issues/13
		//if (offset < lastoffset) w_scrambled = true;
		//lastoffset = offset;
		if (thisout > outlen) outlen = thisout;
		if (patchat >= patchend) return ips_invalid;
		offset = read24();
	}
	study->outlen_min_mem = outlen;
	study->outlen_max = 0xFFFFFFFF;
	if (patchat+3 == patchend)
	{
		unsigned int truncate = read24();
		study->outlen_max = truncate;
		if (outlen > truncate)
		{
			outlen = truncate;
			w_scrambled = true;
		}
	}
	if (patchat != patchend) return ips_invalid;
	study->outlen_min = outlen;
#undef read8
#undef read16
#undef read24
	study->error = ips_ok;
	if (w_scrambled) study->error = ips_scrambled;
	return study->error;
}

enum ipserror ips_apply_study(struct mem patch, struct ipsstudy * study, struct mem in, struct mem * out)
{
	out->ptr = NULL;
	out->len = 0;
	if (study->error == ips_invalid) return study->error;
#define read8() (*patchat++)//guaranteed to not overflow at this point, we already checked the patch
#define read16() (patchat += 2,(                      (patchat[-2] << 8) | patchat[-1]))
#define read24() (patchat += 3,((patchat[-3] << 16) | (patchat[-2] << 8) | patchat[-1]))
	unsigned int outlen = clamp(study->outlen_min, in.len, study->outlen_max);
	out->ptr = (byte*)malloc(max(outlen, study->outlen_min_mem));
	out->len = outlen;
	
	bool anychanges = false;
	if (outlen != in.len) anychanges = true;
	
	if (out->len > in.len)
	{
		memcpy(out->ptr, in.ptr, in.len);
		memset(out->ptr + in.len, 0, out->len - in.len);
	}
	else memcpy(out->ptr, in.ptr, outlen);
	
	const unsigned char * patchat = patch.ptr+5;
	unsigned int offset = read24();
	while (offset != 0x454F46)
	{
		unsigned int size = read16();
		if (size == 0)
		{
			size = read16();
			//if (!size) return ips_invalid; // rejected in ips_study
			unsigned char b = read8();
			
			if (!anychanges && size && (out->ptr[offset] != b || memcmp(out->ptr + offset, out->ptr + offset, size - 1)))
				anychanges = true;
			
			memset(out->ptr + offset, b, size);
		}
		else
		{
			if (!anychanges && memcmp(out->ptr + offset, patchat, size))
				anychanges = true;
			
			memcpy(out->ptr + offset, patchat, size);
			patchat += size;
		}
		offset = read24();
	}
#undef read8
#undef read16
#undef read24
	
	//truncate data without this being needed is a poor idea
	if (study->outlen_max != 0xFFFFFFFF && in.len <= study->outlen_max)
		study->error = ips_notthis;
	
	if (!anychanges)
		study->error = ips_thisout;
	return study->error;
}

enum ipserror ips_apply(struct mem patch, struct mem in, struct mem * out)
{
	struct ipsstudy study;
	ips_study(patch, &study);
	return ips_apply_study(patch, &study, in, out);
}

//Known situations where this function does not generate an optimal patch:
//In:  80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80 80
//Out: FF FF FF FF FF FF FF FF 00 01 02 03 04 05 06 07 FF FF FF FF FF FF FF FF
//IPS: [         RLE         ] [        Copy         ] [         RLE         ]
//Possible improvement: RLE across the entire file, copy on top of that.
//Rationale: It would be a huge pain to create such a multi-pass tool if it should support writing a byte
//  more than twice, and I don't like half-assing stuff. It's also unlikely to apply to anything.


//Known improvements over LIPS:
//In:  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
//Out: FF 01 02 03 04 05 FF FF FF FF FF FF FF FF FF FF
//LIPS:[      Copy     ] [            RLE            ]
//Mine:[] [ Unchanged  ] [            RLE            ]
//Rationale: While LIPS can break early if it finds something RLEable in the middle of a block, it's not
//  smart enough to back off if there's something unchanged between the changed area and the RLEable spot.

//In:  FF FF FF FF FF FF FF
//Out: 00 00 00 00 01 02 03
//LIPS:[   RLE   ] [ Copy ]
//Mine:[       Copy       ]
//Rationale: Mistuned heuristics in LIPS.

//It is also known that I win in some other situations. I didn't bother checking which, though.

//There are no known cases where LIPS wins over libips.

enum ipserror ips_create(struct mem sourcemem, struct mem targetmem, struct mem * patchmem)
{
	unsigned int sourcelen = sourcemem.len;
	unsigned int targetlen = targetmem.len;
	const unsigned char * source = sourcemem.ptr;
	const unsigned char * target = targetmem.ptr;
	
	patchmem->ptr = NULL;
	patchmem->len = 0;
	
	if (targetlen > 16777216) return ips_16MB;
	if (targetlen >= 16777216 && sourcelen > targetlen) return ips_16MB; // can't truncate to exactly 16MB
	
	unsigned int offset = 0;
	unsigned int outbuflen = 4096;
	unsigned char * out = (byte*)malloc(outbuflen);
	unsigned int outlen = 0;
#define write8(val) do { \
		out[outlen++] = (val); \
		if (outlen == outbuflen) { outbuflen *= 2; out = (byte*)realloc(out, outbuflen); } \
		} while(0)
#define write16(val) do {                      write8((val) >> 8); write8((val)); } while(0)
#define write24(val) do { write8((val) >> 16); write8((val) >> 8); write8((val)); } while(0)
	write8('P');
	write8('A');
	write8('T');
	write8('C');
	write8('H');
	int lastknownchange = 0;
	//int forcewrite = (targetlen > sourcelen ? 1 : 0);
	while (offset < targetlen)
	{
		while (offset < sourcelen && (offset < sourcelen ? source[offset] : 0) == target[offset])
			offset++;
		//check how much we need to edit until it starts getting similar
		int thislen = 0;
		int consecutiveunchanged = 0;
		thislen = lastknownchange - offset;
		if (thislen < 0) thislen = 0;
		while (true)
		{
			unsigned int thisbyte = offset + thislen + consecutiveunchanged;
			if (thisbyte < sourcelen && (thisbyte < sourcelen ? source[thisbyte] : 0) == target[thisbyte])
			{
				consecutiveunchanged++;
			}
			else
			{
				thislen += consecutiveunchanged + 1;
				consecutiveunchanged = 0;
			}
			if (consecutiveunchanged >= 6 || thislen >= 65536) break;
		}
		
		//avoid premature EOF
		if (offset == 0x454F46)
		{
			offset--;
			thislen++;
		}
		
		lastknownchange = offset + thislen;
		if (thislen > 65535) thislen = 65535;
		if (offset + thislen > targetlen) thislen = targetlen - offset;
		if (offset == targetlen) continue;
		
		//check if RLE here is worthwhile
		int byteshere;
		for (byteshere=0; byteshere<thislen && target[offset] == target[offset+byteshere]; byteshere++) {}
		if (byteshere == thislen)
		{
			int thisbyte = target[offset];
			int i = 0;
			while (true)
			{
				unsigned int pos = offset + byteshere + i - 1;
				if (pos >= targetlen || target[pos] != thisbyte || byteshere + i > 65535)
					break;
				if (pos >= sourcelen || (pos < sourcelen ? source[pos] : 0) != thisbyte)
				{
					byteshere += i;
					thislen += i;
					i = 0;
				}
				i++;
			}
		}
		if ((byteshere > 8-5 && byteshere == thislen) || byteshere > 8)
		{
			write24(offset);
			write16(0);
			write16(byteshere);
			write8(target[offset]);
			offset += byteshere;
		}
		else
		{
			//check if we'd gain anything from ending the block early and switching to RLE
			int byteshere = 0;
			int stopat = 0;
			while (stopat + byteshere < thislen)
			{
				if (target[offset + stopat] == target[offset + stopat + byteshere])
				{
					byteshere++;
				}
				else
				{
					stopat += byteshere;
					byteshere = 0;
				}
				if (byteshere > 8+5 || //rle-worthy despite two ips headers
				   (byteshere > 8 && stopat + byteshere == thislen) || //rle-worthy at end of data
				   (byteshere > 8 && !memcmp(&target[offset +stopat + byteshere], //rle-worthy before another rle-worthy
				                             &target[offset +stopat + byteshere + 1],
				                             9-1)))
				{
					if (stopat) thislen = stopat;
					break; //we don't scan the entire block if we know we'll want to RLE, that'd gain nothing.
				}
			}
			//don't write unchanged bytes at the end of a block if we want to RLE the next couple of bytes
			if (offset + thislen != targetlen)
			{
				while (offset + thislen - 1 < sourcelen &&
				       target[offset + thislen - 1] == (offset + thislen - 1 < sourcelen ? source[offset + thislen - 1] : 0))
				{
					thislen--;
				}
			}
			
			if (thislen > 3 && !memcmp(&target[offset], &target[offset + 1], thislen - 1)) //still worth it?
			{
				write24(offset);
				write16(0);
				write16(thislen);
				write8(target[offset]);
			}
			else
			{
				write24(offset);
				write16(thislen);
				int i;
				for (i=0; i<thislen; i++)
					write8(target[offset + i]);
			}
			offset += thislen;
		}
	}
	write8('E');
	write8('O');
	write8('F');
	if (sourcelen > targetlen) write24(targetlen);
#undef write
	patchmem->ptr = out;
	patchmem->len = outlen;
	if (outlen == 8)
		return ips_identical;
	return ips_ok;
}

void ips_free(struct mem mem)
{
	free(mem.ptr);
}

#if 0
#warning Disable this in release versions.
#include <stdio.h>

//Congratulations, you found the undocumented feature! I don't think it's useful for anything except debugging libips, though.
void ips_dump(struct mem patch)
{
	if (patch.len < 8)
	{
		puts("Invalid");
		return;
	}
	const unsigned char * patchat = patch.ptr;
	const unsigned char * patchend = patchat + patch.len;
#define read8() ((patchat < patchend) ? (*patchat++) : 0)
#define read16() ((patchat+1 < patchend) ? (patchat += 2, (                      (patchat[-2] << 8) | patchat[-1])) : 0)
#define read24() ((patchat+2 < patchend) ? (patchat += 3, ((patchat[-3] << 16) | (patchat[-2] << 8) | patchat[-1])) : 0)
	if (read8() != 'P' ||
			read8() != 'A' ||
			read8() != 'T' ||
			read8() != 'C' ||
			read8() != 'H')
	{
		puts("Invalid");
		return;
	}
	int blockstart = patchat - patch.ptr;
	int offset = read24();
	int outlen = 0;
	int thisout = 0;
	while (offset != 0x454F46)
	{
		int size = read16();
		if (size == 0)
		{
			int rlelen = read16();
			thisout = offset + rlelen;
			printf("[%X] %X: %i (RLE)\n", blockstart, offset, rlelen);
			read8();
		}
		else
		{
			thisout = offset + size;
			printf("[%X] %X: %i\n", blockstart, offset, size);
			patchat += size;
		}
		if (thisout > outlen) outlen = thisout;
		if (patchat >= patchend)
		{
			puts("Invalid");
			return;
		}
		blockstart = patchat - patch.ptr;
		offset = read24();
	}
	printf("Expand to 0x%X\n", outlen);
	if (patchat + 3 == patchend)
	{
		int truncate = read24();
		printf("Truncate to 0x%X\n", truncate);
	}
	if (patchat != patchend) puts("Invalid");
#undef read8
#undef read16
#undef read24
}
#endif
