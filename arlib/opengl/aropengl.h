#pragma once
#include "../global.h"
#include "../gui/window.h"

#if !defined(_WIN32) || __has_include(<GL/glext.h>)
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include "../deps/gl.h"
#include "../deps/glext.h"
#endif

#ifndef GLAPIENTRY
#ifdef _WIN32
#define GLAPIENTRY APIENTRY
#else
#define GLAPIENTRY
#endif
#endif

class aropengl : nocopy {
public:
	enum {
		t_ver_1_0 = 100, t_ver_1_1 = 110, t_ver_1_2 = 120, t_ver_1_3 = 130, t_ver_1_4 = 140, t_ver_1_5 = 150,
		t_ver_2_0 = 200, t_ver_2_1 = 210,
		t_ver_3_0 = 300, t_ver_3_1 = 310, t_ver_3_2 = 320, t_ver_3_3 = 330,
		t_ver_4_0 = 400, t_ver_4_1 = 410, t_ver_4_2 = 420, t_ver_4_3 = 430, t_ver_4_4 = 440, t_ver_4_5 = 450,
		
		t_opengl_es      = 0x001000, // Probably not supported.
		t_debug_context  = 0x002000, // Requests a debug context. Doesn't actually enable debugging, use gl.enableDefaultDebugger or gl.DebugMessageControl/etc.
		t_depth_buffer   = 0x004000, // These two only apply to the main buffer. You can always create additional FBOs with or without depth/stencil.
		t_stencil_buffer = 0x008000,
		
#ifdef AROPENGL_D3DSYNC
		//Direct3D vsync is an advanced feature that uses WGL_NV_DX_interop and D3DSWAPEFFECT_FLIPEX to ensure smooth framerate on Windows.
		//Advantages:
		//- Less stuttering, especially with DWM enabled (at least on some computers, sometimes vsync is already smooth)
		//Disadvantages:
		//- Requires Windows 7 or newer
		//- Some graphics cards and drivers are not compatible
		//- Poorly tested driver path, may be slow or buggy (in fact, I believe I found a Nvidia driver bug while creating this)
		//- You may not render to the default framebuffer, 0; you must render to gl.defaultFramebuffer()
		//    (if you don't use framebuffers, you can ignore this; defaultFramebuffer is bound on creation)
		//- You must call gl.notifyResize() whenever the window is resized (whether by the application or the user), in addition to gl.Viewport/etc
		//- Swap intervals other than 0 and 1 are not supported, not even -1
		//- May be slower, especially with vsync off
		//- D/S buffers are currently not created (TODO: remove limitation)
		//The flag is ignored on non-Windows systems.
		//It is safe to use gl.defaultFramebuffer and gl.notifyResize on non-d3dsync objects.
# ifdef _WIN32
		t_direct3d_vsync = 0x010000,
# else
		t_direct3d_vsync = 0,
#  undef AROPENGL_D3DSYNC
# endif
#endif
	};
	
	class context : nocopy {
	public:
		//this is basically the common subset of WGL/GLX/etc
		//you want the outer class, as it offers proper extension/symbol management
		static context* create(uintptr_t parent, uintptr_t* window, uint32_t flags);
		
		virtual void makeCurrent(bool make) = 0; // If false, releases the context. The context is current on creation.
		virtual void swapInterval(int interval) = 0;
		virtual void swapBuffers() = 0;
		virtual funcptr getProcAddress(const char * proc) = 0;
#ifdef AROPENGL_D3DSYNC
		virtual
#endif
		void notifyResize(unsigned int width, unsigned int height) {}
		
#ifdef AROPENGL_D3DSYNC
		virtual
#endif
		GLuint outputFramebuffer() { return 0; }
		
		virtual void destroy() = 0;
		//implementations must ensure the destructor is safe even after having its window destroyed
		//the best method is putting everything into destroy() and having the destructor just call that
		virtual ~context() {}
	};
	
	bool create(context* core);
	
	bool create(uintptr_t parent, uintptr_t* window, uint32_t flags)
	{
		return create(context::create(parent, window, flags));
	}
	
	bool create(widget_viewport* port, uint32_t flags)
	{
		uintptr_t newwindow;
		if (!create(port->get_parent(), &newwindow, flags)) return false;
		this->port = port;
		port->set_child(newwindow,
		                bind_ptr(&aropengl::context::notifyResize, this->core),
		                bind_ptr(&aropengl::destroy, this));
		return true;
	}
	
	aropengl() { create(NULL); }
	aropengl(context* core) { create(core); }
	aropengl(uintptr_t parent, uintptr_t* window, uint32_t flags) { create(parent, window, flags); }
	aropengl(widget_viewport* port, uint32_t flags) { create(port, flags); }
	explicit operator bool() { return core!=NULL; }
	
	~aropengl()
	{
		destroy();
	}
	
	//Arlib usually uses underscores, but since OpenGL doesn't, this object follows suit.
	//To ensure no collisions, Arlib-specific functions start with a lowercase (or are C++-only, like operator bool), standard GL functions are uppercase.
	
	//If false, releases the context. The context is current on creation.
	void makeCurrent(bool make) { core->makeCurrent(make); }
	void swapInterval(int interval) { core->swapInterval(interval); }
	void swapBuffers() { core->swapBuffers(); }
	funcptr getProcAddress(const char * proc) { return core->getProcAddress(proc); }
	
	//If the window is resized, use this function to report the new size.
	//Not needed if the object is created from a viewport.
	void notifyResize(GLsizei width, GLsizei height) { core->notifyResize(width, height); }
	//Used only for Direct3D sync. If you're not using that, just use 0.
	GLuint outputFramebuffer() { return core->outputFramebuffer(); }
	
	//Releases all resources owned by the object; the object may not be used after this.
	//Use if the destructor isn't guaranteed to run while the driver's window still exists.
	//Not needed if the object is created from a viewport.
	void destroy()
	{
		if (port)
		{
			port->set_child(0, NULL, NULL);
			port = NULL;
		}
		delete core;
		core = NULL;
	}
	
	//void (GLAPIENTRY * ClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
	//void (GLAPIENTRY * Clear)(GLbitfield mask);
	//etc
#define AROPENGL_GEN_HEADER
#include "generated.c"
#undef AROPENGL_GEN_HEADER
	//It's intended that this object is named 'gl', resulting in gl.Clear(GL_etc), somewhat like WebGLRenderingContext.
	//It is not guaranteed that a non-NULL function will actually work, or even successfully return. Check gl.hasExtension.
	
	bool hasExtension(const char * ext);
	void enableDefaultDebugger(FILE* out = NULL); //Use only if the context was created with the debug flag.
	
private:
	context* core;
	widget_viewport* port;
};
