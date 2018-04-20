//Module name: Floating IPS, command line frontend
//Author: Alcaro
//Date: See Git history
//Licence: GPL v3.0 or higher

#include "flips.h"

#ifdef FLIPS_CLI
file* file::create(const char * filename) { return file::create_libc(filename); }
filewrite* filewrite::create(const char * filename) { return filewrite::create_libc(filename); }


int main(int argc, char * argv[])
{
	return flipsmain(argc, argv);
}
#endif
