#pragma once
#include "global.h"
#include "string.h"

class filewrite;
class file : nocopy {
	file(){}
protected:
	file(cstring filename) : path(filename) {}
	file(cstring filename, size_t len) : path(filename), len(len) {}
	
	//This one will create the file from the filesystem.
	//create() can simply return create_fs(filename), or can additionally support stuff like gvfs.
	static file* open_fs(cstring filename);
	
	class mem;
public:
	//While this object may look thread-safe, it isn't. One thread at the time only.
	//Interacting with mmap() results doesn't count as interaction.
	
	//A path refers to a directory if it ends with a slash, and file otherwise. Directories may not be open()ed, though listdir() is valid.
	static file* open(cstring filename);
	//Returns all items in the given directory path, as absolute paths.
	static array<string> listdir(cstring path);
	
	//If the input path is a directory, the basename is the last component after the final slash.
	static string dirname(cstring path);
	static string basename(cstring path);
	
	//Changing these two is undefined behavior. Only the implementation may do that.
	string path;
	size_t len;
	
	//Reading outside the file will return partial results.
	virtual size_t read(arrayvieww<byte> target, size_t start) = 0;
	array<byte> read()
	{
		array<byte> ret;
		ret.resize(this->len);
		size_t actual = this->read(ret, 0);
		ret.resize(actual);
		return ret;
	}
	static array<byte> read(cstring path)
	{
		autoptr<file> f = file::open(path);
		if (f) return f->read();
		else return NULL;
	}
	
	//Mappings must be deallocated before deleting the file object.
	//If the underlying file is changed, it's undefined whether the mappings update. To force an update, delete and recreate the mapping.
	//Mapping outside the file is undefined behavior.
	virtual arrayview<byte> mmap(size_t start, size_t len) = 0;
	arrayview<byte> mmap() { return this->mmap(0, this->len); }
	virtual void unmap(arrayview<byte> data) = 0;
	
	virtual ~file() {}
	
	//Mostly usable for debug purposes.
	static file* create_mem_view(arrayview<byte> data);
	static filewrite* create_mem_copy(array<byte> data);
};


class filewrite : public file {
protected:
	filewrite(cstring filename) : file(filename) {}
	filewrite(cstring filename, size_t len) : file(filename, len) {}
	
public:
	enum mode {
		m_default,        // If the file exists, opens it. If it doesn't, creates a new file.     (O_CREAT)         (OPEN_ALWAYS)
		m_existing,       // Fails if the file doesn't exist.                                     (0)               (OPEN_EXISTING)
		m_replace,        // If the file exists, it's either deleted and recreated, or truncated. (O_CREAT|O_TRUNC) (CREATE_ALWAYS)
		m_create_excl,    // Fails if the file does exist.                                        (O_CREAT|O_EXCL)  (CREATE_NEW)
	};
protected:
	//These refer to the physical file system. The public versions can forward to these, or can additionally support stuff like gvfs.
	static filewrite* open_fs(cstring filename, mode m = m_default);
	static bool unlink_fs(cstring filename);
public:
	static filewrite* open(cstring filename, mode m = m_default);
	static bool unlink(cstring filename); // Returns whether the file is now gone. If the file didn't exist, returns true.
	
	virtual bool resize(size_t newsize) = 0; // May only be used if there are no mappings alive, not even read-only.
	//Writes outside the file will extend it. If the write starts after the current size, it's zero extended. Includes mmapw.
	virtual bool write(arrayview<byte> data, size_t start = 0) = 0;
	virtual bool replace(arrayview<byte> data) { return resize(data.size()) && write(data); }
	bool replace(cstring data) { return replace(data.bytes()); }
	bool write(cstring data) { return write(data.bytes()); }
	
	static bool write(cstring path, arrayview<byte> data)
	{
		autoptr<filewrite> f = filewrite::open(path, m_replace);
		return f->write(data);
	}
	
	//The only allowed method on a file object that has an existing writable mapping is unmapw.
	//Fails if it goes outside the file; use resize().
	virtual arrayvieww<byte> mmapw(size_t start, size_t len) = 0;
	arrayvieww<byte> mmapw() { return this->mmapw(0, this->len); }
	virtual void unmapw(arrayvieww<byte> data) = 0;
};

void _window_init_file();
