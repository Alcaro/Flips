#include "file.h"
#include "os.h"

#ifdef _WIN32
#undef bind
#include <windows.h>
#define bind bind_func
#include <string.h>

//#define MMAP_THRESHOLD 32*1024

////other platforms: http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe
//const char * window_get_proc_path()
//{
//	//TODO: not thread safe
//	static char path[MAX_PATH];
//	GetModuleFileName(NULL, path, MAX_PATH);
//	for (int i=0;path[i];i++)
//	{
//		if (path[i]=='\\') path[i]='/';
//	}
//	char * end=strrchr(path, '/');
//	if (end) end[1]='\0';
//	return path;
//}

//char * _window_native_get_absolute_path(const char * basepath, const char * path, bool allow_up)
//{
//	if (!path || !basepath) return NULL;
//	
//	DWORD len=GetFullPathName(basepath, 0, NULL, NULL);
//	char * matchdir=malloc(len);
//	char * filepart;
//	GetFullPathName(basepath, len, matchdir, &filepart);
//	if (filepart) *filepart='\0';
//	window_cwd_enter(matchdir);
//	for (unsigned int i=0;matchdir[i];i++)
//	{
//		if (matchdir[i]=='\\') matchdir[i]='/';
//	}
//	
//	len=GetFullPathName(path, 0, NULL, NULL);
//	char * ret=malloc(len);
//	GetFullPathName(path, len, ret, NULL);
//	
//	window_cwd_leave();
//	
//	for (unsigned int i=0;i<len;i++)
//	{
//		if (ret[i]=='\\') ret[i]='/';
//	}
//	
//	if (!allow_up)
//	{
//		if (strncasecmp(matchdir, ret, strlen(matchdir))!=0)
//		{
//			free(matchdir);
//			free(ret);
//			return NULL;
//		}
//	}
//	free(matchdir);
//	
//	return ret;
//}

//static char * cwd_init;
//static char * cwd_bogus;
//static char * cwd_bogus_check;
//static DWORD cwd_bogus_check_len;
//static mutex cwd_lock;
//
//static void window_cwd_enter(const char * dir)
//{
//	cwd_lock.lock();
//	GetCurrentDirectory(cwd_bogus_check_len, cwd_bogus_check);
//	//if this fires, someone changed the directory without us knowing - not allowed. cwd belongs to the frontend.
//	if (strcmp(cwd_bogus, cwd_bogus_check)!=0) abort();
//	SetCurrentDirectory(dir);
//}
//
//static void window_cwd_leave()
//{
//	SetCurrentDirectory(cwd_bogus);
//	cwd_lock.unlock();
//}
//
//const char * window_get_cwd()
//{
//	return cwd_init;
//}

//void _window_init_file()
//{
//	DWORD len=GetCurrentDirectory(0, NULL);
//	cwd_init=malloc(len+1);
//	GetCurrentDirectory(len, cwd_init);
//	len=strlen(cwd_init);
//	for (unsigned int i=0;i<len;i++)
//	{
//		if (cwd_init[i]=='\\') cwd_init[i]='/';
//	}
//	if (cwd_init[len-1]!='/')
//	{
//		cwd_init[len+0]='/';
//		cwd_init[len+1]='\0';
//	}
//	
//	//try a couple of useless directories and hope one of them works
//	//(this code is downright Perl-like, but the alternative is a pile of ugly nesting)
//	SetCurrentDirectory("\\Users") ||
//	SetCurrentDirectory("\\Documents and Settings") ||
//	SetCurrentDirectory("\\Windows") ||
//	(SetCurrentDirectory("C:\\") && false) ||
//	SetCurrentDirectory("\\Users") ||
//	SetCurrentDirectory("\\Documents and Settings") ||
//	SetCurrentDirectory("\\Windows") ||
//	SetCurrentDirectory("\\");
//	
//	len=GetCurrentDirectory(0, NULL);
//	cwd_bogus=malloc(len);
//	cwd_bogus_check=malloc(len);
//	cwd_bogus_check_len=len;
//	GetCurrentDirectory(len, cwd_bogus);
//}



//static void* file_alloc(int fd, size_t len, bool writable)
//{

//}
//static size_t pagesize()
//{
//	SYSTEM_INFO inf;
//	GetSystemInfo(&inf);
//	return inf.dwPageSize;
//}
//static size_t allocgran()
//{
//	SYSTEM_INFO inf;
//	GetSystemInfo(&inf);
//	return inf.dwAllocationGranularity;
//}

namespace {
	class file_fs : public filewrite {
	public:
		HANDLE handle;
		
		file_fs(cstring filename, HANDLE handle) : filewrite(filename), handle(handle)
		{
			LARGE_INTEGER size;
			GetFileSizeEx(this->handle, &size);
			this->len = size.QuadPart;
		}
		
		/*private*/ void seek(size_t pos)
		{
			LARGE_INTEGER lipos;
			lipos.QuadPart = pos;
			SetFilePointerEx(this->handle, lipos, NULL, FILE_BEGIN);
		}
		
		size_t read(arrayvieww<byte> target, size_t start)
		{
			seek(start);
			DWORD actual;
			ReadFile(this->handle, target.ptr(), target.size(), &actual, NULL);
			return actual;
		}
		
		bool resize(size_t newsize)
		{
			seek(newsize);
			if (SetEndOfFile(this->handle))
			{
				this->len = newsize;
				return true;
			}
			else return false;
		}
		
		bool write(arrayview<byte> data, size_t start)
		{
			seek(start);
			DWORD actual;
			WriteFile(this->handle, data.ptr(), data.size(), &actual, NULL);
			if (actual==data.size())
			{
				this->len = max(this->len, start+data.size());
				return true;
			}
			else return false;
		}
		
		//stupid allocation granularity, its reason to exist (Alpha AXP) is long gone
		//and it never had a reason to exist outside Alpha anyways, porting to a new processor is always more than just recompiling
		//removing one of 9999 issues, especially one so rarely encountered as this, is not worth the trouble it causes
		//judging by https://blogs.msdn.microsoft.com/oldnewthing/20031008-00/?p=42223 ,
		// the allocation granularity is relevant for compiler/linker/etc authors only, who need to be aware of platform differences already
		//and even then, it's only relevant for file-backed executable pages, and as such, should only be enforced there
		//it does not belong in the kernel
		
		//as an example of said trouble, consider the case where I want to map a .txt file, but ensure it's NUL terminated
		//if the size is not a multiple of the page size, the remainder is automatically zeroed
		//if the size is a multiple of the allocation granularity, I can VirtualAlloc an anonymous page there
		// (this could race, but I'll just unmap that and try again)
		//but if the size is a multiple of page size but not alloc gran, I can't get rid of the trap pages.
		
		//or, more plausibly, consider the case of a program that wants to run on both Windows and Linux
		//the more differences, the harder, especially stupid ones like this
		
		//yes, I just wrote a 1KB rant about a single extra F in this mask
		/*private*/ const size_t mmap_gran_mask = 0xFFFF;
		
		/*private*/ arrayvieww<byte> mmap(bool write, size_t start, size_t len)
		{
			HANDLE mem = CreateFileMapping(handle, NULL, write ? PAGE_READWRITE : PAGE_READONLY, 0, 0, NULL);
			
			size_t round = (start&mmap_gran_mask);
			start &= ~mmap_gran_mask;
			
			byte* ptr = (byte*)MapViewOfFile(mem, write ? (FILE_MAP_READ|FILE_MAP_WRITE) : FILE_MAP_READ, start>>16>>16, start&0xFFFFFFFF, len+round);
			CloseHandle(mem);
			
			if (ptr) return arrayvieww<byte>(ptr+round, len);
			else return arrayvieww<byte>(NULL, 0);
		}
		
		arrayview<byte> mmap(size_t start, size_t len) { return mmap(false, start, len); }
		void unmap(arrayview<byte> data)
		{
			//docs say this should be identical to a MapViewOfFile return value, but it works fine with the low bits garbled
			UnmapViewOfFile(data.ptr());
		}
		
		arrayvieww<byte> mmapw(size_t start, size_t len) { return mmap(true, start, len); }
		void unmapw(arrayvieww<byte> data) { unmap(data); }
		
		~file_fs() { CloseHandle(handle); }
	};
}

#define FILE_SHARE_ALL (FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE)
file* file::open_fs(cstring filename)
{
	HANDLE file = CreateFile(filename, GENERIC_READ, FILE_SHARE_ALL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return NULL;
	return new file_fs(filename, file);
}

filewrite* filewrite::open_fs(cstring filename, mode m)
{
	DWORD dispositions[] = { OPEN_ALWAYS, OPEN_EXISTING, CREATE_ALWAYS, CREATE_NEW };
	HANDLE file = CreateFile(filename, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_ALL, NULL, dispositions[m], FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return NULL;
	return new file_fs(filename, file);
}

bool filewrite::unlink_fs(cstring filename)
{
	if (DeleteFile(filename)) return true;
	else return (GetLastError() == ERROR_FILE_NOT_FOUND);
}

//#ifdef ARGUI_NONE
file* file::open(cstring filename)
{
	return open_fs(filename);
}

filewrite* filewrite::open(cstring filename, mode m)
{
	return open_fs(filename, m);
}

bool filewrite::unlink(cstring filename)
{
	return unlink_fs(filename);
}
//#endif

void _window_init_file() {}
#endif
