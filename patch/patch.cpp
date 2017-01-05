#include "patch.h"

namespace patch {
type identify(const file& patch)
{
	byte head[16];
	size_t len = patch.read(arrayview<byte>(head));
	return identify(arrayview<byte>(head, len));
}

type identify(arrayview<byte> patch)
{
	if (patch.size()>=5 && !memcmp(patch.ptr(), "PATCH", 5)) return ty_ips;
	if (patch.size()>=4 && !memcmp(patch.ptr(), "UPS1", 4)) return ty_ups;
	if (patch.size()>=4 && !memcmp(patch.ptr(), "BPS1", 4)) return ty_bps;
	return ty_unknown;
}

type identify_ext(cstring path)
{
	if (path.endswith(".ips")) return ty_ips;
	if (path.endswith(".ups")) return ty_ups;
	if (path.endswith(".bps")) return ty_bps;
	return ty_unknown;
}

bool memstream::bpsnum(size_t* out)
{
	//similar to uleb128, but bpsnum adds another 1<<shift for every byte except the first
	//this ensures there's only one way to encode an integer
	//(other than -0, but this one decodes only unsigned)
	
	//heavily optimized, so it looks a bit weird
	uint8_t b = *(at++);
	if (LIKELY(b&0x80))
	{
		*out = b ^ (1<<7);
		return true;
	}
	size_t tmp = b;
	b = *(at++);
	tmp |= b<<7;
	if (LIKELY(b&0x80))
	{
		*out = tmp + (1<<7) - (1<<7<<7);
		return true;
	}
	
	//these weird subtractions and additions wouldn't be needed if the 0x80 bits were inverted
	//but I can't change the BPS spec, so they'll have to stay
	size_t ret = tmp + (1<<7) + (1<<7<<7);
	size_t shift = 7+7;
	while (true)
	{
		uint8_t next = *(at++);
		if (safeint<size_t>::lslov(next^0x80, shift, &tmp)) return false;
		if (safeint<size_t>::addov(ret, tmp, &ret)) return false;
		if (next&0x80) break;
		shift+=7;
	}
	*out = ret;
	return true;
}
}
