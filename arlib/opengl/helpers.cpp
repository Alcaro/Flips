#include "../arlib.h"


const char gl_proc_names[] =
#define AROPENGL_GEN_NAMES
#include "generated.c"
#undef AROPENGL_GEN_NAMES
;

bool aropengl::create(context* core)
{
	this->core = core;
	this->port = NULL;
	if (!core) return false;
	
	const char * names = gl_proc_names;
	funcptr* out = (funcptr*)this;
	
	while (*names)
	{
		*out = core->getProcAddress(names);
		out++;
		names += strlen(names)+1;
	}
	
	return true;
}



//Checks if needle is one of the space-separated words in the haystack. The needle may not contain spaces or be empty.
static bool strtoken(const char * haystack, const char * needle)
{
	//this is annoyingly complex to parse
	//I suspect 'people using fixed-size buffers, then extension list grows and app explodes' isn't the only reason the GL_EXTENSIONS string was deprecated
	int nlen = strlen(needle);
	while (true)
	{
		const char * found = strstr(haystack, needle);
		if (!found) break;
		
		if ((found==haystack || found[-1]==' ') && // ensure the match is the start of a word
				(found[nlen]==' ' || found[nlen]=='\0')) // ensure the match is the end of a word
		{
			return true;
		}
		
		haystack = strchr(found, ' '); // try again, could've found GL_foobar_limited when looking for GL_foobar
		if (!haystack) return false;
	}
	return false;
}

test()
{
	assert(strtoken("aa", "aa"));
	assert(!strtoken("aa", "a"));
	assert(!strtoken("aa", "aaa"));
	assert(strtoken("aa aa aa aa", "aa"));
	assert(!strtoken("aa aa aa aa", "a"));
	assert(!strtoken("aa aa aa aa", "aaa"));
	assert(!strtoken("12345", "1234"));
	assert(!strtoken("12345", "2345"));
	assert(!strtoken("12345", "234"));
	assert(strtoken("1234 123456 2345 123456 0123456 012345 12345 12345", "12345"));
	assert(strtoken("a b b", "a"));
	assert(strtoken("b a b", "a"));
	assert(strtoken("b b a", "a"));
}

bool aropengl::hasExtension(const char * ext)
{
	aropengl& gl = *this;
	int major = strtol((char*)gl.GetString(GL_VERSION), NULL, 0);
	if (major >= 3)
	{
		GLint n;
		this->GetIntegerv(GL_NUM_EXTENSIONS, &n);
		for (GLint i=0;i<n;i++)
		{
			if (!strcmp((char*)gl.GetStringi(GL_EXTENSIONS, i), ext)) return true;
		}
		return false;
	}
	else
	{
		return strtoken((char*)gl.GetString(GL_EXTENSIONS), ext);
	}
}



static void APIENTRY debug_cb(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char * message, void* userParam)
{
	const char * source_s;
	const char * type_s;
	const char * severity_s;
	enum { sev_unk=255, sev_not=0, sev_warn=1, sev_err=2 } severity_l;
	
	switch (source)
	{
		case GL_DEBUG_SOURCE_API:             source_s="API"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   source_s="Window system"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: source_s="Shader compiler"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:     source_s="3rd party"; break;
		case GL_DEBUG_SOURCE_APPLICATION:     source_s="Application"; break;
		case GL_DEBUG_SOURCE_OTHER:           source_s="Other"; break;
		default:                              source_s="Unknown"; break;
	}
	
	switch (type)
	{
		case GL_DEBUG_TYPE_ERROR:               type_s="Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_s="Deprecated behavior"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_s="Undefined behavior"; break;
		case GL_DEBUG_TYPE_PORTABILITY:         type_s="Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE:         type_s="Performance"; break;
		case GL_DEBUG_TYPE_MARKER:              type_s="Marker"; break;
		case GL_DEBUG_TYPE_PUSH_GROUP:          type_s="Push group"; break;
		case GL_DEBUG_TYPE_POP_GROUP:           type_s="Pop group"; break;
		case GL_DEBUG_TYPE_OTHER:               type_s="Other"; break;
		default:                                type_s="Unknown"; break;
	}
	
	switch (severity)
	{
		case GL_DEBUG_SEVERITY_HIGH:         severity_s="High"; severity_l=sev_err; break;
		case GL_DEBUG_SEVERITY_MEDIUM:       severity_s="Medium"; severity_l=sev_warn; break;
		case GL_DEBUG_SEVERITY_LOW:          severity_s="Low"; severity_l=sev_not; break;
		case GL_DEBUG_SEVERITY_NOTIFICATION: severity_s="Notice"; severity_l=sev_not; break;
		default:                             severity_s="Unknown"; severity_l=sev_unk; break;
	}
	
	fprintf((FILE*)userParam, "[GL debug: sev %s, source %s, topic %s: %s]\n", severity_s, source_s, type_s, message);
	
	if (severity_l >= sev_warn) debug_or_exit();
}

void aropengl::enableDefaultDebugger(FILE* out)
{
	aropengl& gl = *this;
	if (!out) out = stderr;
	
	gl.DebugMessageCallback((GLDEBUGPROC)debug_cb, out);//some headers have 'const' on the userdata, some don't
	//https://www.opengl.org/sdk/docs/man/html/glDebugMessageCallback.xhtml says it shouldn't be const
	gl.DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	gl.Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
}
