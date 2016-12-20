#pragma once
#include "global.h"
#include "string.h"
#include "array.h"

class filewrite;
class file : nocopy {
public:
	class impl : nocopy {
		friend class file;
	protected:
		virtual size_t size() = 0;
		virtual bool resize(size_t newsize) = 0;
		
		virtual size_t read(arrayvieww<byte> target, size_t start) = 0;
		virtual bool write(arrayview<byte> data, size_t start = 0) = 0;
		virtual bool replace(arrayview<byte> data) { return resize(data.size()) && write(data); }
		
		virtual arrayview<byte> mmap(size_t start, size_t len) = 0;
		virtual void unmap(arrayview<byte> data) = 0;
		virtual arrayvieww<byte> mmapw(size_t start, size_t len) = 0;
		virtual void unmapw(arrayvieww<byte> data) = 0;
		
		virtual ~impl() {}
	};
	
	class implrd : impl {
		friend class file;
	protected:
		virtual size_t size() = 0;
		bool resize(size_t newsize) { return false; }
		
		virtual size_t read(arrayvieww<byte> target, size_t start) = 0;
		bool write(arrayview<byte> data, size_t start = 0) { return false; }
		bool replace(arrayview<byte> data) { return false; }
		
		virtual arrayview<byte> mmap(size_t start, size_t len) = 0;
		virtual void unmap(arrayview<byte> data) = 0;
		virtual arrayvieww<byte> mmapw(size_t start, size_t len) { return NULL; }
		virtual void unmapw(arrayvieww<byte> data) {}
	};
private:
	impl* core;
	
public:
	enum mode {
		m_read,
		m_write,          // If the file exists, opens it. If it doesn't, creates a new file.
		m_wr_existing,    // Fails if the file doesn't exist.
		m_replace,        // If the file exists, it's either deleted and recreated, or truncated.
		m_create_excl,    // Fails if the file does exist.
	};
	
	file() : core(NULL) {}
	file(file&& f) { core=f.core; f.core=NULL; }
	file& operator=(file&& f) { delete core; core=f.core; f.core=NULL; return *this; }
	file(impl* core) : core(core) {}
	file(cstring filename, mode m = m_read) : core(NULL) { open(filename, m); }
	
	bool open(cstring filename, mode m = m_read)
	{
		delete core;
		core = open_impl(filename, m);
		return core;
	}
	void close()
	{
		delete core;
		core = NULL;
	}
	
private:
	//This one will create the file from the filesystem.
	//create() can simply return create_fs(filename), or can additionally support stuff like gvfs.
	static impl* open_impl_fs(cstring filename, mode m);
	//A path refers to a directory if it ends with a slash, and file otherwise. Directories may not be open()ed.
	static impl* open_impl(cstring filename, mode m);
public:
	
	operator bool() const { return core; }
	
	//Reading outside the file will return partial results.
	size_t size() const { return core->size(); }
	size_t read(arrayvieww<byte> target, size_t start) const { return core->read(target, start); }
	array<byte> read() const
	{
		array<byte> ret;
		ret.resize(this->size());
		size_t actual = this->read(ret, 0);
		ret.resize(actual);
		return ret;
	}
	static array<byte> read(cstring path)
	{
		file f(path);
		if (f) return f.read();
		else return NULL;
	}
	
	// May only be used if there are no mappings alive, not even read-only.
	bool resize(size_t newsize) { return core->resize(newsize); }
	//Writes outside the file will extend it. If the write starts after the current size, it's zero extended. Includes mmapw.
	bool write(arrayview<byte> data, size_t start = 0) { return core->write(data, start); }
	bool replace(arrayview<byte> data) { return core->replace(data); }
	bool replace(cstring data) { return replace(data.bytes()); }
	bool write(cstring data) { return write(data.bytes()); }
	
	//Mappings must be deallocated before deleting the file object.
	//If the underlying file is changed, it's undefined whether the mappings update. To force an update, delete and recreate the mapping.
	//Mapping outside the file is undefined behavior.
	arrayview<byte> mmap(size_t start, size_t len) const { return core->mmap(start, len); }
	arrayview<byte> mmap() const { return this->mmap(0, this->size()); }
	void unmap(arrayview<byte> data) const { return core->unmap(data); }
	
	arrayvieww<byte> mmapw(size_t start, size_t len) { return core->mmapw(start, len); }
	arrayvieww<byte> mmapw() { return this->mmapw(0, this->size()); }
	void unmapw(arrayvieww<byte> data) { return core->unmapw(data); }
	
	~file() { delete core; }
	
	static file mem(arrayview<byte> data)
	{
		return file(new file::memimpl(data));
	}
	static file mem(array<byte>& data)
	{
		return file(new file::memimpl(&data));
	}
private:
	class memimpl : public file::implrd {
	public:
		arrayview<byte> datard;
		array<byte>* datawr; // this object does not own the array
		
		memimpl(arrayview<byte> data) : datard(data), datawr(NULL) {}
		memimpl(array<byte>* data) : datard(*data), datawr(data) {}
		
		size_t size() { return datard.size(); }
		bool resize(size_t newsize)
		{
			if (!datawr) return false;
			datawr->resize(newsize);
			datard=*datawr;
			return true;
		}
		
		size_t read(arrayvieww<byte> target, size_t start)
		{
			size_t nbyte = min(target.size(), datard.size()-start);
			memcpy(target.ptr(), datard.slice(start, nbyte).ptr(), nbyte);
			return nbyte;
		}
		virtual bool write(arrayview<byte> newdata, size_t start = 0)
		{
			if (!datawr) return false;
			size_t nbyte = newdata.size();
			datawr->reserve(start+nbyte);
			memcpy(datawr->slice(start, nbyte).ptr(), newdata.ptr(), nbyte);
			datard=*datawr;
			return true;
		}
		virtual bool replace(arrayview<byte> newdata)
		{
			if (!datawr) return false;
			*datawr = newdata;
			datard = *datawr;
			return true;
		}
		
		arrayview<byte>   mmap(size_t start, size_t len) { return datard.slice(start, len); }
		arrayvieww<byte> mmapw(size_t start, size_t len) { if (!datawr) return NULL; return datawr->slice(start, len); }
		void  unmap(arrayview<byte>  data) {}
		void unmapw(arrayvieww<byte> data) {}
	};
public:
	
	//Returns all items in the given directory path, as absolute paths.
	static array<string> listdir(cstring path);
	static bool unlink(cstring filename); // Returns whether the file is now gone. If the file didn't exist, returns true.
	//If the input path is a directory, the basename is blank.
	static string dirname(cstring path);
	static string basename(cstring path);
private:
	static bool unlink_fs(cstring filename);
};

void _window_init_file();
