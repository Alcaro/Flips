#include "bml.h"
#include "test.h"

inline string bmlwriter::indent()
{
	string ret;
	arrayvieww<byte> bytes = ret.construct(m_indent*2);
	memset(bytes.ptr(), ' ', m_indent*2);
	return ret;
}

void bmlwriter::node(cstring name, cstring val, mode m, bool enter)
{
	m = typeof(val, m);
	bool inlined = (m <= icol && m_caninline && !enter);
	if (m_data)
	{
		if (inlined) m_data += " ";
		else m_data += "\n"+indent();
	}
	
	m_indent++;
	
	if ((m==anon || m==eq || m==quote) && enter) m_caninline = true;
	if (m==icol || m==col || m==multiline) m_caninline = false;
	//for other modes, inlinability isn't affected
	
	switch (m)
	{
		case ianon:
		case anon:
			m_data += name;
			break;
		case ieq:
		case eq:
			m_data += name+"="+val;
			break;
		case iquote:
		case quote:
			m_data += name+"=\""+val+"\"";
			break;
		case icol:
		case col:
			m_data += name+": "+val;
			break;
		case multiline:
			string prefix = "\n"+indent()+":";
			m_data += name + prefix + val.replace("\n", prefix);
			break;
	}
	if (!enter) m_indent--;
}

void bmlwriter::comment(cstring c)
{
	if (m_data) m_data += "\n"+indent();
	m_data += "#"+c;
	m_caninline = false;
}

void bmlwriter::enter(cstring name, cstring val, mode m) { node(name, val, m, true); }
void bmlwriter::node(cstring name, cstring val, mode m) { node(name, val, m, false); }
void bmlwriter::exit() { m_indent--; m_caninline = false; }

bmlwriter::mode bmlwriter::typeof_core(cstring val)
{
	if (val.contains("\n") || val[0]==' ' || val[0]=='\t' || val[~0]==' ' || val[~0]=='\t') return multiline;
	if (val.contains("\"")) return col;
	if (val.contains(" ") || val.contains("\t")) return quote;
	if (val!="") return eq;
	return anon;
}

bmlwriter::mode bmlwriter::typeof(cstring val, mode m) const
{
	bool inlined = (m <= icol);
	if (m <= icol) m = (mode)(m+4);
	
	m = max(typeof_core(val), m);
	if (inlined && m != multiline && m_caninline) return (mode)(m-4);
	else return m;
}


#ifdef ARLIB_TEST
test()
{
	{
		bmlwriter w;
		w.enter("a", "");
		w.node("b", "1");
		w.node("c", "");
		w.exit();
		
		assert_eq(w.finish(), "a b=1 c");
	}
	
	{
		bmlwriter w;
		w.enter("a", "");
		w.enter("b", "");
		w.enter("c", "");
		w.enter("d", "");
		w.exit();
		w.exit();
		w.exit();
		w.exit();
		
		assert_eq(w.finish(), "a\n  b\n    c\n      d");
	}
	
	{
		bmlwriter w;
		w.enter("a", "foo bar");
		w.node("b", "1");
		w.node("c", "foo \"bar\"");
		w.node("d", "");
		w.exit();
		
		assert_eq(w.finish(), "a=\"foo bar\" b=1 c: foo \"bar\"\n  d");
	}
	
	{
		bmlwriter w;
		w.enter("a", "foo bar");
		w.node("b", "1");
		w.enter("c", "foo \"bar\"");
		w.node("d", "");
		w.exit();
		w.node("e", "");
		w.exit();
		
		assert_eq(w.finish(), "a=\"foo bar\" b=1\n  c: foo \"bar\"\n    d\n  e");
	}
	
	{
		bmlwriter w;
		w.enter("a", "foo\nbar");
		w.node("b", "");
		w.node("c", "foo\nbar");
		w.enter("d", "foo\nbar");
		w.exit();
		w.exit();
		
		assert_eq(w.finish(), "a\n  :foo\n  :bar\n  b\n  c\n    :foo\n    :bar\n  d\n    :foo\n    :bar");
	}
	
	{
		bmlwriter w;
		w.node("a", "");
		w.enter("b", "");
		w.exit();
		
		assert_eq(w.finish(), "a\nb");
	}
	
	{
		bmlwriter w;
		w.node("a", "1");
		w.node("b", "2");
		
		assert_eq(w.finish(), "a=1\nb=2"); // these must not be children of each other
	}
	
	{
		bmlwriter w;
		w.enter("a", "");
		w.node("b", "c\nd");
		w.exit();
		
		assert_eq(w.finish(), "a\n  b\n    :c\n    :d"); // ensure this is properly non-inlined
	}
	
	//repeat some earlier tests with extra comments
	{
		bmlwriter w;
		w.enter("a", "");
		w.node("b", "1");
		w.comment("x");
		w.node("c", "");
		w.exit();
		
		assert_eq(w.finish(), "a b=1\n  #x\n  c");
	}
	
	{
		bmlwriter w;
		w.enter("a", "foo bar");
		w.node("b", "1");
		w.comment("x");
		w.node("c", "foo \"bar\"");
		w.node("d", "");
		w.exit();
		
		assert_eq(w.finish(), "a=\"foo bar\" b=1\n  #x\n  c: foo \"bar\"\n  d");
	}
	
	{
		bmlwriter w;
		w.node("a", "");
		w.comment("x");
		w.enter("b", "");
		w.exit();
		
		assert_eq(w.finish(), "a\n#x\nb");
	}
	
	{
		bmlwriter w;
		w.node("a", "1");
		w.comment("x");
		w.node("b", "2");
		
		assert_eq(w.finish(), "a=1\n#x\nb=2");
	}
	
	{
		bmlwriter w;
		w.enter("a", "");
		w.comment("x");
		w.node("b", "c\nd");
		w.comment("x");
		w.exit();
		
		assert_eq(w.finish(), "a\n  #x\n  b\n    :c\n    :d\n  #x");
	}
}

#endif
