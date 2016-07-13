#pragma once
#include "global.h"

//this is more thread.h than anything else - dylib is the only non-thread-related part. But there's
// no other place where dylib would fit, so os.h it is.

#ifdef __unix__
#define DYLIB_EXT ".so"
#define DYLIB_MAKE_NAME(name) "lib" name DYLIB_EXT
#endif
#ifdef _WIN32
#define DYLIB_EXT ".dll"
#define DYLIB_MAKE_NAME(name) name DYLIB_EXT
#endif

//Nasty stuff going on here... it's impossible to construct this object.
//The size varies per platform, so I have to allocate the object. This could be done by putting in a void* member,
// but that's a pointless level of indirection - instead, I cast the allocated value and return that!
//It's probably undefined, but the compiler won't be able to prove that, so it has to do what I want.
//Perhaps it would be better to let the configure script declare what the size is so they can have a
// member of type uint32_t data[12] and be constructed normally, but this is good enough for now.
class dylib : private nocopy {
	dylib(){}
public:
	static dylib* create(const char * filename, bool * owned=NULL);
	static const char * ext() { return DYLIB_EXT; }
	void* sym_ptr(const char * name);
	funcptr sym_func(const char * name);
	
	//per http://chadaustin.me/cppinterface.html - redirect operator delete to a function, this doesn't come from the normal allocator.
	static void operator delete(void* p) { if (p) ((dylib*)p)->release(); }
	void release();//this is the real destructor, you can use either this one or delete it
};

//If the program is run under a debugger, this triggers a breakpoint. If not, ignored.
void debug_break();
//If the program is run under a debugger, this triggers a breakpoint. The program is then terminated.
void debug_abort();
