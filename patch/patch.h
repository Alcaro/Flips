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
//ups is worthless
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
class filebufreader {
	file& f;
	size_t fpos;
	
	array<byte> buf;
	size_t bufpos;
	
	uint32_t crc;
	
public:
	filebufreader(file& f) : f(f), fpos(0), bufpos(0), crc(0) {}
	arrayview<byte> peek(size_t bytes)
	{
		if (buf.size()-bufpos < bytes)
		{
			buf = buf.slice(bufpos, buf.size()-bufpos);
			bufpos = 0;
			size_t bytehave = buf.size();
			size_t byteread = bytes + 4096;
			buf.resize(bytehave + byteread);
			byteread = f.read(buf.slice(bytehave, byteread), fpos);
			fpos += byteread;
			buf.resize(bytehave + byteread);
		}
		return buf.slice(bufpos, min(buf.size()-bufpos, bytes));
	}
	arrayview<byte> read(size_t bytes)
	{
		arrayview<byte> ret = peek(bytes);
		if (ret.size() != bytes) return NULL;
		bufpos += bytes;
		crc = crc32_update(ret, crc); // TODO: perhaps it's faster if this one is calculated in large batches
		return ret;
	}
	byte read() { return read(1)[0]; }
	size_t remaining() { return buf.size()-bufpos + f.size()-fpos; }
	uint32_t crc32() { return crc; }
};
class streamreader {
	filebufreader f;
public:
	streamreader(file& f) : f(f) {}
	arrayview<byte> bytes(size_t n) { return f.read(n); }
	uint8_t u8()
	{
		return f.read(1)[0];
	}
	uint16_t u16()
	{
		arrayview<byte> b = f.read(2);
		return b[0] | b[1]<<8;
	}
	uint32_t u24()
	{
		arrayview<byte> b = f.read(3);
		return b[0] | b[1]<<8 | b[2]<<16;
	}
	uint32_t u32()
	{
		arrayview<byte> b = f.read(4);
		return b[0] | b[1]<<8 | b[2]<<16 | b[3]<<24;
	}
//	size_t bpsnum() // close to uleb128, but uleb lacks the +1 that ensures there's only one way to encode an integer
//	{
//		size_t ret = 0;
//		size_t shift = 0;
//		while (true)
//		{
//			uint8_t next = f.read();
//			if (SIZE_MAX>>shift < (next&0x7F)) return (size_t)-1;
//			size_t shifted = (next&0x7F)<<shift;
//
//#define assert_sum(a,b) do { if (SIZE_MAX-(a)<(b)) error(e_too_big); } while(0)
//#define assert_shift(a,b) do { if (SIZE_MAX>>(b)<(a)) error(e_too_big); } while(0)
//			
//		}
//#define decodeto(var) \
//				do { \
//					var=0; \
//					unsigned int shift=0; \
//					while (true) \
//					{ \
//						uint8_t next=readpatch8(); \
//						assert_shift(next&0x7F, shift); \
//						size_t addthis=(next&0x7F)<<shift; \
//						assert_sum(var, addthis); \
//						var+=addthis; \
//						if (next&0x80) break; \
//						shift+=7; \
//						assert_sum(var, 1U<<shift); \
//						var+=1<<shift; \
//					} \
//				} while(false)
//		
//		arrayview<byte> b = f.peek(16);
//	}
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
