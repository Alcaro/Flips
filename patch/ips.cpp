#include "patch.h"

namespace patch { namespace ips {

result apply(arrayview<byte> patchmem, const file& in, array<byte>& out)
{
	result error = e_ok;
	memstream patch = patchmem;
	
	if (patch.size()<8 || !patch.signature("PATCH")) return e_broken;
	out = in.read();
	
	bool anychanges = false;
	uint32_t lastoffset = 0;
#define error(which) do { error=which; goto exit; } while(0)
	while (true)
	{
		uint32_t offset = patch.u24be();
		if (offset == 0x454F46) break;
		
		if (offset < lastoffset) error = e_damaged;
		lastoffset = offset;
		
		if (patch.remaining() < 2+1+3) error(e_broken);
		uint32_t size = patch.u16be();
		
		if (size==0)
		{
			if (patch.remaining() < 2+1+3) error(e_broken);
			size = patch.u16be();
			uint8_t b = patch.u8();
			if (!size) error(e_broken); // don't know if this is defined, probably isn't
			
			out.reserve(offset+size);
			if (!anychanges &&
				(out[offset]!=b || out.slice(offset, size-1).ptr()!=out.slice(offset+1, size-1).ptr()))
			{
				anychanges = true;
			}
			
			memset(out.slice(offset, size).ptr(), b, size);
		}
		else
		{
			if (patch.remaining() < size+3) error(e_broken);
			
			out.reserve(offset+size);
			arrayview<byte> newdat = patch.bytes(size);
			if (!anychanges && newdat!=out.slice(offset, size)) anychanges = true;
			memcpy(out.slice(offset, size).ptr(), newdat.ptr(), newdat.size());
		}
	}
	if (patch.remaining()==3)
	{
		uint32_t newsize = patch.u24be();
		if (newsize >= in.size() && !error) error = e_not_this;
		out.resize(newsize);
	}
	if (patch.remaining()!=0) error = e_damaged;
	if (!anychanges && in.size()==out.size() && error != e_damaged) error = e_to_output;
	return error;
	
exit:
	out.resize(0);
	return error;
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

//There are no known cases where LIPS wins over this.

result create(array<byte> sourcea, arrayview<byte> target, array<byte>& patchmem)
{
	int truesourcelen=sourcea.size();
	int targetlen=target.size();
	sourcea.resize(target.size());
	arrayview<byte> source = sourcea;
	//const unsigned char * source=sourcemem.ptr();
	//const unsigned char * target=targetmem.ptr();
	
	if (targetlen>=16777216) return e_too_big;
	
	int offset=0;
	
	array<byte> out;
	out.append('P');
	out.append('A');
	out.append('T');
	out.append('C');
	out.append('H');
	int lastknownchange=0;
	int lastwritten=0;
#define ENC24(n) (byte)((n)>>16), (byte)((n)>>8), (byte)((n)>>0)
#define ENC16(n) (byte)((n)>>8), (byte)((n)>>0)
#define ENC8(n) (byte)((n)>>0)
	while (offset<targetlen)
	{
		//skip unchanged bytes
		offset += memcmp_d(source.slice(offset, targetlen-offset).ptr(),
		                   target.slice(offset, targetlen-offset).ptr(),
		                   targetlen-offset);
		
		//how many bytes to edit
		int thislen=0;
		thislen=lastknownchange-offset; // cache results of this loop
		if (thislen<0) thislen=0;
		
		int searchat = offset+thislen;
		int unchangedtimer = 6;
		//int consecutiveunchanged=0;
		int stop = min(targetlen, offset+65535);
		
		while (searchat < stop)
		{
			byte b = source[searchat];
			if (target[searchat++] == b)
			{
				unchangedtimer--;
				if (!unchangedtimer) break;
			}
			else unchangedtimer=6;
		}
		thislen = searchat-offset-6+unchangedtimer;
		
		//avoid premature EOF
		if (offset==0x454F46)
		{
			offset--;
			thislen++;
		}
		
		lastknownchange=offset+thislen;
		
		if (offset+thislen>targetlen) thislen=targetlen-offset;
		if (offset==targetlen) continue;
		
		//check if starting a RLE here is worthwhile
		int byteshere;
		for (byteshere=0;byteshere<thislen && target[offset]==target[offset+byteshere];byteshere++) {}
		if (byteshere==thislen)
		{
			int thisbyte=target[offset];
			int i=0;
			while (true)
			{
				int pos=offset+byteshere+i-1;
				if (pos>=targetlen || target[pos]!=thisbyte || byteshere+i>65535) break;
				if (pos>=truesourcelen || source[pos]!=thisbyte)
				{
					byteshere+=i;
					thislen+=i;
					i=0;
				}
				i++;
			}
		}
		if ((byteshere>8-5 && byteshere==thislen) || byteshere>8)
		{
			byte bytes[] = { ENC24(offset), ENC16(0), ENC16(byteshere), target[offset] };
			out += arrayview<byte>(bytes);
			offset+=byteshere;
			lastwritten=offset;
		}
		else
		{
			//check if we'd gain anything from ending the block early and switching to RLE
			//int byteshere=0;
			//int stopat=0;
			
			//RLE is a win if
			//- 13 consecutive bytes are same
			//- 8 consecutive bytes are same, followed by 8 consecutive but different bytes (two RLE blocks)
			//- 8 consecutive bytes are same, followed by end of the changed block
			//for all of them, unchanged (compared to input) bytes before, after or between them count towards said 8 or 13 bytes
			//however, counting unchanged bytes isn't implemented; it only gave me losses and infinite loops when I tried
			//nor is 8+8, seems too rare to hit
			
			int rlestart = 0;
			int rlebyte = -1;
			for (int i=0;i<thislen;i++)
			{
				if (target[offset+i] != rlebyte) rlestart = i;
				rlebyte = target[offset+i];
				if (i-rlestart >= 12)
				{
					thislen=rlestart;
					break;
				}
			}
			if (thislen-rlestart >= 8)
			{
				thislen=rlestart;
			}
			
			//don't copy unchanged bytes at the end of a block
			if (offset+thislen!=targetlen)
			{
				while (thislen>1 && target[offset+thislen-1]==source[offset+thislen-1])
				{
					thislen--;
				}
			}
			
			//avoid infinite loops if an RLEable block starts at 'EOF'
			if (offset+thislen == 0x454F46)
			{
				if (thislen==0xFFFF) thislen--;
				else thislen++;
			}
			
			if (thislen>3 && !memcmp(&target[offset], &target[offset+1], thislen-1))//can we switch to RLE for these few bytes?
			{
				byte bytes[] = { ENC24(offset), ENC16(0), ENC16(thislen), target[offset] };
				out += arrayview<byte>(bytes);
			}
			else
			{
				byte bytes[] = { ENC24(offset), ENC16(thislen) };
				out += arrayview<byte>(bytes);
				out += target.slice(offset, thislen);
			}
			offset+=thislen;
			lastwritten=offset;
		}
	}
	
	if (truesourcelen<targetlen && lastwritten!=targetlen)
	{
		if (targetlen-1==0x454F46)
		{
			byte bytes[] = { ENC24(targetlen-2), ENC16(2), target[targetlen-2], target[targetlen-1] };
			out += arrayview<byte>(bytes);
		}
		else
		{
			byte bytes[] = { ENC24(targetlen-1), ENC16(1), target[targetlen-1] };
			out += arrayview<byte>(bytes);
		}
	}
	byte bytes[] = { 'E', 'O', 'F', ENC24(targetlen) };
	if (truesourcelen>targetlen) out+=arrayview<byte>(bytes, 6);
	else out+=arrayview<byte>(bytes, 3);
#undef ENC8
#undef ENC16
#undef ENC24
	patchmem = std::move(out);
	if (patchmem.size()==8) return e_identical;
	return e_ok;
}

#if 0
#warning Disable this in release versions.
#include <stdio.h>

//Congratulations, you found the undocumented feature! I don't think it's useful for anything except debugging this, though.
void ips_dump(struct mem patch)
{
	if (patch.len<8)
	{
		puts("Invalid");
		return;
	}
	const unsigned char * patchat=patch.ptr;
	const unsigned char * patchend=patchat+patch.len;
#define read8() ((patchat<patchend)?(*patchat++):0)
#define read16() ((patchat+1<patchend)?(patchat+=2,((patchat[-2]<<8)|patchat[-1])):0)
#define read24() ((patchat+2<patchend)?(patchat+=3,((patchat[-3]<<16)|(patchat[-2]<<8)|patchat[-1])):0)
	if (read8()!='P' ||
			read8()!='A' ||
			read8()!='T' ||
			read8()!='C' ||
			read8()!='H')
	{
		puts("Invalid");
		return;
	}
	int blockstart=patchat-patch.ptr;
	int offset=read24();
	int outlen=0;
	int thisout=0;
	while (offset!=0x454F46)//454F46=EOF
	{
		int size=read16();
		if (size==0)
		{
			int rlelen=read16();
			thisout=offset+rlelen;
			printf("[%X] %X: %i (RLE)\n", blockstart, offset, rlelen);
			read8();
		}
		else
		{
			thisout=offset+size;
			printf("[%X] %X: %i\n", blockstart, offset, size);
			patchat+=size;
		}
		if (thisout>outlen) outlen=thisout;
		if (patchat>=patchend)
		{
			puts("Invalid");
			return;
		}
		blockstart=patchat-patch.ptr;
		offset=read24();
	}
	printf("Expand to 0x%X\n", outlen);
	if (patchat+3==patchend)
	{
		int truncate=read24();
		printf("Truncate to 0x%X\n", truncate);
	}
	if (patchat!=patchend) puts("Invalid");
#undef read8
#undef read16
#undef read24
}
#endif
}}
