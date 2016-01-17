//Module name: Floating IPS, command line frontend
//Author: Alcaro
//Date: December 22, 2014
//Licence: GPL v3.0 or higher

#include "flips.h"

#ifdef FLIPS_CLI
class file_libc : public file {
	size_t size;
	FILE* io;
	
public:
	static file* create(const char * filename)
	{
		FILE* f = fopen(filename, "rb");
		if (!f) return NULL;
		return new file_libc(f);
	}
	
private:
	file_libc(FILE* io) : io(io)
	{
		fseek(io, 0, SEEK_END);
		size = ftell(io);
	}
	
public:
	size_t len() { return size; }
	
	bool read(uint8_t* target, size_t start, size_t len)
	{
		fseek(io, start, SEEK_SET);
		return (fread(target, 1,len, io) == len);
	}
	
	~file_libc() { fclose(io); }
};

file* file::create(const char * filename) { return file_libc::create(filename); }


class filewrite_libc : public filewrite {
	FILE* io;
	
public:
	static filewrite* create(const char * filename)
	{
		FILE* f = fopen(filename, "wb");
		if (!f) return NULL;
		return new filewrite_libc(f);
	}
	
private:
	filewrite_libc(FILE* io) : io(io) {}
	
public:
	bool append(const uint8_t* data, size_t len)
	{
		return (fwrite(data, 1,len, io)==len);
	}
	
	~filewrite_libc() { fclose(io); }
};

filewrite* filewrite::create(const char * filename) { return filewrite_libc::create(filename); }


int main(int argc, char * argv[])
{
	return flipsmain(argc, argv);
}
#endif
