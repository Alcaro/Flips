//Module name: Floating IPS, command line frontend
//Author: Alcaro
//Date: See Git history
//Licence: GPL v3.0 or higher

#include "flips.h"

#ifdef FLIPS_CLI
file* file::create(const char * filename) { return file::create_libc(filename); }
filewrite* filewrite::create(const char * filename) { return filewrite::create_libc(filename); }
filemap* filemap::create(const char * filename) { return filemap::create_fallback(filename); }


int main(int argc, char * argv[])
{
	char buf[16];
	sprintf(buf, "Welcome to %s", argv[0]);
	puts(buf);
	return flipsmain(argc, argv);
}
#endif
