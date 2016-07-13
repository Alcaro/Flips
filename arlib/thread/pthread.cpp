#include "thread.h"
#if defined(__unix__) && !defined(__linux__)
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//list of synchronization points: http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap04.html#tag_04_10

struct threaddata_pthread {
	function<void()> func;
};
static void * threadproc(void * userdata)
{
	struct threaddata_pthread * thdat=(struct threaddata_pthread*)userdata;
	thdat->func();
	free(thdat);
	return NULL;
}

void thread_create(function<void()> start)
{
	struct threaddata_pthread * thdat=malloc(sizeof(struct threaddata_pthread));
	thdat->func=start;
	pthread_t thread;
	if (pthread_create(&thread, NULL, threadproc, thdat)) abort();
	pthread_detach(thread);
}

unsigned int thread_num_cores()
{
	//for more OSes: https://qt.gitorious.org/qt/qt/source/HEAD:src/corelib/thread/qthread_unix.cpp#L411, idealThreadCount()
	//or http://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
	return sysconf(_SC_NPROCESSORS_ONLN);
}


mutex* mutex::create()
{
	pthread_mutex_t* ret=malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(ret, NULL);
	return (mutex*)ret;
}

void mutex::lock()
{
	pthread_mutex_lock((pthread_mutex_t*)this);
}

bool mutex::try_lock()
{
	return (pthread_mutex_trylock((pthread_mutex_t*)this)==0);
}

void mutex::unlock()
{
	pthread_mutex_unlock((pthread_mutex_t*)this); 
}

void mutex::release()
{
	pthread_mutex_destroy((pthread_mutex_t*)this);
	free(this);
}


//now I have to write futex code myself! How fun!
void mutex2::lock()
{
#error not implemented yet
}
bool mutex2::try_lock()
{
}
void mutex2::unlock()
{
}


event::event()
{
	this->data=malloc(sizeof(sem_t));
	sem_init((sem_t*)this->data, 0, 0);
}

event::~event()
{
	sem_destroy((sem_t*)this->data);
	free(this->data);
}

void event::signal()
{
	if (!this->signalled()) sem_post((sem_t*)this->data);
}

void event::wait()
{
	sem_wait((sem_t*)this->data);
}

bool event::signalled()
{
	int active;
	sem_getvalue((sem_t*)this->data, &active);
	return (active>0);
}


multievent::multievent()
{
	this->data=malloc(sizeof(sem_t));
	sem_init((sem_t*)this->data, 0, 0);
}

multievent::~multievent()
{
	sem_destroy((sem_t*)this->data);
	free(this->data);
}

void multievent::signal(unsigned int count)
{
	while (count--) sem_post((sem_t*)this->data);
}

void multievent::wait(unsigned int count)
{
	while (count--) sem_wait((sem_t*)this->data);
}

signed int multievent::count()
{
	int active;
	sem_getvalue((sem_t*)this->data, &active);
	return active;
}


uintptr_t thread_get_id()
{
	//disassembly:
	//jmpq   0x400500 <pthread_self@plt>
	//jmpq   *0x200b22(%rip)        # 0x601028 <pthread_self@got.plt>
	//mov    %fs:0x10,%rax
	//retq
	//(it's some big mess the first time, apparently the dependency is dynamically loaded)
	return pthread_self();
}


//pthread doesn't seem to contain anything like this, but gcc is the only supported compiler here, so I can use its builtins.
//or if I get any non-gcc compilers, I can throw in the C++11 threads. That's why these builtins exist, anyways.
//for Clang, if these GCC builtins aren't supported (most are), http://clang.llvm.org/docs/LanguageExtensions.html#c11-atomic-builtins
#if __GNUC__*10000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL__*1 >= 40700
//https://gcc.gnu.org/onlinedocs/gcc-4.7.0/gcc/_005f_005fatomic-Builtins.html
uint32_t lock_incr(uint32_t * val) { return __atomic_add_fetch(val, 1, __ATOMIC_ACQ_REL); }
uint32_t lock_decr(uint32_t * val) { return __atomic_sub_fetch(val, 1, __ATOMIC_ACQ_REL); }
uint32_t lock_read(uint32_t * val) { return __atomic_load_n(val, __ATOMIC_ACQUIRE); }

void* lock_read_i(void* * val) { return __atomic_load_n(val, __ATOMIC_ACQUIRE); }
void lock_write_i(void** val, void* newval) { return __atomic_store_n(val, newval, __ATOMIC_RELEASE); }
//there is a modern version of this, but it adds another move instruction for whatever reason and otherwise gives the same binary.
void* lock_write_eq_i(void** val, void* old, void* newval) { return __sync_val_compare_and_swap(val, old, newval); }
//void* lock_write_eq_i(void** val, void* old, void* newval)
//{
//	__atomic_compare_exchange_n(val, &old, newval, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
//	return old;
//}
void* lock_xchg_i(void** val, void* newval) { return __atomic_exchange_n(val, newval, __ATOMIC_ACQ_REL); }
#else
//https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html
uint32_t lock_incr(uint32_t * val) { return __sync_add_and_fetch(val, 1); }
uint32_t lock_decr(uint32_t * val) { return __sync_sub_and_fetch(val, 1); }
uint32_t lock_read(uint32_t * val) { return __sync_val_compare_and_swap(val, 0, 0); }

inline void* lock_read_i(void* * val) { return __sync_val_compare_and_swap(val, 0, 0); }
void lock_write_i(void** val, void* newval) { *val=newval; __sync_synchronize(); }
void* lock_write_eq_i(void** val, void* old, void* newval) { return __sync_val_compare_and_swap(val, old, newval); }

//no such thing - emulate it
void* lock_xchg_i(void** val, void* newval)
{
	void* prev=lock_read(val);
	while (true)
	{
		void* prev2=lock_write_eq(val, prev, newval);
		if (prev==prev2) break;
		else prev=prev2;
	}
}
#endif
#endif
