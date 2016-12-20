#pragma once
#include "../global.h"
#include "../string.h"

//Allows safely executing untrusted code.
//
//Exact rules:
// If the child process is running hostile code, the parent may deadlock or _exit(), but may not crash or use an invalid handle.
// Failing an access check in the child yields undefined behavior (subject to the above, of course).
// It can return EACCES, kill the process, or whatever.
//
//WARNING: There is NO SECURITY WHATSOEVER on Windows. A "sandboxed" process can do anything the parent can.
//Windows provides plenty of ways to restrict a process, but
//- Most of them are blacklists, disabling a particular privilege; I want whitelists
//- There are so many resource kinds I can't keep track how to restrict everything, or even list
//    them; it will also fail open if a new resource kind is introduced
//- Many lockdown functions temporarily disable privileges, rather than completely delete them
//- There's little or no documentation on which privileges are required for the operations I need
//- The lockdown functions are often annoying to call, involving variable-width arrays in
//    structures, and LCIDs that likely vary between reboots
//And I cannot trust such a system. I only trust whitelists, like Linux seccomp.
//Even Chrome couldn't find anything comprehensive; they use everything they can find (restricted token, job object, desktop,
// SetProcessMitigationPolicy, firewall, etc), but some operations, such as accessing FAT32 volumes, still pass through.
//It feels like the Windows sandboxing functions are designed for trusted code operating on untrusted data, rather than untrusted code.
//Since I can't create satisfactory results in such an environment, I won't even try.
//
//The Linux implementation is, of course, secure. It has a solid whitelist mechanism (seccomp),
// denying access to creating any and all unauthorized resources - and even using authorized
// resources in an unauthorized way. I don't even need to drop privileges, it won't get a chance to
// use them.
//Documentation is also excellent; source code is available everywhere, all syscalls (both Linux-only and Unix-global)
// are well documented, and if I miss something, strace quickly tells me what.
//
//On Linux, this requires exclusive control over SIGSYS in the child process.
//According to <http://lxr.free-electrons.com/ident?i=SIGSYS>, kernel version 4.6, all uses are either seccomp,
// hardware events on rare platforms that won't be delivered to me, or catching / passing on the signal (as opposed to raising it).
// kill(2) can, of course, also send SIGSYS, but that's rare.
//Therefore, this requirement is safe.
//
//Chrome sandbox entry points: http://stackoverflow.com/questions/1590337/using-the-google-chrome-sandbox

class sandbox : nocopy {
public:
	//Must be the first thing in main(), before window_init() and similar.
	//If the process is created via sandbox::create, this doesn't return. Otherwise, it does nothing.
	static void enter(int argc, char** argv);
	
	//TODO: half of those aren't used
	struct params {
		enum flags_t {
			//Always allowed: Allocate memory, synchronization operations, terminate self.
			
			//If true, stdout and stderr go to the same places as in the parent. If false, /dev/null.
			allow_stdout = 1,
			
			////Creates a sandbox facade; it acts like a normal sandbox, but the child process isn't
			////  restricted. It could even be a thread in the same process.
			//no_security = 2,
		};
		unsigned int flags;
		
		//The operating system decides how memory is counted and how to round this.
		//0 means unlimited; anything else is in bytes.
		size_t max_mem;
		
		//Tells how many synchronization channels and shared memory regions are available in this sandbox.
		//Both must be 8 or less.
		int n_channel;
		int n_shmem;
		
		//TODO: replace with some kind of stringlist thing
		//If the allow_file flag is not set, this is called when the child tries to open a file.
		//If it returns true, the request is granted. If false, or if the function is NULL, the child's attempt fails with EACCES.
		//Can be called at any time when a function is executed on this sandbox object. The sandbox
		//  manager creates a thread to handle such requests.
		//function<bool(const char *, bool write)> file_access;
		
		//Since this goes cross-process, passing a normal userdata won't work. Instead, it's provided by the sandbox object, via shalloc.
		void(*run)(sandbox* box);
	};
	//The params object is used only during this call. You can free it afterwards.
	static sandbox* create(const params* param);
	
	//Used to synchronize the two processes. 'chan' can be any number 0 through 7. Negative channels may be used internally.
	//While this may look like a mutex, it's also usable as event; release and wait can be in different processes.
	//Each channel may only be used by one thread on each side.
	//The channels start in the locked state.
	void wait(int chan);
	bool try_wait(int chan);
	void release(int chan);
	
	//Allows synchronized(box->channel(1)) {}.
	struct channel_t
	{
		sandbox* parent; int id;
		void lock() { parent->wait(id); }
		bool try_lock() { return parent->try_wait(id); }
		void unlock() { parent->release(id); }
	};
	channel_t channel(int id) { channel_t ret; ret.parent=this; ret.id=id; return ret; }
	
	//Allocates memory shared between the two processes. At least 8 memory areas are supported. It's
	// up to the user how to use them; a recommendation is to put a fixed-size control data block in
	// area 0, and put variable-size stuff in other areas.
	//Rules (optional if you make reasonable assumptions):
	// Both processes must call this; don't share pointers directly, they may vary between the processes.
	// The function is synchronous; neither process will return until the other has entered it. If one
	//  fails, the other does too. For this reason, the two processes must perform the same sequence
	//  of calls; they must also agree on the sizes.
	// You can resize a memory area by calling this function again with the same index. However,
	//  unlike realloc, the new contents are implementation defined. Whether it fails or succeeds, the
	//  old shared area is deleted.
	// The implementation must ensure the parent does not crash even if the child's internal
	//  structures are corrupt, including but not limited to size mismatch.
	// This function is not thread safe. Only one thread per side may enter.
	// It is implementation defined which processes are charged for these bytes. It could be parent,
	//  child, a little of each, both, neither, or something weirder.
	void* shalloc(int index, size_t bytes);
	
	//Convenience function, just calls the above.
	template<typename T> T* shalloc(int index, size_t count=1) { return (T*)this->shalloc(index, sizeof(T)*count); }
	
	//This will be called if the child process attempts to open a file, but the attempt is rejected by policy.
	//If the return value from this callback is not equal to -1, that will be returned as a file handle.
	//It is safe to call accept_fd() from this function.
	//On Windows, the intptr_t is a casted HANDLE. On Linux, int.
	void set_fopen_fallback(function<intptr_t(cstring path, bool write)> callback); // Child only.
	
	//Clones a file handle into the child. The handle remains open in the parent. The child may get another ID.
	//Like shalloc(), both processes are allowed to sleep until the other enters.
	void give_fd(intptr_t fd); // Parent only.
	intptr_t accept_fd(); // Child only.
	
	//This forcibly terminates the child process. If called from the child, undefined behaviour; instead, call exit() or return from run().
	//Also unmaps all shared memory.
	~sandbox();
	
private:
	struct impl;
	impl * m;
	
private:
	sandbox(){}
	sandbox(impl* m) : m(m) {}
};
