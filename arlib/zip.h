#pragma once
#include "array.h"
#include "file.h"

class zip : nocopy {
	struct locfhead;
	struct centdirrec;
	struct endofcdr;
	
	endofcdr* getendofcdr(arrayview<byte> data);
	centdirrec* getcdr(arrayview<byte> data, endofcdr* end);
	centdirrec* nextcdr(arrayview<byte> data, centdirrec* cdr);
	locfhead* geth(arrayview<byte> data, centdirrec* cdr);
	arrayview<byte> fh_fname(arrayview<byte> data, locfhead* fh);
	arrayview<byte> fh_data(arrayview<byte> data, locfhead* fh);
	
	array<string> filenames;
	struct file {
		//would've put filenames here too, but then I'd need funky tricks in files()
		uint32_t decomplen;
		uint16_t method;
		array<byte> data;
		uint32_t crc32;
		uint32_t dosdate;
	};
	array<file> filedat;
	
public:
	zip() {}
	zip(arrayview<byte> data)
	{
		init(data);
	}
	
	bool init(arrayview<byte> data);
	
	//Invalidated whenever the file list changes.
	arrayview<string> files()
	{
		return filenames;
	}
	
private:
	size_t find_file(cstring name);
	static array<byte> unpackfiledat(file& f);
public:
	
	array<byte> read(cstring name, time_t * time = NULL);
	
	//Writing a blank array deletes the file.
	void write(cstring name, arrayview<byte> data, time_t date = 0);
	
private:
	static int fileminver(file& f);
	static bool strascii(cstring s);
public:
	array<byte> pack();
};
