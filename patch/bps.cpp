#include "patch.h"

namespace patch { namespace bps {
static uint32_t read32(uint8_t * ptr)
{
	uint32_t out;
	out =ptr[0];
	out|=ptr[1]<<8;
	out|=ptr[2]<<16;
	out|=ptr[3]<<24;
	return out;
}

enum { SourceRead, TargetRead, SourceCopy, TargetCopy };

static bool try_add(size_t& a, size_t b)
{
	if (SIZE_MAX-a < b) return false;
	a+=b;
	return true;
}

static bool try_shift(size_t& a, size_t b)
{
	if (SIZE_MAX>>b < a) return false;
	a<<=b;
	return true;
}

static bool decodenum(const uint8_t*& ptr, size_t& out)
{
	out=0;
	unsigned int shift=0;
	while (true)
	{
		uint8_t next=*ptr++;
		size_t addthis=(next&0x7F);
		if (shift) addthis++;
		if (!try_shift(addthis, shift)) return false;
		// unchecked because if it was shifted, the lowest bit is zero, and if not, it's <=0x7F.
		if (!try_add(out, addthis)) return false;
		if (next&0x80) return true;
		shift+=7;
	}
}

#define error(which) do { error=which; goto exit; } while(0)
#define assert_sum(a,b) do { if (SIZE_MAX-(a)<(b)) error(bps_too_big); } while(0)
#define assert_shift(a,b) do { if (SIZE_MAX>>(b)<(a)) error(bps_too_big); } while(0)
result apply(const file& patch_, const file& source_, file& target_, bool accept_wrong_input)
{
	struct mem patch = patch_.mmap();
	struct mem in = source_.mmap();
	struct mem out_;
	struct mem * out = &out_;
	struct mem * metadata = NULL;
	
	result error = e_ok;
	out->len=0;
	out->ptr=NULL;
	if (metadata)
	{
		metadata->len=0;
		metadata->ptr=NULL;
	}
	if (patch.len<4+3+12) return e_broken;
	
	if (true)
	{
#define read8() (*(patchat++))
#define decodeto(var) \
				do { \
					if (!decodenum(patchat, var)) error(e_too_big); \
				} while(false)
#define write8(byte) (*(outat++)=byte)
		
		const uint8_t * patchat=patch.ptr;
		const uint8_t * patchend=patch.ptr+patch.len-12;
		
		if (read8()!='B') error(e_broken);
		if (read8()!='P') error(e_broken);
		if (read8()!='S') error(e_broken);
		if (read8()!='1') error(e_broken);
		
		uint32_t crc_in_e = read32(patch.ptr+patch.len-12);
		uint32_t crc_out_e = read32(patch.ptr+patch.len-8);
		uint32_t crc_patch_e = read32(patch.ptr+patch.len-4);
		
		uint32_t crc_in_a = crc32(in.v());
		uint32_t crc_patch_a = crc32(patch.v().slice(0, patch.len-4));
		
		if (crc_patch_a != crc_patch_e) error(e_broken);
		
		size_t inlen;
		decodeto(inlen);
		
		size_t outlen;
		decodeto(outlen);
		
		if (inlen!=in.len || crc_in_a!=crc_in_e)
		{
			if (in.len==outlen && crc_in_a==crc_out_e) error=e_to_output;
			else error=e_not_this;
			if (!accept_wrong_input) goto exit;
		}
		
		out->len=outlen;
		out->ptr=(uint8_t*)malloc(outlen);
		
		const uint8_t * instart=in.ptr;
		const uint8_t * inreadat=in.ptr;
		const uint8_t * inend=in.ptr+in.len;
		
		uint8_t * outstart=out->ptr;
		uint8_t * outreadat=out->ptr;
		uint8_t * outat=out->ptr;
		uint8_t * outend=out->ptr+out->len;
		
		size_t metadatalen;
		decodeto(metadatalen);
		
		if (metadata && metadatalen)
		{
			metadata->len=metadatalen;
			metadata->ptr=(uint8_t*)malloc(metadatalen+1);
			for (size_t i=0;i<metadatalen;i++) metadata->ptr[i]=read8();
			metadata->ptr[metadatalen]='\0';//just to be on the safe side - that metadata is assumed to be text, might as well terminate it
		}
		else
		{
			for (size_t i=0;i<metadatalen;i++) (void)read8();
		}
		
		while (patchat<patchend)
		{
			size_t thisinstr;
			decodeto(thisinstr);
			size_t length=(thisinstr>>2)+1;
			int action=(thisinstr&3);
			if (outat+length>outend) error(e_broken);
			
			switch (action)
			{
				case SourceRead:
				{
					if (outat-outstart+length > in.len) error(e_broken);
					for (size_t i=0;i<length;i++)
					{
						size_t pos = outat-outstart; // don't inline, write8 changes outat
						write8(instart[pos]);
					}
				}
				break;
				case TargetRead:
				{
					if (patchat+length>patchend) error(e_broken);
					for (size_t i=0;i<length;i++) write8(read8());
				}
				break;
				case SourceCopy:
				{
					size_t encodeddistance;
					decodeto(encodeddistance);
					size_t distance=encodeddistance>>1;
					if ((encodeddistance&1)==0) inreadat+=distance;
					else inreadat-=distance;
					
					if (inreadat<instart || inreadat+length>inend) error(e_broken);
					for (size_t i=0;i<length;i++) write8(*inreadat++);
				}
				break;
				case TargetCopy:
				{
					size_t encodeddistance;
					decodeto(encodeddistance);
					size_t distance=encodeddistance>>1;
					if ((encodeddistance&1)==0) outreadat+=distance;
					else outreadat-=distance;
					
					if (outreadat<outstart || outreadat>=outat || outreadat+length>outend) error(e_broken);
					for (size_t i=0;i<length;i++) write8(*outreadat++);
				}
				break;
			}
		}
		if (patchat!=patchend) error(e_broken);
		if (outat!=outend) error(e_broken);
		
		uint32_t crc_out_a = crc32(out->v());
		
		if (crc_out_a!=crc_out_e)
		{
			error=e_not_this;
			if (!accept_wrong_input) goto exit;
		}
		
		target_.write(out->v());
		free(out->ptr);
		return error;
#undef read8
#undef decodeto
#undef write8
	}
	
exit:
	free(out->ptr);
	out->len=0;
	out->ptr=NULL;
	if (metadata)
	{
		free(metadata->ptr);
		metadata->len=0;
		metadata->ptr=NULL;
	}
	return error;
}
#undef error




result info::parse(const file& patch, bool changefrac)
{
	size_t len = patch.size();
	if (len<4+3+12) return e_broken;
	
	uint8_t top[256];
	size_t toplen = len>256 ? 256 : len;
	if (patch.read(arrayvieww<byte>(top, toplen), 0) < toplen) return e_io;
	if (memcmp(top, "BPS1", 4)!=0) return e_broken;
	
	const uint8_t* patchdat=top+4;
	if (!decodenum(patchdat, this->size_in)) return e_too_big;
	if (!decodenum(patchdat, this->size_out)) return e_too_big;
	
	uint8_t checksums[12];
	if (patch.read(arrayvieww<byte>(checksums), len-12) < 12) return e_io;
	this->crc_in  = read32(checksums+0);
	this->crc_out = read32(checksums+4);
	//this->crc_patch=read32(checksums+8);
	
	if (changefrac && this->size_in>0)
	{
		//algorithm: each command adds its length to the numerator, unless it's above 32, in which case
		// it adds 32; or if it's SourceRead, in which case it adds 0
		//denominator is just input length
		array<byte> patchbytes = patch.read();
		size_t outpos=0; // position in the output file
		size_t changeamt=0; // change score
		const uint8_t* patchat=patchbytes.ptr()+(patchdat-top);
		
		size_t metasize;
		if (!decodenum(patchat, metasize)) return e_too_big;
		patchat+=metasize;
		
		const uint8_t* patchend=patchbytes.ptr()+len-12;
		
		while (patchat<patchend && outpos<this->size_in)
		{
			size_t thisinstr;
			decodenum(patchat, thisinstr);
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
					patchat+=length;
				}
				break;
				case SourceCopy:
				case TargetCopy:
				{
					changeamt+=min_len_32;
					size_t ignore;
					decodenum(patchat, ignore);
				}
				break;
			}
			outpos+=length;
		}
		if (patchat>patchend || outpos>this->size_out) return e_broken;
		this->change_num = (changeamt<this->size_in ? changeamt : this->size_in);
		this->change_denom = this->size_in;
	}
	else
	{
		//this also happens if change fraction is not requested, but if so, reading it is undefined behaviour anyways.
		this->change_num=1;
		this->change_denom=1;
	}
	
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
