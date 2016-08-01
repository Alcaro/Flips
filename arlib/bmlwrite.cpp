#include "bml.h"
#include "test.h"

//This is also streaming. It may disobey the mode if the value is not supported; for example, val!="" on mode=anon won't work.
//It also disobeys mode <= inl_col on enter(), you need node() for that.
//Calling exit() without a matching enter(), or finish() without closing every enter(), is undefined behavior.
class bmlwriter {
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
	
	bmlwriter() { m_indent = 0; }
	
	void enter(cstring name, cstring val, mode m); // Always uses mode=eq or higher.
	void exit() { m_indent--; }
	void linebreak();
	void comment(cstring text);
	void node(cstring name, cstring val, mode m);
	
	//Tells what mode will actually be used if node() is called with these parameters.
	mode typeof(cstring val, mode m) const;
	
	string finish() { return m_data; }
};


#ifdef ARLIB_TEST

#endif
