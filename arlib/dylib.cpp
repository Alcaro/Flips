#include "os.h"
#include "thread.h"
#include <stdlib.h>

#ifdef __unix__
#include <dlfcn.h>

static mutex dylib_lock;

dylib* dylib::create(const char * filename, bool * owned)
{
	dylib_lock.lock();
	dylib* ret=NULL;
	
	if (owned)
	{
		ret=(dylib*)dlopen(filename, RTLD_LAZY|RTLD_NOLOAD);
		*owned=(!ret);
		if (ret) return ret;
	}
	if (!ret) ret=(dylib*)dlopen(filename, RTLD_LAZY);
	
	dylib_lock.unlock();
	return ret;
}

void* dylib::sym_ptr(const char * name)
{
	return dlsym((void*)this, name);
}

funcptr dylib::sym_func(const char * name)
{
	funcptr ret;
	*(void**)(&ret)=dlsym((void*)this, name);
	return ret;
}

void dylib::release()
{
	dlclose((void*)this);
}
#endif


#ifdef _WIN32
#undef bind
#include <windows.h>
#define bind bind_func

static mutex dylib_lock;

dylib* dylib::create(const char * filename, bool * owned)
{
	dylib_lock.lock();
	dylib* ret=NULL;
	
	if (owned)
	{
		if (!GetModuleHandleEx(0, filename, (HMODULE*)&ret)) ret=NULL;
		*owned=(!ret);
		//Windows may be able to force load a DLL twice using ntdll!LdrLoadDll
		// <https://github.com/wine-mirror/wine/blob/master/dlls/ntdll/loader.c#L2324>
		//but Linux can't, and calling ntdll is generally discouraged, so I'm not using that.
	}
	
	if (!ret)
	{
		//this is so weird dependencies, for example winpthread-1.dll, can be placed beside the dll where they belong
		char * filename_copy=strdup(filename);
		char * filename_copy_slash=strrchr(filename_copy, '/');
		if (!filename_copy_slash) filename_copy_slash=strrchr(filename_copy, '\0');
		filename_copy_slash[0]='\0';
		SetDllDirectory(filename_copy);
		free(filename_copy);
		
		ret=(dylib*)LoadLibrary(filename);
		SetDllDirectory(NULL);
	}
	
	dylib_lock.unlock();
	return ret;
}

void* dylib::sym_ptr(const char * name)
{
	return (void*)GetProcAddress((HMODULE)this, name);
}

funcptr dylib::sym_func(const char * name)
{
	return (funcptr)GetProcAddress((HMODULE)this, name);
}

void dylib::release()
{
	FreeLibrary((HMODULE)this);
}
#endif
