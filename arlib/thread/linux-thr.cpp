#include "../endian.h"
#include "thread.h"
#ifdef __linux__
//I could try to rewrite all of this without pthread, but I'd rather not set up TLS stuff myself, that'd require replacing half of libc.
//However, I can remove everything except pthread_create.
//Minimum kernel version: 2.6.22 (FUTEX_PRIVATE_FLAG), released in 8 July, 2007 (source: http://kernelnewbies.org/LinuxVersions)
//Dropping the private mutex flag would drop requirements to 2.5.40, October 1, 2002.
#include <pthread.h>
#include <unistd.h>
#include <limits.h>

#include <linux/futex.h>
#include <sys/syscall.h>

#include "endian.h"

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

void thread_sleep(unsigned int usec)
{
	usleep(usec);
}


//spurious wakeups are possible
//return can tell if the wakeup is bogus, but I don't really need that
static int futex_wait(int * uaddr, int val, const struct timespec * timeout = NULL)
{
	return syscall(__NR_futex, uaddr, FUTEX_WAIT_PRIVATE, val, timeout);
}
static int futex_wake(int * uaddr)
{
	return syscall(__NR_futex, uaddr, FUTEX_WAKE_PRIVATE, 1);
}
static int futex_wake_all(int * uaddr)
{
	return syscall(__NR_futex, uaddr, FUTEX_WAKE_PRIVATE, INT_MAX);
}


//futexes. complex threading code. fun
#define MUT_UNLOCKED 0
#define MUT_LOCKED 1
#define MUT_CONTENDED 2

void mutex::lock()
{
	int result = lock_cmpxchg_acq(&fut, MUT_UNLOCKED, MUT_LOCKED);
	if (LIKELY(result == MUT_UNLOCKED))
	{
		return; // unlocked, fast path
	}
	
	//If it was locked, mark it contended and force whoever to wake us.
	//In the common contended case, it was previously MUT_LOCKED, so the futex would instantly return.
	//Therefore, the xchg should be run first.
	//loose is fine, since we already did an acquire above (and futex() probably performs a memory barrier).
	
	while (true)
	{
		result = lock_xchg_loose(&fut, MUT_CONTENDED);
		//results:
		//MUT_UNLOCKED - we got it, continue
		//MUT_CONTENDED - didn't get it, sleep for a while
		//MUT_LOCKED - someone else got it and locked it, thinking it's empty, while we're here. force it to wake us.
		if (result == MUT_UNLOCKED) break;
		
		futex_wait(&fut, MUT_CONTENDED);
	}
}

bool mutex::try_lock()
{
	return (lock_cmpxchg_acq(&fut, MUT_UNLOCKED, MUT_LOCKED) == MUT_UNLOCKED);
}

void mutex::unlock()
{
	int result = lock_xchg_rel(&fut, MUT_UNLOCKED);
	if (UNLIKELY(result == MUT_CONTENDED))
	{
		futex_wake(&fut);
	}
}



#define ONCE_NEW_I 0
#define ONCE_ONE_I 1
#define ONCE_CONTENDED_I 2
#define ONCE_NEW (void*)ONCE_NEW_I
#define ONCE_ONE (void*)ONCE_ONE_I
#define ONCE_CONTENDED (void*)ONCE_CONTENDED_I
//This is a fair bit shorter than the generic thread_once. And it doesn't have the objects-holding-up-each-other bug either.
//I could use Windows 8 WaitOnAddress for this, but I still (1) don't want to make 8-only binaries (2) don't have an 8.
//
//That is, it would be, if a futex was pointer rather than int. Ah well, at least it loses the bug.
void* thread_once_core(void* * item, function<void*()> calculate)
{
	void* rd = *item;
	//common case - initialized already
	//not using an atomic read because stale values are fine, they're caught by the cmpxchg
	if (rd!=ONCE_NEW && rd!=ONCE_ONE && rd!=ONCE_CONTENDED) return rd;
	
	void* old = lock_cmpxchg(item, ONCE_NEW, ONCE_ONE);
	if (old == ONCE_NEW)
	{
		void* result = calculate();
		//'item' is either ONE or CONTENDED here.
		//It's not NEW because we wrote ONE, and it can't be anything else
		// because the other threads know that they're only allowed to replace it with CONTENDED.
		if (lock_xchg(item, result) != ONCE_ONE)
		{
			futex_wake_all((ENDIAN==END_BIG)+(int*)item);
		}
		return result;
	}
	else if (old == ONCE_ONE || old == ONCE_CONTENDED)
	{
		lock_cmpxchg(item, ONCE_ONE, ONCE_CONTENDED);
		//the timeout is necessary so we don't risk deadlocks if
		//- we're on a 64bit platform
		//- calculate() returns (void*)0x????????00000002 (or, on a big endian system, 0x00000002????????)
		//- it's swapped in between cmpxchg(NEW->ONE) and the futex checks it
		//due to the extremely low likelihood of #2, and #3 also being pretty unlikely, the timeout is
		// set high (by computer standards), to 16ms.
		//poking ENDIAN like that is necessary for similar reasons.
		struct timespec timeout;
		timeout.tv_sec = 0;
		timeout.tv_nsec = 16*1000*1000;
		while (true)
		{
			futex_wait((ENDIAN==END_BIG)+(int*)item, ONCE_CONTENDED_I, &timeout);
			void* val = lock_read(item);
			if (val != ONCE_CONTENDED) return val;
		}
	}
	else return old;
}






//stuff I should rewrite follows
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


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
#endif
