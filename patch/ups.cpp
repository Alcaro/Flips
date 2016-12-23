#include "patch.h"

namespace patch { namespace ups {
//TODO: HEAVY cleanups needed here

#define error(which) do { error=which; goto exit; } while(0)
result apply(const file& patch_, const file& source_, file& target_)
{
	if (patch_.size()<4+2+12) return e_broken;
	
	arrayview<byte> patchmem = patch_.mmap();
	memstream patch = patchmem;
	arrayview<byte> inmem = source_.mmap();
	memstream in = inmem;
	
	result error;
	
	if (true)
	{
#define decodeto(var) \
				do { \
					safeint<size_t> ret = patch.bpsnum(); \
					if (!ret.valid()) error(e_too_big); \
					var = ret.val(); \
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
		
		array<byte> outmem;
		outmem.resize(outlen);
		membufwriter out = outmem;
		
		while (patch.remaining() > 12)
		{
			size_t skip;
			decodeto(skip);
			size_t skip_fast = min(skip, outlen-out.size(), in.remaining());
			out.write(in.bytes(skip_fast));
			skip -= skip_fast;
			while (skip>0)
			{
				uint8_t outb = in.u8_or(0);
				if (out.size()<outlen) out.write(outb);
				skip--;
			}
			uint8_t tmp;
			do
			{
				tmp=patch.u8();
				uint8_t outb = in.u8_or(0);
				if (out.size()<outlen) out.write(outb^tmp);
				else if (outb != 0) error(e_broken);
			}
			while (tmp);
		}
		if (patch.remaining()!=12) error(e_broken);
		
		uint32_t crc_in=crc32(inmem);
		uint32_t crc_out=crc32(outmem);
		uint32_t crc_patch=crc32(patchmem.slice(0, patchmem.size()-4));
		
		uint32_t crc_in_expected=patch.u32();
		uint32_t crc_out_expected=patch.u32();
		uint32_t crc_patch_expected=patch.u32();
		
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
		
		patch_.unmap(patchmem);
		source_.unmap(inmem);
		target_.write(outmem);
		return e_ok;
#undef decodeto
	}
	
exit:
	patch_.unmap(patchmem);
	source_.unmap(inmem);
	return error;
}

#if 0
//Sorry, no undocumented features here. The only thing that can change an UPS patch is swapping the two sizes and checksums, and I don't create anyways.
#endif
}}
