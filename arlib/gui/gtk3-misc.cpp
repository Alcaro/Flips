#include "window.h"
//#include "../image.h"
//#include "minir.h"
#include "../file.h"
#include "../os.h"
#ifdef ARGUI_GTK3
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <gtk/gtk.h>
#ifdef ARGUIPROT_X11
#include <gdk/gdkx.h>
#else
#error Only X11 supported.
#endif

//Number of ugly hacks: 8
//The status bar is a GtkGrid with GtkLabel, not a GtkStatusbar, because I couldn't get GtkStatusbar
// to cooperate. While the status bar is a GtkBox, I couldn't find how to get rid of its child.
//The status bar manages layout manually (by resizing the GtkLabels), because a GtkGrid with a large
// number of slots seem to assign pixels 3,3,2,2 if told to split 10 pixels to 4 equally sized
// slots, as opposed to 2,3,2,3 or 3,2,3,2.
//Label widgets that ellipsize, as well as status bar labels, are declared with max width 1, and
// then ellipsizes. Apparently they use more space than maximum if they can. This doesn't seem to be
// documented, and is therefore not guaranteed to continue working.
//gtk_main_iteration_do(false) is called twice, so GTK thinks we're idle and sends out the mouse
// move events. Most of the time is spent waiting for A/V drivers, and our mouse move processor is
// cheap. (Likely fixable in GTK+ 3.12, but I'm on 3.8.)
//Refreshing a listbox is done by telling it to redraw, not by telling it that contents have changed.
// It's either that or send tens of thousands of contents-changed events, and I'd rather not.
//gtk_widget_override_background_color does nothing to GtkTextEntry, due to a background-image
// gradient. I had to throw a bit of their fancy CSS at it.
//GtkTreeView has non-constant performance for creating a pile of rows, and gets terrible around
// 100000. I had to cap it to 65536.
//The size of GtkTreeView is complete nonsense. I haven't found how to get it to give out its real
// row height, nor could I figure out what the nonsense I get from the tell-me-your-height functions
// is, but I am sure that whatever tricks I will need to pull fits here.

//static GdkFilterReturn scanfilter(GdkXEvent* xevent, GdkEvent* event, gpointer data)
//{
//	XEvent* ev=(XEvent*)xevent;
//	if (ev->type==Expose) printf("ex=%lu\n", ev->xexpose.window);
//	return GDK_FILTER_CONTINUE;
//}

//#include<sys/resource.h>

#ifdef ARGUIPROT_X11
struct window_x11_info window_x11;
#endif

void window_init(int * argc, char * * argv[])
{
//struct rlimit core_limits;core_limits.rlim_cur=core_limits.rlim_max=64*1024*1024;setrlimit(RLIMIT_CORE,&core_limits);
#ifdef DEBUG
g_log_set_always_fatal((GLogLevelFlags)(G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING));
#endif
#ifdef ARGUIPROT_X11
	XInitThreads();
#endif
	gtk_init(argc, argv);
	_window_init_file();
	//gdk_window_add_filter(NULL,scanfilter,NULL);
//#ifndef NO_ICON
//	struct image img;
//	png_decode(icon_minir_64x64_png,sizeof(icon_minir_64x64_png), &img, fmt_argb8888);
//	//we could tell it how to free this, but it will be used until replaced, and it won't be replaced.
//	gtk_window_set_default_icon(gdk_pixbuf_new_from_data((guchar*)img.pixels, GDK_COLORSPACE_RGB, true, 8, 64,64, 64*4, NULL, NULL));
//#endif
#ifdef ARGUIPROT_X11
	window_x11.display=gdk_x11_get_default_xdisplay();
	window_x11.screen=gdk_x11_get_default_screen();
	window_x11.root=gdk_x11_get_default_root_xwindow();//alternatively XRootWindow(window_x11.display, window_x11.screen)
#endif
	errno=0;
}

//file* file::create(const char * filename)
//{
//	//TODO
//	return create_fs(filename);
//}

//static void * mem_from_g_alloc(void * mem, size_t size)
//{
//	if (g_mem_is_system_malloc()) return mem;
//	
//	if (!size) size=strlen((char*)mem)+1;
//	
//	void * ret=malloc(size);
//	memcpy(ret, mem, size);
//	g_free(ret);
//	return ret;
//}
//
////enum mbox_sev { mb_info, mb_warn, mb_err };
////enum mbox_btns { mb_ok, mb_okcancel, mb_yesno };
//bool window_message_box(const char * text, const char * title, enum mbox_sev severity, enum mbox_btns buttons)
//{
//	//"Please note that GTK_BUTTONS_OK, GTK_BUTTONS_YES_NO and GTK_BUTTONS_OK_CANCEL are discouraged by the GNOME HIG."
//	//I do not listen to advise without a rationale. Tell me which section it violates, and I'll consider it.
//	GtkMessageType sev[3]={ GTK_MESSAGE_OTHER, GTK_MESSAGE_WARNING, GTK_MESSAGE_ERROR };
//	GtkButtonsType btns[3]={ GTK_BUTTONS_OK, GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_YES_NO };
//	GtkWidget* dialog=gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, sev[severity], btns[buttons], "%s", text);
//	gint ret=gtk_dialog_run(GTK_DIALOG(dialog));
//	gtk_widget_destroy(dialog);
//	return (ret==GTK_RESPONSE_ACCEPT || ret==GTK_RESPONSE_OK || ret==GTK_RESPONSE_YES);
//}
//
//const char * const * window_file_picker(window * parent,
//                                        const char * title,
//                                        const char * const * extensions,
//                                        const char * extdescription,
//                                        bool dylib,
//                                        bool multiple)
//{
//	static char * * ret=NULL;
//	if (ret)
//	{
//		char * * del=ret;
//		while (*del)
//		{
//			free(*del);
//			del++;
//		}
//		free(ret);
//		ret=NULL;
//	}
//	
//	GtkFileChooser* dialog=GTK_FILE_CHOOSER(
//	                         gtk_file_chooser_dialog_new(
//	                           title,
//	                           GTK_WINDOW(parent?(void*)parent->_get_handle():NULL),
//	                           GTK_FILE_CHOOSER_ACTION_OPEN,
//	                           "_Cancel",
//	                           GTK_RESPONSE_CANCEL,
//	                           "_Open",
//	                           GTK_RESPONSE_ACCEPT,
//	                           NULL));
//	gtk_file_chooser_set_select_multiple(dialog, multiple);
//	gtk_file_chooser_set_local_only(dialog, dylib);
//	
//	GtkFileFilter* filter;
//	
//	if (*extensions)
//	{
//		filter=gtk_file_filter_new();
//		gtk_file_filter_set_name(filter, extdescription);
//		char extstr[64];
//		extstr[0]='*';
//		extstr[1]='.';
//		while (*extensions)
//		{
//			strcpy(extstr+2, *extensions+(**extensions=='.'));
//			gtk_file_filter_add_pattern(filter, extstr);
//			extensions++;
//		}
//		gtk_file_chooser_add_filter(dialog, filter);
//	}
//	
//	filter=gtk_file_filter_new();
//	gtk_file_filter_set_name(filter, "All files");
//	gtk_file_filter_add_pattern(filter, "*");
//	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
//	
//	if (gtk_dialog_run(GTK_DIALOG(dialog))!=GTK_RESPONSE_ACCEPT)
//	{
//		gtk_widget_destroy(GTK_WIDGET(dialog));
//		return NULL;
//	}
//	
//	GSList * list=gtk_file_chooser_get_uris(dialog);
//	gtk_widget_destroy(GTK_WIDGET(dialog));
//	unsigned int listlen=g_slist_length(list);
//	if (!listlen)
//	{
//		g_slist_free(list);
//		return NULL;
//	}
//	ret=malloc(sizeof(char*)*(listlen+1));
//	
//	char * * retcopy=ret;
//	GSList * listcopy=list;
//	while (listcopy)
//	{
//		*retcopy=window_get_absolute_path(NULL, (char*)listcopy->data, true);
//		g_free(listcopy->data);
//		retcopy++;
//		listcopy=listcopy->next;
//	}
//	ret[listlen]=NULL;
//	g_slist_free(list);
//	return (const char * const *)ret;
//}

void window_run_iter()
{
	gtk_main_iteration_do(false);
	
	//Workaround for GTK thinking we're processing events slower than they come in. We're busy waiting
	// for the AV drivers, and waiting less costs us nothing.
	gtk_main_iteration_do(false);
}

void window_run_wait()
{
	gtk_main_iteration_do(true);
}



#if 0
char * window_get_absolute_path(const char * basepath, const char * path, bool allow_up)
{
	if (!path) return NULL;
	
	GFile* file;
	const char * pathend = NULL; // gcc bug: this initialization does nothing, but gcc whines
	if (basepath)
	{
		pathend = strrchr(basepath, '/');
		gchar * basepath_dir = g_strndup(basepath, pathend+1-basepath);
		file = g_file_new_for_commandline_arg_and_cwd(path, basepath_dir);
		g_free(basepath_dir);
	}
	else
	{
		if (!allow_up) return NULL;
		//not sure if gvfs URIs are absolute or not, so if absolute, let's use the one that works for both
		//if not absolute, demand it's an URI
		//per glib/gconvert.c g_filename_from_uri, file:// URIs are absolute
		if (g_path_is_absolute(path)) file = g_file_new_for_commandline_arg(path);
		else file = g_file_new_for_uri(path);
	}
	
	gchar * ret;
	if (g_file_is_native(file)) ret = g_file_get_path(file);
	else ret = g_file_get_uri(file);
	g_object_unref(file);
	
	if (!ret) return NULL;
	
	if (!allow_up && strncmp(basepath, ret, pathend+1-basepath) != 0)
	{
		g_free(ret);
		return NULL;
	}
	
	return (char*)mem_from_g_alloc(ret, 0);
}

char * window_get_native_path(const char * path)
{
	if (!path) return NULL;
	GFile* file=g_file_new_for_commandline_arg(path);
	gchar * ret=g_file_get_path(file);
	g_object_unref(file);
	if (!ret) return NULL;
	return (char*)mem_from_g_alloc(ret, 0);
}



bool file_read(const char * filename, void* * data, size_t * len)
{
	if (!filename) return false;
	GFile* file=g_file_new_for_commandline_arg(filename);
	if (!file) return false;
	
	char* ret;
	gsize glen;
	if (!g_file_load_contents(file, NULL, &ret, &glen, NULL, NULL))
	{
		g_object_unref(file);
		return false;
	}
	g_object_unref(file);
	if (len) *len=glen;
	*data=mem_from_g_alloc(ret, glen);
	return true;
}

bool file_write(const char * filename, const anyptr data, size_t len)
{
	if (!filename) return false;
	if (!len) return true;
	GFile* file=g_file_new_for_commandline_arg(filename);
	if (!file) return false;
	bool success=g_file_replace_contents(file, data, len, NULL, false, G_FILE_CREATE_NONE, NULL, NULL, NULL);
	g_object_unref(file);
	return success;
}

bool file_read_to(const char * filename, anyptr data, size_t len)
{
	if (!filename) return false;
	if (!len) return true;
	GFile* file=g_file_new_for_commandline_arg(filename);
	if (!file) return false;
	GFileInputStream* io=g_file_read(file, NULL, NULL);
	if (!io)
	{
		g_object_unref(file);
		return false;
	}
	GFileInfo* info=g_file_input_stream_query_info(io, G_FILE_ATTRIBUTE_STANDARD_SIZE, NULL, NULL);
	gsize size=g_file_info_get_size(info);
	if (size!=len) return false;
	gsize actualsize;
	bool success=g_input_stream_read_all(G_INPUT_STREAM(io), data, size, &actualsize, NULL, NULL);
	g_input_stream_close(G_INPUT_STREAM(io), NULL, NULL);
	g_object_unref(file);
	g_object_unref(io);
	g_object_unref(info);
	if (!success || size!=actualsize)
	{
		memset(data, 0, len);
		return false;
	}
	return true;
}


void* file_find_create(const char * path)
{
	if (!path) return NULL;
	GFile* parent=g_file_new_for_path(path);
	GFileEnumerator* children=g_file_enumerate_children(parent, G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
	                                                    G_FILE_QUERY_INFO_NONE, NULL, NULL);
	g_object_unref(parent);
	return children;
}

bool file_find_next(void* find, char * * path, bool * isdir)
{
	if (!find) return false;
	GFileEnumerator* children=(GFileEnumerator*)find;
	
	GFileInfo* child=g_file_enumerator_next_file(children, NULL, NULL);
	if (!child) return false;
	
	*path=strdup(g_file_info_get_name(child));
	*isdir=(g_file_info_get_file_type(child)==G_FILE_TYPE_DIRECTORY);
	g_object_unref(child);
	return true;
}

void file_find_close(void* find)
{
	if (!find) return;
	g_object_unref((GFileEnumerator*)find);
}
#endif
#endif
