#include "file.h"
#include "os.h"

namespace {
	class file_mem : public filewrite {
	public:
		array<byte> datawr;
		arrayview<byte> datard;
		
		file_mem(arrayview<byte> data) : filewrite("", data.size()), datard(data) {}
		file_mem(array<byte> data) : filewrite("", data.size()), datawr(data), datard(datawr) {}
		
		size_t read(arrayvieww<byte> target, size_t start)
		{
			size_t bytes_dst = target.size();
			size_t bytes_src = datard.size()-start;
			size_t bytes = min(bytes_dst, bytes_src);
			memcpy(target.ptr(), datard.ptr()+start, bytes);
			return bytes;
		}
		
		arrayview<byte> mmap(size_t start, size_t len) { return datard.slice(start, len); }
		void unmap(arrayview<byte> data) {}
		
		bool resize(size_t newsize)
		{
			datawr.resize(newsize);
			datard = datawr;
			len = newsize;
			return true;
		}
		
		bool write(arrayview<byte> data, size_t start)
		{
			size_t bytes_src = data.size();
			size_t bytes_dst = datawr.size()-start;
			size_t bytes = min(bytes_dst, bytes_src);
			memcpy(datawr.ptr()+start, data.ptr(), bytes);
			return true;
		}
		
		arrayvieww<byte> mmapw(size_t start, size_t len) { return datawr.slice(start, len); }
		void unmapw(arrayvieww<byte> data) {}
	};
}

file* file::create_mem_view(arrayview<byte> data)
{
	return new file_mem(data);
}

filewrite* file::create_mem_copy(array<byte> data)
{
	return new file_mem(data);
}
