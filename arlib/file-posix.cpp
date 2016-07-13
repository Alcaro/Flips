#include "file.h"
#include "os.h"
#include "thread.h"

#define MMAP_THRESHOLD 128*1024

#ifdef __unix__
#include <unistd.h>
//#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>

//other platforms: http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe
const char * window_get_proc_path()
{
	//we could lstat it, but apparently that just returns zero on /proc on Linux.
	
	ssize_t bufsize=64;
	static char * linkname=NULL;
	if (linkname) return linkname;
	
	while (true)
	{
		linkname=malloc(bufsize);
		ssize_t r=readlink("/proc/self/exe", linkname, bufsize);
		if (r<0 || r>=bufsize)
		{
			free(linkname);
			if (r<0) return NULL;
			
			bufsize*=2;
			continue;
		}
		linkname[r]='\0';
		char * end=strrchr(linkname, '/');
		if (end) *end='\0';
		
		return linkname;
	}
}

static void window_cwd_enter(const char * dir);
static void window_cwd_leave();

char * _window_native_get_absolute_path(const char * basepath, const char * path, bool allow_up)
{
	if (!basepath || !path) return NULL;
	const char * filepart=strrchr(basepath, '/');
	if (!filepart) return NULL;
	char * basedir=strndup(basepath, filepart+1-basepath);
	
	window_cwd_enter(basedir);
	char * ret=realpath(path, NULL);
	window_cwd_leave();
	
	if (!allow_up && ret && strncasecmp(basedir, ret, filepart+1-basepath)!=0)
	{
		free(ret);
		ret=NULL;
	}
	free(basedir);
	
	return ret;
}

static const char * cwd_init;
static const char * cwd_bogus;
static mutex cwd_mutex;

static void window_cwd_enter(const char * dir)
{
	cwd_mutex.lock();
	char * cwd_bogus_check=getcwd(NULL, 0);
	if (strcmp(cwd_bogus, cwd_bogus_check)!=0) abort();//if this fires, someone changed the directory without us knowing - not allowed. cwd belongs to the frontend.
	free(cwd_bogus_check);
	ignore(chdir(dir));
}

static void window_cwd_leave()
{
	ignore(chdir(cwd_bogus));
	cwd_mutex.unlock();
}

const char * window_get_cwd()
{
	return cwd_init;
}

void _window_init_file()
{
	char * cwd_init_tmp=getcwd(NULL, 0);
	char * cwdend=strrchr(cwd_init_tmp, '/');
	if (!cwdend) cwd_init="/";
	else if (cwdend[1]=='/') cwd_init=cwd_init_tmp;
	else
	{
		size_t cwdlen=strlen(cwd_init_tmp);
		char * cwd_init_fixed=malloc(cwdlen+1+1);
		memcpy(cwd_init_fixed, cwd_init_tmp, cwdlen);
		cwd_init_fixed[cwdlen+0]='/';
		cwd_init_fixed[cwdlen+1]='\0';
		cwd_init=cwd_init_fixed;
		free(cwd_init_tmp);
	}
	
	//try a couple of useless directories and hope one of them works
	//this seems to be the best one:
	//- even root can't create files here
	//- it contains no files with a plausible name on a standard Ubuntu box (I have an ath9k-phy0, nothing will ever want that filename)
	//- a wild write will not do anything dangerous except turn on some lamps
	!chdir("/sys/class/leds/") ||
		//the rest are in case it's not accessible (weird chroot? not linux?), so try some random things
		!chdir("/sys/") ||
		!chdir("/dev/") ||
		!chdir("/home/") ||
		!chdir("/tmp/") ||
		!chdir("/");
	cwd_bogus = getcwd(NULL, 0);//POSIX does not specify getcwd(NULL), it's Linux-specific
}



/*
static void* file_alloc(int fd, size_t len, bool writable)
{
	if (len <= MMAP_THRESHOLD)
	{
		uint8_t* data=malloc(len+1);
		pread(fd, data, len, 0);
		data[len]='\0';
		return data;
	}
	else
	{
		void* data=mmap(NULL, len+1, writable ? (PROT_READ|PROT_WRITE) : PROT_READ, MAP_SHARED, fd, 0);
		if (data==MAP_FAILED) return NULL;
		
		if (len % sysconf(_SC_PAGESIZE) == 0)
		{
			mmap((char*)data + len, 1, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
		}
		return data;
	}
}
*/

static long pagesize() { return sysconf(_SC_PAGESIZE); }

namespace {
	class file_fs : public file {
		int fd;
	public:
		//do not use the file pointer, dup() doesn't clone that one
		file_fs(const char * filename, int fd, size_t len) : file(filename) { this->fd=fd; this->len=len; }
		
		file* clone() { return new file_fs(this->filename, dup(this->fd), this->len); }
		
		size_t read(void* target, size_t start, size_t len)
		{
			ssize_t ret = pread(fd, target, len, start);
			if (ret < 0) return 0;
			else return ret;
		}
		
		void* mmap(size_t start, size_t len)
		{
			size_t offset = start % pagesize();
			void* data=::mmap(NULL, len+offset, PROT_READ, MAP_SHARED, this->fd, start-offset);
			if (data==MAP_FAILED) return NULL;
			return (char*)data+offset;
		}
		
		void unmap(const void* data, size_t len)
		{
			size_t offset = (uintptr_t)data % pagesize();
			munmap((char*)data-offset, len+offset);
		}
		~file_fs() { close(fd); }
	};
}

file* file::create_fs(const char * filename)
{
	int fd=open(filename, O_RDONLY);
	if (fd<0) return NULL;
	
	struct stat st;
	if (fstat(fd, &st)<0) goto fail;
	
	return new file_fs(filename, fd, st.st_size);
	
fail:
	close(fd);
	return NULL;
}

#ifdef ARGUI_NONE
file* file::create(const char * filename)
{
	return create_fs(filename);
}
#endif

#if 0
namespace {
	class file_fs_wr : public filewrite {
	public:
		int fd;
		bool truncate;
		file_fs_wr(int fd) : fd(fd) {}
		
		/*private*/ void alloc(size_t size)
		{
			this->data=file_alloc(this->fd, size, true);
			this->len=size;
			if (this->data==NULL) abort();
		}
		
		/*private*/ void dealloc()
		{
			//no msync - munmap is guaranteed to do that already (and linux tracks dirty pages anyways)
			if (this->len <= MMAP_THRESHOLD)
			{
				pwrite(this->fd, this->data, this->len, 0);
				free(this->data);
			}
			else
			{
				munmap(this->data, this->len+1);
			}
		}
		
		bool resize(size_t newsize)
		{
			if (ftruncate(this->fd, newsize) < 0) return false;
			if (this->len < MMAP_THRESHOLD && newsize < MMAP_THRESHOLD)
			{
				this->len=newsize;
				uint8_t* data=realloc(this->data, newsize+1);
				data[newsize]='\0';
				this->data=data;
				return true;
			}
			dealloc();
			alloc(newsize);
			return true;
		}
		
		void sync()
		{
			if (this->truncate)
			{
				ftruncate(this->fd, this->len);
				this->truncate=false;
			}
			msync(this->data, this->len, MS_SYNC);//no MS_INVALIDATE because I can't figure out what it's supposed to do
			                                      //on linux, it does nothing whatsoever, except in some EINVAL handlers
		}
		
		~file_fs_wr()
		{
			sync();
			dealloc();
			close(this->fd);
		}
	};
};

filewrite* filewrite::create_fs(const char * filename, bool truncate)
{
	static const int oflags[]={ O_RDWR|O_CREAT, O_WRONLY|O_CREAT };
	int fd=open(filename, oflags[truncate], 0666);//umask defaults to turning this to 644
	if (fd<0) return NULL;
	
	if (truncate)
	{
		file_fs_wr* f=new file_fs_wr(fd);
		f->truncate=true;
		return f;
	}
	else
	{
		struct stat st;
		if (fstat(fd, &st)<0) goto fail;
		
		file_fs_wr* f; f=new file_fs_wr(fd);
		f->alloc(st.st_size);
		return f;
	}
	
fail:
	close(fd);
	return NULL;
}
#endif
#endif
