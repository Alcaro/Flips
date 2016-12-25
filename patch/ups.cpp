#include "patch.h"

namespace patch { namespace ups {
//TODO: HEAVY cleanups needed here

result apply(arrayview<byte> patchmem, const file& in, array<byte>& outmem)
{
	if (patchmem.size()<4+2+12) return e_broken;
	
	memstream patch = patchmem.slice(0, patchmem.size()-12);
	result error;
	
	if (true)
	{
#define error(which) do { error=which; goto exit; } while(0)
#define decodeto(var) \
				do { \
					if (!patch.bpsnum(var)) error(e_too_big); \
				} while(false)
		
		bool backwards=false;
		
		if (patch.u8()!='U') error(e_broken);
		if (patch.u8()!='P') error(e_broken);
		if (patch.u8()!='S') error(e_broken);
		if (patch.u8()!='1') error(e_broken);
		
		size_t inlen;
		size_t outlen;
		decodeto(inlen);
		decodeto(outlen);
		if (inlen!=in.size())
		{
			size_t tmp=inlen;
			inlen=outlen;
			outlen=tmp;
			backwards=true;
		}
		if (inlen!=in.size()) error(e_not_this);
		
		outmem = in.read();
		uint32_t crc_in = crc32(outmem);
		outmem.resize(outlen);
		membufwriter out = outmem;
		
		while (patch.remaining())
		{
			size_t skip;
			decodeto(skip);
			out.write_skip(min(skip, out.remaining()));
			
			uint8_t tmp;
			do
			{
				tmp=patch.u8();
				if (out.remaining()) out.write_xor(tmp);
				//else if (in[outpos] != tmp) error(e_broken);
				//can't do the above without mmapping the input, and doing that just for error checking is a waste of time
			}
			while (tmp);
		}
		
		//uint32_t crc_in; // done elsewhere
		uint32_t crc_out=crc32(outmem);
		uint32_t crc_patch=crc32(patchmem.slice(0, patchmem.size()-4));
		
		memstream checks = patchmem.slice(patchmem.size()-12, 12);
		uint32_t crc_in_expected=checks.u32();
		uint32_t crc_out_expected=checks.u32();
		uint32_t crc_patch_expected=checks.u32();
		
		if (inlen==outlen)
		{
			if ((crc_in!=crc_in_expected || crc_out!=crc_out_expected) &&
			    (crc_in!=crc_out_expected || crc_out!=crc_in_expected))
			{
				error(e_not_this);
			}
		}
		else
		{
			if (!backwards)
			{
				if (crc_in!=crc_in_expected) error(e_not_this);
				if (crc_out!=crc_out_expected) error(e_not_this);
			}
			else
			{
				if (crc_in!=crc_out_expected) error(e_not_this);
				if (crc_out!=crc_in_expected) error(e_not_this);
			}
		}
		if (crc_patch!=crc_patch_expected) error(e_broken);
		
		return e_ok;
#undef decodeto
#undef error
	}
	
exit:
	outmem.resize(0);
	return error;
}

#if 0
//Sorry, no undocumented features here. UPS is a very restricted format; for any source/target file pair,
// the ONLY flexibility the patcher has is to swap the two sizes and checksums. Any other change makes the patch broken.
//And I don't create anyways, so I have no reason to compare UPS patches.
#endif
}}
