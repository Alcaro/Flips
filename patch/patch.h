#pragma once
#include "../arlib.h"

namespace patch {
enum type {
	t_unknown,
	t_ips,
	t_ups,
	t_bps
};
type identify(const file& patch);

enum result {
	e_ok,
	
	//You may get an output file along with some of these errors.
	e_to_output,//You attempted to apply a patch to its output.
	e_not_this, //This is not the intended input file for this patch.
	e_damaged,  //The patch is technically valid, but seems scrambled or malformed.
	e_broken,   //This patch is not of the expected format, or it's malformed somehow.
	e_io,       //The patch could not be read.
	
	e_identical, //The input files are identical.
	e_too_big,   //The program (or the patch format) can't handle that big files.
	e_out_of_mem,//Memory allocation failure.
	e_canceled   //Patch creation callback said cancel.
};

namespace ips {
result apply(const file& patch, const file& source, file& target);
static inline result apply(const file& patch, const file& source, file&& target) { return apply(patch, source, (file&)target); }
result create(const file& source, const file& target, file& patch);
static inline result create(const file& source, const file& target, file&& patch) { return create(source, target, (file&)patch); }
}

namespace ups {
result apply(const file& patch, const file& source, file& target);
static inline result apply(const file& patch, const file& source, file&& target) { return apply(patch, source, (file&)target); }
//no need to implement this
//result create(const file& source, const file& target, file& patch);
}

namespace bps {
result apply(const file& patch, const file& source, file& target, bool accept_wrong_input = false);
static inline result apply(const file& patch, const file& source, file&& target, bool accept_wrong_input = false)
{
	return apply(patch, source, (file&)target, accept_wrong_input);
}
//Because this one can take quite a long time, a progress meter is supplied. total is guaranteed to
//  be constant between every call until this function returns, done is guaranteed to increase
//  between each call, and done/total is an approximate percentage counter. Anything else is
//  undefined; for example, progress may or may not be called for done=0, progress may or may not be
//  called for done=total, done may or may not increase by the same amount between each call, and
//  the duration between each call may or may not be constant.
//To cancel patch creation, return true from the callback. It's safe to pass in NULL if you're not interested.
result create(const file& source, const file& target, const file& metadata, file& patch,
              function<bool(size_t done, size_t total)> progress);
static inline result create(const file& source, const file& target, const file& metadata, file&& patch,
              function<bool(size_t done, size_t total)> progress)
{
	return create(source, target, metadata, (file&)patch, progress);
}
static inline result create(const file& source, const file& target, file& patch,
              function<bool(size_t done, size_t total)> progress)
{
	return create(source, target, file::mem(NULL), (file&)patch, progress);
}
static inline result create(const file& source, const file& target, file&& patch,
              function<bool(size_t done, size_t total)> progress)
{
	return create(source, target, (file&)patch, progress);
}

struct info {
	result parse(const file& patch, bool changefrac = false);
	
	size_t size_in;
	size_t size_out;
	
	uint32_t crc_in;
	uint32_t crc_out;
	
	array<byte> metadata;
	
	//Tells approximately how much of the input ROM is changed compared to the output ROM.
	//It's quite heuristic. The algorithm may change with or without notice.
	//As of writing, I believe this is accurate to 2 significant digits in base 10.
	//It's also more expensive to calculate than the other data, so it's optional.
	//If you don't want it, their values are undefined.
	//The denominator is always guaranteed nonzero, even if something else says it's undefined.
	//Note that this can return success for invalid patches.
	size_t change_num;
	size_t change_denom;
};
}

//Used for patch application.
class memstream {
	const byte* start;
	const byte* at;
	const byte* end;
	
	//arrayview<byte> buf;
	//size_t pos;
public:
	memstream(arrayview<byte> buf) : start(buf.ptr()), at(buf.ptr()), end(buf.ptr()+buf.size()) {}
	arrayview<byte> bytes(size_t n) { arrayview<byte> ret = arrayview<byte>(at, n); at+=n; return ret; }
	uint8_t u8()
	{
		return *(at++);
	}
	uint8_t u8_or(uint8_t otherwise)
	{
		if (at==end) return otherwise;
		return *(at++);
	}
	uint16_t u16()
	{
		arrayview<byte> b = bytes(2);
		return b[0] | b[1]<<8;
	}
	uint32_t u24()
	{
		arrayview<byte> b = bytes(3);
		return b[0] | b[1]<<8 | b[2]<<16;
	}
	uint32_t u32()
	{
		arrayview<byte> b = bytes(4);
		return b[0] | b[1]<<8 | b[2]<<16 | b[3]<<24;
	}
	size_t size() { return end-start; }
	size_t remaining() { return end-at; }
	
	//if the bpsnum is too big, number of read bytes is undefined
	//does not do bounds checks, there must be at least 10 unread bytes in the buffer
	safeint<size_t> bpsnum()
	{
		//similar to uleb128, but uleb lacks the +1 that ensures there's only one way to encode an integer
		uint8_t first = u8();
		if (LIKELY(first&0x80)) return first&0x7F;
		
		safeint<size_t> ret = 0;
		safeint<size_t> shift = 0;
		while (true)
		{
			shift+=7;
			ret+=1<<shift;
			
			uint8_t next = u8();
			safeint<size_t> shifted = (next&0x7F)<<shift;
			ret+=shifted;
			if (next&0x80 || !ret.valid()) break;
		}
		return ret;
	}
};

class filebufwriter {
	file& f;
	size_t fpos;
	
	array<byte> buf;
	size_t totalbytes;
	
	uint32_t crc;
	
	void flush()
	{
		crc = crc32_update(buf, crc);
		f.write(buf, fpos);
		fpos += buf.size();
		buf.reset();
	}
	
public:
	filebufwriter(file& f) : f(f), fpos(0), totalbytes(0), crc(0) {}
	void write(arrayview<byte> bytes)
	{
		buf += bytes;
		totalbytes += bytes.size();
		if (buf.size() > 65536) flush();
	}
	void write(byte b)
	{
		buf.append(b);
		totalbytes++;
		if (buf.size() > 65536) flush();
	}
	size_t size() { return totalbytes; }
	uint32_t crc32() { flush(); return crc; }
	void cancel() { f.resize(0); }
};
class membufwriter {
	arrayvieww<byte> buf;
	size_t bufpos;
	
	uint32_t crc;
	size_t crcpos;
	
public:
	membufwriter(arrayvieww<byte> buf) : buf(buf), bufpos(0), crc(0), crcpos(0) {}
	void write(arrayview<byte> bytes)
	{
		memcpy(buf.slice(bufpos, buf.size()-bufpos).ptr(), bytes.ptr(), bytes.size());
		bufpos += bytes.size();
	}
	void write(byte b)
	{
		buf[bufpos++] = b;
	}
	size_t size() { return bufpos; }
	uint32_t crc32()
	{
		crc = crc32_update(buf.slice(crcpos, bufpos-crcpos), crc);
		crcpos = bufpos;
		return crc;
	}
};

//Deprecated
struct mem {
	mem() : ptr(NULL), len(0) {}
	mem(uint8_t* ptr, size_t len) : ptr(ptr), len(len) {}
	mem(arrayview<byte> v) : ptr((byte*)v.ptr()), len(v.size()) {}
	arrayvieww<byte> v() { return arrayvieww<byte>(ptr, len); }
	uint8_t * ptr;
	size_t len;
};
}
