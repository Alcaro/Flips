#include "patch.h"

/*
make clean; rm callgrind.out.*; make test -j8 TESTRUNNER='time valgrind --tool=callgrind' CFLAGS='-Os -g' && kcachegrind callgrind.out.*
*/

namespace patch {
//test("filebufreader")
//{
//	array<byte> bytes;
//	for (int i=0;i<65536;i++)
//	{
//		bytes[i] = i ^ i>>8;
//	}
//	
//	memstream stream = bytes;
//	
//	size_t pos = 0;
//#define EXPECT(n) \
//		do { \
//			assert_eq(stream.remaining(), 65536-pos); \
//			arrayview<byte> var = stream.read(n); \
//			for (size_t i=0;i<n;i++) \
//				assert_eq(var[i], bytes[pos+i]); \
//			pos += n; \
//			assert_eq(stream.remaining(), 65536-pos); \
//			/* assert_eq(stream.crc32(), crc32(bytes.slice(0, pos))); */ \
//		} while(0)
//	
//	EXPECT(1);
//	EXPECT(6);
//	EXPECT(14);
//	EXPECT(4000);
//	EXPECT(4000); // cross the buffers
//	assert_eq(stream.read(), bytes[pos++]); // single-byte reader
//	assert_eq(stream.read(), bytes[pos++]);
//	assert_eq(stream.read(), bytes[pos++]);
//	assert_eq(stream.read(), bytes[pos++]);
//	EXPECT(16000);
//	assert(br.read(65536).ptr() == NULL);
//	EXPECT(4000);
//}
//
//test("streamreader")
//{
//	array<byte> bytes;
//	for (int i=0;i<65536;i++)
//	{
//		bytes[i] = i ^ i>>8;
//	}
//	file f = file::mem(bytes);
//	assert_eq(f.size(), 65536);
//	
//	//streamreader r = f;
//	test_skip("not yet");
//}

static bool testips;
static bool testbps;
static void createtest(arrayview<byte> a, arrayview<byte> b, size_t ipssize, size_t bpssize)
{
	if (testips && b.size()<=16777216)
	{
		array<byte> patch;
		result r = ips::create(a, b, patch);
		if (r!=e_identical) assert_eq(r, e_ok);
		array<byte> b2;
		r = ips::apply(patch, a, b2);
		if (r!=e_to_output) assert_eq(r, e_ok);
		assert(b == b2);
		
		//ensure no accidental creation size regressions - or improving it without moving the goalposts
		//if (patch.size()!=ipssize)
		//{
			//for(byte g:patch)printf("%.2X ",g);
		//}
		assert_eq(patch.size(), ipssize);
//if (patch.size()!=ipssize)
//printf("\nexpected %zu got %zu",ipssize,patch.size());
	}
	
	if (testbps)
	{
		array<byte> patch;
		result r = bps::create(file::mem(a), file::mem(b), file::mem(patch), NULL);
		if (r!=e_identical) assert_eq(r, e_ok);
		array<byte> b2;
		r = bps::apply(patch, a, b2);
		if (r!=e_to_output) assert_eq(r, e_ok);
		assert(b == b2);
		//if (patch.size()!=bpssize)
		//{
			//for(byte g:patch)printf("%.2X ",g);
		//}
		assert_eq(patch.size(), bpssize);
//if (patch.size()!=bpssize)
//printf("\nexpected %zu got %zu",bpssize,patch.size());
	}
}

MAYBE_UNUSED
static void simpletests()
{
	array<byte> empty;
	array<byte> one0; one0[0] = 0;
	array<byte> one1; one1[0] = 1;
	array<byte> seq128; for (int i=0;i<128;i++) seq128[i]=i;
	array<byte> seq256; for (int i=0;i<256;i++) seq256[i]=i;
	array<byte> seq256nul4; for (int i=0;i<256;i++) seq256nul4[i]=i; seq256nul4[255+4]=0;
	array<byte> seq256nul5; for (int i=0;i<256;i++) seq256nul5[i]=i; seq256nul5[255+5]=0;
	array<byte> seq256nul6; for (int i=0;i<256;i++) seq256nul6[i]=i; seq256nul6[255+6]=0;
	array<byte> seq256nul7; for (int i=0;i<256;i++) seq256nul7[i]=i; seq256nul7[255+7]=0;
	array<byte> seq256b4; for (int i=0;i<256;i++) seq256b4[i]=i; seq256b4[255+4]=1;
	array<byte> seq256b5; for (int i=0;i<256;i++) seq256b5[i]=i; seq256b5[255+5]=1;
	array<byte> seq256b6; for (int i=0;i<256;i++) seq256b6[i]=i; seq256b6[255+6]=1;
	array<byte> seq256b7; for (int i=0;i<256;i++) seq256b7[i]=i; seq256b7[255+7]=1;
	array<byte> eof1;                                            eof1[0x454F46] = 1;
	array<byte> eof2;                     for (int i=0;i<16;i++) eof2[0x454F46+i] = 1;
	array<byte> eof3;                                            eof3[0x454F46] = 0;
	array<byte> eof4; eof4[0x454F45] = 2;                        eof4[0x454F46] = 1;
	array<byte> eof5; eof5[0x454F45] = 2; for (int i=0;i<16;i++) eof5[0x454F46+i] = 1;
	array<byte> eof6; eof6[0x454F45] = 2;                        eof6[0x454F46] = 0;
	
	int base = 5+3; // PATCHEOF
	int record = 3+2; // offset, len
	int rle = 3+2+2+1; // offset, len=0, rlelen, byte
	int trunc = 3;
	testcall(createtest(empty,  empty,      base,                     19));
	testcall(createtest(empty,  one1,       base+record+1,            21));
	testcall(createtest(one1,   empty,      base+trunc,               19));
	testcall(createtest(one0,   one1,       base+record+1,            21));
	testcall(createtest(seq256, seq128,     base+trunc,               23));
	testcall(createtest(seq128, seq256,     base+record+128,          153));
	testcall(createtest(empty,  seq256nul4, base+record+255+6,        282)); // mistuned IPS heuristics?
	testcall(createtest(empty,  seq256nul5, base+record+255+6,        283)); // strange how this one is bigger
	testcall(createtest(empty,  seq256nul6, base+record+255+6,        282)); // guess the heuristics don't like EOF
	testcall(createtest(empty,  seq256nul7, base+record+255+record+1, 282));
	testcall(createtest(empty,  seq256b4,   base+record+255+4,        282));
	testcall(createtest(empty,  seq256b5,   base+record+255+5,        283));
	testcall(createtest(empty,  seq256b6,   base+record+255+6,        284));
	testcall(createtest(empty,  seq256b7,   base+record+255+record+1, 284));
	if (testips) // don't need these for BPS, 0x454F46 isn't significant there
	{ // big files are sufficiently tested with the bigones tests
		testcall(createtest(empty,  eof1,       base+record+2,     57));
		testcall(createtest(empty,  eof2,       base+record+2+rle, 59));
		testcall(createtest(empty,  eof3,       base+record+2,     55));
		testcall(createtest(empty,  eof4,       base+record+2,     58));
		testcall(createtest(empty,  eof5,       base+record+2+rle, 60));
		testcall(createtest(empty,  eof6,       base+record+2,     58));
	}
}

test("IPS")
{
	testips=true;
	testbps=false;
	
	simpletests();
}

test("BPS")
{
	testips=false;
	testbps=true;
	
	simpletests();
}

test("the big ones")
{
	testips=true;
	testbps=true;
	testbps=false;
	
	array<byte> smw      = file::read("patch/test/smw.sfc");
	array<byte> smw_bps  = file::read("patch/test/smwcp.bps");
	array<byte> dl       = file::read("patch/test/langrisser.sfc");
	array<byte> dl_ups   = file::read("patch/test/dl.ups");
	array<byte> sm64     = file::read("patch/test/sm64.z64");
	array<byte> sm64_bps = file::read("patch/test/star.bps");
	if (!smw || !smw_bps || !dl || !dl_ups || !sm64 || !sm64_bps) test_skip("test files not present; see patch/test/readme.txt");
	result r;
	
	array<byte> smwhack;
	r = bps::apply(smw_bps, smw, smwhack);
	assert_eq(r, e_ok);
	assert_eq(smwhack.size(), 4194304);
	testcall(createtest(smw, smwhack, 3302746, 2077386));
	
	//this is the only UPS test, UPS is pretty much an easter egg in Flips
	array<byte> dlhack;
	r = ups::apply(dl_ups, dl, dlhack);
	assert_eq(r, e_ok);
	assert_eq(dlhack.size(), 3145728);
	array<byte> dl2;
	r = ups::apply(dl_ups, dlhack, dl2);
	assert_eq(r, e_ok);
	assert(dl == dl2);
	testcall(createtest(dl, dlhack, 852124, 817190));
	
	array<byte> sm64hack;
	r = bps::apply(sm64_bps, sm64, sm64hack);
	assert_eq(r, e_ok);
	assert_eq(sm64hack.size(), 50331648);
	testbps=false; // too slow
	testcall(createtest(sm64, sm64hack, -1, 6788133));
}
}
