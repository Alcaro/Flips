#include "patch.h"

//Deprecated
//struct mem {
	//mem() : ptr(NULL), len(0) {}
	//mem(uint8_t* ptr, size_t len) : ptr(ptr), len(len) {}
	//mem(arrayview<byte> v) : ptr((byte*)v.ptr()), len(v.size()) {}
	//arrayvieww<byte> v() { return arrayvieww<byte>(ptr, len); }
	//uint8_t * ptr;
	//size_t len;
//};

//These two give minor performance penalties and will print some random stuff to stdout.
//The former will verify the correctness of the output patch, the latter will print some performance data.
//Can be useful for debugging, but should be disabled for release builds.
#ifdef BPS_STANDALONE
#endif
//#define TEST_CORRECT
//#define TEST_PERF

//If the suffix array of [0, 0, 0, 0] is [3, 2, 1, 0], set to true. If it's [0, 1, 2, 3], this is false.
//If it's [4, 3, 2, 1, 0] or [0, 1, 2, 3, 4], remove the 4 (easily done with some pointer math), and follow the above.
//If it's something else, get a non-broken array calculator.
#define EOF_IS_LAST false

//Algorithm description:
//
//This is heavily built upon suffix sorting; the implementation I use, libdivsufsort, claims
// O(n log n) complexity, so I'll believe that. There is also SA-IS, which claims O(n), but if that
// is true, its constant factors are ridiculously high.
//
//The program starts by taking an equal amount of the source file and target file, concatenates that
// with target first, and suffix sorts it.
//
//To find a match, it finds the sortpos where sorted[sortpos]==outpos. This is a binary search; it's
// called O(n) times, with O(log n) comparisons per iteration. Each comparison is potentially O(n),
// but for each matched byte, another iteration is removed from the outer loop, so the comparisons
// can be considered O(1) each; the sum is O(n log n).
//
//After it's found sortpos, it scans sorted[] up and down for the closest entry that either starts
// before the current output position, or is somewhere in the source file.
//As the source file comes last, the end-of-file marker (whose value is outside the range of a byte)
// is guaranteed to not be in the way for a better match.
//This is called O(n) times, and averages O(1) as at least 50% of sorted[] is in range. However, it
// is worst-case O(n) for sorted inputs, giving a total of O(n^2).
//
//It then checks which of the two candidates are superior, by checking how far they match each
// other, and then checking if the upper one has another correct byte.
//This is potentially O(n), but like the binary search, long matches reduce the outer loop. The sum
// is O(n).
//
//When the program approaches the end of the sorted area, it re-sorts twice as much as last time.
// This gives O(log n) calls to the suffix sorter.
//Given O(n log n) for one sorting step, the time taken is O(n/1 log n/1 + n/2 log n/2 +
// n/4 log n/4 + ...), which is strictly less than O(n/1 log n + n/2 log n + n/4 log n + ...), which
// equals O(2n log n), which is O(n log n). (The exact value of that infinite sum is 2n*log(n/2).)
//
//Many details were omitted from the above, but that's the basic setup.
//
//Thus, the program is O(max(n log n, n log n, n, n) = n log n) average and O(max(n log n, n log n,
// n^2, n) = n^2) worst case.
//
//I conclude that the task of finding, understanding and implementing a sub-O(n^2) algorithm for
// delta patching is resolved.


//Known cases where this function does not emit the optimal encoding:
//If a match in the target file would extend further than target_search_size, it's cut off.
// Penalty: O(log n), with extremely low constants (it'd require a >256B match to be exactly there).
// Even for big files, the penalty is very likely to remain zero; even hitting double-digit bytes
// would require a file designed exactly for that.
//If multiple matches are equally good, it picks one at random, not the one that's cheaper to encode.
// Penalty: Likely O(n) or O(n log log n), with low constants. I'd guess ~1.4% for my 48MB test file.
//However, due to better heuristics and others' performance optimizations, this one still beats its
// competitors.

//TODO: test multiple same-length matches
// but only for lengths <= 64, 


//Possible optimizations:
//divsufsort() takes approximately 1/2 of the total time.
//Each iteration takes four times as long as the previous one.
//If each iteration takes 4 times as long as the previous one, then the last one takes 3/4 of the total time.
//Since divsufsort doesn't depend on anything else, the last iteration can be split off to its own thread.
//This would split it to
//Search, non-final:   1/2 * 1/4 = 1/8
//Search, final:       1/2 * 3/4 = 3/8
//Sort+rev, non-final: 1/2 * 1/4 = 1/8
//Sort+rev, final:     1/2 * 3/4 = 3/8
//All non-final must be done sequentially. Both Sort Final and non-final must be done before Search Final can start.
//This means the final time, if Sort Final is split off, is
//max(1/8+1/8, 3/8) + 3/8 = 6/8 = 3/4
//of the original time.
//Due to
//- the considerable complexity costs (OpenMP doesn't seem able to represent the "insert a wait in
//   the middle of this while loop" I would need)
//- the added memory use, approximately 25% higher - it's already high enough
//- libdivsufsort already using threads, which would make the gains lower
//   and would increase complexity, as I have to ensure the big one remains threaded -
//    and that the small ones are not, as that'd starve the big one
//I deem a possible 25% boost not worthwhile.

//Both sorting algorithms claim O(1) memory use, in addition to the in/outputs. For most hardware,
// this is 5*(source.len+target.len).
//If the output is stored to disk, that's all this algorithm needs as well.


namespace patch { namespace bps {
//TODO: HEAVY cleanups needed here
#include "sais.cpp"
template<typename sais_index_type>
static void sufsort(sais_index_type* SA, const uint8_t* T, sais_index_type n) {
  if(n <= 1) { if(n == 1) SA[0] = 0; return; }
  sais_main<sais_index_type>(T, SA, 0, n, 256);
}

//According to <https://code.google.com/p/libdivsufsort/wiki/SACA_Benchmarks>, divsufsort achieves
// approximately half the time of SAIS for nearly all files, despite SAIS' promises of linear
// performance (divsufsort claims O(n log n)).

//divsufsort only allocates O(1) for some radix/bucket sorting. SAIS seems constant too.
//I'd prefer to let them allocate from an array I give it, but divsuf doesn't allow that, and there
// are only half a dozen allocations per call anyways.

//This ends up in libdivsufsort if available, otherwise sais.cpp.
#include "divsufsort.h"
static void sufsort(int32_t* SA, uint8_t* T, int32_t n)
{
	divsufsort(T, SA, n);
}

#ifdef USE_DIVSUFSORT64
#include "divsufsort64.h"

static void sufsort(int64_t* SA, uint8_t* T, int64_t n)
{
	divsufsort(T, SA, n);
}
#endif



namespace {
//class filecache {
//	file& f;
//	uint32_t crc32;
//	int bytes_used;
//	uint8_t bytes[65536];
//	
//	void append(const uint8_t * data, size_t len)
//	{
//		
//	}
//};

struct bps_creator {
	array<byte> out;
	
	void appendnum(size_t num)
	{
#ifdef TEST_CORRECT
		if (num > 1000000000)
			printf("ERROR: Attempt to write %.8lX\n",(unsigned long)num),abort();
#endif
		
		while (num >= 128)
		{
			out.append((num&0x7F));
			num>>=7;
			num--;
		}
		out.append(num|0x80);
	}
	
	void appendnum32(uint32_t num)
	{
		out.append(num>>0);
		out.append(num>>8);
		out.append(num>>16);
		out.append(num>>24);
	}
	
	static size_t maxsize()
	{
		return SIZE_MAX>>2; // can probably be reduced to SIZE_MAX>>1, but the mallocs overflow at that point anyways.
	}
	
	size_t sourcelen;
	size_t targetlen;
	const uint8_t* targetmem;
	
	enum bpscmd { SourceRead, TargetRead, SourceCopy, TargetCopy };
	
	size_t outpos;
	
	size_t sourcecopypos;
	size_t targetcopypos;
	
	size_t numtargetread;
	
	bps_creator(const file& source, const file& target, const file& metadata)
	{
		outpos = 0;
		
		sourcelen = source.size();
		targetlen = target.size();
		
		sourcecopypos = 0;
		targetcopypos = 0;
		
		numtargetread = 0;
		
		out += arrayview<byte>((byte*)"BPS1", 4);
		appendnum(sourcelen);
		appendnum(targetlen);
		appendnum(metadata.size());
		arrayview<byte> tmp = metadata.mmap();
		out += tmp;
		metadata.unmap(tmp);
	}
	
	
	void move_target(const uint8_t* ptr)
	{
		targetmem = ptr;
	}
	
	size_t encode_delta(size_t prev, size_t next)
	{
		bool negative = (next<prev);
		size_t offset = negative ? prev-next : next-prev;
		return (negative?1:0) | (offset<<1);
	}
	
	void append_delta(size_t prev, size_t next)
	{
		appendnum(encode_delta(prev, next));
	}
	
	void append_cmd(bpscmd command, size_t count)
	{
		appendnum((count-1)<<2 | command);
	}
	
	void flush_target_read()
	{
		if (!numtargetread) return;
		append_cmd(TargetRead, numtargetread);
		out += arrayview<byte>(targetmem+outpos-numtargetread, numtargetread);
		numtargetread = 0;
	}
	
	size_t emit_source_copy(size_t location, size_t count)
	{
		if (location == outpos) return emit_source_read(location, count);
		flush_target_read();
		append_cmd(SourceCopy, count);
		append_delta(sourcecopypos, location);
		sourcecopypos = location+count;
		outpos += count;
		return count;
	}
	
	size_t emit_source_read(size_t location, size_t count)
	{
		flush_target_read();
#ifdef TEST_CORRECT
		if (location != outpos)
			puts("ERROR: SourceRead not from source pointer"),abort();
#endif
		append_cmd(SourceRead, count);
		outpos+=count;
		return count;
	}
	
	size_t emit_target_copy(size_t location, size_t count)
	{
		flush_target_read();
		append_cmd(TargetCopy, count);
		append_delta(targetcopypos, location);
		targetcopypos = location+count;
		outpos += count;
		return count;
	}
	
	size_t emit_target_read()
	{
		numtargetread++;
		outpos++;
		return 1;
	}
	
	
	size_t abs_diff(size_t a, size_t b)
	{
		return (b<a) ? (a-b) : (b-a);
	}
	size_t num_cost(size_t num)
	{
		if (num<128) return 1;
		if (num<128*128) return 2; // 32KB
		if (num<128*128*128) return 3; // 2MB
		if (num<128*128*128*128) return 4; // 256MB
		// 128^5 is 32GB, let's just assume the sizes don't go any higher...
		// worst case, a bad match is used. except a 32GB match is by definition good.
		return 5;
	}
	
	bool use_match(bool hastargetread, size_t cost, size_t len)
	{
		//numbers calculated via trial and error; checking for each cost, optimizing 'len' for each, and checking what happens
		//then a pattern was identified and used
		//yes, it looks weird
		return len >= 1+cost+hastargetread+(len==1);
	}
	
	
	//Return value is how many bytes were used. If you believe the given one sucks, use TargetRead and return 1.
	size_t match(bool is_target, size_t pos, size_t len)
	{
		if (!use_match(
		     numtargetread,
		     (!is_target && pos==outpos) ? 1 : // SourceRead
		         (num_cost(abs_diff(pos, (is_target ? targetcopypos : sourcecopypos)))+1),
		     len
		   ))
		{
			return emit_target_read();
		}
		
		if (is_target) return emit_target_copy(pos, len);
		else return emit_source_copy(pos, len);
	}
	
	
	function<bool(size_t done, size_t total)> progress;
	
	
	void finish(const uint8_t* source, const uint8_t* target)
	{
		flush_target_read();
#ifdef TEST_CORRECT
		if (outpos != targetlen)
			puts("ERROR: patch creates wrong ROM size"),abort();
#endif
		
		appendnum32(crc32(arrayview<byte>(source, sourcelen)));
		appendnum32(crc32(arrayview<byte>(target, targetlen)));
		appendnum32(crc32(out));
	}
	
	size_t outlen() { return out.size(); }
	
	array<byte> getpatch()
	{
		return std::move(out);
	}
};
}



#ifdef TEST_PERF
static int match_len_n=0;
static int match_len_tot=0;
#endif

static size_t match_len(const uint8_t* a, const uint8_t* b, size_t len)
{
	//don't replace with memcmp_d, the average match length is so small it's a net loss
	size_t i;
	for (i=0;i<len && a[i]==b[i];i++) {}
#ifdef TEST_PERF
	match_len_n++;
	match_len_tot+=i;
#endif
	return i;
}

//This one assumes that the longest common prefix of 'a' and 'b' is shared also by 'search'.
//In practice, lexographically, a < search < b, which is a stronger guarantee.
template<typename off_t>
static off_t pick_best_of_two(const uint8_t* search, off_t searchlen,
                              const uint8_t* data, off_t datalen,
                              off_t a, off_t b,
                              off_t* bestlen)
{
	off_t commonlen = match_len(data+a, data+b, min(datalen-a, datalen-b));
	if (commonlen>=searchlen)
	{
		*bestlen=searchlen;
		return a;
	}
	
	if (a+commonlen<datalen && search[commonlen]==data[a+commonlen])
	{
		// a is better
		*bestlen = commonlen + match_len(search+commonlen, data+a+commonlen, min(searchlen, datalen-a)-commonlen);
		return a;
	}
	else
	{
		// b is better, or they're equal
		*bestlen = commonlen + match_len(search+commonlen, data+b+commonlen, min(searchlen, datalen-b)-commonlen);
		return b;
	}
}

//This one takes a match, which is assumed optimal, and looks for the lexographically closest one
// that either starts before 'maxstart', or starts at or after 'minstart'.
template<typename off_t>
static off_t adjust_match(off_t match, const uint8_t* search, off_t searchlen,
                          const uint8_t* data,off_t datalen, off_t maxstart,off_t minstart,
                          const off_t* sorted, off_t sortedlen,
                          off_t* bestlen)
{
	off_t match_up = match;
	off_t match_dn = match;
	while (match_up>=0        && sorted[match_up]>=maxstart && sorted[match_up]<minstart) match_up--;
	while (match_dn<sortedlen && sorted[match_dn]>=maxstart && sorted[match_dn]<minstart) match_dn++;
	if (match_up<0 || match_dn>=sortedlen)
	{
		if (match_up<0 && match_dn>=sortedlen)
		{
			*bestlen=0;
			return 0;
		}
		off_t pos = sorted[match_up<0 ? match_dn : match_up];
		*bestlen = match_len(search, data+pos, min(searchlen, datalen-pos));
		return pos;
	}
	
	return pick_best_of_two(search,searchlen, data,datalen, sorted[match_up],sorted[match_dn], bestlen);
}



static uint16_t read2_uc(const uint8_t* data)
{
	return data[0]<<8 | data[1];
}

template<typename off_t>
static uint16_t read2(const uint8_t* data, off_t len)
{
	if (len>=2) return read2_uc(data);
	else
	{
		uint16_t out = (EOF_IS_LAST ? 0xFFFF : 0x0000);
		if (len==1) out = (data[0]<<8) | (out&0x00FF);
		return out;
	}
}

template<typename off_t>
static void create_buckets(const uint8_t* data, off_t* index, off_t len, off_t* buckets)
{
	off_t low = 0;
	off_t high;
	
	for (int n=0;n<65536;n++)
	{
		//'low' remains from the previous iteration and is a known minimum
		high = low+(len/131072)+1; // optimal value: slightly above a third of the distance to the next one
		while (true)
		{
			if (high > len-1) break;
			
			off_t pos = index[high];
			uint16_t here = read2(data+pos, len-pos);
			
			if (here >= n) break;
			else
			{
				off_t diff = high-low;
				low = high;
				high = high+diff*2;
			}
		}
		if (high > len-1) high = len-1;
		
		
		while (low < high)
		{
			off_t mid = low + (high-low)/2;
			off_t midpos = index[mid];
			
			uint16_t here = read2(data+midpos, len-midpos);
			if (here < n) low = mid+1;
			else high = mid;
		}
		buckets[n] = low;
	}
	
	buckets[65536] = len;
	
#ifdef TEST_CORRECT
	if (buckets[0]!=0)
	{
		printf("e: buckets suck, [0]=%i\n", buckets[0]);
		abort();
	}
	for (int n=0;n<65536;n++)
	{
		off_t low = buckets[n];
		off_t high = buckets[n+1];
		for (off_t i=low;i<high;i++)
		{
			if (read2(data+index[i], len-index[i])!=n)
			{
				printf("e: buckets suck, %i != (%i)[%i]%i [%i-%i]", n, i,index[i],read2(data+index[i],len-index[i]),low,high);
				abort();
			}
		}
		//printf("%i:[%i]%i\n",n,low,read2(data+index[low],len-low));
	}
#endif
}

template<typename off_t>
static off_t find_index(off_t pos, const uint8_t* data, off_t datalen, const off_t* index, const off_t* buckets)
{
	//if (datalen<2) return 0;
	uint16_t bucket = read2(data+pos, datalen-pos);
//printf("p=%i b=%i\n",pos,bucket);
	
	//TODO
	//off_t low = 0;
	//off_t high = datalen-1;
	off_t low = buckets[bucket];
	off_t high = buckets[bucket+1]-1;
	
	off_t lowmatch = 2;
	off_t highmatch = 2;
	
//printf("b=%i r=%i(%i)-%i(%i)\n",bucket,low,read2(data+index[low],datalen-index[low]),high,read2(data+index[high],datalen-index[high]));
//fflush(stdout);
	while (true)
	{
		off_t mid = low + (high-low)/2;
		off_t midpos = index[mid];
		if (midpos == pos) return mid;
//printf("r=[%i]%i-%i \n",high-low,low,high,);
//fflush(stdout);
#ifdef TEST_CORRECT
		if (low >= high)
		{
			printf("E: [%i](%i): stuck at %i(%i)-%i(%i)\n", pos, read2_uc(data+pos),
				low, read2_uc(data+index[low]), high, read2_uc(data+index[high]));
			int n=0;
			while (index[n]!=pos) n++;
			printf("correct one is %i(%i)\n",n, read2_uc(data+index[n]));
			abort();
		}
#endif
		
		off_t matchlenstart = min(lowmatch, highmatch);
		
		off_t len = datalen - max(pos, midpos) - matchlenstart;
		
		const uint8_t* search = data+pos+matchlenstart;
		const uint8_t* here = data+midpos+matchlenstart;
		
		//don't replace with match_len, it's not inlined properly
		while (len>0 && *search==*here)
		{
			search++;
			here++;
			len--;
		}
		
		off_t matchlen = search-data-pos;
		
		bool less;
		if (len > 0) less = (*here<*search);
		else less = (here > search) ^ EOF_IS_LAST;
		
		if (less)
		{
			low = mid+1;
			lowmatch = matchlen;
		}
		else
		{
			high = mid-1;
			highmatch = matchlen;
		}
		
		if (low+256 > high)
		{
			off_t i=low;
			while (true)
			{
				if (index[i]==pos) return i;
				i++;
			}
		}
	}
}


template<typename off_t>
static off_t nextsize(off_t outpos, off_t sortedsize, off_t targetlen)
{
	while (outpos >= sortedsize-256 && sortedsize < targetlen)
		sortedsize = min(sortedsize*4+3, targetlen);
	return sortedsize;
}

template<typename off_t>
off_t lerp(off_t x, off_t y, float frac)
{
	return x + (y-x)*frac;
}

template<typename off_t>
static result create_suf_core(const file& source, const file& target, struct bps_creator * out)
{
#define error(which) do { err = which; goto error; } while(0)
	result err;
	
	size_t realsourcelen = source.size();
	size_t realtargetlen = target.size();
	
	size_t overflowtest = realsourcelen + realtargetlen;
	
	//source+target length is bigger than size_t
	if (overflowtest < realsourcelen) return e_too_big;
	
	//source+target doesn't fit in unsigned off_t
	if ((size_t)(off_t)overflowtest != overflowtest) return e_too_big;
	
	//source+target doesn't fit in signed off_t
	if ((off_t)overflowtest < 0) return e_too_big;
	
	//the mallocs would overflow
	if (realsourcelen+realtargetlen >= SIZE_MAX/sizeof(off_t)) return e_too_big;
	
	if (realsourcelen+realtargetlen >= out->maxsize()) return e_too_big;
	
	
	off_t sourcelen = realsourcelen;
	off_t targetlen = realtargetlen;
	
	uint8_t* mem_joined = (uint8_t*)malloc(sizeof(uint8_t)*(realsourcelen+realtargetlen));
	
	off_t* sorted = (off_t*)malloc(sizeof(off_t)*(realsourcelen+realtargetlen));
	
	off_t* buckets = (off_t*)malloc(sizeof(off_t)*65537);
	
	if (!sorted || !mem_joined || !buckets)
	{
		free(mem_joined);
		free(sorted);
		free(buckets);
		return e_out_of_mem;
	}
	
	//sortedsize is how much of the target file is sorted
	off_t sortedsize = targetlen;
	//divide by 4 for each iteration, to avoid sorting 50% of the file (the sorter is slow)
	while (sortedsize/4 > sourcelen && sortedsize > 1024) sortedsize >>= 2;
	
	off_t prevsortedsize = 0;
	off_t outpos = 0;
	
	goto reindex; // jump into the middle so I won't need a special case to enter it
	
	while (outpos < targetlen)
	{
		if (outpos >= sortedsize-256 && sortedsize < targetlen)
		{
			sortedsize = nextsize(outpos, sortedsize, targetlen);
			
		reindex:
			
			//this isn't an exact science
			const float percSort = 0.50;
			const float percBuck = 0.10;
			//const float percFind = 0.40; // unused
			
			const size_t progPreSort = lerp(prevsortedsize, sortedsize, 0);
			const size_t progPreBuck = lerp(prevsortedsize, sortedsize, percSort);
			const size_t progPreFind = lerp(prevsortedsize, sortedsize, percSort+percBuck);
			
			prevsortedsize = sortedsize;
			
			if (out->progress(progPreSort, targetlen)) error(e_canceled);
			
			if (target.read(arrayvieww<byte>(mem_joined, sortedsize), 0) < (size_t)sortedsize) error(e_io);
			if (source.read(arrayvieww<byte>(mem_joined+sortedsize, sourcelen), 0) < (size_t)sourcelen) error(e_io);
			out->move_target(mem_joined);
			
			if (targetlen==0) goto emitempty; // size-0 targets are silly, but have to be handled
			
			sufsort(sorted, mem_joined, sortedsize+sourcelen);
			
			if (out->progress(progPreBuck, targetlen)) error(e_canceled);
			
			create_buckets(mem_joined, sorted, sortedsize+sourcelen, buckets);
			
			if (out->progress(progPreFind, targetlen)) error(e_canceled);
		}
		
		off_t matchlen = 0;
		off_t matchpos = adjust_match(find_index(outpos, mem_joined, sortedsize+sourcelen, sorted, buckets),
		                              mem_joined+outpos, sortedsize-outpos,
		                              mem_joined,sortedsize+sourcelen, outpos,sortedsize,
		                              sorted, sortedsize+sourcelen,
		                              &matchlen);
		
#ifdef TEST_CORRECT
		if (matchlen && matchpos >= outpos && matchpos < sortedsize) puts("ERROR: found match in invalid location"),abort();
		if (memcmp(mem_joined+matchpos, mem_joined+outpos, matchlen)) puts("ERROR: found match doesn't match"),abort();
#endif
		
		off_t taken;
		if (matchpos >= sortedsize) taken = out->match(false, matchpos-sortedsize, matchlen);
		else taken = out->match(true, matchpos, matchlen);
#ifdef TEST_CORRECT
		if (taken < 0) puts("ERROR: match() returned negative"),abort();
		if (matchlen >= 7 && taken < matchlen) printf("ERROR: match() took %i bytes, offered %i\n", taken, matchlen),abort();
#endif
		outpos += taken;
	}
emitempty:
	
	out->finish(mem_joined+sortedsize, mem_joined);
	
	err = e_ok;
	
error:
	free(buckets);
	free(sorted);
	free(mem_joined);
	
	return err;
}


template<typename T> static result create_suf_pick(const file& source, const file& target, struct bps_creator * bps);
template<> result create_suf_pick<uint32_t>(const file& source, const file& target, struct bps_creator * bps)
{
	return create_suf_core<int32_t>(source, target, bps);
}
template<> result create_suf_pick<uint64_t>(const file& source, const file& target, struct bps_creator * bps)
{
	result err = create_suf_core<int32_t>(source, target, bps);
	if (err==e_too_big) err = create_suf_core<int64_t>(source, target, bps);
	return err;
}

//This one picks a function based on 32-bit integers if that fits. This halves memory use for common inputs.
//It also handles some stuff related to the BPS headers and footers.
result create(const file& source, const file& target, const file& metadata, array<byte>& patch,
              function<bool(size_t done, size_t total)> progress)
{
	bps_creator bps(source, target, metadata);
	bps.progress = progress;
	
	size_t maindata = bps.outlen();
	
	//off_t must be signed
	result err = create_suf_pick<size_t>(source, target, &bps);
	if (err!=e_ok) return err;
	
	patch = bps.getpatch();
	
	while ((patch[maindata]&0x80) == 0x00) maindata++;
	if (maindata==patch.size()-12-1) return e_identical;
	return e_ok;
}




#ifdef BPS_STANDALONE
#include <stdio.h>
static struct mem ReadWholeFile(const char * filename)
{
	struct mem null = {NULL, 0};
	
	FILE * file=fopen(filename, "rb");
	if (!file) return null;
	fseek(file, 0, SEEK_END);
	size_t len=ftell(file);
	fseek(file, 0, SEEK_SET);
	unsigned char * data=(unsigned char*)malloc(len);
	size_t truelen=fread(data, 1,len, file);
	fclose(file);
	if (len!=truelen)
	{
		free(data);
		return null;
	}
	
	struct mem ret = { (unsigned char*)data, len };
	return ret;
}
static bool WriteWholeFile(const char * filename, struct mem data)
{
	FILE * file=fopen(filename, "wb");
	if (!file) return false;
	unsigned int truelen=fwrite(data.ptr, 1,data.len, file);
	fclose(file);
	return (truelen==data.len);
}
int main(int argc, char * argv[])
{
//struct mem out = ReadWholeFile(argv[2]);
//printf("check=%.8X\n",crc32(out.ptr, out.len));

struct mem in = ReadWholeFile(argv[1]);
struct mem out = ReadWholeFile(argv[2]);
struct mem null = {NULL, 0};
struct mem p={NULL,0};
//int n=50;
//for(int i=0;i<n;i++)
//printf("%i/%i\n",i,n),
bps_create_delta(in,out,null,&p, NULL,NULL);
printf("len=%lu        \n",p.len);
printf("check=%.8X\n",*(uint32_t*)(p.ptr+p.len-4));
WriteWholeFile(argv[3], p);
free(in.ptr);
free(out.ptr);
free(p.ptr);

#ifdef TEST_PERF
printf("%i/%i=%f\n",match_len_tot,match_len_n,(float)match_len_tot/match_len_n);
#endif
}
#endif
}}
