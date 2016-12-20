#include "window.h"
#include "../file.h"
#ifdef ARGUI_GTK3
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#ifdef ARGUIPROT_X11
#include <gdk/gdkx.h>
#endif

//BUG - The viewport widget does not resize the parent's status bar if resize()d.

static bool in_callback=false;



widget_padding::widget_padding(bool vertical)
{
	widget=GTK_DRAWING_AREA(gtk_drawing_area_new());
	widthprio=(vertical ? 0 : 2);
	heightprio=(vertical ? 2 : 0);
}

widget_padding::~widget_padding() {}



struct widget_layout::impl {
	widget_base* * children;
	unsigned int numchildren;
};

void widget_layout::construct(unsigned int numchildren, widget_base* * children,
                             unsigned int totwidth,  unsigned int * widths,  bool uniformwidths,
                             unsigned int totheight, unsigned int * heights, bool uniformheights)
{
	m=new impl;
	
	GtkGrid* grid=GTK_GRID(gtk_grid_new());
	widget=grid;
	
	m->numchildren=numchildren;
	m->children=malloc(sizeof(struct widget_base*)*numchildren);
	memcpy(m->children, children, sizeof(struct widget_base*)*numchildren);
	
	widthprio=0;
	heightprio=0;
	
	bool posused[totheight*totwidth];
	memset(posused, 0, sizeof(posused));
	unsigned int firstempty=0;
	for (unsigned int i=0;i<numchildren;i++)
	{
		while (posused[firstempty]) firstempty++;
		
		unsigned int width=(widths ? widths[i] : 1);
		unsigned int height=(heights ? heights[i] : 1);
		
		gtk_grid_attach(grid, GTK_WIDGET(children[i]->widget),
		                firstempty%totwidth, firstempty/totwidth,
		                width, height);
		
		for (unsigned int x=0;x<width ;x++)
		for (unsigned int y=0;y<height;y++)
		{
			posused[firstempty + y*totwidth + x]=true;
		}
		
		if (children[i]->widthprio  > widthprio)  widthprio =children[i]->widthprio;
		if (children[i]->heightprio > heightprio) heightprio=children[i]->heightprio;
	}
	
	for (unsigned int i=0;i<numchildren;i++)
	{
		gtk_widget_set_hexpand(GTK_WIDGET(children[i]->widget), (children[i]->widthprio  == widthprio));
		gtk_widget_set_vexpand(GTK_WIDGET(children[i]->widget), (children[i]->heightprio == heightprio));
	}
}

widget_layout::~widget_layout()
{
	for (unsigned int i=0;i<m->numchildren;i++)
	{
		delete m->children[i];
	}
	free(m->children);
	delete m;
}



widget_label::widget_label(const char * text)
{
	widget=gtk_label_new(text);
	widthprio=1;
	heightprio=1;
}

widget_label::~widget_label() {}

widget_label* widget_label::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), enable);
	return this;
}

widget_label* widget_label::set_text(const char * text)
{
	gtk_label_set_text(GTK_LABEL(widget), text);
	return this;
}

widget_label* widget_label::set_ellipsize(bool ellipsize)
{
	if (ellipsize)
	{
		gtk_label_set_ellipsize(GTK_LABEL(widget), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(widget), 1);//why does this work
	}
	else
	{
		gtk_label_set_ellipsize(GTK_LABEL(widget), PANGO_ELLIPSIZE_NONE);
		gtk_label_set_max_width_chars(GTK_LABEL(widget), -1);
	}
	return this;
}

widget_label* widget_label::set_alignment(int alignment)
{
	gtk_misc_set_alignment(GTK_MISC(widget), ((float)alignment)/2, 0.5);
	return this;
}



struct widget_button::impl {
	function<void()> onclick;
};

widget_button::widget_button(const char * text) : m(new impl)
{
	widget=gtk_button_new_with_label(text);
	widthprio=1;
	heightprio=1;
}

widget_button::~widget_button()
{
	delete m;
}

widget_button* widget_button::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), enable);
	return this;
}

widget_button* widget_button::set_text(const char * text)
{
	gtk_button_set_label(GTK_BUTTON(widget), text);
	return this;
}

static void widget_button_onclick(GtkButton* button, gpointer user_data)
{
	widget_button* obj=(widget_button*)user_data;
	obj->m->onclick();
}

widget_button* widget_button::set_onclick(function<void()> onclick)
{
	g_signal_connect(widget, "clicked", G_CALLBACK(widget_button_onclick), this);
	m->onclick=onclick;
	return this;
}



struct widget_checkbox::impl {
	function<void(bool checked)> onclick;
};

widget_checkbox::widget_checkbox(const char * text) : m(new impl)
{
	widget=gtk_check_button_new_with_label(text);
	widthprio=1;
	heightprio=1;
}

widget_checkbox::~widget_checkbox()
{
	delete m;
}

widget_checkbox* widget_checkbox::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), enable);
	return this;
}

widget_checkbox* widget_checkbox::set_text(const char * text)
{
	gtk_button_set_label(GTK_BUTTON(widget), text);
	return this;
}

bool widget_checkbox::get_state()
{
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

widget_checkbox* widget_checkbox::set_state(bool checked)
{
	in_callback=true;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), checked);
	in_callback=false;
	return this;
}

static void widget_checkbox_onclick(GtkButton* button, gpointer user_data)
{
	if (in_callback) return;
	widget_checkbox* obj=(widget_checkbox*)user_data;
	obj->m->onclick(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(obj->widget)));
}

widget_checkbox* widget_checkbox::set_onclick(function<void(bool checked)> onclick)
{
	g_signal_connect(widget, "clicked", G_CALLBACK(widget_checkbox_onclick), this);
	m->onclick=onclick;
	return this;
}



struct widget_radio::impl {
	GtkLabel* label;
	
	unsigned int grouplen;
	widget_radio * * group;
	
	unsigned int id;//if state is set before grouping, this is used as state
	widget_radio * parent;
	
	function<void(unsigned int state)> onclick;
};

static void widget_radio_onclick(GtkToggleButton* togglebutton, gpointer user_data);
widget_radio::widget_radio(const char * text) : m(new impl)
{
	widget=gtk_radio_button_new(NULL);
	g_signal_connect(widget, "toggled", G_CALLBACK(widget_radio_onclick), this);
	m->label=GTK_LABEL(gtk_label_new(text));
	gtk_container_add(GTK_CONTAINER(widget), GTK_WIDGET(m->label));
	widthprio=1;
	heightprio=1;
	
	m->group=NULL;
	m->id=0;
}

widget_radio::~widget_radio()
{
	free(m->group);
	delete m;
}

widget_radio* widget_radio::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), enable);
	return this;
}

widget_radio* widget_radio::set_text(const char * text)
{
	gtk_label_set_text(m->label, text);
	return this;
}

widget_radio* widget_radio::group(unsigned int numitems, widget_radio * * group)
{
	m->parent=this;
	for (unsigned int i=1;i<numitems;i++)
	{
		group[i]->m->parent=this;
		group[i]->m->id=i;
		gtk_radio_button_join_group(GTK_RADIO_BUTTON(group[i]->widget), GTK_RADIO_BUTTON(group[i-1]->widget));
	}
	m->group=malloc(sizeof(widget_radio*)*numitems);
	memcpy(m->group, group, sizeof(widget_radio*)*numitems);
	m->grouplen=numitems;
	in_callback=true;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(group[m->id]->widget), true);
	m->id=0;
	in_callback=false;
	return this;
}

unsigned int widget_radio::get_state()
{
	unsigned int i=0;
	while (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m->group[i]->widget))) i++;
	return i;
}

widget_radio* widget_radio::set_state(unsigned int state)
{
	if (m->group)
	{
		in_callback=true;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->group[state]->widget), true);
		in_callback=false;
	}
	else
	{
		m->id=state;
	}
	return this;
}

static void widget_radio_onclick(GtkToggleButton* togglebutton, gpointer user_data)
{
	if (in_callback) return;
	widget_radio* obj=(widget_radio*)user_data;
	if (!gtk_toggle_button_get_active(togglebutton)) return;
	if (obj->m->parent->m->onclick) obj->m->parent->m->onclick(obj->m->id);
}

widget_radio* widget_radio::set_onclick(function<void(unsigned int state)> onclick)
{
	m->onclick=onclick;
	return this;
}



struct widget_textbox::impl {
	function<void(const char * text)> onchange;
	function<void(const char * text)> onactivate;
};

static void widget_textbox_onchange(GtkEntry* entry, gpointer user_data);
widget_textbox::widget_textbox() : m(new impl)
{
	widget=gtk_entry_new();
	widthprio=3;
	heightprio=1;
	
	gtk_entry_set_width_chars(GTK_ENTRY(widget), 5);
	
	g_signal_connect(widget, "changed", G_CALLBACK(widget_textbox_onchange), this);
	m->onchange=NULL;
}

widget_textbox::~widget_textbox()
{
	delete m;
}

widget_textbox* widget_textbox::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), enable);
	return this;
}

widget_textbox* widget_textbox::focus()
{
	gtk_widget_grab_focus(GTK_WIDGET(widget));
	return this;
}

widget_textbox* widget_textbox::set_text(const char * text)
{
	gtk_entry_set_text(GTK_ENTRY(widget), text);
	return this;
}

widget_textbox* widget_textbox::set_length(unsigned int maxlen)
{
	gtk_entry_set_max_length(GTK_ENTRY(widget), maxlen);
	return this;
}

widget_textbox* widget_textbox::set_width(unsigned int xs)
{
	gtk_entry_set_width_chars(GTK_ENTRY(widget), xs);
	return this;
}

widget_textbox* widget_textbox::set_invalid(bool invalid)
{
	if (invalid)
	{
		static GtkCssProvider* cssprovider = NULL;
		if (!cssprovider)
		{
			cssprovider = gtk_css_provider_new();
			gtk_css_provider_load_from_data(cssprovider,
				"GtkEntry#invalid { background-image: none; background-color: #F66; color: #FFF; }"
				"GtkEntry#invalid:selected { background-color: #3465A4; color: #FFF; }"
				//this selection doesn't look too good, but not terrible either.
				, -1, NULL);
		}
		GtkStyleContext* context=gtk_widget_get_style_context(GTK_WIDGET(widget));
		gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(cssprovider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		gtk_widget_set_name(GTK_WIDGET(widget), "invalid");
		gtk_widget_grab_focus(GTK_WIDGET(widget));
	}
	else
	{
		gtk_widget_set_name(GTK_WIDGET(widget), "x");
	}
	return this;
}

const char * widget_textbox::get_text()
{
	return gtk_entry_get_text(GTK_ENTRY(widget));
}

static void widget_textbox_onchange(GtkEntry* entry, gpointer user_data)
{
	widget_textbox* obj=(widget_textbox*)user_data;
	gtk_widget_set_name(GTK_WIDGET(obj->widget), "x");
	if (obj->m->onchange)
	{
		obj->m->onchange(gtk_entry_get_text(GTK_ENTRY(obj->widget)));
	}
}

widget_textbox* widget_textbox::set_onchange(function<void(const char * text)> onchange)
{
	m->onchange=onchange;
	return this;
}

static void widget_textbox_onactivate(GtkEntry* entry, gpointer user_data)
{
	widget_textbox* obj=(widget_textbox*)user_data;
	obj->m->onactivate(gtk_entry_get_text(GTK_ENTRY(obj->widget)));
}

widget_textbox* widget_textbox::set_onactivate(function<void(const char * text)> onactivate)
{
	g_signal_connect(widget, "activate", G_CALLBACK(widget_textbox_onactivate), this);
	m->onactivate=onactivate;
	return this;
}



class widget_canvas;



struct widget_viewport::impl {
	Window child;
	
	GdkRectangle lastpos;
	
	function<void(unsigned int width, unsigned int height)> onresize;
	function<void()> ondestroy;
	
	//bool hide_mouse;
	//bool hide_mouse_timer_active;
	//
	//guint32 hide_mouse_at;
	//GdkCursor* hidden_cursor;
	//
	//function<void(const char * const * filenames)> on_file_drop;
};

static void viewport_submit_resize(widget_viewport::impl* m)
{
	if (m->child && m->lastpos.width>0)
	{
		XMoveResizeWindow(window_x11.display, m->child,
		                  m->lastpos.x, m->lastpos.y,
		                  m->lastpos.width, m->lastpos.height);
		m->onresize(m->lastpos.width, m->lastpos.height);
	}
}

static void viewport_resize_handler(GtkWidget* widget, GdkRectangle* allocation, gpointer user_data)
{
	widget_viewport* obj = (widget_viewport*)user_data;
	widget_viewport::impl* m = obj->m;
	
	if (memcmp(&m->lastpos, allocation, sizeof(GdkRectangle)) != 0)
	{
		m->lastpos = *allocation;
		viewport_submit_resize(m);
	}
}

widget_viewport::widget_viewport(unsigned int width, unsigned int height) : m(new impl)
{
	widget = gtk_drawing_area_new();
	widthprio = 0;
	heightprio = 0;
	
	//m->hide_mouse_at = 0;
	//m->hidden_cursor = NULL;
	gtk_widget_set_size_request(GTK_WIDGET(widget), width, height);
	
	m->child = 0;
	memset(&m->lastpos, -1, sizeof(m->lastpos));
	g_signal_connect(widget, "size-allocate", G_CALLBACK(viewport_resize_handler), this);
}

widget_viewport::~widget_viewport()
{
	//if (m->hidden_cursor) g_object_unref(m->hidden_cursor);
	m->ondestroy();
	delete m;
}

widget_viewport* widget_viewport::resize(unsigned int width, unsigned int height)
{
	gtk_widget_set_size_request(GTK_WIDGET(widget), width, height);
	//GtkWindow* window=GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widget)));
	//gtk_window_resize(window, 1, 1);
//printf("S=%i,%i\n",width,height);
	return this;
}

uintptr_t widget_viewport::get_parent()
{
	gtk_widget_realize(GTK_WIDGET(this->widget));
	return gdk_x11_window_get_xid(gtk_widget_get_window(GTK_WIDGET(widget)));
}

void widget_viewport::set_child(uintptr_t windowhandle,
                                function<void(unsigned int width, unsigned int height)> onresize,
                                function<void()> ondestroy)
{
	m->child = (Window)windowhandle;
	m->onresize = onresize;
	m->ondestroy = ondestroy;
	viewport_submit_resize(m);
}

//void widget_viewport::get_position(int * x, int * y, unsigned int * width, unsigned int * height)
//{
//	gtk_widget_realize(GTK_WIDGET(widget));
//	GdkWindow* window=gtk_widget_get_window(GTK_WIDGET(widget));
//	gdk_window_get_origin(window, x, y);
//	if (width) *width=gdk_window_get_width(window);
//	if (height) *height=gdk_window_get_height(window);
//	//if (x) *height=gdk_window_get_height(window);
//	//if (y) *height=gdk_window_get_height(window);
//}

//static void viewport_set_hide_cursor_now(widget_viewport* obj, bool hide)
//{
//	GdkWindow* gdkwindow=gtk_widget_get_window(GTK_WIDGET(obj->widget));
//	if (gdkwindow) gdk_window_set_cursor(gdkwindow, hide ? obj->m->hidden_cursor : NULL);
//}
//
//static gboolean viewport_mouse_timeout(gpointer user_data)
//{
//	widget_viewport* obj=(widget_viewport*)user_data;
//	
//	guint32 now=g_get_monotonic_time()/1000;
//	if (now >= obj->m->hide_mouse_at)
//	{
//		obj->m->hide_mouse_timer_active=false;
//		viewport_set_hide_cursor_now(obj, obj->m->hide_mouse);
//	}
//	else
//	{
//		guint32 remaining=obj->m->hide_mouse_at-now+10;
//		g_timeout_add(remaining, viewport_mouse_timeout, obj);
//	}
//	
//	return G_SOURCE_REMOVE;
//}
//
//static gboolean viewport_mouse_move_handler(GtkWidget* widget, GdkEvent* event, gpointer user_data)
//{
//	widget_viewport* obj=(widget_viewport*)user_data;
//	
//	obj->m->hide_mouse_at=g_get_monotonic_time()/1000 + 990;
//	if (!obj->m->hide_mouse_timer_active)
//	{
//		obj->m->hide_mouse_timer_active=true;
//		g_timeout_add(1000, viewport_mouse_timeout, obj);
//		viewport_set_hide_cursor_now(obj, false);
//	}
//	
//	return G_SOURCE_CONTINUE;
//}
//
//widget_viewport* widget_viewport::set_hide_cursor(bool hide)
//{
//	if (m->hide_mouse_at && g_get_monotonic_time()/1000 >= m->hide_mouse_at)
//	{
//		viewport_set_hide_cursor_now(this, hide);
//	}
//	
//	m->hide_mouse=hide;
//	if (!hide || m->hide_mouse_at) return this;
//	
//	if (!m->hidden_cursor) m->hidden_cursor=gdk_cursor_new(GDK_BLANK_CURSOR);
//	
//	gtk_widget_add_events(GTK_WIDGET(widget), GDK_POINTER_MOTION_MASK);
//	g_signal_connect(widget, "motion-notify-event", G_CALLBACK(viewport_mouse_move_handler), this);
//	
//	//seems to not exist in gtk+ 3.8
//	//and gdk_event_request_motions does nothing, either - am I building for an older GTK+ than I'm using?
//	//gdk_window_set_event_compression(gtk_widget_get_window(this->i._base.widget), false);
//	
//	m->hide_mouse_timer_active=false;
//	viewport_mouse_move_handler(NULL, NULL, this);
//	
//	return this;
//}

/*
void (*keyboard_cb)(struct window * subject, unsigned int keycode, void* userdata);
void* keyboard_ud;

//static gboolean kb_signal(GtkWidget* widget, GdkEvent* event, gpointer user_data)
//{
//	struct window_gtk3 * this=(struct window_gtk3*)user_data;
//	return FALSE;
//}

static void set_kb_callback(struct window * this_,
                            void (*keyboard_cb)(struct window * subject, unsigned int keycode, void* userdata), void* userdata)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	gtk_widget_add_events(GTK_WIDGET(this->wndw), GDK_KEY_PRESS_MASK);
	//g_signal_connect(this->contents, "key-press-event", G_CALLBACK(kb_signal), this);
	this->keyboard_cb=keyboard_cb;
	this->keyboard_ud=userdata;
}
*/

//static void viewport_drop_handler(GtkWidget* widget, GdkDragContext* drag_context, gint x, gint y,
//                                  GtkSelectionData* selection_data, guint info, guint time, gpointer user_data)
//{
//	widget_viewport* obj=(widget_viewport*)user_data;
//	if (!selection_data || !gtk_selection_data_get_length(selection_data))
//	{
//		gtk_drag_finish(drag_context, FALSE, FALSE, time);
//		return;
//	}
//	
//	const char * data=(gchar*)gtk_selection_data_get_data(selection_data);
//	int numstr=0;
//	for (int i=0;data[i];i++)
//	{
//		if (data[i]=='\n') numstr++;
//	}
//	
//	char* datacopy=strdup(data);
//	char** strings=malloc(sizeof(char*)*(numstr+1));
//	char* last=datacopy;
//	int strnum=0;
//	for (int i=0;datacopy[i];i++)
//	{
//		if (datacopy[i]=='\r') datacopy[i]='\0';//where did those come from? this isn't Windows, we shouldn't be getting Windows-isms.
//		if (datacopy[i]=='\n')
//		{
//			datacopy[i]='\0';
//			strings[strnum]=window_get_absolute_path(NULL, last, true);
//			last=datacopy+i+1;
//			strnum++;
//		}
//	}
//	strings[numstr]=NULL;
//	free(datacopy);
//	
//	obj->m->on_file_drop(strings);
//	
//	for (int i=0;strings[i];i++) free(strings[i]);
//	free(strings);
//	gtk_drag_finish(drag_context, TRUE, FALSE, time);
//}
//
//widget_viewport* widget_viewport::set_support_drop(function<void(const char * const * filenames)> on_file_drop)
//{
//	GtkTargetList* list=gtk_target_list_new(NULL, 0);
//	gtk_target_list_add_uri_targets(list, 0);
//	
//	int n_targets;
//	GtkTargetEntry* targets=gtk_target_table_new_from_list(list, &n_targets);
//	//GTK_DEST_DEFAULT_MOTION|GTK_DEST_DEFAULT_DROP
//	gtk_drag_dest_set(GTK_WIDGET(widget), GTK_DEST_DEFAULT_ALL, targets,n_targets, GDK_ACTION_COPY);
//	
//	gtk_target_table_free(targets, n_targets);
//	gtk_target_list_unref(list);
//	
//	g_signal_connect(widget, "drag-data-received", G_CALLBACK(viewport_drop_handler), this);
//	m->on_file_drop=on_file_drop;
//	
//	return this;
//}



class widget_listbox;



struct widget_frame::impl {
	struct widget_base* child;
};

widget_frame::widget_frame(const char * text, widget_base* child) : m(new impl)
{
	widget=gtk_frame_new(text);
	widthprio=child->widthprio;
	heightprio=child->heightprio;
	m->child=child;
	gtk_container_add(GTK_CONTAINER(widget), GTK_WIDGET(child->widget));
}

widget_frame::~widget_frame()
{
	delete m->child;
	delete m;
}

widget_frame* widget_frame::set_text(const char * text)
{
	gtk_frame_set_label(GTK_FRAME(m->child), text);
	return this;
}
#endif
