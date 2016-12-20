#ifdef _WIN32
#include "aropengl.h"

//https://www.opengl.org/registry/specs/NV/DX_interop.txt
//https://github.com/halogenica/WGL_NV_DX/blob/master/SharedResource.cpp
//https://msdn.microsoft.com/en-us/library/windows/desktop/bb174336(v=vs.85).aspx


#undef bind
#ifdef _MSC_VER
//MSVC's gl.h doesn't seem to include the stuff it should. Copying these five lines from mingw's gl.h...
# if !(defined(WINGDIAPI) && defined(APIENTRY))
#  include <windows.h>
# else
#  include <stddef.h>
# endif
//Also disable a block of code that defines int32_t to something not identical to my msvc-compatible stdint.h.
# define GLEXT_64_TYPES_DEFINED
#endif

#if defined(__has_include)
#if __has_include(<GL/wglext.h>)
#include <GL/wglext.h>
#endif
#endif
#ifndef WGL_WGLEXT_VERSION
#include "../deps/wglext.h"
#endif

#ifdef AROPENGL_D3DSYNC
# include <D3D9.h>

# ifndef D3DPRESENT_FORCEIMMEDIATE
#  define D3DPRESENT_FORCEIMMEDIATE 0x00000100L
# endif
# ifndef D3DPRESENT_DONOTWAIT
#  define D3DPRESENT_DONOTWAIT 0x00000001L
# endif

# define IFD3D(x) x
#else
# define IFD3D(x)
#endif

#define bind bind_func

namespace {

#define WGL_SYMS() \
	WGL_SYM(HGLRC, CreateContext, (HDC hdc)) \
	WGL_SYM(BOOL, DeleteContext, (HGLRC hglrc)) \
	WGL_SYM(HGLRC, GetCurrentContext, ()) \
	WGL_SYM(PROC, GetProcAddress, (LPCSTR lpszProc)) \
	WGL_SYM(BOOL, MakeCurrent, (HDC hdc, HGLRC hglrc)) \

//WINGDIAPI BOOL  WINAPI wglCopyContext(HGLRC, HGLRC, UINT);
//WINGDIAPI HGLRC WINAPI wglCreateContext(HDC);
//WINGDIAPI HGLRC WINAPI wglCreateLayerContext(HDC, int);
//WINGDIAPI BOOL  WINAPI wglDeleteContext(HGLRC);
//WINGDIAPI HGLRC WINAPI wglGetCurrentContext(VOID);
//WINGDIAPI HDC   WINAPI wglGetCurrentDC(VOID);
//WINGDIAPI PROC  WINAPI wglGetProcAddress(LPCSTR);
//WINGDIAPI BOOL  WINAPI wglMakeCurrent(HDC, HGLRC);
//WINGDIAPI BOOL  WINAPI wglShareLists(HGLRC, HGLRC);
//WINGDIAPI BOOL  WINAPI wglUseFontBitmapsA(HDC, DWORD, DWORD, DWORD);
//WINGDIAPI BOOL  WINAPI wglUseFontBitmapsW(HDC, DWORD, DWORD, DWORD);
//#ifdef UNICODE
//#define wglUseFontBitmaps  wglUseFontBitmapsW
//#else
//#define wglUseFontBitmaps  wglUseFontBitmapsA
//#endif // !UNICODE
//WINGDIAPI BOOL  WINAPI SwapBuffers(HDC);

//SwapBuffers and various others are actually in gdi32.dll, which is used elsewhere and can safely be included here too

#define WGL_EXTS() \
	WGL_EXT(PFNWGLSWAPINTERVALEXTPROC, SwapIntervalEXT) /* must be first */ \
	IFD3D( \
		WGL_EXT(PFNWGLDXSETRESOURCESHAREHANDLENVPROC, DXSetResourceShareHandleNV) \
		WGL_EXT(PFNWGLDXOPENDEVICENVPROC, DXOpenDeviceNV) \
		WGL_EXT(PFNWGLDXCLOSEDEVICENVPROC, DXCloseDeviceNV) \
		WGL_EXT(PFNWGLDXREGISTEROBJECTNVPROC, DXRegisterObjectNV) \
		WGL_EXT(PFNWGLDXUNREGISTEROBJECTNVPROC, DXUnregisterObjectNV) \
		WGL_EXT(PFNWGLDXOBJECTACCESSNVPROC, DXObjectAccessNV) \
		WGL_EXT(PFNWGLDXLOCKOBJECTSNVPROC, DXLockObjectsNV) \
		WGL_EXT(PFNWGLDXUNLOCKOBJECTSNVPROC, DXUnlockObjectsNV) \
	) \

struct {
#define WGL_SYM(ret, name, args) ret (WINAPI * name) args;
	WGL_SYMS()
#undef WGL_SYM
#define WGL_EXT(type, name) type name;
	WGL_EXTS()
#undef WGL_EXT
	HMODULE lib;
} static wgl;
#define WGL_SYM(ret, name, args) "wgl" #name "\0"
static const char wgl_proc_names[] = WGL_SYMS() ;
#undef WGL_SYM
#define WGL_EXT(type, name) "wgl" #name "\0"
static const char wgl_ext_names[] = WGL_EXTS() ;
#undef WGL_EXT



#ifdef AROPENGL_D3DSYNC
typedef HRESULT (WINAPI * Direct3DCreate9Ex_t)(UINT SDKVersion, IDirect3D9Ex* * ppD3D);
static HMODULE hD3D9=NULL;
static Direct3DCreate9Ex_t lpDirect3DCreate9Ex;

static bool libLoadD3D()
{
	hD3D9=LoadLibrary("d3d9.dll");
	if (!hD3D9) return false;
	//lpDirect3DCreate9=Direct3DCreate9;//these are for verifying that Direct3DCreate9Ex_t matches the real function; they're not needed anymore
	//lpDirect3DCreate9Ex=Direct3DCreate9Ex;
	lpDirect3DCreate9Ex=(Direct3DCreate9Ex_t)GetProcAddress(hD3D9, "Direct3DCreate9Ex");
	if (!lpDirect3DCreate9Ex) { FreeLibrary(hD3D9); return false; }
	//if (!lpDirect3DCreate9Ex) return false;
	return true;
}

static void libUnloadD3D()
{
	FreeLibrary(hD3D9);
}
#endif

static bool libLoadGL()
{
	//this can yield multiple unsynchronized writers to global variables
	//however, this is safe, because they all write the same values in the same order
	//(except if writing that cache line also discards other changes to the same cache line, but that just won't happen.)
	wgl.lib = LoadLibrary("opengl32.dll");
	if (!wgl.lib) return false;
	
	//HMODULE gdilib=GetModuleHandle("gdi32.dll");
	
	const char * names = wgl_proc_names;
	FARPROC* functions = (FARPROC*)&wgl;
	
	while (*names)
	{
		*functions = GetProcAddress(wgl.lib, names);
		if (!*functions) return false;
		
		functions++;
		names += strlen(names)+1;
	}
	
	return true;
}

static void libUnloadGL()
{
	if (wgl.lib) FreeLibrary(wgl.lib);
}

class aropengl_windows : public aropengl::context {
public:
	HWND GL_hwnd;
	HDC GL_hdc;
	HGLRC GL_hglrc;
	
#ifdef AROPENGL_D3DSYNC
	bool d3dsync;
	
	HWND D3D_hwnd;
	
	IDirect3DDevice9Ex* D3D_device;
	IDirect3DSurface9* D3D_backbuf;
	IDirect3DSurface9* D3D_GLtarget;
	
	HANDLE D3D_sharehandle;
	HANDLE D3D_sharetexture;
	HANDLE GL_htexture;
	
	GLuint GL_fboname;
	GLuint GL_texturename;
	
	bool vsync;
#endif
	
	/*private*/ bool init(HWND parent, HWND* window_, uint32_t flags)
	{
		DWORD glwndflags = WS_CHILD | WS_VISIBLE;
#ifdef AROPENGL_D3DSYNC
		this->d3dsync = (flags & aropengl::t_direct3d_vsync);
		if (this->d3dsync) glwndflags &= ~WS_VISIBLE;
#endif
		
		this->GL_hwnd = CreateWindow("arlib", NULL, glwndflags, 0, 0, 1, 1, parent, NULL, NULL, NULL);
		
		*window_ = this->GL_hwnd;
		this->GL_hdc = GetDC(this->GL_hwnd);
		this->GL_hglrc = NULL;
		
		if (!libLoadGL()) return false;
		if (!CreateContext(flags)) return false;
		
#ifdef AROPENGL_D3DSYNC
		if (this->d3dsync)
		{
			this->D3D_hwnd = CreateWindow("arlib", NULL, WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, parent, NULL, NULL, NULL);
			*window_ = this->D3D_hwnd;
			D3D_sharehandle = NULL;
			D3D_sharetexture = NULL;
			GL_htexture = NULL;
			
			if (!libLoadD3D()) return false;
			if (!CreateD3DContext()) return false;
			if (!JoinGLD3D()) return false;
		}
#endif
		
		return true;
	}
	
	///*private*/ HWND CreateDummyWindow(HWND parent)
	//{
	//	WNDCLASS wc = {};
	//	wc.lpfnWndProc = DefWindowProc;
	//	wc.lpszClassName = "arlib_opengl_dummy";
	//	RegisterClass(&wc);
	//	
	//	return CreateWindow("arlib", NULL, WS_CHILD, -1, 0, 1, 1, parent, NULL, NULL, NULL);
	//	//return CreateWindow("arlib", "OPENGL", WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
	//}
	
	/*private*/ bool CreateContext(uint32_t flags)
	{
		if (wgl.GetCurrentContext()) return false;
		
		bool debug = (flags & aropengl::t_debug_context);
		bool depthbuf = (flags & aropengl::t_depth_buffer);
		bool stenbuf = (flags & aropengl::t_stencil_buffer);
		uint32_t version = (flags & 0xFFF);
		
		PIXELFORMATDESCRIPTOR pfd;
		memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
		pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 24;
		pfd.cAlphaBits = 0;
		pfd.cAccumBits = 0;
		pfd.cDepthBits = (stenbuf ? 24 : depthbuf ? 16 : 0);
		pfd.cStencilBits = (stenbuf ? 8 : 0);
		pfd.cAuxBuffers = 0;
#ifdef AROPENGL_D3DSYNC
		if (this->d3dsync)
		{
			pfd.dwFlags &= ~PFD_DOUBLEBUFFER;
			pfd.cDepthBits = 0;
			pfd.cStencilBits = 0;
		}
#endif
		pfd.iLayerType = PFD_MAIN_PLANE;
		SetPixelFormat(this->GL_hdc, ChoosePixelFormat(this->GL_hdc, &pfd), &pfd);
		this->GL_hglrc = wgl.CreateContext(this->GL_hdc);
		if (!this->GL_hglrc) return false;
		
		wgl.MakeCurrent(this->GL_hdc, this->GL_hglrc);
		
		if (version >= 310)
		{
			HGLRC hglrc_old = this->GL_hglrc;
			PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribs =
				(PFNWGLCREATECONTEXTATTRIBSARBPROC)wgl.GetProcAddress("wglCreateContextAttribsARB");
			const int attribs[] = {
				WGL_CONTEXT_MAJOR_VERSION_ARB, (int)version/100,
				WGL_CONTEXT_MINOR_VERSION_ARB, (int)version/10%10,
				WGL_CONTEXT_FLAGS_ARB, debug ? WGL_CONTEXT_DEBUG_BIT_ARB : 0,
				//WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
				//https://www.opengl.org/wiki/Core_And_Compatibility_in_Contexts says do not use
			0 };
			this->GL_hglrc = wglCreateContextAttribs(this->GL_hdc, /*share*/NULL, attribs);
			wgl.DeleteContext(hglrc_old);
			
			if (!this->GL_hglrc)
			{
				wgl.MakeCurrent(NULL, NULL);
				return false;
			}
			
			wgl.MakeCurrent(this->GL_hdc, this->GL_hglrc);
		}
		
		const char * names = wgl_ext_names;
		FARPROC* functions = (FARPROC*)&wgl.SwapIntervalEXT;
		
		while (*names)
		{
			*functions = wgl.GetProcAddress(names);
#ifdef AROPENGL_D3DSYNC
			if (d3dsync && !*functions) return false; // this demands wglSwapIntervalEXT even for d3d sync, but that one is supported by everything.
#endif
			
			functions++;
			names += strlen(names)+1;
		}
		if (!wgl.SwapIntervalEXT) return false;
		
		return true;
	}
	
#ifdef AROPENGL_D3DSYNC
	/*private*/ bool CreateD3DContext()
	{
		IDirect3D9Ex* d3d;
		this->D3D_device = NULL;
		
		if (FAILED(lpDirect3DCreate9Ex(D3D_SDK_VERSION, &d3d))) return false;
		
		D3DPRESENT_PARAMETERS parameters = {};
		parameters.BackBufferCount = 2; // D3DPRESENT_FORCEIMMEDIATE|D3DPRESENT_DONOTWAIT doesn't work without this
		parameters.SwapEffect = D3DSWAPEFFECT_FLIPEX;
		parameters.hDeviceWindow = this->D3D_hwnd;
		parameters.Windowed = TRUE;
		//https://msdn.microsoft.com/en-us/library/windows/desktop/bb172585(v=vs.85).aspx
		//_ONE is _DEFAULT, but also calls timeBeginPeriod to improve precision
		//anything opting in to Direct3D vsync is clearly a high-performance program, and thus wants the increased precision
		parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		
		if (FAILED(d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, this->D3D_hwnd,
		                               D3DCREATE_MIXED_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,
		                               &parameters, NULL, &this->D3D_device)))
		{
			return false;
		}
		
		d3d->Release();
		
		return true;
	}
	
	/*private*/ bool JoinGLD3D()
	{
		//a bit untidy, but this object doesn't have access to the real one
#define SYM(type, name) type name = (type)this->getProcAddress(STR(name)); if (!name) return false
		typedef void (GLAPIENTRY * PFNGLGENTEXTURES)(GLsizei n, GLuint *textures);
		typedef void (GLAPIENTRY * PFNGLBINDTEXTURE)(GLenum target, GLuint texture);
		SYM(PFNGLGENTEXTURES, glGenTextures);
		SYM(PFNGLBINDTEXTURE, glBindTexture);
		SYM(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers);
		SYM(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer);
		SYM(PFNGLFRAMEBUFFERTEXTUREPROC, glFramebufferTexture);
#undef SYM
		
		glGenTextures(1, &GL_texturename);
		glBindTexture(GL_TEXTURE_2D, GL_texturename);
		
		D3D_sharehandle = wgl.DXOpenDeviceNV(this->D3D_device);
		
		this->D3D_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &this->D3D_backbuf);
		
		AllocRenderTarget();
		
		//the framebuffer must be bound after calling AllocRenderTarget, or the Nvidia driver claims the framebuffer is incomplete
		//this bug can be fixed by querying the current FBO and binding that, which shouldn't have any effect
		//additionally, the Intel driver is happy with either order
		//I suspect driver bug of some kind
		glGenFramebuffers(1, &GL_fboname);
		glBindFramebuffer(GL_FRAMEBUFFER, GL_fboname);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_texturename, 0);
		
		return true;
	}
	
	/*private*/ void AllocRenderTarget()
	{
		RECT wndsize;
		GetClientRect(this->D3D_hwnd, &wndsize);
		this->D3D_device->CreateRenderTarget(wndsize.right, wndsize.bottom, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, false, &this->D3D_GLtarget, &D3D_sharetexture);
		
		wgl.DXSetResourceShareHandleNV(this->D3D_GLtarget, D3D_sharetexture);
		this->GL_htexture = wgl.DXRegisterObjectNV(this->D3D_sharehandle, this->D3D_GLtarget, GL_texturename, GL_TEXTURE_2D, WGL_ACCESS_WRITE_DISCARD_NV);
		
		wgl.DXLockObjectsNV(D3D_sharehandle, 1, &this->GL_htexture);
	}
	
	/*private*/ void DeallocRenderTarget()
	{
		if (!this->D3D_GLtarget) return;
		
		wgl.DXUnlockObjectsNV(D3D_sharehandle, 1, &this->GL_htexture);
		wgl.DXUnregisterObjectNV(D3D_sharehandle, this->GL_htexture);
		
		this->D3D_GLtarget->Release();
		this->D3D_GLtarget = NULL;
		
		//those D3D share handles are weird stuff like 0xC0007000, closing them throws errors
		//I'll assume it's freed by deleting the rendertarget, device or IDirect3D9Ex, and that reusing it does not leak
		//CloseHandle(D3D_sharetexture);
		//D3D_sharetexture = NULL;
	}
#endif
	
	
	
	void makeCurrent(bool make)
	{
		if (make) wgl.MakeCurrent(this->GL_hdc, this->GL_hglrc);
		else wgl.MakeCurrent(NULL, NULL);
	}
	
	funcptr getProcAddress(const char * proc)
	{
		PROC ret = wgl.GetProcAddress(proc);
		if (!ret) ret = ::GetProcAddress(wgl.lib, proc); // lol windows
		return (funcptr)ret;
	}
	
	void swapInterval(int interval)
	{
#ifdef AROPENGL_D3DSYNC
		if (d3dsync) vsync = (interval==1);
		else
#endif
			wgl.SwapIntervalEXT(interval);
	}
	
	void swapBuffers()
	{
#ifdef AROPENGL_D3DSYNC
		if (d3dsync)
		{
			wgl.DXUnlockObjectsNV(D3D_sharehandle, 1, &this->GL_htexture);
			this->D3D_device->StretchRect(this->D3D_GLtarget, NULL, this->D3D_backbuf, NULL, D3DTEXF_NONE);
			this->D3D_device->PresentEx(NULL, NULL, NULL, NULL, (vsync ? 0 : D3DPRESENT_FORCEIMMEDIATE|D3DPRESENT_DONOTWAIT));
			wgl.DXLockObjectsNV(D3D_sharehandle, 1, &this->GL_htexture);
		}
		else
#endif
			::SwapBuffers(this->GL_hdc);
	}
	
#ifdef AROPENGL_D3DSYNC
	void notifyResize(GLsizei width, GLsizei height)
	{
		if (d3dsync)
		{
			DeallocRenderTarget();
			AllocRenderTarget();
		}
	}
	
	GLuint outputFramebuffer()
	{
		if (d3dsync) return GL_fboname;
		else return 0;
	}
#endif
	
	void destroy()
	{
		//early return to ensure the libUnload functions aren't called too much
		//this is the first member set in init()
		if (!this->GL_hwnd) return;
		
#ifdef AROPENGL_D3DSYNC
		if (this->d3dsync)
		{
			DeallocRenderTarget();
			if (D3D_sharehandle) wgl.DXCloseDeviceNV(D3D_sharehandle);
			
			if (this->D3D_device) this->D3D_device->Release();
			if (this->D3D_backbuf) this->D3D_backbuf->Release();
			
			//don't bother cleaning up the GL resources, wglDeleteContext does that already
			if (this->D3D_hwnd) DestroyWindow(this->D3D_hwnd);
			
			libUnloadD3D();
		}
#endif
		
		if (this->GL_hglrc && wgl.DeleteContext) wgl.DeleteContext(this->GL_hglrc);
		if (this->GL_hdc) ReleaseDC(this->GL_hwnd, this->GL_hdc);
		
		DestroyWindow(this->GL_hwnd);
		this->GL_hwnd = NULL;
		
		libUnloadGL();
	}
	
	~aropengl_windows() { destroy(); }
};

}

aropengl::context* aropengl::context::create(uintptr_t parent, uintptr_t* window, uint32_t flags)
{
	aropengl_windows* ret = new aropengl_windows();
	if (ret->init((HWND)parent, (HWND*)window, flags)) return ret;
	
	delete ret;
	return NULL;
}
#endif
