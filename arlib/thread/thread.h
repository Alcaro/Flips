#pragma once
#include "../global.h"

#ifdef ARLIB_THREAD
//Any data associated with this thread is freed once the thread procedure returns.
//It is safe to malloc() something in one thread and free() it in another.
//It is not safe to call window_run_*() from a thread other than the one entering main().
//A thread is rather heavy; for short-running jobs, use thread_create_short or thread_split.
void thread_create(function<void()> start);

//Returns the number of threads to create to utilize the system resources optimally.
unsigned int thread_num_cores();

#include "atomic.h"
#include <string.h>

//This is a simple tool that ensures only one thread is doing a certain action at a given moment.
//Memory barriers are inserted as appropriate. Any memory access done while holding a lock is finished while holding this lock.
//This means that if all access to an object is done exclusively while holding the lock, no further synchronization is needed.
//It is not allowed for a thread to call lock() or try_lock() while holding the lock already. It is not allowed
// for a thread to release the lock unless it holds it. It is not allowed to delete the lock while it's held.
//However, it it allowed to hold multiple locks simultaneously.
//lock() is not guaranteed to yield the CPU if it can't grab the lock. It may be implemented as a
// busy loop, or a hybrid scheme that spins a few times and then sleeps.
//Remember to create all relevant mutexes before creating a thread.
class mutex : nocopy {
#if defined(__linux__)
	int fut = 0;
	
public:
	//TODO: inline fast path
	void lock();
	bool try_lock();
	void unlock();
	
#elif defined(__unix__)
#error enable thread/pthread.cpp
#elif _WIN32_WINNT >= 0x0600
	
#if !defined(_MSC_VER) || _MSC_VER > 1600
	SRWLOCK srwlock = SRWLOCK_INIT;
#else
	// apparently MSVC2008 doesn't understand struct S item = {0}. let's do something it does understand and hope it's optimized out.
	SRWLOCK srwlock;
public:
	mutex() { srwlock.Ptr = NULL; } // and let's hope MS doesn't change the definition of RTL_SRWLOCK.
#endif
	//I could define a path for Windows 8+ that uses WaitOnAddress to shrink it to one single byte, but
	//(1) The more code paths, the more potential for bugs, especially the code paths I don't regularly test
	//(2) Saving seven bytes is pointless, a mutex is for protecting other resources and they're bigger
	//(3) Microsoft's implementation is probably better optimized
	//(4) I can't test it without a machine running 8 or higher, and I don't have that.
	
public:
	void lock() { AcquireSRWLockExclusive(&srwlock); }
	bool try_lock() { return TryAcquireSRWLockExclusive(&srwlock); }
	void unlock() { ReleaseSRWLockExclusive(&srwlock); }
	
#elif _WIN32_WINNT >= 0x0501
	
	CRITICAL_SECTION cs;
	
public:
	//yay, initializers. no real way to avoid them here.
	mutex() { InitializeCriticalSection(&cs); }
	void lock() { EnterCriticalSection(&cs); }
	bool try_lock() { return TryEnterCriticalSection(&cs); }
	void unlock() { LeaveCriticalSection(&cs); }
	~mutex() { DeleteCriticalSection(&cs); }
#endif
};


//Some shenanigans: gcc throws errors about strict-aliasing rules if I don't force its hand, and most
// implementations aren't correctly optimized (they leave copies on the stack).
//This is one of few that confuse the optimizer exactly as much as I want.
template<typename T> char* allow_alias(T* ptr) { return (char*)ptr; }

//Executes 'calculate' exactly once. The return value is stored in 'item'. If multiple threads call
// this simultaneously, none returns until calculate() is done.
//'item' must be initialized to NULL. calculate() must return a valid pointer to an object.
// 'return new mutex;' is valid, as is returning the address of something static.
//Non-pointers, such as (void*)1, are not allowed.
//Returns *item.
void* thread_once_core(void* * item, function<void*()> calculate);

template<typename T> T* thread_once(T* * item, function<T*()> calculate)
{
	return (T*)thread_once_core((void**)item, *(function<void*()>*)allow_alias(&calculate));
}

//This is like thread_once, but calculate() can be called multiple times. If this happens, undo()
//will be called for all except one; the last one will be returned.
void* thread_once_undo_core(void* * item, function<void*()> calculate, function<void(void*)> undo);

template<typename T> T* thread_once_undo(T* * item, function<T*()> calculate, function<void(T*)> undo)
{
	return (T*)thread_once_undo_core((void**)item,
	                                 *(function<void*()>*)allow_alias(&calculate),
	                                 *(function<void(void*)>*)allow_alias(&undo));
}


//This function is a workaround for a GCC bug. Don't call it yourself.
template<void*(*create)(), void(*undo)(void*)> void* thread_once_create_gccbug(void* * item)
{
	return thread_once_undo(item, bind(create), bind(undo));
}
//Simple convenience function, just calls the above.
template<typename T> T* thread_once_create(T* * item)
{
	return (T*)thread_once_create_gccbug<generic_new_void<T>, generic_delete_void<T> >((void**)item);
}


class mutexlocker : nocopy {
	mutexlocker();
	mutex* m;
public:
	mutexlocker(mutex* m) { this->m=m; this->m->lock(); }
	~mutexlocker() { this->m->unlock(); }
};
#define synchronized(mutex) with(mutexlocker LOCK(mutex))

//This one lets one thread wake another.
//The conceptual difference between this and a mutex is that while a mutex is intended to protect a
// shared resource from being accessed simultaneously, an event is intended to wait until another
// thread is done with something. A mutex is unlocked on the same thread as it's locked; an event is
// unlocked on a different thread.
//An example would be a producer-consumer scenario; if one thread is producing 200 items per second,
// and another thread processes them at 100 items per second, then there will soon be a lot of
// waiting items. An event allows the consumer to ask the producer to get to work, so it'll spend
// half of its time sleeping, instead of filling the system memory.
//An event is boolean; calling signal() twice will drop the extra signal. It is created in the unsignalled state.
//Can be used by multiple threads, but each of signal(), wait() and signalled() should only be used by one thread.
class event : nocopy {
public:
	event();
	~event();
	
	void signal();
	void wait();
	bool signalled();
	
private:
	void* data;
};

//This is like event, but it allows setting the event multiple times.
class multievent {
public:
	multievent();
	~multievent();
	
	//count is how many times to signal or wait. Calling it multiple times is equivalent to calling it with the sum of the arguments.
	void signal(unsigned int count=1);
	void wait(unsigned int count=1);
	//This is how many signals are waiting to be wait()ed for. Can be below zero if something is currently waiting for this event.
	//Alternate explaination: Increased for each entry to signal() and decreased for each entry to wait().
	signed int count();
private:
	void* data;
	signed int n_count;//Not used by all implementations.
};


void thread_sleep(unsigned int usec);

//Returns a value that's unique to the current thread for as long as the process lives. Does not
// necessarily have any relationship to OS-level thread IDs, but usually is.
//This just forwards to somewhere in libc or kernel32 or something, but it's so rarely called it doesn't matter.
size_t thread_get_id();

//This one creates 'count' threads, calls work() in each of them with 'id' from 0 to 'count'-1, and
// returns once each thread has returned.
//Unlike thread_create, thread_split is expected to be called often, for short-running tasks. The threads may be reused.
//It is safe to use the values 0 and 1. However, you should avoid going above thread_ideal_count().
void thread_split(unsigned int count, function<void(unsigned int id)> work);


//It is permitted to define this as (e.g.) QThreadStorage<T> rather than compiler magic.
//However, it must support operator=(T) and operator T(), so QThreadStorage is not directly usable. A subclass may be.
//An implementation must support all stdint.h types, all basic integral types (char, short, etc), and all pointers.
#ifdef __GNUC__
#define THREAD_LOCAL(t) __thread t
#endif
#ifdef _MSC_VER
#define THREAD_LOCAL(t) __declspec(thread) t
#endif

#else

//Some parts of arlib want to work with threads disabled.
class mutex : nocopy {
public:
	void lock() {}
	bool try_lock() { return true; }
	void unlock() { }
};

#endif
