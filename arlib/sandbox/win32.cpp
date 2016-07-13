#ifdef _WIN32
#include "sandbox.h"
#include <windows.h>
#include <stdlib.h>

struct sandbox::impl {
	//handle type annotations per https://msdn.microsoft.com/en-us/library/windows/desktop/ms724515%28v=vs.85%29.aspx
	//child process control block is in shared memory, so parent can update it
	//parent control block is unshared (child isn't trusted, and handle values vary between processes)
	
	HANDLE child_handle; // process
	sandbox::impl* child_struct;
	
	void* shalloc_ptr[8];
	HANDLE shalloc_handle; // file mapping
	HANDLE shalloc_wake_parent; // event
	HANDLE shalloc_wake_child; // event
	
	HANDLE channel_handle[8]; // event
	
	void(*run)(sandbox* box);
	
	//if a sandbox is ever added, hijack ntdll!NtOpenFile and ntdll!NtCreateFile and send requests to a thread on the parent
	//use GetFinalPathNameByHandle if ObjectAttributes->RootDirectory is non-NULL
	//there are \??\ prefixes sometimes; according to <https://msdn.microsoft.com/en-us/library/windows/hardware/ff565384%28v=vs.85%29.aspx>,
	// it means "this is an absolute file path", somewhat like \\?\; I think \\?\ is only used in userspace
};

#define DupRaw(process, in, out) DuplicateHandle(GetCurrentProcess(), in, process, &out, 0, FALSE, DUPLICATE_SAME_ACCESS)
#define Dup(impl, name) DupRaw(impl->child_handle, impl->name, impl->child_struct->name)

static HANDLE shmem_par = NULL;

void sandbox::enter(int argc, char** argv)
{
	if (!shmem_par) return;
	
	sandbox::impl* box = (sandbox::impl*)MapViewOfFile(shmem_par, FILE_MAP_WRITE, 0,0, sizeof(sandbox::impl));
	
	sandbox boxw(box);
	
	//TODO: lockdown
	
	box->run(&boxw);
	exit(0);
}



static WCHAR* selfpathw()
{
	static WCHAR* ret = NULL;
	if (ret) return ret;
	
	DWORD len = MAX_PATH;
again:
	WCHAR* ptr = malloc(sizeof(WCHAR)*len);
	DWORD gmfnret = GetModuleFileNameW(NULL, ptr, len);
	if (!gmfnret || gmfnret==len)
	{
		free(ptr);
		len*=2;
		goto again;
	}
	
	//ensure thread safety
	WCHAR* prevret = (WCHAR*)InterlockedCompareExchangePointer((void**)&ret, ptr, NULL);
	if (prevret == NULL)
	{
		return ptr;
	}
	else
	{
		free(ptr);
		return prevret;
	}
}

sandbox* sandbox::create(const params* param)
{
	STARTUPINFOW sti;
	memset(&sti, 0, sizeof(sti));
	sti.cb=sizeof(sti);
	
	PROCESS_INFORMATION pi;
	CreateProcessW(selfpathw(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &sti, &pi);
	
	HANDLE shmem = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,sizeof(sandbox::impl), NULL);
	
	HANDLE shmem_tar;
	DupRaw(pi.hProcess, shmem, shmem_tar);
	SIZE_T ignore;
	WriteProcessMemory(pi.hProcess, &shmem_par, &shmem_tar, sizeof(HANDLE), &ignore);
	
	sandbox::impl* par = new sandbox::impl;
	memset(par, 0, sizeof(*par));
	sandbox::impl* chi = (sandbox::impl*)MapViewOfFile(shmem, FILE_MAP_WRITE, 0,0, sizeof(sandbox::impl));
	//<https://msdn.microsoft.com/en-us/library/windows/desktop/aa366537%28v=vs.85%29.aspx> says
	// The initial contents of the pages in a file mapping object backed by the operating system paging file are 0 (zero).
	//so no need to clear chi
	
	CloseHandle(shmem);
	par->child_handle = pi.hProcess;
	par->child_struct = chi;
	chi->run = param->run;
	
	for (int i=0;i<param->n_channel;i++)
	{
		par->channel_handle[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		Dup(par, channel_handle[i]);
	}
	
	par->shalloc_wake_parent = CreateEvent(NULL, FALSE, FALSE, NULL);
	Dup(par, shalloc_wake_parent);
	par->shalloc_wake_child = CreateEvent(NULL, FALSE, FALSE, NULL);
	Dup(par, shalloc_wake_child);
	
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	
	return new sandbox(par);
}

void sandbox::wait(int chan)
{
	WaitForSingleObject(m->channel_handle[chan], INFINITE);
}

bool sandbox::try_wait(int chan)
{
	return WaitForSingleObject(m->channel_handle[chan], 0)==WAIT_OBJECT_0;
}

void sandbox::release(int chan)
{
	SetEvent(m->channel_handle[chan]);
}

void* sandbox::shalloc(int index, size_t bytes)
{
	if (m->shalloc_ptr[index]) UnmapViewOfFile(m->shalloc_ptr[index]);
	
	void* ret;
	if (m->child_handle) // parent
	{
		m->shalloc_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,bytes, NULL);
		ret = MapViewOfFile(m->shalloc_handle, FILE_MAP_WRITE, 0,0, bytes);
		
		Dup(m, shalloc_handle);
		//<https://msdn.microsoft.com/en-us/library/windows/desktop/aa366882%28v=vs.85%29.aspx> says
		// Although an application may close the file handle used to create a file mapping object,
		// the system holds the corresponding file open until the last view of the file is unmapped.
		// Files for which the last view has not yet been unmapped are held open with no sharing restrictions.
		//making use of that will simplify stuff
		CloseHandle(m->shalloc_handle);
		
		SetEvent(m->shalloc_wake_child);
		WaitForSingleObject(m->shalloc_wake_parent, INFINITE);
		
		if (!m->child_struct->shalloc_handle)
		{
			UnmapViewOfFile(ret);
			ret=NULL;
		}
	}
	else // child
	{
		WaitForSingleObject(m->shalloc_wake_child, INFINITE);
		
		ret = MapViewOfFile(m->shalloc_handle, FILE_MAP_WRITE, 0,0, bytes);
		CloseHandle(m->shalloc_handle);
		if (!ret) m->shalloc_handle = NULL;
		SetEvent(m->shalloc_wake_parent);
	}
	
	m->shalloc_ptr[index] = ret;
	return ret;
}

sandbox::~sandbox()
{
	TerminateProcess(m->child_handle, 0);
	CloseHandle(m->child_handle);
	
	for (int i=0;i<8;i++)
	{
		if (m->channel_handle[i]) CloseHandle(m->channel_handle[i]);
	}
	for (int i=0;i<8;i++)
	{
		if (m->shalloc_ptr[i]) UnmapViewOfFile(m->shalloc_ptr[i]);
	}
	UnmapViewOfFile(m->child_struct);
	free(m);
}
#endif
