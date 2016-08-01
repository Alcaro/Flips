#include "serialize.h"
#include "test.h"
#include <string.h>

#ifdef ARLIB_TEST
struct serializable_test {
int a;
int b;

onserialize() {
	SER(a);
	SER(b) SER_HEX;
}
};

class serializer_test : public serializer_base<serializer_test> {
public:
	int phase;
	
	template<typename T>
	void serialize(const char * name, T& member, const serialize_opts& opts)
	{
		if(0);
		else if (phase==0 && !strcmp(name, "a") && member==16 && opts.hex==false) phase++;
		else if (phase==1 && !strcmp(name, "b") && member==32 && opts.hex==true) phase++;
		else phase=-1;
		member++;
	}
};

test()
{
	serializer_test s;
	s.phase = 0;
	serializable_test item;
	item.a = 16;
	item.b = 32;
	item.serialize(s);
	
	return s.phase==2 && item.a==17 && item.b==33;
}

//test BML serialization - not sure which file that belongs in, so just pick one
#include "bml.h"

test()
{
	
}
#endif
