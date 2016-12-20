#include "thread.h"

#ifdef ARLIB_THREAD
namespace {

//TODO: there is no procedure for destroying threads
struct threadpool {
	mutex lock;
	
	multievent* wake;
	multievent* started;
	uint32_t numthreads;
	uint32_t numidle;
	
	//these vary between each piece of work
	function<void(unsigned int id)> work;
	uint32_t id;
	multievent* done;
};

static struct threadpool * pool;

void threadproc(struct threadpool * This)
{
	while (true)
	{
		This->wake->wait();
		lock_decr(&This->numidle);
		
		function<void(unsigned int id)> work = This->work;
		unsigned int id = lock_incr(&This->id);
		multievent* done = This->done;
		
		This->started->signal();
		work(id);
		done->signal();
		lock_incr(&This->numidle);
	}
}

struct threadpool* pool_create()
{
	struct threadpool * pool = new threadpool;
	
	pool->wake=new multievent();
	pool->started=new multievent();
	pool->numthreads=0;
	pool->numidle=0;
	
	return pool;
}

void pool_delete(struct threadpool* pool)
{
	delete pool->wake;
	delete pool->started;
	delete pool;
}

}

void thread_split(unsigned int count, function<void(unsigned int id)> work)
{
	if (!count) return;
	if (count==1)
	{
		work(0);
		return;
	}
	struct threadpool * This = thread_once_undo(&pool, bind(pool_create), bind(pool_delete));
	
	This->lock.lock();
	multievent* done=new multievent();
	
	This->work=work;
	This->id=1;
	This->done=done;
	
	while (lock_read(&This->numidle) < count-1)
	{
		This->numthreads++;
		lock_incr(&This->numidle);
		thread_create(bind_ptr(threadproc, This));
	}
	
	This->wake->signal(count-1);
	This->started->wait(count-1);
	This->lock.unlock();
	
	work(0);
	
	done->wait(count-1);
	delete done;
}
#endif
