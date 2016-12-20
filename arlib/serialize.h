#pragma once
#include "global.h"
#include "bml.h"
#include "stringconv.h"

#define SERIALIZE_CORE(member) s(STR(member), member);
#define SERIALIZE(...) template<typename T> void serialize(T& s) { PPFOREACH(SERIALIZE_CORE, __VA_ARGS__); }

class bmlserialize_impl {
	bmlwriter w;
	template<typename T> friend string bmlserialize(T& item);
	
public:
	
	static const bool serializing = true;
	
	template<typename T> void operator()(cstring name, T& item)
	{
		w.enter(name, "");
		item.serialize(*this);
		w.exit();
	}
	
#define LEAF(T) void operator()(cstring name, T& item) { w.node(name, tostring(item)); }
	ALLSTRINGABLE(LEAF);
#undef LEAF
};

template<typename T> string bmlserialize(T& item)
{
	bmlserialize_impl s;
	item.serialize(s);
	return s.w.finish();
}



class bmlunserialize_impl {
	bmlparser p;
	int pdepth = 0;
	
	int thisdepth = 0;
	cstring thisnode;
	cstring thisval;
	bool matchagain;
	
	bmlparser::event event()
	{
		bmlparser::event ret = p.next();
		if (ret.action == bmlparser::enter) pdepth++;
		if (ret.action == bmlparser::exit) pdepth--;
		if (ret.action == bmlparser::finish) pdepth=-2;
		return ret;
	}
	
	void skipchildren()
	{
		while (pdepth > thisdepth) event();
	}
	
	bmlunserialize_impl(cstring bml) : p(bml) {}
	template<typename T> friend T bmlunserialize(cstring bml);
	
	template<typename T> void item(T& out)
	{
		while (pdepth >= thisdepth)
		{
			bmlparser::event ev = event();
			if (ev.action == bmlparser::enter)
			{
				thisdepth++;
				thisnode = ev.name;
				thisval = ev.value;
				do {
					matchagain = false;
					out.serialize(*this);
				} while (matchagain);
				thisdepth--;
				skipchildren();
			}
		}
	}
	
	void next()
	{
		matchagain = false;
		
		if (pdepth >= thisdepth)
		{
			thisdepth--;
			skipchildren();
			
			bmlparser::event ev = event();
			if (ev.action == bmlparser::enter)
			{
				matchagain = true;
				thisnode = ev.name;
				thisval = ev.value;
			}
			
			thisdepth++;
		}
	}
	
#define LEAF(T) void item(T& out) { fromstring(thisval, out); }
	ALLSTRINGABLE(LEAF);
#undef LEAF
	
public:
	
	static const bool serializing = false;
	
	template<typename T> void operator()(cstring name, T& out)
	{
		while (thisnode == name) // this should be a loop, in case of documents like 'foo bar=1 bar=2 bar=3'
		{
			item(out);
			thisnode = "";
			next();
		}
	}
};

template<typename T> T bmlunserialize(cstring bml)
{
	T out{};
	bmlunserialize_impl s(bml);
	s.item(out);
	return out;
}
