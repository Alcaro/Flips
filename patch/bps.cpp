#include "patch.h"

//Things I would've done differently if I had a chance to redesign the BPS format:
//- Don't allow encoding -0 in Source/TargetCopy
//- Ditch metadata, it goes in a separate file (and it's already used in nonstandard ways, spec says XML but bsnes disagrees)
//- Reconsider SourceRead; maybe patches would be smaller if of the three others was one bit rather than two, or maybe a new command
//    or maybe only Read/Copy commands? Read is TargetRead, Copy treats target as concatenated to source
//- Invert 0x80 bit in the encoded numbers, set means continue; it would simplify the decoder
//- Replace BPS1 signature with something not containing an 1
//    I prefer four-byte signatures over three, but this format is intended to never ever change, so appending '1' is the wrong choice
//    PNGs \x89 would work
//- Make the checksums mandatory
//  (1) Ignoring them allows all files of that size, including ones that are clearly not the proper source
//  (2) Even if a ROM hacker is careful to only change a few bytes, BPS likes copying stuff around,
//      so the patch could break due to changes to unrelated and unpredictable locations
//  (3) Patches designed to cooperate are most likely written in assembly, not binary, and should be shared in that format.
//      Closed source patches help nobody.

namespace patch { namespace bps {
enum { SourceRead, TargetRead, SourceCopy, TargetCopy };

result apply(arrayview<byte> patchmem, arrayview<byte> inmem, array<byte>& outmem, bool accept_wrong_input)
{
	if (patchmem.size()<4+3+12) return e_broken;
	
	result error = e_ok;
	memstream patch = patchmem.slice(0, patchmem.size()-12);
	
	if (true)
	{
#define error(which) do { error=which; goto exit; } while(0)
#define decodeto(var) \
				do { \
					if (!patch.bpsnum(&var)) error(e_too_big); \
				} while(false)
		
		if (!patch.signature("BPS1")) error(e_broken);
		
		memstream checks = patchmem.slice(patchmem.size()-12, 12);
		uint32_t crc_in_e = checks.u32();
		uint32_t crc_out_e = checks.u32();
		uint32_t crc_patch_e = checks.u32();
		
		uint32_t crc_in_a = crc32(inmem);
		uint32_t crc_patch_a = crc32(patchmem.slice(0, patchmem.size()-4));
		
		if (crc_patch_a != crc_patch_e) error(e_broken);
		
		size_t inlen;
		decodeto(inlen);
		
		size_t outlen;
		decodeto(outlen);
		
		if (inlen!=inmem.size() || (crc_in_a!=crc_in_e && !accept_wrong_input))
		{
			if (inmem.size()==outlen && crc_in_a==crc_out_e) error=e_to_output;
			else error=e_not_this;
			if (inlen==inmem.size() && !accept_wrong_input) goto exit;
		}
		
		outmem.reserve_noinit(outlen);
		membufwriter out = outmem;
		
		size_t inreadat = 0;
		size_t outreadat = 0;
		
		size_t metadatalen;
		decodeto(metadatalen);
		patch.bytes(metadatalen); // discard this, you can grab it from info::parse
		
		while (patch.remaining())
		{
			size_t thisinstr;
			decodeto(thisinstr);
			size_t length=(thisinstr>>2)+1;
			int action=(thisinstr&3);
			if (length > out.remaining()) error(e_broken);
			
			switch (action)
			{
				case SourceRead:
				{
					if (out.pos()+length > inmem.size()) error(e_broken);
					out.write(inmem.slice(out.pos(), length));
				}
				break;
				case TargetRead:
				{
					if (length > patch.remaining()) error(e_broken);
					out.write(patch.bytes(length));
				}
				break;
				case SourceCopy:
				{
					size_t encodeddistance;
					decodeto(encodeddistance);
					size_t distance=encodeddistance>>1;
					if ((encodeddistance&1)==0)
					{
						inreadat+=distance;
						if (inreadat+length > inmem.size()) error(e_broken);
					}
					else
					{
						if (distance > inreadat) error(e_broken);
						inreadat-=distance;
					}
					
					out.write(inmem.slice(inreadat, length));
					inreadat+=length;
				}
				break;
				case TargetCopy:
				{
					size_t encodeddistance;
					decodeto(encodeddistance);
					size_t distance=encodeddistance>>1;
					if ((encodeddistance&1)==0)
					{
						outreadat+=distance;
						if (outreadat+length > out.size()) error(e_broken);
						if (outreadat >= out.pos()) error(e_broken);
					}
					else
					{
						if (distance > outreadat) error(e_broken);
						outreadat-=distance;
					}
					
					size_t outreadstart = outreadat;
					outreadat += length;
					
					while (outreadstart+length > out.pos())
					{
						size_t chunk = out.pos()-outreadstart;
						out.write(outmem.slice(outreadstart, chunk));
						length -= chunk;
					}
					out.write(outmem.slice(outreadstart, length));
				}
				break;
			}
		}
		if (out.remaining() != 0) error(e_broken);
		
		uint32_t crc_out_a = crc32(outmem);
		
		if (crc_out_a!=crc_out_e)
		{
			error=e_not_this;
			if (!accept_wrong_input) goto exit;
		}
		
		return error;
#undef decodeto
#undef error
	}
	
exit:
	outmem.resize(0);
	return error;
}




result info::parse(arrayview<byte> data, bool changefrac)
{
	size_t len = data.size();
	if (len<4+3+12) return e_broken;
	
	memstream patch = data.slice(0, data.size()-12);
	if (!patch.signature("BPS1")) return e_broken;
	
	if (!patch.bpsnum(&this->size_in)) return e_too_big;
	if (!patch.bpsnum(&this->size_out)) return e_too_big;
	
	size_t metasize;
	if (!patch.bpsnum(&metasize)) return e_too_big;
	this->metadata = patch.bytes(metasize);
	
	if (changefrac && this->size_in>0)
	{
		//algorithm: each command adds its length to the numerator, unless it's above 32, in which case
		// it adds 32; or if it's SourceRead, in which case it adds 0
		//denominator is just input length
		size_t outpos=0; // position in the output file
		size_t changeamt=0; // change score
		
		while (patch.remaining() && outpos<this->size_in)
		{
			size_t thisinstr;
			patch.bpsnum(&thisinstr);
			size_t length=(thisinstr>>2)+1;
			int action=(thisinstr&3);
			int min_len_32 = (length<32 ? length : 32);
			
			switch (action)
			{
				case SourceRead:
				{
					changeamt+=0;
				}
				break;
				case TargetRead:
				{
					changeamt+=min_len_32;
					patch.bytes(length);
				}
				break;
				case SourceCopy:
				case TargetCopy:
				{
					changeamt+=min_len_32;
					size_t ignore;
					patch.bpsnum(&ignore);
				}
				break;
			}
			outpos+=length;
		}
		this->change_num = (changeamt<this->size_in ? changeamt : this->size_in);
		this->change_denom = this->size_in;
	}
	else
	{
		//this also happens if change fraction is not requested, but if so, reading it is undefined behaviour anyways.
		this->change_num=1;
		this->change_denom=1;
	}
	
	memstream checks = data.slice(data.size()-12, 12);
	this->crc_in  = checks.u32();
	this->crc_out = checks.u32();
	
	return e_ok;
}



#if 0
#warning Disable this in release versions.

#include <stdio.h>

//Congratulations, you found the undocumented feature! It compares two equivalent BPS patches and
//  tells where each one is more compact. (It crashes or gives bogus answers on invalid or
//  non-equivalent patches.) Have fun.
void bps_compare(struct mem patch1mem, struct mem patch2mem)
{
	const uint8_t * patch[2]={patch1mem.ptr, patch2mem.ptr};
	size_t patchpos[2]={0,0};
	size_t patchlen[2]={patch1mem.len-12, patch2mem.len-12};
	size_t patchoutpos[2]={0,0};
	
	size_t patchcopypos[2][4]={0,0};//[0] and [1] are unused, but this is just debug code, it doesn't need to be neat.
	
#define read8(id) (patch[id][patchpos[id]++])
#define decodeto(id, var) \
				do { \
					var=0; \
					int shift=0; \
					while (true) \
					{ \
						uint8_t next=read8(id); \
						size_t addthis=(next&0x7F)<<shift; \
						var+=addthis; \
						if (next&0x80) break; \
						shift+=7; \
						var+=1<<shift; \
					} \
				} while(false)
	
	size_t lastmatch=0;
	size_t patchposatmatch[2]={0,0};
	
	size_t outlen;
	patch[0]+=4; patch[1]+=4;//BPS1
	size_t tempuint;
	decodeto(0, tempuint); decodeto(1, tempuint);//source-size
	decodeto(0, outlen); decodeto(1, outlen);//target-size
	decodeto(0, tempuint); patch[0]+=tempuint;//metadata
	decodeto(1, tempuint); patch[1]+=tempuint;//metadata
	
	bool show=false;
	while (patchpos[0]<patchlen[0] && patchpos[1]<patchlen[1])
	{
		bool step[2]={(patchoutpos[0]<=patchoutpos[1]), (patchoutpos[0]>=patchoutpos[1])};
		char describe[2][256];
		for (int i=0;i<2;i++)
		{
			if (step[i])
			{
				size_t patchposstart=patchpos[i];
				decodeto(i, tempuint);
				size_t len=(tempuint>>2)+1;
				patchoutpos[i]+=len;
				int action=(tempuint&3);
//enum { SourceRead, TargetRead, SourceCopy, TargetCopy };
				const char * actionnames[]={"SourceRead", "TargetRead", "SourceCopy", "TargetCopy"};
				if (action==TargetRead) patchpos[i]+=len;
				if (action==SourceCopy || action==TargetCopy)
				{
					decodeto(i, tempuint);
					int delta = tempuint>>1;
					if (tempuint&1) delta=-delta;
					patchcopypos[i][action]+=delta;
					sprintf(describe[i], "%s from %i (%+i) for %i in %i",  actionnames[action], patchcopypos[i][action], delta, len, patchpos[i]-patchposstart);
					patchcopypos[i][action]+=len;
				}
				else sprintf(describe[i], "%s from %i for %i in %i",  actionnames[action], patchoutpos[i], len, patchpos[i]-patchposstart);
				if (!step[i^1])
				{
					printf("%i: %s\n", i+1, describe[i]);
					show=true;
				}
			}
		}
		if (step[0] && step[1])
		{
			if (!strcmp(describe[0], describe[1])) /*printf("3: %s\n", describe[0])*/;
			else
			{
				printf("1: %s\n2: %s\n", describe[0], describe[1]);
				show=true;
			}
		}
		if (patchoutpos[0]==patchoutpos[1])
		{
			size_t used[2]={patchpos[0]-patchposatmatch[0], patchpos[1]-patchposatmatch[1]};
			char which='=';
			if (used[0]<used[1]) which='+';
			if (used[0]>used[1]) which='-';
			if (show)
			{
				printf("%c: %i,%i bytes since last match (%i)\n", which, used[0], used[1], patchoutpos[0]);
				show=false;
			}
			patchposatmatch[0]=patchpos[0];
			patchposatmatch[1]=patchpos[1];
			lastmatch=patchoutpos[0];
		}
	}
}

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
int main(int argc,char**argv)
{
bps_compare(ReadWholeFile(argv[1]),ReadWholeFile(argv[2]));
}
#endif
}}
