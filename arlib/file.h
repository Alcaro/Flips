#pragma once
#include "global.h"
#include <string.h>

//These are implemented by the window manager; however, due to file operations being far more common than GUI, they're split off.

//Returns the working directory at the time of process launch.
//The true working directory is set to something unusable, and the program may not change or use it.
const char * window_get_cwd();

//Returns the process path, without the filename. Multiple calls will return the same pointer.
const char * window_get_proc_path();
//Converts a relative path (../roms/mario.smc) to an absolute path (/home/admin/roms/mario.smc).
// Implemented by the window manager, so gvfs can be supported. If the file doesn't exist, it is
// implementation defined whether the return value is a nonexistent path, or if it's NULL.
//basepath is the directory you want to use as base, or a file in this directory. NULL means current
// directory as set by window_cwd_enter, or that 'path' is expected absolute.
//If allow_up is false, NULL will be returned if 'path' attempts to go up the directory tree (for example ../../../../../etc/passwd).
//If path is absolute already, it will be returned (possibly canonicalized) if allow_up is true, or rejected otherwise.
//Send it to free() once it's done.
char * window_get_absolute_path(const char * basepath, const char * path, bool allow_up);
//Converts any file path to something accessible on the local file system. The resulting path can
// be both ugly and temporary, so only use it for file I/O, and store the absolute path instead.
//It is not guaranteed that window_get_absolute_path can return the original path, or anything useful at all, if given the output of this.
//It can return NULL, even for paths which file_read understands. If it doesn't, use free() when you're done.
char * window_get_native_path(const char * path);

class file : nocopy {
private:
	file(){}
protected:
	file(const char * filename) : filename(strdup(filename)) {}
	file(const char * filename, size_t(len)) : filename(strdup(filename)), len(len) {}
	
	//This one will create the file from the filesystem.
	//create() can simply return create_fs(filename), or can additionally support stuff like gvfs.
	static file* create_fs(const char * filename);
	
	class mem;
public:
	
	static file* create(const char * filename);
	
	char* filename;
	size_t len;
	
	//The returned object is guaranteed equivalent to the given one, assuming the file is not changed or removed in the meanwhile.
	virtual file* clone() { return file::create(this->filename); }
	
	virtual size_t read(void* target, size_t start, size_t len) = 0;
	size_t read(void* target, size_t len) { return this->read(target, 0, len); }
	virtual void* mmap(size_t start, size_t len) = 0;
	void* mmap() { return this->mmap(0, this->len); }
	virtual void unmap(const void* data, size_t len) = 0;
	
	virtual ~file() { free(filename); }
};

class file::mem : public file {
	void* data;
public:
	mem(const char * filename, void* data, size_t len) : file(filename) { this->data=data; this->len=len; }
	
	file* clone()
	{
		void* newdat=malloc(this->len);
		memcpy(newdat, this->data, this->len);
		return new file::mem(this->filename, newdat, this->len);
	}
	
	size_t read(void* target, size_t start, size_t len) { memcpy(target, (char*)data+start, len); return len; }
	void* mmap(size_t start, size_t len) { return (char*)data+start; }
	void unmap(const void* data, size_t len) {}
	~mem() { free(data); }
};

//virtual bool resize(size_t newsize) { return false; }
////Sends all the data to the disk. Does not return until it's there.
////The destructor also sends the data to disk, but does not guarantee that it's done immediately.
////There may be more ways to send the file to disk, but this is not guaranteed either.
//virtual void sync(){}

//These are implemented by the window manager, despite looking somewhat unrelated.
//Support for absolute filenames is present.
//Support for relative filenames will be rejected as much as possible. However, ../../../../../etc/passwd may work.
//Other things, for example http://example.com/roms/snes/smw.sfc, may work too.
//Directory separator is '/', extension separator is '.'.
//file_read appends a '\0' to the output (whether the file is text or binary); this is not reported in the length.
//Use free() on the return value from file_read().
bool file_read(const char * filename, void* * data, size_t * len);
bool file_write(const char * filename, const anyptr data, size_t len);
bool file_read_to(const char * filename, anyptr data, size_t len);//If size differs, this one fails.

//Some simple wrappers for the above three.
inline bool file_read_rel(const char * basepath, bool allow_up, const char * filename, void* * data, size_t * len)
{
	char* path=window_get_absolute_path(basepath, filename, allow_up);
	if (!path) return false;
	bool ret=file_read(path, data, len);
	free(path);
	return ret;
}

inline bool file_write_rel(const char * basepath, bool allow_up, const char * filename, const anyptr data, size_t len)
{
	char* path=window_get_absolute_path(basepath, filename, allow_up);
	if (!path) return false;
	bool ret=file_write(path, data, len);
	free(path);
	return ret;
}

inline bool file_read_to_rel(const char * basepath, bool allow_up, const char * filename, anyptr data, size_t len)
{
	char* path=window_get_absolute_path(basepath, filename, allow_up);
	if (!path) return false;
	bool ret=file_read_to(path, data, len);
	free(path);
	return ret;
}

//These will list the contents of a directory. The returned paths from window_find_next should be
// sent to free(). The . and .. components will not be included; however, symlinks and other loops
// are not guarded against. It is implementation defined whether hidden files are included. The
// returned filenames are relative to the original path and contain no path information nor leading
// or trailing slashes.
void* file_find_create(const char * path);
bool file_find_next(void* find, char* * path, bool * isdir);
void file_find_close(void* find);

//If the window manager does not implement any non-native paths (like gvfs), it can use this one;
// it's implemented by something that knows the local file system, but not the window manager.
//There is no _window_native_get_native_path; since the local file system doesn't understand
// anything except the local file system, it would only be able to return the input, or be
// equivalent to _window_native_get_absolute_path, making it redundant and therefore useless.
char * _window_native_get_absolute_path(const char * basepath, const char * path, bool allow_up);

void _window_init_file();


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//The above was defined before Arlib was split off from minir, and is unlikely to still work.
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


//TODO before all of this: create string class
//TODO before all of this: create container classes (let API roughly mirror C#)
//TODO: file_find should return an array
//TODO: create a path sanitizer that canonicalizes a path
//TODO: create a path verifier that checks if a path is within a specified directory
//TODO: create a path policy that checks if a path is within one of many allowed directories







