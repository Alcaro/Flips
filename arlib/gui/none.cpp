#include "window.h"
#include "../file.h"
#include "../os.h"
#ifdef ARGUI_MINIMAL
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

void window_init(int * argc, char * * argv[]) {}

uint64_t window_get_time()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec*1000000 + ts.tv_nsec/1000;
}

file* file::create(const char * filename) { return create_fs(filename); }
#endif
