#include "file.h"
#include "test.h"

#ifdef ARLIB_TEST

//criteria:
//- must be a normal file, no /dev/*
//- minimum 66000 bytes
//- the first few bytes must be known, no .txt files or possibly-shebanged stuff
//- the file must contain somewhat unpredictable data, nothing from /dev/zero
//- must be readable by everyone (assuming absense of sandboxes)
//- must NOT be writable or deletable by this program
//recommended choice: some random executable
#ifdef _WIN32
#define READONLY_FILE "C:/Windows/notepad.exe" // screw anything where the windows directory isn't on C:
#define READONLY_FILE_HEAD "MZ"
#else
#define READONLY_FILE "/bin/sh"
#define READONLY_FILE_HEAD "\x7F""ELF"
#endif

//criteria:
//- no funny symbols in the name
//- implausible name, nothing of value may be lost by deleting it
#define WRITABLE_FILE "arlib-selftest.txt"

test("file reading")
{
	file f;
	assert(f.open(READONLY_FILE));
	assert(f.size());
	assert(f.size() > strlen(READONLY_FILE_HEAD));
	assert(f.size() >= 66000);
	array<byte> bytes = f.read();
	assert(bytes.size() == f.size());
	assert(!memcmp(bytes.ptr(), READONLY_FILE_HEAD, strlen(READONLY_FILE_HEAD)));
	
	arrayview<byte> map = f.mmap();
	assert(map.ptr());
	assert(map.size() == f.size());
	assert(!memcmp(bytes.ptr(), map.ptr(), bytes.size()));
	
	arrayview<byte> map2 = f.mmap();
	assert(map2.ptr());
	assert(map2.size() == f.size());
	assert(!memcmp(bytes.ptr(), map2.ptr(), bytes.size()));
	f.unmap(map2);
	
	const size_t t_start[] = { 0,     65536, 4096, 1,     1,     1,     65537, 65535 };
	const size_t t_len[]   = { 66000, 400,   400,  65535, 65536, 65999, 400,   2     };
	for (size_t i=0;i<ARRAY_SIZE(t_start);i++)
	{
		arrayview<byte> map3 = f.mmap(t_start[i], t_len[i]);
		assert(map3.ptr());
		assert(map3.size() == t_len[i]);
		assert(!memcmp(bytes.ptr()+t_start[i], map3.ptr(), t_len[i]));
		f.unmap(map3);
	}
	
	f.unmap(map);
}

test("file writing")
{
	file f;
	
	assert(!f.open(READONLY_FILE, file::m_write));
	assert(!f.open(READONLY_FILE, file::m_wr_existing));
	assert(!f.open(READONLY_FILE, file::m_replace));
	assert(!f.open(READONLY_FILE, file::m_create_excl));
	
	assert(file::unlink(WRITABLE_FILE));
	
	assert(!f.open(WRITABLE_FILE));
	
	assert(f.open(WRITABLE_FILE, file::m_write));
	assert(f.replace("foo"));
	
	assert_eq(string(file::read(WRITABLE_FILE)), "foo");
	
	f.resize(8);
	assert(f.size() == 8);
	byte expected[8]={'f','o','o',0,0,0,0,0};
	array<byte> actual = file::read(WRITABLE_FILE);
	assert(actual.ptr());
	assert(actual.size()==8);
	assert(!memcmp(actual.ptr(), expected, 8));
	
	arrayvieww<byte> map = f.mmapw();
	assert(map.ptr());
	assert_eq(map.size(), 8);
	assert(!memcmp(map.ptr(), expected, 8));
	map[3]='t';
	f.unmapw(map);
	
	expected[3] = 't';
	actual = file::read(WRITABLE_FILE);
	assert(actual.ptr());
	assert(actual.size()==8);
	assert(!memcmp(actual.ptr(), expected, 8));
	
	//test the various creation modes
	//file exists, these three should work
	assert( (f.open(WRITABLE_FILE, file::m_write)));
	assert( (f.open(WRITABLE_FILE, file::m_wr_existing)));
	assert_eq(f.size(), 8);
	assert( (f.open(WRITABLE_FILE, file::m_replace)));
	assert_eq(f.size(), 0);
	assert(!(f.open(WRITABLE_FILE, file::m_create_excl)));//but this shouldn't
	
	assert(file::unlink(WRITABLE_FILE));
	assert(!f.open(WRITABLE_FILE, file::m_wr_existing)); // this should fail
	assert(f.open(WRITABLE_FILE, file::m_create_excl)); // this should create
	assert(file::unlink(WRITABLE_FILE));
	
	assert(f.open(WRITABLE_FILE, file::m_replace)); // replacing a nonexistent file is fine
	//opening a nonexistent file with m_write is tested at the start of this function
	f.close();
	assert(file::unlink(WRITABLE_FILE));
	assert(file::unlink(WRITABLE_FILE)); // ensure it properly deals with unlinking a nonexistent file
}

test("in-memory files")
{
	array<byte> bytes;
	for (int i=0;i<8;i++) bytes[i]=i;
	array<byte> bytes2;
	bytes2.resize(4);
	
	file f = file::mem(bytes.slice(0, 8));
	assert(f);
	assert_eq(f.size(), 8);
	assert(f.read(bytes2, 1));
	for (int i=0;i<4;i++) assert_eq(bytes2[i], i+1);
	
	//readonly
	assert(!f.write(bytes2, 6));
	assert(!f.replace(bytes2));
	assert(!f.mmapw());
	
	f = file::mem(bytes);
	assert(f);
	assert_eq(f.size(), 8);
	
	assert(f.write(bytes2, 6));
	assert_eq(f.size(), 10);
	for (int i=0;i<6;i++) assert_eq(bytes[i], i);
	for (int i=0;i<4;i++) assert_eq(bytes[i+6], i+1);
	
	assert(f.replace(bytes2));
	for (int i=0;i<4;i++) assert_eq(bytes[i], i+1);
	assert_eq(f.size(), 4);
}
#endif
