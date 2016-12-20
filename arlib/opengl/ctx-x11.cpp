#ifdef ARGUIPROT_X11
#include "aropengl.h"

//TODO: wipe -lGL dependency
//TODO: fix SwapInterval
//TODO: wipe printfs

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <dlfcn.h>

namespace {

#define GLX_SYMS() \
	/* GLX 1.0 */ \
	GLX_SYM(funcptr, GetProcAddress, (const GLubyte * procName)) \
	GLX_SYM(void, SwapBuffers, (Display* dpy, GLXDrawable drawable)) \
	GLX_SYM(Bool, MakeCurrent, (Display* dpy, GLXDrawable drawable, GLXContext ctx)) \
	GLX_SYM(Bool, QueryVersion, (Display* dpy, int* major, int* minor)) \
	GLX_SYM(GLXContext, GetCurrentContext, ()) \
	/* GLX 1.3 */ \
	GLX_SYM(GLXFBConfig*, ChooseFBConfig, (Display* dpy, int screen, const int * attrib_list, int * nelements)) \
	GLX_SYM(XVisualInfo*, GetVisualFromFBConfig, (Display* dpy, GLXFBConfig config)) \
	GLX_SYM(int, GetFBConfigAttrib, (Display* dpy, GLXFBConfig config, int attribute, int* value)) \
	GLX_SYM(void, DestroyContext, (Display* dpy, GLXContext ctx)) \

struct {
#define GLX_SYM(ret, name, args) ret (*name) args;
	GLX_SYMS()
#undef GLX_SYM
	PFNGLXSWAPINTERVALSGIPROC SwapIntervalSGI;
	void* lib;
} static glx;
#define GLX_SYM(ret, name, args) "glX" #name "\0"
static const char glx_proc_names[]={ GLX_SYMS() };
#undef GLX_SYM

bool libLoad()
{
	glx.lib = dlopen("libGL.so", RTLD_LAZY);
	if (!glx.lib) return false;
	
	const char * names = glx_proc_names;
	funcptr* functions=(funcptr*)&glx;
	while (*names)
	{
		*functions = (funcptr)dlsym(glx.lib, names);
		if (!*functions) return false;
		
		functions++;
		names += strlen(names)+1;
	}
	
	return true;
}

void libUnload()
{
	if (glx.lib) dlclose(glx.lib);
}



class aropengl_x11 : public aropengl::context {
public:
	GLXContext ctx;
	Window win;
	bool current;
	
	/*private*/ bool init(Window parent, Window* window_, uint32_t flags)
	{
		ctx = None;
		win = None;
		current = False;
		
		if (!libLoad()) return false;
		if (glx.GetCurrentContext()) return false;
		
		bool debug = (flags & aropengl::t_debug_context);
		bool depthbuf = (flags & aropengl::t_depth_buffer);
		bool stenbuf = (flags & aropengl::t_stencil_buffer);
		uint32_t version = (flags & 0xFFF);
		
		int glx_major = 0;
		int glx_minor = 0;
		if (!glx.QueryVersion( window_x11.display, &glx_major, &glx_minor )) return false;
		if (glx_major != 1 || glx_minor < 3) return false;
		
		// Get a matching FB config
		int visual_attribs[] = {
			GLX_X_RENDERABLE,  True,
			GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
			GLX_RED_SIZE,      8,
			GLX_GREEN_SIZE,    8,
			GLX_BLUE_SIZE,     8,
			GLX_DEPTH_SIZE,    (stenbuf ? 24 : depthbuf ? 16 : 0),
			GLX_STENCIL_SIZE,  (stenbuf ? 8 : 0),
			GLX_DOUBLEBUFFER,  True,
			None
		};
		
		int fbcount;
		GLXFBConfig* fbcs = glx.ChooseFBConfig(window_x11.display, window_x11.screen, visual_attribs, &fbcount);
		if (!fbcs) return false;
		GLXFBConfig fbc = fbcs[0];
		XFree(fbcs);
		
		XVisualInfo* vi = glx.GetVisualFromFBConfig( window_x11.display, fbc );
		
		XSetWindowAttributes swa;
		swa.colormap = XCreateColormap(window_x11.display, parent, vi->visual, AllocNone );
		swa.background_pixmap = None;
		swa.border_pixel      = 0;
		swa.event_mask        = StructureNotifyMask;
		
		win = XCreateWindow(window_x11.display, parent, 0, 0, 1, 1, 0,
		                    vi->depth, InputOutput, vi->visual, CWBorderPixel|CWColormap|CWEventMask, &swa );
		if (!win) return false;
		
		*window_ = win;
		XFreeColormap(window_x11.display, swa.colormap);
		XFree(vi);
		
		XMapWindow(window_x11.display, win);
		
		PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB =
			(PFNGLXCREATECONTEXTATTRIBSARBPROC)glx.GetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");
		if (!glXCreateContextAttribsARB) return false;
		
		int context_attribs[] = {
			GLX_CONTEXT_MAJOR_VERSION_ARB, (int)version/100,
			GLX_CONTEXT_MINOR_VERSION_ARB, (int)version/10%10,
			GLX_CONTEXT_FLAGS_ARB, debug ? GLX_CONTEXT_DEBUG_BIT_ARB : 0,
			None
		};
		
		ctx = glXCreateContextAttribsARB(window_x11.display, fbc, 0, True, context_attribs);
		if (!ctx) return false;
		
		XSync(window_x11.display, False);
		
		makeCurrent(true);
		glx.SwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)glx.GetProcAddress((const GLubyte*)"glXSwapIntervalSGI");
		return true;
	}
	
	
	
	void makeCurrent(bool make)
	{
		if (make) glx.MakeCurrent( window_x11.display, win, ctx );
		else glx.MakeCurrent( window_x11.display, None, NULL );
	}
	
	funcptr getProcAddress(const char * proc)
	{
		return (funcptr)glx.GetProcAddress((GLubyte*)proc);
	}
	
	void swapInterval(int interval)
	{
		//EXT isn't supported on my glx client/server
		//MESA isn't in my headers
		//that leaves only one
		glx.SwapIntervalSGI(interval);
	}
	
	void swapBuffers()
	{
		glx.SwapBuffers(window_x11.display, win);
	}
	
	void destroy()
	{
		glx.MakeCurrent(window_x11.display, None, NULL);
		glx.DestroyContext(window_x11.display, ctx);
		
		XDestroyWindow(window_x11.display, win);
		libUnload();
	}
	
	~aropengl_x11() { destroy(); }
};

}

aropengl::context* aropengl::context::create(uintptr_t parent, uintptr_t* window, uint32_t flags)
{
	aropengl_x11* ret = new aropengl_x11();
	if (ret->init((Window)parent, (Window*)window, flags)) return ret;
	
	delete ret;
	return NULL;
}
#endif
