#include "thread.h"
#ifdef _WIN32
#undef bind
#include <windows.h>
#define bind bind_func
#include <stdlib.h>
#include <string.h>

//list of synchronization points: http://msdn.microsoft.com/en-us/library/windows/desktop/ms686355%28v=vs.85%29.aspx

struct threaddata_win32 {
	function<void()> func;
};
static DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	struct threaddata_win32 * thdat=(struct threaddata_win32*)lpParameter;
	thdat->func();
	free(thdat);
	return 0;
}
void thread_create(function<void()> start)
{
	struct threaddata_win32 * thdat=malloc(sizeof(struct threaddata_win32));
	thdat->func=start;
	
	//CreateThread is not listed as a synchronization point; it probably is, but I'd rather use a pointless
	// operation than risk a really annoying bug. It's lightweight compared to creating a thread, anyways.
	
	//MemoryBarrier();//gcc lacks this, and msvc lacks the gcc builtin I could use instead.
	//And of course my gcc supports only ten out of the 137 InterlockedXxx functions. Let's pick the simplest one...
	LONG ignored=0;
	InterlockedIncrement(&ignored);
	
	HANDLE h=CreateThread(NULL, 0, ThreadProc, thdat, 0, NULL);
	if (!h) abort();
	CloseHandle(h);
}

unsigned int thread_num_cores()
{
	SYSTEM_INFO sysinf;
	GetSystemInfo(&sysinf);
	return sysinf.dwNumberOfProcessors;
}

void thread_sleep(unsigned int usec)
{
	Sleep(usec/1000);
}



//rewritables follow
event::event() { data=(void*)CreateEvent(NULL, false, false, NULL); }
void event::signal() { SetEvent((HANDLE)this->data); }
void event::wait() { WaitForSingleObject((HANDLE)this->data, INFINITE); }
bool event::signalled() { if (WaitForSingleObject((HANDLE)this->data, 0)==WAIT_OBJECT_0) { SetEvent((HANDLE)this->data); return true; } else return false; }
event::~event() { CloseHandle((HANDLE)this->data); }


multievent::multievent()
{
	this->data=(void*)CreateSemaphore(NULL, 0, 127, NULL);
	this->n_count=0;
}

void multievent::signal(unsigned int count)
{
	InterlockedExchangeAdd((volatile LONG*)&this->n_count, count);
	ReleaseSemaphore((HANDLE)this->data, count, NULL);
}

void multievent::wait(unsigned int count)
{
	InterlockedExchangeAdd((volatile LONG*)&this->n_count, -(LONG)count);
	while (count)
	{
		WaitForSingleObject((HANDLE)this->data, INFINITE);
		count--;
	}
}

signed int multievent::count()
{
	return InterlockedCompareExchange((volatile LONG*)&this->n_count, 0, 0);
}

multievent::~multievent() { CloseHandle((HANDLE)this->data); }


uintptr_t thread_get_id()
{
	//disassembly:
	//call   *0x406118
	//jmp    0x76c11427 <KERNEL32!GetCurrentThreadId+7>
	//jmp    *0x76c1085c
	//mov    %fs:0x10,%eax
	//mov    0x24(%eax),%eax
	//ret
	return GetCurrentThreadId();
}
#endif
