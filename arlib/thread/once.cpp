#include "thread.h"

//a nonatomic read to an atomic variable is safe only if correct results are guaranteed if any old value is read
//a write of non-NULL and non-tag is guaranteed to be the final write, and if anything else seems to be there, we do an atomic read
void* thread_once_undo_core(void* * item, function<void*()> calculate, function<void(void*)> undo)
{
	if (*item) return *item;//nonatomic - if something weird happens, all that happens is that another item is created and deleted.
	void* obj = calculate();
	void* prev = lock_cmpxchg(item, NULL, obj);
	if (prev == NULL) return obj;
	else
	{
		undo(obj);
		return prev;
	}
}

#ifndef __linux__
static event* contention_unlocker=NULL;

#if 1 //if NULL==0 and points to a permanently reserved area of at least 3 bytes (the limit is 65536 on all modern OSes)
#define MAKE_TAG(n) ((void*)(n+1))
#else //assume sizeof(obj*)>=2 - no other thread can return this, they don't know where it is
#define MAKE_TAG(n) (void*)(((char*)&contention_unlocker)+n)
#endif
#define tag_busy MAKE_TAG(0)
#define tag_contended MAKE_TAG(1)

//Bug: If two objects are simultaneously initialized by two threads each, then one of the objects may hold up the other.
//This is not fixable without borrowing at least one bit from the item, which we don't want to do; alternatively waking all waiters, which can't be done either.
void* thread_once_core(void* * item, function<void*()> calculate)
{
	void* check=*item;
	//common case - initialized already
	//not using an atomic read because stale values are fine, they're caught by the cmpxchg
	if (check != NULL && check != tag_busy && check != tag_contended) return check;
	
	void* old = lock_cmpxchg(item, NULL, tag_busy);
	if (old == NULL)
	{
		void* result = calculate();
		//'written' is either tag_busy or tag_contended here.
		//It's not NULL because we wrote tag_busy, and it can't be anything else
		// because the other threads know that they're only allowed to replace it with tag_contended.
		if (lock_cmpxchg(item, tag_busy, result) == tag_contended)
		{
			thread_once_create(&contention_unlocker);
			lock_write(item, result);
			contention_unlocker->signal();
		}
	}
	else if (old == tag_busy || old == tag_contended)
	{
		//don't bother optimizing this, contention only happens a few times during program lifetime
		lock_cmpxchg(item, tag_busy, tag_contended);
		thread_once_create(&contention_unlocker);
		while (lock_read(item) == tag_busy) contention_unlocker->wait();
		contention_unlocker->signal();
	}
	//it's possible to hit neither of the above if the object was written between the initial read and the swap
	return *item;
}
#endif
