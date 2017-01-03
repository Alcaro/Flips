#include "patch.h"

namespace patch {
type identify(const file& patch)
{
	byte head[16];
	size_t len = patch.read(arrayview<byte>(head));
	if (len>=5 && !memcmp(head, "PATCH", 5)) return t_ips;
	if (len>=4 && !memcmp(head, "UPS1", 4)) return t_ups;
	if (len>=4 && !memcmp(head, "BPS1", 4)) return t_bps;
	return t_unknown;
}

type identify_ext(cstring path)
{
	if (path.endswith(".ips")) return t_ips;
	if (path.endswith(".ups")) return t_ups;
	if (path.endswith(".bps")) return t_bps;
	return t_unknown;
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
