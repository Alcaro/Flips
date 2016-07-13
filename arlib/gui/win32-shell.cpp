#include "window.h"
#ifdef ARGUI_WINDOWS
#undef bind
#include <windows.h>
#define bind bind_func
#include <commctrl.h>
#include <ctype.h>

//TODO: check if DwmEnableMMCSS does anything useful

//force some year-old C code to compile properly as C++ - I decided to switch long ago but still haven't finished.

//TODO:
//menu_create: check where it's resized if size changes

//static bool isxp;

void _window_init_shell()
{
	WNDCLASS wc;
	wc.style=0;
	wc.lpfnWndProc=DefWindowProc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=GetModuleHandle(NULL);
	wc.hIcon=LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=GetSysColorBrush(COLOR_3DFACE);
	wc.lpszMenuName=NULL;
	wc.lpszClassName="minir";
	RegisterClass(&wc);
	
	//DWORD version=GetVersion();
	//DWORD trueversion=(LOBYTE(LOWORD(version))<<8 | HIBYTE(LOWORD(version)));
	//isxp=(trueversion<=0x0501);
}

static HMENU menu_to_hmenu(windowmenu_menu* menu);
//static void menu_delete(struct windowmenu_win32 * This);
static void menu_activate(HMENU menu, DWORD pos);
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace {
#define WS_BASE WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX // okay microsoft, did I miss anything?
#define WS_RESIZABLE (WS_BASE|WS_MAXIMIZEBOX|WS_THICKFRAME)
#define WS_NONRESIZ (WS_BASE|WS_BORDER)
//static bool _reflow(struct window * this_);
//static void reflow_force(struct window_win32 * this_);

static HWND activedialog;

static struct window_win32 * firstwindow;
static struct window_win32 * modalwindow;

class window_win32 : public window {
public:

//used by modality
struct window_win32 * prev;
struct window_win32 * next;
bool modal;
//char padding[7];

HWND hwnd;
widget_base* contents;
unsigned int numchildwin;

DWORD lastmousepos;

windowmenu_menu* menu;

HWND status;
int * status_align;
int * status_div;
uint16_t status_count;
uint8_t status_resizegrip_width;
bool status_extra_spacing;

bool resizable;
bool isdialog;

bool menuactive;//odd position to reduce padding
//uint8_t delayfree;//0=normal, 1=can't free now, 2=free at next opportunity

function<bool()> onclose;

/*private*/ void getBorderSizes(unsigned int * width, unsigned int * height)
{
	RECT inner;
	RECT outer;
	GetClientRect(this->hwnd, &inner);
	GetWindowRect(this->hwnd, &outer);
	if (width) *width=(outer.right-outer.left)-(inner.right);
	if (height) *height=(outer.bottom-outer.top)-(inner.bottom);
	
	if (height && this->status)
	{
		RECT statsize;
		GetClientRect(this->status, &statsize);
		*height+=(statsize.bottom-statsize.top);
	}
}

/*private*/ void resize_stbar(unsigned int width)
{
	if (this->status)
	{
		SendMessage(this->status, WM_SIZE, 0,0);
		
		unsigned int statuswidth=width;
		if (this->resizable)
		{
			if (!this->status_resizegrip_width)
			{
				RECT rect;
				SendMessage(this->status, SB_GETRECT, 0, (LPARAM)&rect);
				this->status_resizegrip_width=rect.bottom-rect.top-8;//assume the size grip has the same width as height
			}
			statuswidth-=this->status_resizegrip_width;
		}
		int * statuspositions=malloc(sizeof(int)*this->status_count);
		for (int i=0;i<this->status_count;i++)
		{
			statuspositions[i]=statuswidth*(this->status_div[i])/240;
		}
		//statuspositions[this->status_count-1]=width;
		SendMessage(this->status, SB_SETPARTS, (WPARAM)this->status_count, (LPARAM)statuspositions);
		free(statuspositions);
	}
}

void set_is_dialog()
{
	this->isdialog=true;
}

void set_parent(struct window * parent_)
{
	struct window_win32 * parent=(struct window_win32*)parent_;
	SetWindowLongPtr(this->hwnd, GWLP_HWNDPARENT, (LONG_PTR)parent->hwnd);
}

/*private*/ void update_modal()
{
	if (this->modal && IsWindowVisible(this->hwnd))
	{
		//disable all windows
		if (!modalwindow)//except if they're already disabled because that's a waste of time.
		{
			struct window_win32 * wndw=firstwindow;
			while (wndw)
			{
				if (wndw!=this) EnableWindow(wndw->hwnd, false);
				wndw=wndw->next;
			}
			modalwindow=this;
		}
	}
	else
	{
		//we're gone now - if we're the one holding the windows locked, enable them
		if (this == modalwindow)
		{
			struct window_win32 * wndw=firstwindow;
			while (wndw)
			{
				EnableWindow(wndw->hwnd, true);
				wndw=wndw->next;
			}
			modalwindow=NULL;
		}
	}
}

void set_modal(bool modal)
{
	this->modal=modal;
	this->update_modal();
}

void resize(unsigned int width, unsigned int height)
{
	unsigned int padx;
	unsigned int pady;
	this->getBorderSizes(&padx, &pady);
	
	SetWindowPos(this->hwnd, NULL, 0, 0, width+padx, height+pady,
	             SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
	
	this->_reflow();
	
	//because we're setting the window position ourselves, reflow is likely to think the status bar is correct, so we need to fix it ourselves.
	RECT size;
	GetClientRect(this->hwnd, &size);
	this->resize_stbar(size.right);
}

void set_resizable(bool resizable, function<void(unsigned int newwidth, unsigned int newheight)> onresize)
{
	if (this->resizable != resizable)
	{
		this->resizable=resizable;
		SetWindowLong(this->hwnd, GWL_STYLE, GetWindowLong(this->hwnd, GWL_STYLE) ^ WS_RESIZABLE^WS_NONRESIZ);
		this->_reflow();
	}
}

void get_pos(int * x, int * y)
{
	//TODO
}

void set_pos(int x, int y)
{
	
}

void set_onmove(function<void(int x, int y)> onmove)
{
	
}

void set_title(const char * title)
{
	/*if (!isxp)*/ SetWindowText(this->hwnd, title);
}

void set_onclose(function<bool()> onclose)
{
	this->onclose = onclose;
}

void set_menu(windowmenu_menu* menu)
{
	delete this->menu;
	
	SetMenu(this->hwnd, menu_to_hmenu(menu));
	this->menu=menu;
}

void statusbar_create(int numslots, const int * align, const int * dividerpos)
{
	if (!numslots)
	{
		if (this->status)
		{
			RECT barsize;
			GetWindowRect(this->status, &barsize);
			RECT mainsize;
			GetWindowRect(this->hwnd, &mainsize);
			SetWindowPos(this->hwnd, NULL, 0, 0, mainsize.right, mainsize.bottom-(barsize.bottom-barsize.top),
			             SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
			DestroyWindow(this->status);
			this->status=NULL;
		}
		return;
	}
	if (!this->status)
	{
		INITCOMMONCONTROLSEX initctrls;
		initctrls.dwSize=sizeof(initctrls);
		initctrls.dwICC=ICC_BAR_CLASSES;
		InitCommonControlsEx(&initctrls);
		
		this->status=CreateWindow(STATUSCLASSNAME, "", WS_CHILD|WS_VISIBLE|WS_DISABLED, 0,0,0,0, this->hwnd, NULL, GetModuleHandle(NULL), NULL);
		//SetWindowLong(this->status, GWL_STYLE, GetWindowLong(this->status, GWL_STYLE)|WS_DISABLED);//phoenix says this is needed, or it can get tab focus
		//EnableWindow(this->status, false);//phoenix says this is needed, or it can get tab focus
	}
	
	free(this->status_align);
	free(this->status_div);
	
	this->status_count=numslots;
	this->status_align=malloc(sizeof(int)*numslots);
	this->status_div=malloc(sizeof(int)*numslots);
	for (int i=0;i<numslots;i++)
	{
		this->status_align[i]=align[i];
		if (i<numslots-1) this->status_div[i]=dividerpos[i];
		else this->status_div[i]=240;
	}
	
	RECT barsize;
	GetWindowRect(this->status, &barsize);
	RECT mainsize;
	GetWindowRect(this->hwnd, &mainsize);
	SetWindowPos(this->hwnd, NULL, 0, 0, mainsize.right, mainsize.bottom+(barsize.bottom-barsize.top),
	             SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
	
	this->resize_stbar(barsize.right);
}

void statusbar_set(int slot, const char * text)
{
	int align=this->status_align[slot];
	if (align==0)
	{
		SendMessage(this->status, SB_SETTEXT, (WPARAM)slot, (LPARAM)text);
	}
	else
	{
		int textlen=strlen(text);
		char * newtext=malloc(1+1+textlen+1+1);
		newtext[0]='\t';
		newtext[1]='\t';
		memcpy(newtext+2, text, textlen);
		newtext[2+textlen]=((align==2)?' ':'\0');
		newtext[2+textlen+1]='\0';
		SendMessage(this->status, SB_SETTEXT, (WPARAM)slot, (LPARAM)(newtext + ((align==1)?1:0)));
		free(newtext);
	}
}

void replace_contents(widget_base* contents)
{
	delete this->contents;
	this->contents=contents;
	this->numchildwin=this->contents->init(this, (uintptr_t)this->hwnd);
	this->_reflow();
}

void set_visible(bool visible)
{
	if (visible)
	{
		this->reflow_force();
		ShowWindow(this->hwnd, SW_SHOWNORMAL);
	}
	else
	{
		ShowWindow(this->hwnd, SW_HIDE);
	}
	this->update_modal();
}

bool is_visible()
{
	return IsWindowVisible(this->hwnd);
}

void focus()
{
	SetForegroundWindow(this->hwnd);
}

bool is_active()
{
	return (GetForegroundWindow()==this->hwnd);
}

bool menu_active()
{
	return (this->menuactive);
}

~window_win32()
{
	//if (this->delayfree)
	//{
	//	this->delayfree=2;
	//	return;
	//}
	
	if (this->prev) this->prev->next=this->next;
	else firstwindow=this->next;
	if (this->next) this->next->prev=this->prev;
	
	if (this->modal)
	{
		this->set_visible(false);
		this->update_modal();
	}
	
	delete this->contents;
	delete this->menu;
	DestroyWindow(this->hwnd);
	//free(this);
}

uintptr_t _get_handle()
{
	return (uintptr_t)this->hwnd;
}

bool _reflow()
{
	if (!IsWindowVisible(this->hwnd)) return false;
	this->reflow_force();
	return true;
}

/*private*/ void reflow_force()
{
	//Resizing our window seems to call the resize callback again. We're not interested, it'll just recurse in irritating ways.
	static bool recursive=false;
	if (recursive) return;
	recursive=true;
	
	RECT size;
	GetClientRect(this->hwnd, &size);
	
	RECT statsize;
	if (this->status)
	{
		GetClientRect(this->status, &statsize);
		size.bottom-=statsize.bottom;
	}
	
	this->contents->measure();
	
	bool badx=(this->contents->width  > (unsigned int)size.right  || (!this->resizable && this->contents->width  != (unsigned int)size.right));
	bool bady=(this->contents->height > (unsigned int)size.bottom || (!this->resizable && this->contents->height != (unsigned int)size.bottom));
	
	if (badx) size.right=this->contents->width;
	if (bady) size.bottom=this->contents->height;
	
	if (badx || bady)
	{
		unsigned int outerw;
		unsigned int outerh;
		this->getBorderSizes(&outerw, &outerh);
		//we can't defer this, or GetClientRect will get stale data, and we need the actual window size to move the rest of the windows
		SetWindowPos(this->hwnd, NULL, 0, 0, size.right+outerw, size.bottom+outerh,
		             SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
		
		unsigned int newouterh;
		this->getBorderSizes(NULL, &newouterh);
		if (newouterh != outerh)//changing the width may change the menu between one and two lines, so resize again
		{
			SetWindowPos(this->hwnd, NULL, 0, 0, size.right+outerw, size.bottom+newouterh,
			             SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
		}
		
		GetClientRect(this->hwnd, &size);
		if (this->status) size.bottom-=statsize.bottom;
		this->resize_stbar(size.right);
	}
	
	HDWP hdwp=BeginDeferWindowPos(this->numchildwin);
	this->contents->place(&hdwp, 0,0, size.right, size.bottom);
	EndDeferWindowPos(hdwp);
	recursive=false;
}

window_win32(widget_base* contents)
{
	this->next=firstwindow;
	this->prev=NULL;
	if (this->next) this->next->prev=this;
	firstwindow=this;
	
	this->contents=(struct widget_base*)contents;
	this->contents->measure();
	//the 6 and 28 are arbitrary; we'll set ourselves to a better size later. Windows' default placement algorithm sucks, anyways.
	//const char * xpmsg="Do not submit bug reports. Windows XP is unsupported by Microsoft, and unsupported by me.";
	this->hwnd=CreateWindow("minir", /*isxp?xpmsg:*/"", WS_NONRESIZ, CW_USEDEFAULT, CW_USEDEFAULT,
	                        this->contents->width+6, this->contents->height+28, NULL, NULL, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SetWindowLongPtr(this->hwnd, GWLP_WNDPROC, (LONG_PTR)WindowProc);
	this->numchildwin = this->contents->init((struct window*)this, (uintptr_t)this->hwnd);
	
	this->status=NULL;
	this->menu=NULL;
	this->menuactive=false;
	this->resizable=false;
	this->onclose=NULL;
	this->lastmousepos=-1;
	this->status_resizegrip_width=0;
	//this->delayfree=0;
	
	this->resize(this->contents->width, this->contents->height);
}

};

}

window* window_create(widget_base* contents)
{
	return new window_win32(contents);
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct window_win32 * This=(struct window_win32*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg)
	{
	case WM_CTLCOLOREDIT: return _window_get_widget_color(uMsg, (HWND)lParam, (HDC)wParam, hwnd);
	case WM_GETMINMAXINFO:
		{
			if (This)
			{
				MINMAXINFO* mmi=(MINMAXINFO*)lParam;
				
				unsigned int padx;
				unsigned int pady;
				This->getBorderSizes(&padx, &pady);
				
				mmi->ptMinTrackSize.x=padx+This->contents->width;
				mmi->ptMinTrackSize.y=pady+This->contents->height;
				
				if (!This->resizable)
				{
					mmi->ptMaxTrackSize.x=mmi->ptMinTrackSize.x;
					mmi->ptMaxTrackSize.y=mmi->ptMinTrackSize.y;
				}
			}
		}
		break;
	case WM_ACTIVATE:
		if (LOWORD(wParam) && This->isdialog) activedialog=hwnd;
		else activedialog=NULL;
		break;
	case WM_CLOSE:
	//case WM_ENDSESSION://this isn't really the most elegant solution, but it should work.
	//disabling the above because I don't want the possibility of being closed between fopen and fwrite.
	CloseWindow:
		{
			if (This->onclose)
			{
				//This->delayfree=1;
				if (!This->onclose())
				{
					//This->delayfree=0;
					break;
				}
				//if (This->delayfree==2)
				//{
				//	This->delayfree=0;
				//	free_((struct window*)This);
				//	break;
				//}
				//This->delayfree=0;
			}
			ShowWindow(hwnd, SW_HIDE);
		}
		break;
	case WM_COMMAND:
		{
//printf("COMM=%.8zX,%.8zX\n",wParam,lParam);
			if (lParam==0)
			{
				//what does this 2 mean? It works, but...
				if (HIWORD(wParam)==0 && LOWORD(wParam)==2) goto CloseWindow;
			}
			else
			{
				NMHDR nmhdr={(HWND)lParam, LOWORD(wParam), HIWORD(wParam)};
				return _window_notify_inner(&nmhdr);
			}
		}
		break;
	case WM_NOTIFY:
		{
			return _window_notify_inner((LPNMHDR)lParam);
		}
		break;
	case WM_MENUCOMMAND:
		{
			menu_activate((HMENU)lParam, wParam);
		}
		break;
	case WM_DESTROY:
		break;
	case WM_SYSCOMMAND:
		{
//printf("SC=%.4X\n",wParam&0xFFFF);
			if ((wParam&0xFFF0)==SC_KEYMENU) break;//go away, we don't want automenus. Alt could be hit by accident.
			//we could eat WM_MOVE and WM_SIZE to nuke the stupid lockups, but that blocks moving the window entirely.
			//We'll have to mess with threads.
			goto _default;
		}
	//check WM_CONTEXTMENU
	case WM_SIZE:
		{
			if (!This) break;//this one seems to hit only on Wine, but whatever, worth checking.
			This->_reflow();
			if (This->status) PostMessage(This->status, WM_SIZE, wParam, lParam);
		}
		break;
	case WM_ENTERMENULOOP:
		{
			This->menuactive=true;
		}
		break;
	case WM_EXITMENULOOP:
		{
			This->menuactive=false;
		}
		break;
	_default:
	default:
		return DefWindowProcA(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

static void handlemessage(MSG * msg)
{
	if (activedialog && IsDialogMessage(activedialog, msg)) return;
	TranslateMessage(msg);
	DispatchMessage(msg);
}

void window_run_iter()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) handlemessage(&msg);
}

void window_run_wait()
{
	MSG msg;
	GetMessage(&msg, NULL, 0, 0);
	handlemessage(&msg);
	window_run_iter();
}





enum menu_type { mtype_item, mtype_check, mtype_radio, mtype_sep, mtype_sub }; // nothing for 'top' since it doesn't have this struct
struct windowmenu::impl {
	uint8_t type;
	uint8_t nativepos;
	//char padding[6];
	union {
		char * text;
		char ** multitext; // used by radio
		HMENU menu_in;
	};
};

static char * menu_transform_name(const char * name)
{
	bool useaccel=(*name=='_');
	if (useaccel) name++;
	unsigned int accelpos=0;
	unsigned int len=0;
	for (unsigned int i=0;name[i];i++)
	{
		if (name[i]=='&') len++;
		if (!accelpos && name[i]=='_') accelpos=i;
		len++;
	}
	char * ret=malloc(len+2);//NUL, extra &
	char * at=ret;
	for (unsigned int i=0;name[i];i++)
	{
		if (name[i]=='&') *(at++)='&';
		if (useaccel && i==accelpos) *(at++)='&';
		//This is an intentional bug. If it's reported, the user is known to use XP, and will be slapped with a large trout.
		//(Details: The menu entries have randomly glitched names.)
		//else if (isxp && rand()%7==0) *(at++)=rand()&255;
		else *(at++)=name[i];
	}
	*at='\0';
	return ret;
}

static unsigned int menu_get_native_length(windowmenu* menu);
static unsigned int menu_get_native_start(windowmenu_menu* menu, unsigned int pos);

static void menu_set_enabled(windowmenu* menu, bool enabled)
{
	unsigned int pos = menu->m->nativepos;
	unsigned int left = menu_get_native_length(menu);
	while (left--)
	{
		MENUITEMINFO info;
		info.cbSize = sizeof(MENUITEMINFO);
		info.fMask = MIIM_STATE;
		GetMenuItemInfo(menu->m->menu_in, pos, TRUE, &info);
		if (enabled) info.fState &= ~MFS_GRAYED;
		else info.fState |= MFS_GRAYED;
		SetMenuItemInfo(menu->m->menu_in, pos, TRUE, &info);
		pos++;
	}
}


struct windowmenu_item::impl {
	function<void(void)> onactivate;
	const char * text; // used only before putting it in a menu
};
void windowmenu_item::set_enabled(bool enable)
{
	menu_set_enabled(this, enable);
}
windowmenu_item* windowmenu_item::create(const char * text, function<void(void)> onactivate)
{
	windowmenu_item* menu = new windowmenu_item;
	menu->m = new windowmenu::impl;
	menu->m->type = mtype_item;
	menu->m->text = menu_transform_name(text);
	
	menu->mu = new windowmenu_item::impl;
	menu->mu->onactivate = onactivate;
	return menu;
}
windowmenu_item::~windowmenu_item() {}


struct windowmenu_check::impl
{
	function<void(bool checked)> onactivate;
};
void windowmenu_check::set_enabled(bool enable)
{
	menu_set_enabled(this, enable);
}
bool windowmenu_check::get_checked()
{
	MENUITEMINFO info;
	info.cbSize=sizeof(MENUITEMINFO);
	info.fMask=MIIM_STATE;
	GetMenuItemInfo(this->m->menu_in, this->m->nativepos, TRUE, &info);
	return (info.fState & MFS_CHECKED);
}
void windowmenu_check::set_checked(bool checked)
{
	CheckMenuItem(this->m->menu_in, this->m->nativepos, MF_BYPOSITION | (checked?MF_CHECKED:MF_UNCHECKED));
}
windowmenu_check* windowmenu_check::create(const char * text, function<void(bool checked)> onactivate)
{
	windowmenu_check* menu = new windowmenu_check;
	menu->m = new windowmenu::impl;
	menu->m->type = mtype_check;
	menu->m->text = menu_transform_name(text);
	menu->mu = new windowmenu_check::impl;
	menu->mu->onactivate = onactivate;
	return menu;
}
windowmenu_check::~windowmenu_check() {}


struct windowmenu_radio::impl
{
	unsigned int numchildren;
	unsigned int state;
	function<void(unsigned int state)> onactivate;
};
void windowmenu_radio::set_enabled(bool enable)
{
	menu_set_enabled(this, enable);
}
unsigned int windowmenu_radio::get_state()
{
	return this->mu->state;
}
void windowmenu_radio::set_state(unsigned int state)
{
	this->mu->state = state;
	CheckMenuRadioItem(this->m->menu_in, this->m->nativepos, this->m->nativepos + this->mu->numchildren-1,
	                   this->m->nativepos + state, MF_BYPOSITION);
}
windowmenu_radio* windowmenu_radio::create(unsigned int numitems, const char * const * texts,
                                           function<void(unsigned int state)> onactivate)
{
	windowmenu_radio* menu = new windowmenu_radio;
	menu->m = new windowmenu::impl;
	menu->mu = new windowmenu_radio::impl;
	menu->m->type = mtype_radio;
	
	menu->m->multitext = malloc(sizeof(const char *) * numitems);
	for (unsigned int i=0;i<numitems;i++)
	{
		menu->m->multitext[i] = menu_transform_name(texts[i]);
	}
	menu->mu->numchildren = numitems;
	menu->mu->state = 0;
	
	menu->mu->onactivate = onactivate;
	
	return menu;
}
windowmenu_radio::~windowmenu_radio() {}


struct windowmenu_separator::impl {};
windowmenu_separator* windowmenu_separator::create()
{
	windowmenu_separator* menu = new windowmenu_separator;
	menu->m = new windowmenu::impl;
	menu->m->type = mtype_sep;
	return menu;
}
windowmenu_separator::~windowmenu_separator() {}


struct windowmenu_menu::impl
{
	HMENU container;
	windowmenu* * children;
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

static void menu_add_item(windowmenu_menu* menu, unsigned int pos, windowmenu* child)
{
	HMENU hmenu = menu->mu->container;
	unsigned int menupos = menu_get_native_start(menu, pos);
	child->m->nativepos = menupos;
	if (child->m->type == mtype_radio)
	{
		windowmenu_radio* rchild = (windowmenu_radio*)child;
		for (unsigned int i=0;i<rchild->mu->numchildren;i++)
		{
			char * name = child->m->multitext[i];
			InsertMenu(hmenu, menupos, MF_BYPOSITION|MF_STRING, 0, name);
			free(name);
			menupos++;
		}
		free(child->m->multitext);
		child->m->menu_in = hmenu;
		
		rchild->set_state(0);
	}
	else if (child->m->type == mtype_sep)
	{
		InsertMenu(hmenu, menupos, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
	}
	else if (child->m->type == mtype_sub)
	{
		windowmenu_menu* mchild = (windowmenu_menu*)child;
		InsertMenu(hmenu, menupos, MF_BYPOSITION|MF_POPUP, (UINT_PTR)mchild->mu->container, mchild->m->text);
		free(mchild->m->text);
	}
	else
	{
		InsertMenu(hmenu, menupos, MF_BYPOSITION|MF_STRING, 0, child->m->text);
		free(child->m->text);
	}
	child->m->menu_in = hmenu;
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
	this->mu->numchildren++;
	this->mu->children=realloc(this->mu->children, sizeof(windowmenu*)*this->mu->numchildren);
	memmove(this->mu->children+pos+1, this->mu->children+pos, sizeof(windowmenu*)*(this->mu->numchildren-pos-1));
	this->mu->children[pos]=(windowmenu*)child;
	menu_add_item(this, pos, child);
	//TODO: DrawMenuBar https://msdn.microsoft.com/en-us/library/windows/desktop/ms647633%28v=vs.85%29.aspx
	
	menu_renumber(this, pos);
}

void windowmenu_menu::remove_child(windowmenu* child)
{
	unsigned int menupos = child->m->nativepos;
	unsigned int remcount = menu_get_native_length(child);
	delete child;
	while (remcount--) DeleteMenu(this->mu->container, menupos, MF_BYPOSITION);
	this->mu->numchildren--;
	
	unsigned int i=0;
	while (this->mu->children[i]!=child) i++;
	menu_renumber(this, i);
}

windowmenu_menu* windowmenu_create_submenu_shared(bool istop, const char * text, unsigned int numchildren, windowmenu* const * children)
{
	windowmenu_menu* menu = new windowmenu_menu;
	if (!istop)
	{
		menu->m = new windowmenu::impl;
		menu->m->type = mtype_sub;
		menu->m->text = menu_transform_name(text);
	}
	menu->mu = new windowmenu_menu::impl;
	menu->mu->container = (istop ? CreateMenu() : CreatePopupMenu());
	
	menu->mu->numchildren = numchildren;
	menu->mu->children = malloc(sizeof(windowmenu*)*numchildren);
	memcpy(menu->mu->children, children, sizeof(windowmenu*)*numchildren);
	
	for (unsigned int i=0;i<numchildren;i++)
	{
		//menu->insert_child(i, children[i]);
		menu_add_item(menu, i, children[i]);
	}
	
	//MENUINFO menuinf={ .cbSize=sizeof(menuinf), .fMask=MIM_STYLE|MIM_MENUDATA, .dwStyle=MNS_NOTIFYBYPOS/*|MNS_MODELESS*/, .dwMenuData=(DWORD_PTR)This };
	MENUINFO menuinf;
	menuinf.cbSize = sizeof(menuinf);
	menuinf.fMask = MIM_STYLE|MIM_MENUDATA;
	menuinf.dwStyle = MNS_NOTIFYBYPOS/*|MNS_MODELESS*/; //MODELESS makes the window border flash in stupid ways when switching between the menus.
	menuinf.dwMenuData = (DWORD_PTR)menu;
	SetMenuInfo(menu->mu->container, &menuinf);
	return menu;
}

windowmenu_menu* windowmenu_menu::create(const char * text, unsigned int numchildren, windowmenu* const * children)
{
	return windowmenu_create_submenu_shared(false, text, numchildren, children);
}

windowmenu_menu* windowmenu_menu::create_top(unsigned int numchildren, windowmenu_menu* const * children)
{
	return windowmenu_create_submenu_shared(true, NULL, numchildren, (windowmenu**)children);
}

windowmenu_menu::~windowmenu_menu()
{
	for (unsigned int i=0;i<this->mu->numchildren;i++) delete this->mu->children[i];
	free(this->mu->children);
}


static HMENU menu_to_hmenu(windowmenu_menu* menu)
{
	return menu->mu->container;
}
static void menu_activate(HMENU hmenu, DWORD pos)
{
	MENUINFO menuinf;
	menuinf.cbSize = sizeof(menuinf);
	menuinf.fMask = MIM_MENUDATA;
	GetMenuInfo(hmenu, &menuinf);
	windowmenu_menu* menu = (windowmenu_menu*)menuinf.dwMenuData;
	
	unsigned int i=0;
	while (pos >= menu->mu->children[i]->m->nativepos + menu_get_native_length(menu->mu->children[i])) i++;
	
	windowmenu* activate = menu->mu->children[i];
	
	if (activate->m->type == mtype_item)
	{
		windowmenu_item* item = (windowmenu_item*)activate;
		item->mu->onactivate();
	}
	if (activate->m->type == mtype_check)
	{
		windowmenu_check* check = (windowmenu_check*)activate;
		
		MENUITEMINFO info;
		info.cbSize=sizeof(MENUITEMINFO);
		info.fMask=MIIM_STATE;
		GetMenuItemInfo(check->m->menu_in, pos, TRUE, &info);
		bool state = (info.fState & MFS_CHECKED);
		state = !state;
		
		CheckMenuItem(check->m->menu_in, pos, MF_BYPOSITION | (state?MF_CHECKED:MF_UNCHECKED));
		check->mu->onactivate(state);
	}
	if (activate->m->type == mtype_radio)
	{
		windowmenu_radio* radio = (windowmenu_radio*)activate;
		unsigned int newstate = pos - radio->m->nativepos;
		if (newstate == radio->mu->state) return;
		radio->mu->state = newstate;
		CheckMenuRadioItem(radio->m->menu_in, radio->m->nativepos, radio->m->nativepos + radio->mu->numchildren-1,
		                   radio->m->nativepos + newstate, MF_BYPOSITION);
		radio->mu->onactivate(newstate);
	}
}
#endif
