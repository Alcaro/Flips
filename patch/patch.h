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
type identify_ext(cstring path);

enum result {
	e_ok,
	
	//You may get an output file along with some of these errors.
	//This is indistinguishable from zero-size output, but such patches are useless and rare anyways.
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

//All of these functions can be called with arrayview inputs and array& outputs, but
// they give lower memory use and/or better performance if you follow the listed types.
//For example, applying an IPS or UPS starts with copying the input file to the output;
// if you give them a file object directly, they'll read it straight from disk to the target buffer.

namespace ips {
result apply(arrayview<byte> patch, const file& in, array<byte>& out);
static inline result apply(arrayview<byte> patch, arrayview<byte> in, array<byte>& out) { return apply(patch, file::mem(in), out); }
result create(array<byte> source, arrayview<byte> target, array<byte>& patch);
}

namespace ups {
result apply(arrayview<byte> patch, const file& in, array<byte>& out);
static inline result apply(arrayview<byte> patch, arrayview<byte> in, array<byte>& out)
{
	file inf = file::mem(in);
	return apply(patch, inf, out);
}
//no need to implement this
//result create(const file& source, const file& target, file& patch);
}

namespace bps {
//metadata is extracted through info::parse
result apply(arrayview<byte> patch, arrayview<byte> source, array<byte>& target, bool accept_wrong_input = false);

//Because this one can take quite a long time, a progress meter is supplied. total is guaranteed to
//  be constant between every call until this function returns, done is guaranteed to increase
//  between each call, and done/total is an approximate percentage counter. Anything else is
//  undefined; for example, progress may or may not be called for done=0, progress may or may not be
//  called for done=total, done may or may not increase by the same amount between each call, and
//  the duration between each call may or may not be constant.
//To cancel patch creation, return true from the callback. It's safe to pass in NULL if you're not interested.
result create(const file& source, const file& target, const file& metadata, array<byte>& patch,
              function<bool(size_t done, size_t total)> progress);
static inline result create(arrayview<byte> source, const file& target, const file& metadata, array<byte>& patch,
              function<bool(size_t done, size_t total)> progress)
{
	return create(file::mem(source), target, metadata, patch, progress);
}
static inline result create(const file& source, arrayview<byte> target, const file& metadata, array<byte>& patch,
              function<bool(size_t done, size_t total)> progress)
{
	return create(source, file::mem(target), metadata, patch, progress);
}
static inline result create(arrayview<byte> source, arrayview<byte> target, const file& metadata, array<byte>& patch,
              function<bool(size_t done, size_t total)> progress)
{
	return create(file::mem(source), file::mem(target), metadata, patch, progress);
}

static inline result create(const file& source, const file& target, array<byte>& patch,
              function<bool(size_t done, size_t total)> progress)
{
	return create(source, target, file::mem(NULL), patch, progress);
}
static inline result create(arrayview<byte> source, const file& target, array<byte>& patch,
              function<bool(size_t done, size_t total)> progress)
{
	return create(file::mem(source), target, file::mem(NULL), patch, progress);
}
static inline result create(const file& source, arrayview<byte> target, array<byte>& patch,
              function<bool(size_t done, size_t total)> progress)
{
	return create(source, file::mem(target), file::mem(NULL), patch, progress);
}
static inline result create(arrayview<byte> source, arrayview<byte> target, array<byte>& patch,
              function<bool(size_t done, size_t total)> progress)
{
	return create(file::mem(source), file::mem(target), file::mem(NULL), patch, progress);
}

struct info {
	result parse(arrayview<byte> data, bool changefrac = false);
	
	size_t size_in;
	size_t size_out;
	
	uint32_t crc_in;
	uint32_t crc_out;
	
	arrayview<byte> metadata;
	
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
	bool signature(cstring sig)
	{
		bool ok = (memcmp(at, sig.bytes().ptr(), sig.length())==0);
		at+=sig.length();
		return ok;
	}
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
	uint16_t u16be()
	{
		arrayview<byte> b = bytes(2);
		return b[0]<<8 | b[1];
	}
	uint32_t u24()
	{
		arrayview<byte> b = bytes(3);
		return b[0] | b[1]<<8 | b[2]<<16;
	}
	uint32_t u24be()
	{
		arrayview<byte> b = bytes(3);
		return b[0]<<16 | b[1]<<8 | b[2];
	}
	uint32_t u32()
	{
		arrayview<byte> b = bytes(4);
		return b[0] | b[1]<<8 | b[2]<<16 | b[3]<<24;
	}
	uint32_t u32at(size_t pos)
	{
		const byte* b = start+pos;
		return b[0] | b[1]<<8 | b[2]<<16 | b[3]<<24;
	}
	size_t pos() { return at-start; }
	size_t size() { return end-start; }
	size_t remaining() { return end-at; }
	
	//if the bpsnum is too big, number of read bytes is undefined
	//does not do bounds checks, there must be at least 10 unread bytes in the buffer (possibly trimmed off from checksums)
	bool bpsnum(size_t* out);
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
	void write_xor(byte b)
	{
		buf[bufpos++] ^= b;
	}
	void write_skip(size_t bytes) { bufpos += bytes; }
	size_t pos() { return bufpos; }
	size_t size() { return buf.size(); }
	size_t remaining() { return buf.size()-bufpos; }
	uint32_t crc32()
	{
		crc = crc32_update(buf.slice(crcpos, bufpos-crcpos), crc);
		crcpos = bufpos;
		return crc;
	}
};
}
