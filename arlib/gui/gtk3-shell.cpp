#include "window.h"
#ifdef ARGUI_GTK3
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gtk/gtk.h>
#ifdef ARGUIPROT_X11
#include <gdk/gdkx.h>
#endif

static gint get_widget_height(GtkWidget* widget)
{
	if (!widget) return 0;
	GtkRequisition size;
	gtk_widget_get_preferred_size(widget, NULL, &size);
	return size.height;
}
static GtkMenuShell* get_menu_bar_widget(windowmenu_menu* menu);

namespace {

static bool in_callback=false;

class window_gtk3 : public window {
public:

GtkWindow* wndw;
GtkGrid* grid;
widget_base* contents;

windowmenu_menu* menu;

GtkGrid* status;
int * status_pos;
int status_count;

bool visible;
//uint8_t delayfree;//0=normal, 1=can't free now, 2=free at next opportunity

//char padding1[2];

function<void(int newwidth, int newheight)> onmove;

function<bool()> onclose;

// /*private*/ void statusbar_resize();

window_gtk3(widget_base* contents_)
{
	this->wndw=GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_gravity(this->wndw, GDK_GRAVITY_STATIC);
	g_signal_connect(this->wndw, "delete-event", G_CALLBACK(onclose_gtk), this);//GtkWidget delete-event maybe
	gtk_window_set_has_resize_grip(this->wndw, false);
	gtk_window_set_resizable(this->wndw, false);
	
	this->grid=GTK_GRID(gtk_grid_new());
	gtk_container_add(GTK_CONTAINER(this->wndw), GTK_WIDGET(this->grid));
	
	this->contents=contents_;
	gtk_grid_attach(this->grid, GTK_WIDGET(this->contents->widget), 0,1, 1,1);
	
	this->menu = NULL;
	this->status = NULL;
	this->status_pos = NULL;
	this->status_count = 0;
	
	this->onclose = NULL;
	
	this->visible=false;
	
//GdkRGBA color={0,0,1,1};
//gtk_widget_override_background_color(GTK_WIDGET(this->wndw),GTK_STATE_FLAG_NORMAL,&color);
}

/*private*/ void resize(unsigned int width, unsigned int height)
{
	gtk_window_resize(this->wndw, width,
	                  (this->menu ? get_widget_height(GTK_WIDGET(this->menu->m)) : 0)+
	                  height+
	                  (this->status ? get_widget_height(GTK_WIDGET(this->status)) : 0));
	if (this->status) this->statusbar_resize();
}

// /*private*/ static gboolean onclose_gtk(GtkWidget* widget, GdkEvent* event, gpointer user_data);
/*private*/ static gboolean popup_esc_close(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	if (event->key.keyval==GDK_KEY_Escape)
	{
		onclose_gtk(widget, event, user_data);
		return TRUE;
	}
	return FALSE;
}

void set_is_dialog()
{
	gtk_widget_add_events(GTK_WIDGET(this->wndw), GDK_KEY_PRESS_MASK);
	g_signal_connect(this->wndw, "key-press-event", G_CALLBACK(popup_esc_close), this);
	
	gtk_window_set_type_hint(this->wndw, GDK_WINDOW_TYPE_HINT_DIALOG);
}

void set_parent(struct window * parent_)
{
	struct window_gtk3 * parent=(struct window_gtk3*)parent_;
	gtk_window_set_transient_for(this->wndw, parent->wndw);
}

void set_modal(bool modal)
{
	gtk_window_set_modal(this->wndw, modal);
}

void set_resizable(bool resizable, function<void(unsigned int newwidth, unsigned int newheight)> onresize)
{
	gtk_window_set_resizable(this->wndw, resizable);
	
	if (this->status) this->statusbar_resize();
}

void get_pos(int * x, int * y)
{
	gtk_window_get_position(this->wndw, x, y);
}

void set_pos(int x, int y)
{
	in_callback=true;
	gdk_window_move(gtk_widget_get_window(GTK_WIDGET(this->wndw)), x, y);
	in_callback=false;
}

/*private*/ static gboolean onmove_activate(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	struct window_gtk3 * p=(struct window_gtk3*)user_data;
	if (!in_callback) p->onmove(event->configure.x, event->configure.y);
	return FALSE;
}

void set_onmove(function<void(int x, int y)> onmove)
{
	this->onmove=onmove;
	gtk_widget_add_events(GTK_WIDGET(this->wndw), GDK_STRUCTURE_MASK);
	g_signal_connect(this->wndw, "configure-event", G_CALLBACK(onmove_activate), this);
}

void set_title(const char * title)
{
	gtk_window_set_title(this->wndw, title);
}

void set_onclose(function<bool()> onclose)
{
	this->onclose = onclose;
}



void set_menu(windowmenu_menu* menu)
{
	delete this->menu;
	
	this->menu=menu;
	gtk_grid_attach(this->grid, GTK_WIDGET(get_menu_bar_widget(this->menu)), 0,0, 1,1);
	gtk_widget_show_all(GTK_WIDGET(get_menu_bar_widget(this->menu)));
}



void statusbar_create(int numslots, const int * align, const int * dividerpos)
{
	if (this->status) gtk_widget_destroy(GTK_WIDGET(this->status));
	this->status = NULL;
	//if (this->status_parent) gtk_widget_destroy(GTK_WIDGET(this->status_parent));
	//this->status_parent=NULL;
	free(this->status_pos);
	this->status_pos = NULL;
	
	gtk_window_set_has_resize_grip(this->wndw, (bool)numslots);
	
	if (!numslots) return;
	
	this->status = GTK_GRID(gtk_grid_new());
	//gtk_box_set_spacing(box, 2);
	for (int i=0;i<numslots;i++)
	{
		GtkWidget* label = gtk_label_new("");
		
		GValue val = G_VALUE_INIT;
		g_value_init(&val, G_TYPE_INT);
		g_value_set_int(&val, 2);
		g_object_set_property(G_OBJECT(label), "margin", &val);
		//gtk_widget_set_margin_top(label, 2);
		//gtk_widget_set_margin_bottom(label, 2);
		//gtk_widget_set_margin_left(label, 2);
		//gtk_widget_set_margin_right(label, 2);
		
//printf("a=%i\n",align[i]);
		//const GtkJustification just[]={ GTK_JUSTIFY_LEFT, GTK_JUSTIFY_CENTER, GTK_JUSTIFY_RIGHT };
		//gtk_label_set_justify(label, just[align[i]]);
		
		gtk_label_set_single_line_mode(GTK_LABEL(label), true);
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(label), 1);//why does this work
		
		gtk_misc_set_alignment(GTK_MISC(label), ((float)align[i])/2, 0.5);
		//const GtkAlign gtkalign[]={ GTK_ALIGN_START, GTK_ALIGN_CENTER, GTK_ALIGN_END };
		//gtk_widget_set_halign(label, gtkalign[align[i]]);
		
		gtk_grid_attach(this->status, label, i,0, 1,1);
//GdkRGBA color={1,0,i&1,1};
//gtk_widget_override_background_color(label,GTK_STATE_FLAG_NORMAL,&color);
	}
	
	this->status_count = numslots;
	this->status_pos = malloc(sizeof(int)*numslots);
	memcpy(this->status_pos, dividerpos, sizeof(int)*(numslots-1));
	this->status_pos[numslots-1] = 240;
	//gtk_widget_set_size_request(GTK_WIDGET(this->status), width, -1);
	
	//this->status_parent=GTK_FRAME(gtk_frame_new(NULL));
	//gtk_frame_set_shadow_type(this->status_parent, GTK_SHADOW_IN);
	//gtk_container_add(GTK_CONTAINER(this->status_parent), GTK_WIDGET(this->status));
	//
	//gtk_grid_attach(this->grid, GTK_WIDGET(this->status_parent), 0,2, 1,1);
	gtk_grid_attach(this->grid, GTK_WIDGET(this->status), 0,2, 1,1);
	gtk_widget_show_all(GTK_WIDGET(this->status));
	
	this->statusbar_resize();
}

void statusbar_set(int slot, const char * text)
{
	GtkLabel* label=GTK_LABEL(gtk_grid_get_child_at(this->status, slot, 0));
	gtk_label_set_text(label, text);
}

/*private*/ void statusbar_resize()
{
	//if (width<0) gtk_widget_get_size_request(GTK_WIDGET(this->contents), &width, NULL);
	unsigned int width;
	if (gtk_window_get_resizable(this->wndw))
	{
		//TODO - this probably bans shrinking. And doesn't resize on window resize, either.
		GtkAllocation size;
		gtk_widget_get_allocation(GTK_WIDGET(this->contents->widget), &size);
		width=size.width;
	}
	else
	{
		GtkRequisition size;
		gtk_widget_get_preferred_size(GTK_WIDGET(this->contents->widget), &size, NULL);
		width=size.width;
	}
	if (width<=1) return;
	
	
	width-=(this->status_count*2*2);
	if (gtk_window_get_resizable(this->wndw))
	{
		gint gripwidth;
		gtk_widget_style_get(GTK_WIDGET(this->wndw), "resize-grip-width", &gripwidth, NULL);
		width-=gripwidth;
	}
	int lastpos=0;
	for (int i=0;i<this->status_count;i++)
	{
		int nextpos=(width*this->status_pos[i] + 120)/240;
		GtkWidget* label=gtk_grid_get_child_at(this->status, i, 0);
		gtk_widget_set_size_request(label, nextpos-lastpos, -1);
		lastpos=nextpos;
	}
}



void replace_contents(widget_base* contents)
{
	gtk_widget_destroy(GTK_WIDGET(this->contents->widget));
	delete this->contents;
	gtk_grid_attach(this->grid, GTK_WIDGET(this->contents->widget), 0,1, 1,1);
	this->contents = contents;
}



bool is_visible()
{
	return this->visible;
}

void set_visible(bool visible)
{
	this->visible=visible;
	if (visible)
	{
		gtk_widget_show_all(GTK_WIDGET(this->wndw));
		this->statusbar_resize();
	}
	else gtk_widget_hide(GTK_WIDGET(this->wndw));
}

void focus()
{
	gtk_window_present(this->wndw);
}

bool is_active()
{
	if (this->menu && gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(get_menu_bar_widget(this->menu)))!=NULL) return false;
	return gtk_window_is_active(this->wndw);
}

bool menu_active()
{
	if (this->menu && gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(get_menu_bar_widget(this->menu)))!=NULL) return true;
	return false;
}

~window_gtk3()
{
	//struct window_gtk3 * this=(struct window_gtk3*)this_;
	//if (this->delayfree)
	//{
	//	this->delayfree=2;
	//	return;
	//}
	delete this->contents;
	delete this->menu;
	
	gtk_widget_destroy(GTK_WIDGET(this->wndw));
	free(this->status_pos);
	
	//free(this);
}

uintptr_t _get_handle()
{
	return (uintptr_t)this->wndw;
}

static gboolean onclose_gtk(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	struct window_gtk3 * This=(struct window_gtk3*)user_data;
	if (This->onclose)
	{
		//This->delayfree=1;
		if (This->onclose()==false)
		{
			//This->delayfree=0;
			return TRUE;
		}
		//if (This->delayfree==2)
		//{
		//	This->delayfree=0;
		//	free_((struct window*)This);
		//	return TRUE;
		//}
		//This->delayfree=0;
	}
	
	This->visible=false;
	gtk_widget_hide(GTK_WIDGET(This->wndw));
	return TRUE;
}

};

}

window* window_create(widget_base* contents)
{
	return new window_gtk3(contents);
}





enum menu_type { mtype_item, mtype_check, mtype_radio, mtype_sep, mtype_sub }; // nothing for 'top' since it doesn't have this struct
struct windowmenu::impl {
	GtkMenuItem* widget;
	uint8_t type;
	uint8_t nativepos;
	//char padding[6];
};
static void menu_set_text(GtkMenuItem* item, const char * text)
{
	GtkWidget* label=gtk_bin_get_child(GTK_BIN(item));
	if (*text=='_') gtk_label_set_text_with_mnemonic(GTK_LABEL(label), text+1);
	else gtk_label_set_text(GTK_LABEL(label), text);
}


struct windowmenu_item::impl {
	function<void(void)> onactivate;
};
static void menu_activate_normal(GtkMenuItem* menuitem, gpointer user_data)
{
	windowmenu_item* menu=(windowmenu_item*)user_data;
	menu->mu->onactivate();
}
windowmenu_item* windowmenu_item::create(const char * text, function<void(void)> onactivate)
{
	windowmenu_item* menu = new windowmenu_item;
	menu->m = new windowmenu::impl;
	menu->m->type = mtype_item;
	menu->m->widget = GTK_MENU_ITEM(gtk_menu_item_new_with_label(""));
	
	menu->mu = new windowmenu_item::impl;
	menu->mu->onactivate = onactivate;
	g_signal_connect(menu->m->widget, "activate", G_CALLBACK(menu_activate_normal), menu);
	
	menu_set_text(menu->m->widget, text);
	return menu;
}
void windowmenu_item::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(this->m->widget), enable);
}
windowmenu_item::~windowmenu_item() {}


struct windowmenu_check::impl
{
	function<void(bool checked)> onactivate;
	bool block_signals;
};
static void menu_activate_check(GtkMenuItem* menuitem, gpointer user_data)
{
	windowmenu_check* menu=(windowmenu_check*)user_data;
	if (menu->mu->block_signals) return;
	
	menu->mu->onactivate(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu->m->widget)));
}
void windowmenu_check::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(this->m->widget), enable);
}
bool windowmenu_check::get_checked()
{
	return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(this->m->widget));
}
void windowmenu_check::set_checked(bool checked)
{
	this->mu->block_signals=true;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(this->m->widget), checked);
	this->mu->block_signals=false;
}
windowmenu_check* windowmenu_check::create(const char * text, function<void(bool checked)> onactivate)
{
	windowmenu_check* ret = new windowmenu_check;
	ret->m = new windowmenu::impl;
	ret->m->type=mtype_check;
	ret->m->widget = GTK_MENU_ITEM(gtk_check_menu_item_new_with_label(""));
	g_signal_connect(ret->m->widget, "activate", G_CALLBACK(menu_activate_check), ret);
	ret->mu = new windowmenu_check::impl;
	ret->mu->onactivate = onactivate;
	ret->mu->block_signals=false;
	menu_set_text(ret->m->widget, text);
	return ret;
}
windowmenu_check::~windowmenu_check() {}


namespace {
struct windowmenu_radio_child {
	GtkRadioMenuItem* widget;
	windowmenu_radio* parent;
};
}
struct windowmenu_radio::impl
{
	windowmenu_radio_child* children;
	unsigned int numchildren;
	
	unsigned int state;
	
	function<void(unsigned int state)> onactivate;
};
static void menu_activate_radio(GtkMenuItem* menuitem, gpointer user_data)
{
	windowmenu_radio_child* item = (windowmenu_radio_child*)user_data;
	windowmenu_radio* menu = item->parent;
	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item->widget))) return;
	
	unsigned int newstate = item - menu->mu->children;
	
	if (newstate != menu->mu->state)
	{
		menu->mu->state = newstate;
		menu->mu->onactivate(menu->mu->state);
	}
}
void windowmenu_radio::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(this->m->widget), enable);
}
unsigned int windowmenu_radio::get_state()
{
	return this->mu->state;
}
void windowmenu_radio::set_state(unsigned int state)
{
	this->mu->state = state;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(this->mu->children[state].widget), true);
}
windowmenu_radio* windowmenu_radio::create(unsigned int numitems, const char * const * texts,
                                           function<void(unsigned int state)> onactivate)
{
	windowmenu_radio* ret = new windowmenu_radio;
	ret->m = new windowmenu::impl;
	ret->m->type = mtype_radio;
	ret->m->widget = NULL;
	
	ret->mu = new windowmenu_radio::impl;
	ret->mu->numchildren = numitems;
	ret->mu->children = malloc(sizeof(windowmenu_radio_child)*numitems);
	
	GSList* group=NULL;
	for (unsigned int i=0;i<numitems;i++)
	{
		GtkRadioMenuItem* widget = GTK_RADIO_MENU_ITEM(gtk_radio_menu_item_new_with_label(group, ""));
		group = gtk_radio_menu_item_get_group(widget);
		menu_set_text(GTK_MENU_ITEM(widget), texts[i]);
		g_signal_connect(widget, "activate", G_CALLBACK(menu_activate_radio), &ret->mu->children[i]);
		ret->mu->children[i].widget = widget;
		ret->mu->children[i].parent = ret;
	}
	ret->mu->state = 0;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ret->mu->children[0].widget), true);
	
	ret->mu->onactivate = onactivate;
	return ret;
}
windowmenu_radio::~windowmenu_radio()
{
	for (unsigned int i=1;i<this->mu->numchildren;i++)
	{
		gtk_widget_destroy(GTK_WIDGET(this->mu->children[i].widget));
	}
	free(this->mu->children);
}


struct windowmenu_separator::impl {};
windowmenu_separator* windowmenu_separator::create()
{
	windowmenu_separator* ret = new windowmenu_separator;
	ret->m = new windowmenu::impl;
	ret->m->type = mtype_sep;
	ret->m->widget = GTK_MENU_ITEM(gtk_separator_menu_item_new());
	return ret;
}
windowmenu_separator::~windowmenu_separator() {}


struct windowmenu_menu::impl
{
public:
	GtkMenuShell* container;
	struct windowmenu* * children;
	uint8_t numchildren;
	//char padding[7];
};

static unsigned int menu_get_native_length(windowmenu* menu)
{
	return (menu->m->type==mtype_radio ? ((windowmenu_radio*)menu)->mu->numchildren : 1);
}

//can't return children[pos]->nativepos because 'pos' may be the number of entries in the menu
//children[pos]->nativepos can also be wrong because it was recently inserted
static unsigned int menu_get_native_start(windowmenu_menu* menu, unsigned int pos)
{
	return (pos ? menu->mu->children[pos-1]->m->nativepos + menu_get_native_length(menu->mu->children[pos-1]) : 0);
}

static void menu_renumber(windowmenu_menu* menu, unsigned int start)
{
	unsigned int menupos = menu_get_native_start(menu, start);
	for (unsigned int i=start;i<menu->mu->numchildren;i++)
	{
		windowmenu* child = menu->mu->children[i];
		child->m->nativepos = menupos;
		menupos += menu_get_native_length(child);
	}
}

void windowmenu_menu::insert_child(unsigned int pos, windowmenu* child)
{
	unsigned int menupos = menu_get_native_start(this, pos);
	if (child->m->type!=mtype_radio)
	{
		gtk_menu_shell_insert(this->mu->container, GTK_WIDGET(child->m->widget), menupos);
		gtk_widget_show_all(GTK_WIDGET(child->m->widget));
	}
	else
	{
		windowmenu_radio* rchild = (windowmenu_radio*)child;
		for (unsigned int i=0;i<rchild->mu->numchildren;i++)
		{
			gtk_menu_shell_insert(this->mu->container, GTK_WIDGET(rchild->mu->children[i].widget), menupos);
			gtk_widget_show_all(GTK_WIDGET(rchild->mu->children[i].widget));
			menupos++;
		}
	}
	if ((this->mu->numchildren&(this->mu->numchildren-1))==0)
	{
		this->mu->children=realloc(this->mu->children, (this->mu->numchildren+1)*2*sizeof(windowmenu*));
	}
	memmove(this->mu->children+pos+1, this->mu->children+pos, (this->mu->numchildren - pos)*sizeof(windowmenu*));
	this->mu->children[pos]=child;
	this->mu->numchildren++;
	
	menu_renumber(this, pos);
}

void windowmenu_menu::remove_child(windowmenu* child)
{
	this->mu->numchildren--;
	unsigned int pos=0;
	while (this->mu->children[pos]!=child) pos++;
	memmove(this->mu->children+pos, this->mu->children+pos+1, (this->mu->numchildren - pos)*sizeof(windowmenu*));
	
	delete child;
}

windowmenu_menu* windowmenu_menu::create(const char * text, unsigned int numchildren, windowmenu* const * children)
{
	windowmenu_menu* ret = new windowmenu_menu;
	ret->m = new windowmenu::impl;
	ret->m->type = mtype_sub;
	ret->m->widget = GTK_MENU_ITEM(gtk_menu_item_new_with_label(""));
	menu_set_text(ret->m->widget, text);
	
	ret->mu = new windowmenu_menu::impl;
	ret->mu->container = GTK_MENU_SHELL(gtk_menu_new());
	ret->mu->children = NULL;
	ret->mu->numchildren = 0;
	for (unsigned int i=0;i<numchildren;i++) ret->insert_child(i, children[i]);
	
	gtk_menu_item_set_submenu(ret->m->widget, GTK_WIDGET(ret->mu->container));
	
	return ret;
}

windowmenu_menu* windowmenu_menu::create_top(unsigned int numchildren, windowmenu_menu* const * children)
{
	windowmenu_menu* ret = new windowmenu_menu;
	ret->m = NULL;
	ret->mu = new windowmenu_menu::impl;
	ret->mu->container = GTK_MENU_SHELL(gtk_menu_bar_new());
	ret->mu->children = NULL;
	ret->mu->numchildren = 0;
	for (unsigned int i=0;i<numchildren;i++) ret->insert_child(i, children[i]);
	return ret;
}

windowmenu_menu::~windowmenu_menu()
{
	for (unsigned int i=0;i<this->mu->numchildren;i++) delete this->mu->children[i];
	free(this->mu->children);
}

static GtkMenuShell* get_menu_bar_widget(windowmenu_menu* menu) { return menu->mu->container; }
#endif
