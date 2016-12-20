/*
windows:
g++ -DARLIB_D3DTEST -DARLIB_OPENGL -DARGUI_WINDOWS -DAROPENGL_D3DSYNC -std=c++11 -fno-exceptions -fno-rtti -O3 *.cpp ../os.cpp ../gui/*.cpp ../malloc.cpp ../file-win32.cpp -lgdi32 -lcomctl32 -lcomdlg32 -o test.exe && test.exe && del test.exe

linux:
g++ -DARLIB_D3DTEST -DARLIB_OPENGL -DARGUI_GTK3 -DARGUIPROT_X11 -std=c++11 -fno-exceptions -fno-rtti -O3 *.cpp ../os.cpp ../gui/*.cpp ../malloc.cpp ../file-win32.cpp -ldl `pkg-config --cflags --libs gtk+-3.0` -lX11 -static-libgcc -o test
*/

#ifdef ARLIB_D3DTEST
//#define NTDDI_VERSION NTDDI_WS03
//#define _WIN32_IE 0x0600
//#include<windows.h> // must include this early, Arlib sets WIN32_LEAN_AND_MEAN which removes timeBeginPeriod

#include <time.h>
#include <algorithm>
#include <math.h>
#include "../arlib.h"

void math(int* data, int ndata, float& avg, float& stddev)
{
	if (!ndata)
	{
		avg=0;
		stddev=0;
		return;
	}
	
	float sum = 0;
	for (int i=0;i<ndata;i++) sum+=data[i];
	avg = sum/ndata;
	
	float stddevtmp = 0;
	for (int i=0;i<ndata;i++) stddevtmp += (data[i]-avg) * (data[i]-avg);
	stddev = sqrt(stddevtmp / ndata);
}

void process(bool d3d)
{
	widget_viewport* port = widget_create_viewport(300, 200);
	window* wnd = window_create(port);
	
	uint32_t flags = aropengl::t_ver_3_3 | aropengl::t_debug_context;
#ifdef AROPENGL_D3DSYNC
	if (d3d) flags |= aropengl::t_direct3d_vsync;
#endif
	//flags |= aropengl::t_depth_buffer;
	aropengl gl(port, flags);
	if (!gl) return;
	
	wnd->set_visible(true);
	
	gl.enableDefaultDebugger();
	gl.swapInterval(1);
	
	bool black = false;
	
	//int width = 640;
	
#define SKIP 20
#define FRAMES 1800
	int times[SKIP+FRAMES]={};
	
	uint64_t prev = perfcounter();
	
	for (int i=0;i<SKIP+FRAMES;i++)
	{
		window_run_iter();
		
		black = !black;
		
		gl.Viewport(0, 0, 640, 480);
		gl.ClearColor(black, 1-black, 0, 1.0);
		gl.Clear(GL_COLOR_BUFFER_BIT);
		
		gl.swapBuffers();
		
		//width++;
		//if(width>1000)width=500;
		//port->resize(width, 480);
		//gl.notifyResize(width, 480);
		
		uint64_t now = perfcounter();
		times[i] = now-prev;
		prev = now;
	}
	
	delete wnd;
	
	float avg;
	float stddev;
	math(times+SKIP, FRAMES, avg, stddev);
	
	printf("d3d=%i avg=%f stddev=%f ", d3d, avg, stddev);
	std::sort(times+SKIP, times+SKIP+FRAMES);
	printf("min=%i,%i ", times[SKIP+0], times[SKIP+1]);
	printf("max=%i,%i,%i,%i,%i\n", times[SKIP+FRAMES-1], times[SKIP+FRAMES-2], times[SKIP+FRAMES-3], times[SKIP+FRAMES-4], times[SKIP+FRAMES-5]);
}

int main(int argc, char * argv[])
{
	window_init(&argc, &argv);
	//timeBeginPeriod(1);
	//this is supposed to measure how much D3D sync helps, but either
	//- there's nonzero time between SwapBuffers/PresentEx return and the frame is presented
	//- DWM and/or the driver has determined that I want high-quality vsync, and auto enables it
	//- I'm measuring wrong thing
	//- the mere act of mentioning d3d9.dll in the binary scares it into submission
	//because I can't measure any meaningful difference whatsoever.
	//When I first got this working, I had std.dev 4000us for pure GL and 250 for D3D sync, what happened?
	
	for (int i=0;i<5;i++)
	{
		process(false);
#ifdef AROPENGL_D3DSYNC
		process(true);
#endif
	}
}
#endif
