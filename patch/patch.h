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
result create(const file& source, const file& target, file& patch);
}

namespace ups {
result apply(const file& patch, const file& source, file& target);
//ups is worthless
//result create(const file& source, const file& target, file& patch);
}

namespace bps {
result apply(const file& patch, const file& source, file& target, bool accept_wrong_input);
//Because this one can take quite a long time, a progress meter is supplied. total is guaranteed to
//  be constant between every call until this function returns, done is guaranteed to increase
//  between each call, and done/total is an approximate percentage counter. Anything else is
//  undefined; for example, progress may or may not be called for done=0, progress may or may not be
//  called for done=total, done may or may not increase by the same amount between each call, and
//  the duration between each call may or may not be constant.
//To cancel patch creation, return true from the callback. It's safe to pass in NULL if you're not interested.
result create(const file& source, const file& target, const file& metadata, file& patch,
              function<bool(size_t done, size_t total)> progress);

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
