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
//would yield { enter, parent, "" } { enter, child, 1 } { exit } { exit } { enter, parent2, "" } { exit }.
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
		               // putting error first would be cleaner in the parser, but reader clarity is more important, and this name is better
	};
	
	//Remember the cstring rules: If this cstring doesn't hold a reference, don't touch its buffer until the object is disposed.
	//If it's originally a string, don't worry about it.
	//It is not allowed to try to stream data into this object.
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

//This is also streaming.
//Calling exit() without a matching enter(), or finish() without closing every enter(), is undefined behavior.
class bmlwriter {
	string m_data;
	int m_indent = 0;
	bool m_caninline = false;
	
	inline string indent();
	
public:
	enum mode {
		ianon,   // parent node
		ieq,     // parent node=value
		iquote,  // parent node="value"
		icol,    // parent node: value
		
		anon,    // node
		eq,      // node=value
		quote,   // node="value"
		col,     // node: value
		
		multiline // node\n  :value
	};
	
	//If you pass in data that's not valid for that mode (for example, val="foo bar" and mode=eq won't work),
	// then it silently switches to the lowest working mode (in the above example, quote).
	//Since enter() implies the tag has children, it will also disobey the inline modes; use node() for that.
	void enter(cstring name, cstring val, mode m = anon);
	void exit();
	void linebreak();
	void comment(cstring text);
	void node(cstring name, cstring val, mode m = ianon); // Equivalent to enter()+exit(), except supports inline nodes.
	
	//TODO: inline nodes on multilines? They're ridiculously ugly, but there are a few cases where they're useful.
	//I'll decide what to do once I actually need a multiline with children. Maybe add mode multiline_i or something.
	
	//Tells what mode will actually be used if node() is called with these parameters and in this context.
	//To ask what enter() would do, call this with a non-inline mode.
	mode typeof(cstring val, mode m) const;
	
private:
	void node(cstring name, cstring val, mode m, bool enter);
	static mode typeof_core(cstring val);
public:
	
	string finish() { return m_data; }
};
