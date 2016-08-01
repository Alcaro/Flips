#pragma once
#include "global.h"
#include "array.h"
#include "string.h"
#include "serialize.h"


//This is a streaming parser. For each node, { enter } then { exit } is returned; more enter/exit pairs may be present between them.
//For example, the document
/*
parent child=1
parent2
*/
//would yield { enter, parent, "" }, { enter, child, 1 }, { exit } { exit } { enter, parent2, "" } { exit }.
//The parser keeps trying after an { error }, giving you a partial view of the damaged document; however,
// there are no guarantees on how much you can see, and it is likely for one error to cause many more, or misplaced nodes.
//enter/exit is always paired, even in the presense of errors.
//After the document ends, { finish } will be returned forever until the object is deleted.
class bmlparser : nocopy {
public:
	enum { enter, exit, error, finish };
	struct event {
		int action;
		cstring name;
		cstring value; // or error message
	};
	
	//Since this takes a cstring, the string must be kept alive until the object is disposed.
	bmlparser(cstring bml) : m_orig_data(bml), m_data(bml), m_exit(false) {}
	event next();
	
private:
	cstring m_orig_data; // keep a reference if we're passed in the only copy of a string object
	cstring m_data;
	cstring m_thisline;
	array<bool> m_indent_step;
	cstring m_indent;
	cstring m_inlines;
	bool m_exit;
	
	inline void getlineraw();
	inline bool getline();
};

//This is also streaming. It may disobey the mode if the value is not supported; for example, val!="" on bml_anon won't help you.
//It also disobeys mode <= bml_inl_col on enter(), you need node().
//Calling exit() without a matching enter(), or finish() without closing every enter(), is undefined behavior.
class bmlwriterx {
	string m_data;
	int m_indent;
	
public:
	enum mode {
		anon,     // parent node
		inl_eq,   // parent node=value
		inl_col,  // parent node: value
		eq,       // node=value
		col,      // node: value
		multiline // node\n  :value
	};
	
	bmlwriterx() { m_indent = 0; }
	
	void enter(cstring name, cstring val, mode m);
	void exit() { m_indent--; }
	void linebreak();
	void comment(cstring text);
	void node(cstring name, cstring val, mode m);
	
	string finish() { return m_data; }
};
