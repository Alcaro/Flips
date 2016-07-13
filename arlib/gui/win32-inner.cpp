#include "window.h"
#ifdef ARGUI_WINDOWS
#undef bind
#include <windows.h>
#define bind bind_func
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdlib.h>

//controls HIG and screenshots of them http://msdn.microsoft.com/en-us/library/dn742399.aspx
//controls docs http://msdn.microsoft.com/en-us/library/windows/desktop/bb773169%28v=vs.85%29.aspx
//alternatively http://msdn.microsoft.com/en-us/library/aa368039%28v=vs.85%29.aspx for some widgets

//NOTE: Widgets must respond to measure() before init() is called. Do not measure them in init.

#define dpi_vert 96
#define dpi_horz 96

#define btn_width 75
#define btn_height 23

#define checkbox_width 17
#define checkbox_height 17

#define radio_width 17
#define radio_height 17

#define frame_top 16
#define frame_left 4
#define frame_right 4
#define frame_bottom 4

#define g_padding 3

#define TIMER_MOUSEHIDE 1

//Keep noninteractive at 0 and defbutton at button+1.
//Otherwise, no rules for which values mean what; the IDs only mean anything inside this file.
#define CTID_NONINTERACTIVE 0
#define CTID_BUTTON 1
#define CTID_DEFBUTTON 2
#define CTID_CHECK 3
#define CTID_RADIO 4
#define CTID_LISTVIEW 5
#define CTID_TEXTBOX 6

static HFONT dlgfont;
static unsigned int xwidth;

static HBRUSH bg_invalid;

//static bool recursive=false;

static HFONT try_create_font(const char * name, int size)
{
	return CreateFont(-MulDiv(size, dpi_vert, 72), 0, 0, 0, FW_NORMAL,
	                  FALSE, FALSE, FALSE, DEFAULT_CHARSET,
	                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE,
	                  name);
}

static void measure_text(const char * text, unsigned int * width, unsigned int * height)
{
	HDC hdc=GetDC(NULL);
	SelectObject(hdc, dlgfont);
	RECT rc={0, 0, 0, 0};
	DrawText(hdc, text, -1, &rc, DT_CALCRECT);
	ReleaseDC(NULL, hdc);
	if (width) *width=rc.right;
	if (height) *height=rc.bottom;
}

static LRESULT CALLBACK viewport_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void _window_init_inner()
{
	//HDC hdc=GetDC(NULL);
	//dpiheight=GetDeviceCaps(hdc, LOGPIXELSY);
	
	dlgfont=try_create_font("Segoe UI", 9);
	if (!dlgfont) dlgfont=try_create_font("MS Shell Dlg 2", 8);//I'm just gonna expect this one not to fail.
	                                                           //If it does, we'll get the bad font; no real catastrophe.
	
//	HGDIOBJ prevfont=SelectObject(hdc, dlgfont);
//	TEXTMETRIC metrics;
//	if (GetTextMetrics(hdc, &metrics))
//	{
////for dialogs: GetDialogBaseUnits http://msdn.microsoft.com/en-us/library/ms645475%28VS.85%29.aspx
////button height is 14/8 times the font height
////button width is 50/4 times the average character width
////can be extracted from http://msdn.microsoft.com/en-us/library/dd144941%28v=vs.85%29.aspx tmHeight and tmAveCharWidth
//		dlgbuttonwidth=MulDiv(metrics.tmAveCharWidth, 50, 4);
//		dlgbuttonheight=MulDiv(metrics.tmHeight, 14, 8);
//	}
//	else
//	{
//		dlgbuttonwidth=80;
//		dlgbuttonheight=20;
//	}
	//"tmAveCharWidth is not precise". fuck that shit, I do the right thing if it's not a pain.
	//actually, the right thing would probably be "fuck Microsoft, more Linux" and delete this file,
	// because that's even less of a pain.
	
	//SelectObject(hdc, prevfont);
	//ReleaseDC(NULL, hdc);
	
	WNDCLASS wc;
	wc.style=0;
	wc.lpfnWndProc=viewport_WindowProc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=GetModuleHandle(NULL);
	wc.hIcon=LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=GetSysColorBrush(COLOR_3DFACE);//(HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName=NULL;
	wc.lpszClassName="minir_viewport";
	RegisterClass(&wc);
	
	measure_text("XXXXXXXXXXXX", &xwidth, NULL);
	
	INITCOMMONCONTROLSEX initctrls;
	initctrls.dwSize=sizeof(initctrls);
	initctrls.dwICC=ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&initctrls);
	
	bg_invalid=CreateSolidBrush(RGB(0xFF,0x66,0x66));
}

static void place_window(HWND hwnd, void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	HDWP* hdwp=(HDWP*)resizeinf;
	*hdwp=DeferWindowPos(*hdwp, hwnd, NULL, x, y, width, height, SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOOWNERZORDER|SWP_NOZORDER);
}



struct widget_label::impl {
	struct window * parent;
	HWND hwnd;
	uint8_t state;
	//char padding[7];
};

widget_label::widget_label(const char * text) : m(new impl)
{
	this->widthprio=1;
	this->heightprio=1;
	
	m->hwnd=(HWND)strdup(text);
	measure_text(text, &this->width, &this->height);
	this->width+=g_padding*2; this->height+=g_padding*2;
	m->state=0;
}

unsigned int widget_label::init(struct window * parent, uintptr_t parenthandle)
{
	m->parent=parent;
	char* text=(char*)m->hwnd;
	m->hwnd=CreateWindow(WC_STATIC, text, WS_CHILD|WS_VISIBLE|SS_NOPREFIX,
	                     0, 0, 16, 16, // just some random sizes, we'll resize it in _place()
	                     (HWND)parenthandle, NULL, GetModuleHandle(NULL), NULL);
	free(text);
	if (m->state==1) EnableWindow(m->hwnd, false);
	m->state=2;
	SendMessage(m->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	return 1;
}

void widget_label::measure() {}

void widget_label::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; /*height-=g_padding*2;*/
	place_window(m->hwnd, resizeinf, x,y+(height-this->height)/2, width,this->height);
}

widget_label::~widget_label()
{
	delete m;
}

widget_label* widget_label::set_enabled(bool enable)
{
	if (m->state==2) EnableWindow(m->hwnd, enable);
	else m->state=!enable;
	return this;
}

widget_label* widget_label::set_text(const char * text)
{
	measure_text(text, &this->width, &this->height);
	this->width+=g_padding*2; this->height+=g_padding*2;
	m->parent->_reflow();
	return this;
}

widget_label* widget_label::set_ellipsize(bool ellipsize)
{
//puts("FIXME: label_set_ellipsize");
	return this;
}

widget_label* widget_label::set_alignment(int alignment)
{
//puts("FIXME: label_set_alignment");
	return this;
}



struct widget_button::impl {
	//struct window * parent;
	HWND hwnd;
	
	function<void()> onclick;
	
	uint8_t state;
	//char padding[7];
};

widget_button::widget_button(const char * text) : m(new impl)
{
	this->widthprio=1;
	this->heightprio=1;
	
	this->width=btn_width+g_padding*2;
	this->height=btn_height+g_padding*2;
	
	m->onclick=NULL;
	m->state=0;
	m->hwnd=(HWND)strdup(text);
}

unsigned int widget_button::init(struct window * parent, uintptr_t parenthandle)
{
	char* text=(char*)m->hwnd;
	m->hwnd=CreateWindow(WC_BUTTON, text, WS_CHILD|WS_VISIBLE|WS_TABSTOP, 0, 0, 16, 16,
	                     (HWND)parenthandle, (HMENU)CTID_BUTTON, GetModuleHandle(NULL), NULL);
	free(text);
	if (m->state==1) EnableWindow(m->hwnd, false);
	m->state=2;
	SetWindowLongPtr(m->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(m->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	return 1;
}

void widget_button::measure() {}

void widget_button::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; height-=g_padding*2;
	place_window(m->hwnd, resizeinf, x, y, width, height);
}

widget_button::~widget_button()
{
	delete m;
}

widget_button* widget_button::set_enabled(bool enable)
{
	if (m->state&2) EnableWindow(m->hwnd, enable);
	else m->state=!enable;
	return this;
}

widget_button* widget_button::set_text(const char * text)
{
	SetWindowText(m->hwnd, text);
	return this;
}

widget_button* widget_button::set_onclick(function<void()> onclick)
{
	m->onclick=onclick;
	return this;
}



struct widget_checkbox::impl {
	struct window * parent;
	HWND hwnd;
	
	function<void(bool checked)> onclick;
	
	uint8_t state;
	//char padding[7];
};

widget_checkbox::widget_checkbox(const char * text) : m(new impl)
{
	this->widthprio=1;
	this->heightprio=1;
	
	this->width=btn_width+g_padding*2;
	this->height=btn_height+g_padding*2;
	
	m->onclick=NULL;
	m->state=0;
	m->hwnd=(HWND)strdup(text);
}

unsigned int widget_checkbox::init(struct window * parent, uintptr_t parenthandle)
{
	m->parent=parent;
	char* text=(char*)m->hwnd;
	m->hwnd=CreateWindow(WC_BUTTON, "", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_CHECKBOX, 0, 0, 16, 16,
	                     (HWND)parenthandle, (HMENU)CTID_CHECK, GetModuleHandle(NULL), NULL);
	if (m->state&2) EnableWindow(m->hwnd, false);
	if (m->state&4) Button_SetCheck(m->hwnd, BST_CHECKED);
	m->state=0x01;
	SetWindowLongPtr(m->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(m->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	
	this->set_text(text);
	free(text);
	return 1;
}

void widget_checkbox::measure() {}

void widget_checkbox::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; height-=g_padding*2;
	place_window(m->hwnd, resizeinf, x, y, width, height);
}

widget_checkbox::~widget_checkbox()
{
	delete m;
}

widget_checkbox* widget_checkbox::set_enabled(bool enable)
{
	if (m->state==1) EnableWindow(m->hwnd, enable);
	else m->state=((m->state&0xFD)|(!enable<<1));
	return this;
}

widget_checkbox* widget_checkbox::set_text(const char * text)
{
	if (m->state==1)
	{
		measure_text(text, &this->width, NULL);
		this->width+=checkbox_width + g_padding*2;
		this->height=checkbox_height + g_padding*2;
		
		SetWindowText(m->hwnd, text);
		m->parent->_reflow();
	}
	else
	{
		free((char*)m->hwnd);
		m->hwnd=(HWND)strdup(text);
	}
	return this;
}

bool widget_checkbox::get_state()
{
	if (m->state==1) return (Button_GetCheck(m->hwnd)==BST_CHECKED);
	else return (m->state&2);
}

widget_checkbox* widget_checkbox::set_state(bool checked)
{
	if (m->state==1) Button_SetCheck(m->hwnd, checked ? BST_CHECKED : BST_UNCHECKED);
	else m->state=((m->state&0xFB)|(checked<<2));
	return this;
}

widget_checkbox* widget_checkbox::set_onclick(function<void(bool checked)> onclick)
{
	m->onclick=onclick;
	return this;
}



struct widget_radio::impl {
	union {
		struct { //active if !initialized (if init() has not been called)
			char* text;//strdup'd
			bool disabled;
		};
		struct { //active otherwise
			struct window * parent;
			HWND hwnd;
		};
	};
	
	bool initialized;
	//char padding[3];
	
	//this can point to itself
	unsigned int id;
	widget_radio* leader;
	
	widget_radio* * group;
	
	function<void(unsigned int state)> onclick;//can't be put in the union because it has a constructor
	union {
		//which one is active depends on 'id'; if it's nonzero, 'last' is valid
		struct { //active if id==0
			unsigned int active;
		};
		struct { //active otherwise
			bool last;//the 'first' flag is known by checking for id==0
			//char padding[3];
		};
	};
	//char padding[4];
};

widget_radio::widget_radio(const char * text) : m(new impl)
{
	this->widthprio=1;
	this->heightprio=1;
	
	measure_text(text, &this->width, &this->height);
	this->width+=this->height;
	this->width+=g_padding*2;
	
	m->leader=NULL;
	m->onclick=NULL;
	m->group=NULL;
	m->active=0;
	
	m->initialized=false;
	m->disabled=false;
	m->text=strdup(text);
}

unsigned int widget_radio::init(struct window * parent, uintptr_t parenthandle)
{
	bool disabled=m->disabled;
	char* text=m->text;
	
	m->parent=parent;
	m->hwnd=CreateWindow(WC_BUTTON, text, WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_RADIOBUTTON, 0, 0, 16, 16,
	                     (HWND)parenthandle, (HMENU)CTID_RADIO, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(m->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(m->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	if (m->leader && m->id==m->leader->m->active) Button_SetCheck(m->hwnd, BST_CHECKED);
	if (disabled) EnableWindow(m->hwnd, false);
	
	m->initialized=true;
	this->set_text(text);
	free(text);
	
	return 1;
}

void widget_radio::measure() {}

void widget_radio::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; width-=g_padding*2;
	if (m->id==0)
	{
		y+=g_padding;
		height-=g_padding;
	}
	else if (m->last)
	{
		height-=g_padding;
	}
	place_window(m->hwnd, resizeinf, x, y, width, height);
}

widget_radio::~widget_radio()
{
	free(m->group);//this frees NULL on anything except the leader
	delete m;
}

widget_radio* widget_radio::set_enabled(bool enable)
{
	if (m->initialized) EnableWindow(m->hwnd, enable);
	else m->disabled=!enable;
	return this;
}

widget_radio* widget_radio::set_text(const char * text)
{
	if (m->initialized)
	{
		measure_text(text, &this->width, NULL);
		this->width+=checkbox_width + g_padding*2;
		this->height=checkbox_height;
		
		if (m->id==0 || m->last) this->height+=g_padding;
		
		SetWindowText(m->hwnd, text);
		m->parent->_reflow();
	}
	else
	{
		free(m->text);
		m->text=strdup(text);
	}
	return this;
}

widget_radio* widget_radio::group(unsigned int numitems, widget_radio* * group)
{
	for (unsigned int i=0;i<numitems;i++)
	{
		group[i]->m->leader=this;
		group[i]->m->id=i;
		if (i)
		{
			group[i]->m->last=false;
			if (i==numitems-1)
			{
				group[i]->m->last=true;
				group[i]->height+=g_padding;
			}
		}
	}
	
	m->group=malloc(sizeof(widget_radio*)*numitems);
	memcpy(m->group, group, sizeof(widget_radio*)*numitems);
	if (m->initialized) Button_SetCheck(m->hwnd, BST_CHECKED);
	this->height+=g_padding;
	if (m->initialized && m->parent) m->parent->_reflow();
	
	return this;
}

unsigned int widget_radio::get_state()
{
	return m->active;
}

widget_radio* widget_radio::set_state(unsigned int state)
{
	Button_SetCheck(m->group[m->active]->m->hwnd, BST_UNCHECKED);
	Button_SetCheck(m->group[state]->m->hwnd, BST_CHECKED);
	m->active=state;
	return this;
}

widget_radio* widget_radio::set_onclick(function<void(unsigned int state)> onclick)
{
	m->onclick=onclick;
	return this;
}



struct widget_textbox::impl {
	union {
		struct { // if initialized
			window* parent;
			HWND hwnd;
		};
		struct { // if !initialized
			unsigned int maxchars;
			
			bool disabled;
			bool focus;
			//char padding[10];
		};
	};
	
	char * text;
	
	bool initialized;
	bool invalid;
	//char padding[6];
	
	function<void(const char * text)> onchange;
	function<void(const char * text)> onactivate;
};

static LRESULT CALLBACK textbox_SubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

widget_textbox::widget_textbox() : m(new impl)
{
	m->initialized=false;
	m->disabled=false;
	m->focus=false;
	m->maxchars=0;
	
	this->widthprio=3;
	this->heightprio=1;
	
	m->text=NULL;
	m->onchange=NULL;
	m->onactivate=NULL;
	m->invalid=false;
	
	measure_text("XXXXXXXXXXXX", NULL, &this->height);
	this->width = 5*xwidth/12 + 6 + g_padding*2;
	this->height += 6 + g_padding*2;
}

unsigned int widget_textbox::init(window* parent, uintptr_t parenthandle)
{
	HWND hwnd = CreateWindow(WC_EDIT, "", WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL, 0, 0, 16, 16,
	                       (HWND)parenthandle, (HMENU)CTID_TEXTBOX, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	SetWindowSubclass(hwnd, textbox_SubclassProc, 0, 0);
	if (m->disabled) EnableWindow(hwnd, false);
	if (m->focus) SetFocus(hwnd);
	if (m->text) SetWindowText(hwnd, m->text);
	
	this->set_length(m->maxchars);
	this->set_invalid(m->invalid);
	
	m->initialized=true;
	
	m->parent=parent;
	m->hwnd=hwnd;
	return 1;
}

void widget_textbox::measure() {}

void widget_textbox::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; height-=g_padding*2;
	place_window(m->hwnd, resizeinf, x+1,y+1, width-2,height-2);//this math is to get the border to the inside of the widget
}

widget_textbox::~widget_textbox()
{
	free(m->text);
	delete m;
}

widget_textbox* widget_textbox::set_enabled(bool enable)
{
	if (m->initialized) EnableWindow(m->hwnd, enable);
	else m->disabled=!enable;
	return this;
}

widget_textbox* widget_textbox::focus()
{
	SetFocus(m->hwnd);
	return this;
}

const char * widget_textbox::get_text()
{
	free(m->text);
	unsigned int len=GetWindowTextLength(m->hwnd);
	m->text=malloc(len+1);
	GetWindowText(m->hwnd, m->text, len+1);
	return m->text;
}

widget_textbox* widget_textbox::set_text(const char * text)
{
	free(m->text);
	m->text=strdup(text);
	if (m->initialized) SetWindowText(m->hwnd, m->text);
	return this;
}

widget_textbox* widget_textbox::set_length(unsigned int maxlen)
{
	Edit_LimitText(m->hwnd, maxlen);//conveniently, we both chose 0 to mean unlimited
	return this;
}

widget_textbox* widget_textbox::set_width(unsigned int xs)
{
	this->width = xs*xwidth/12 + 6 + g_padding*2;
	m->parent->_reflow();
	return this;
}

widget_textbox* widget_textbox::set_invalid(bool invalid)
{
	m->invalid=invalid;
	if (m->initialized)
	{
		InvalidateRect(m->hwnd, NULL, FALSE);
		if (invalid) SetFocus(m->hwnd);
	}
	return this;
}

widget_textbox* widget_textbox::set_onchange(function<void(const char * text)> onchange)
{
	m->onchange=onchange;
	return this;
}

widget_textbox* widget_textbox::set_onactivate(function<void(const char * text)> onactivate)
{
	m->onactivate=onactivate;
	return this;
}


static LRESULT CALLBACK textbox_SubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	widget_textbox* obj=(widget_textbox*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_KEYDOWN:
			if (wParam==VK_RETURN && !(lParam & 0x40000000))
			{
				obj->m->onactivate(obj->get_text());
				return 0;
			}
			break;
		case WM_NCDESTROY:
			RemoveWindowSubclass(hwnd, textbox_SubclassProc, 0);
			break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}



struct widget_canvas_win32;



struct widget_viewport::impl {
	struct window * parent;
	HWND hwnd;
	
	bool hide_cursor_user;
	bool hide_cursor_timer;
	LPARAM lastmousepos;
	
	function<void(const char * const * filenames)> on_file_drop;
};



unsigned int widget_viewport::init(struct window * parent, uintptr_t parenthandle)
{
	m->parent=parent;
	m->hwnd=CreateWindow("minir_viewport", "", WS_CHILD|WS_VISIBLE, 0, 0, 16, 16, // TODO: figure out why this isn't resized properly
	                     (HWND)parenthandle, NULL, GetModuleHandle(NULL), NULL);  // probably a missed reflow
	SetWindowLongPtr(m->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	return 1;
}

widget_viewport::widget_viewport(unsigned int width, unsigned int height) : m(new impl)
{
	this->widthprio=0;
	this->heightprio=0;
	
	this->width=width;
	this->height=height;
	//no padding here
	
	m->hide_cursor_user=false;
	m->hide_cursor_timer=true;
	m->lastmousepos=-1;//random value, just so Valgrind-like tools won't throw.
	m->on_file_drop=NULL;
}

void widget_viewport::measure() {}

void widget_viewport::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	place_window(m->hwnd, resizeinf, x, y, width, height);
}

widget_viewport::~widget_viewport()
{
	delete m;
}

widget_viewport* widget_viewport::resize(unsigned int width, unsigned int height)
{
	this->width=width;
	this->height=height;
	if (!m->parent->_reflow())
	{
		SetWindowPos(m->hwnd, NULL, 0, 0, width, height, SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
	}
	return this;
}

uintptr_t widget_viewport::get_window_handle()
{
	return (uintptr_t)m->hwnd;
}

widget_viewport* widget_viewport::set_hide_cursor(bool hide)
{
	m->hide_cursor_user=hide;
	if (m->hide_cursor_user && m->hide_cursor_timer) SetCursor(NULL);
	else SetCursor(LoadCursor(NULL, IDC_ARROW));
	
	return this;
}

widget_viewport* widget_viewport::set_support_drop(function<void(const char * const * filenames)> on_file_drop)
{
	DragAcceptFiles(m->hwnd, TRUE);
	m->on_file_drop=on_file_drop;
	
	return this;
}

static LRESULT CALLBACK viewport_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	widget_viewport* obj=(widget_viewport*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg)
	{
	case WM_MOUSEMOVE:
		{
			//windows, what exactly is the point behind the bogus mouse move messages
			//you clearly know they exist (http://blogs.msdn.com/b/oldnewthing/archive/2009/06/17/9763416.aspx), why not block them?
			if (lParam==obj->m->lastmousepos) break;
			obj->m->lastmousepos=lParam;
			
			SetTimer(obj->m->hwnd, TIMER_MOUSEHIDE, 1000, NULL);
			TRACKMOUSEEVENT tme={ sizeof(tme), TME_LEAVE, obj->m->hwnd, HOVER_DEFAULT };
			TrackMouseEvent(&tme);
			obj->m->hide_cursor_timer=false;
		}
		break;
	case WM_MOUSELEAVE:
	case WM_NCMOUSEMOVE:
		{
			KillTimer(obj->m->hwnd, TIMER_MOUSEHIDE);
			obj->m->hide_cursor_timer=false;
		}
		break;
	case WM_TIMER:
		{
			if (wParam==TIMER_MOUSEHIDE)
			{
				obj->m->hide_cursor_timer=true;
				if (obj->m->hide_cursor_user) SetCursor(NULL);
				KillTimer(obj->m->hwnd, TIMER_MOUSEHIDE);
			}
		}
		break;
	case WM_SETCURSOR:
		{
			if (obj->m->hide_cursor_user && obj->m->hide_cursor_timer) SetCursor(NULL);
			else goto _default;
		}
		break;
	case WM_DROPFILES:
		{
			HDROP hdrop=(HDROP)wParam;
			UINT numfiles=DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);//but what if I drop four billion files?
			char * * filenames=malloc(sizeof(char*)*(numfiles+1));
			for (UINT i=0;i<numfiles;i++)
			{
				UINT len=DragQueryFile(hdrop, i, NULL, 0);
				filenames[i]=malloc(len+1);
				DragQueryFile(hdrop, i, filenames[i], len+1);
				for (unsigned int j=0;filenames[i][j];j++)
				{
					if (filenames[i][j]=='\\') filenames[i][j]='/';
				}
			}
			filenames[numfiles]=NULL;
			DragFinish(hdrop);
			obj->m->on_file_drop((const char * const *)filenames);
			for (UINT i=0;i<numfiles;i++) free(filenames[i]);
			free(filenames);
		}
		break;
		_default:
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}



//http://msdn.microsoft.com/en-us/library/windows/desktop/bb774737(v=vs.85).aspx
//http://www.codeproject.com/Articles/7891/Using-virtual-lists - codeproject being useful for once

#ifndef LVCFMT_FIXED_WIDTH
#define LVCFMT_FIXED_WIDTH 0x100
#endif

struct widget_listbox_virtual::impl {
	struct window * parent;
	HWND hwnd;
	
	unsigned int rows;
	unsigned int columns;
	
	unsigned int columnwidthsum;
	unsigned int * columnwidths;
	
	bool initialized;
	bool disabled;
	
	bool checkboxes;//oddly placed due to padding
	//char padding[1];
	
	function<const char * (size_t row, int column)> get_cell;
	function<size_t(const char * prefix, size_t start, bool up)> search;
	
	function<void(size_t row)> onfocuschange;
	function<void(size_t row)> onactivate;
	
	function<void(size_t row)> ontoggle;
};

void widget_listbox_virtual::construct(unsigned int numcolumns, const char * * columns)
{
	m=new impl;
	
	this->widthprio=3;
	this->heightprio=3;
	
	m->columnwidths=malloc(sizeof(unsigned int)*numcolumns);
	m->columns=numcolumns;
	m->rows=0;
	m->disabled=false;
	m->onfocuschange=NULL;
	m->onactivate=NULL;
	m->checkboxes=NULL;
	
	this->width=1+19+1+m->columns*20;
	this->height=1+25+19*0+2;
	
	m->hwnd=malloc(sizeof(const char*)*numcolumns);
	memcpy(m->hwnd, columns, sizeof(const char*)*numcolumns);
	
	m->initialized=false;
}

unsigned int widget_listbox_virtual::init(struct window * parent, uintptr_t parenthandle)
{
	m->parent=parent;
	const char * * columns=(const char**)m->hwnd;
	m->hwnd=CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "", WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_OWNERDATA,
	                          0, 0, 16, 16, (HWND)parenthandle, (HMENU)CTID_LISTVIEW, GetModuleHandle(NULL), NULL);
	
	SetWindowLongPtr(m->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(m->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	
	LVCOLUMN col;
	col.mask=LVCF_FMT|LVCF_WIDTH|LVCF_TEXT;
	col.fmt=LVCFMT_LEFT|LVCFMT_FIXED_WIDTH;
	col.cx=20;
	for (unsigned int i=0;i<m->columns;i++)
	{
		//measure_text(columns[i], (unsigned int*)&col.cx, NULL);
		//col.cx+=12;
		col.pszText=(char*)columns[i];
		(void)ListView_InsertColumn(m->hwnd, i, &col);
		m->columnwidths[i]=1;
		//this->width+=col.cx;
	}
	m->columnwidthsum=m->columns;
	
	if (m->checkboxes) (void)ListView_SetExtendedListViewStyleEx(m->hwnd, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
	if (m->disabled) EnableWindow(m->hwnd, false);
	ListView_SetItemCountEx(m->hwnd, m->rows, 0);//not redrawing because the window isn't even visible at this point
	
	free(columns);
	
	m->initialized=true;
	
	//pretty sure the listbox has children, but I can't ask how many of those there are. Probably varies if I add checkboxes, too.
	//But I'm not the one resizing them, so I can't do anything with that even if I knew the number.
	return 1;
}

void widget_listbox_virtual::measure() {}

static void listbox_resize_column(HWND hwnd, int col, unsigned int width)
{
	//microsoft, which drugs did you take this time? LVCFMT_FIXED_WIDTH blocks me from resizing from code too!
	LVCOLUMN mkrz;
	mkrz.mask=LVCF_FMT;
	mkrz.fmt=LVCFMT_LEFT;
	LVCOLUMN mknrz;
	mknrz.mask=LVCF_FMT;
	mknrz.fmt=LVCFMT_LEFT|LVCFMT_FIXED_WIDTH;
	
	(void)ListView_SetColumn(hwnd, col, &mkrz);
	(void)ListView_SetColumnWidth(hwnd, col, width);
	(void)ListView_SetColumn(hwnd, col, &mknrz);
}

void widget_listbox_virtual::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; height-=g_padding*2;
	place_window(m->hwnd, resizeinf, x, y, width, height);
	
	width-=GetSystemMetrics(SM_CXVSCROLL)+4;
	unsigned char width_div=m->columnwidthsum;
	unsigned int width_frac=0;
	for (unsigned int i=0;i<m->columns;i++)
	{
		width_frac+=m->columnwidths[i]*width;
		unsigned int yourwidth=width_frac/width_div;
		width_frac%=width_div;
		listbox_resize_column(m->hwnd, i, yourwidth);
	}
}

widget_listbox_virtual::~widget_listbox_virtual()
{
	free(m->columnwidths);
	delete m;
}

widget_listbox_virtual* widget_listbox_virtual::set_enabled(bool enable)
{
	if (m->initialized) EnableWindow(m->hwnd, enable);
	else m->disabled=!enable;
	return this;
}

widget_listbox_virtual* widget_listbox_virtual::set_num_rows(size_t rows)
{
	if (rows > 100000000) rows=100000000;//Windows "only" supports 100 million (0x05F5E100); more than that and it gets empty.
	m->rows=rows;
	if (m->initialized)
	{
		ListView_SetItemCountEx(m->hwnd, rows, 0);
		(void)ListView_RedrawItems(m->hwnd, 0, rows-1);
	}
	return this;
}

widget_listbox_virtual* widget_listbox_virtual::set_contents(function<const char * (size_t row, int column)> get_cell,
                                                             function<size_t(const char * prefix, size_t start, bool up)> search)
{
	m->get_cell = get_cell;
	m->search = search;
	return this;
}

widget_listbox_virtual* widget_listbox_virtual::refresh()
{
	if (m->initialized)
	{
		(void)ListView_RedrawItems(m->hwnd, 0, m->rows-1);
	}
	return this;
}

#if 0
widget_listbox_virtual* widget_listbox_virtual::set_size(unsigned int height, const unsigned int * widths, int expand)
{
	int widthpx=-1;
	int heightpx=1+25+height*19+2;
	
	if (widths)
	{
		widthpx=1+19+1;
		m->columnwidthsum=0;
		for (unsigned int i=0;i<m->columns;i++)
		{
			listbox_resize_column(m->hwnd, i, widths[i]*xwidth/12+12+5+(i==0));//why are these columns not resized to the sizes I tell them to resize to
			widthpx+=widths[i]*xwidth/12+12+5;
			m->columnwidths[i]=widths[i];
			m->columnwidthsum+=widths[i];
		}
	}
	
	if (expand!=-1)
	{
		LVCOLUMN mkrz;
		mkrz.mask=LVCF_FMT;
		mkrz.fmt=LVCFMT_LEFT;
		(void)ListView_SetColumn(m->hwnd, expand, &mkrz);
	}
	
	DWORD widthheight=ListView_ApproximateViewRect(m->hwnd, widthpx, heightpx, height);
	this->width=LOWORD(widthheight);
	this->height=HIWORD(widthheight)-GetSystemMetrics(SM_CYHSCROLL)+2;//microsoft really isn't making this easy for me
	this->width+=g_padding*2; this->height+=g_padding*2;
//printf("%u->%u %u->%u\n",widthpx,this->i._base.width,heightpx,this->i._base.height);
	
	m->parent->_reflow();
	
	return this;
}
#endif

size_t widget_listbox_virtual::get_active_row()
//TODO: check retval from ListView_GetSelectionMark if there is no selection
{
	return ListView_GetSelectionMark(m->hwnd);
}

widget_listbox_virtual* widget_listbox_virtual::set_on_focus_change(function<void(size_t row)> onchange)
{
	m->onfocuschange=onchange;
	return this;
}

widget_listbox_virtual* widget_listbox_virtual::set_onactivate(function<void(size_t row)> onactivate)
{
	m->onactivate=onactivate;
	return this;
}

widget_listbox_virtual* widget_listbox_virtual::add_checkboxes(function<void(size_t row)> ontoggle)
{
	m->ontoggle=ontoggle;
	m->checkboxes=true;
	if (m->initialized) (void)ListView_SetExtendedListViewStyleEx(m->hwnd, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
	return this;
}

static uintptr_t listbox_notify(NMHDR* nmhdr)
{
	widget_listbox_virtual* obj = (widget_listbox_virtual*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
	if (nmhdr->code == LVN_GETDISPINFO)
	{
		LV_DISPINFO* info = (LV_DISPINFO*)nmhdr;
		
		unsigned int row = info->item.iItem;
		unsigned int column = info->item.iSubItem;
		
		if (info->item.mask & LVIF_TEXT)
		{
			const char * str = obj->m->get_cell(row, column);
			unsigned int len = strlen(str);
			if (len > (unsigned int)info->item.cchTextMax-1) len = info->item.cchTextMax-1;
			memcpy(info->item.pszText, str, len);
			info->item.pszText[len] = '\0';
		}
		
		if (info->item.mask & LVIF_IMAGE)
		{
			info->item.iImage = 0;
			info->item.mask |= LVIF_STATE;
			info->item.stateMask = LVIS_STATEIMAGEMASK;
			info->item.state = INDEXTOSTATEIMAGEMASK(obj->m->checkboxes ? obj->m->get_cell(row, -1) ? 2 : 1 : 0);
		}
	}
	if (nmhdr->code==LVN_ODCACHEHINT)
	{
		//ignore
	}
	if (nmhdr->code==LVN_ODFINDITEM)
	{
		NMLVFINDITEM* find=(NMLVFINDITEM*)nmhdr;
		if (!(find->lvfi.flags&LVFI_STRING)) return 0;
		
		if (obj->m->search)
		{
			size_t ret=obj->m->search(find->lvfi.psz, find->iStart, false);
			if (ret >= obj->m->rows) ret=(size_t)-1;
			return (uintptr_t)ret;
		}
		else
		{
			return _widget_listbox_search(obj->m->get_cell, obj->m->rows, find->lvfi.psz, find->iStart, false);
		}
	}
	if (nmhdr->code==LVN_KEYDOWN)
	{
		NMLVKEYDOWN* keydown=(NMLVKEYDOWN*)nmhdr;
		if (keydown->wVKey==VK_SPACE && obj->m->checkboxes)
		{
			int row=ListView_GetSelectionMark(obj->m->hwnd);
			if (row!=-1)
			{
				obj->m->ontoggle(row);
				(void)ListView_RedrawItems(obj->m->hwnd, row, row);
			}
		}
		if (keydown->wVKey==VK_RETURN)
		{
			int row=ListView_GetSelectionMark(obj->m->hwnd);
			if (row!=-1) obj->m->onactivate(row);
		}
	}
	if (nmhdr->code==NM_CLICK)
	{
		NMITEMACTIVATE* click=(NMITEMACTIVATE*)nmhdr;
		LVHITTESTINFO hitinfo;
		hitinfo.pt=click->ptAction;
		int row=ListView_HitTest(obj->m->hwnd, &hitinfo);
		if (row!=-1 && (hitinfo.flags & LVHT_ONITEMSTATEICON))
		{
			obj->m->ontoggle(row);
			(void)ListView_RedrawItems(obj->m->hwnd, row, row);
		}
	}
	if (nmhdr->code==NM_DBLCLK)
	{
		NMITEMACTIVATE* click=(NMITEMACTIVATE*)nmhdr;
		LVHITTESTINFO hitinfo;
		hitinfo.pt=click->ptAction;
		int row=ListView_HitTest(obj->m->hwnd, &hitinfo);
		if (row!=-1 && (hitinfo.flags & LVHT_ONITEMLABEL))
		{
			obj->m->onactivate(row);
		}
	}
	if (nmhdr->code==LVN_ITEMCHANGED)
	{
		NMLISTVIEW* change=(NMLISTVIEW*)nmhdr;
		if (!(change->uOldState&LVIS_FOCUSED) && (change->uNewState&LVIS_FOCUSED) && change->iItem>=0)
		{
			obj->m->onfocuschange(change->iItem);
		}
	}
	return 0;
}



struct widget_frame::impl {
	bool initialized;
	//char padding[7];
	HWND hwnd;
	widget_base* child;
};

widget_frame::widget_frame(const char * text, widget_base* contents) : m(new impl)
{
	m->initialized=false;
	m->child=(struct widget_base*)contents;
	m->hwnd=(HWND)strdup(text);
}

unsigned int widget_frame::init(struct window * parent, uintptr_t parenthandle)
{
	//this->parent=parent;//this one can't do anything that changes its size
	char * text=(char*)m->hwnd;
	m->hwnd=CreateWindow(WC_BUTTON, text, WS_CHILD|WS_VISIBLE|WS_GROUP|WS_DISABLED|BS_GROUPBOX, 0, 0, 16, 16,
	                     (HWND)parenthandle, (HMENU)CTID_NONINTERACTIVE, GetModuleHandle(NULL), NULL);
	//SetWindowLongPtr(m->hwnd, GWLP_USERDATA, (LONG_PTR)this);//this one sends no notifications
	SendMessage(m->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	free(text);
	m->initialized=true;
	return 1 + m->child->init(parent, parenthandle);
}

void widget_frame::measure()
{
	m->child->measure();
	this->width=m->child->width + frame_left+frame_right + g_padding*2;
	this->height=m->child->height + frame_top+frame_bottom + g_padding*2;
	this->widthprio=m->child->widthprio;
	this->heightprio=m->child->heightprio;
}

void widget_frame::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; height-=g_padding*2;
	m->child->place(resizeinf, x+frame_left, y+frame_top, width-frame_left-frame_right, height-frame_top-frame_bottom);
	place_window(m->hwnd, resizeinf, x, y, width, height);
}

widget_frame::~widget_frame()
{
	delete m->child;
	delete m;
}

widget_frame* widget_frame::set_text(const char * text)
{
	if (m->initialized) SetWindowText(m->hwnd, text);
	else
	{
		free((char*)m->hwnd);
		m->hwnd=(HWND)strdup(text);
	}
	return this;
}



uintptr_t _window_notify_inner(void* notification)
{
	NMHDR* nmhdr=(NMHDR*)notification;
	switch (nmhdr->idFrom)
	{
		case CTID_NONINTERACTIVE: break;
		case CTID_BUTTON:
		case CTID_DEFBUTTON:
		{
			if (nmhdr->code==BN_CLICKED)
			{
				widget_button* obj=(widget_button*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				obj->m->onclick();
			}
			break;
		}
		case CTID_CHECK:
		{
			if (nmhdr->code==BN_CLICKED)
			{
				widget_checkbox * obj=(widget_checkbox*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				bool state=obj->get_state();
				state=!state;
				obj->set_state(state);
				obj->m->onclick(state);
			}
			break;
		}
		case CTID_RADIO:
		{
			if (nmhdr->code==BN_CLICKED)
			{
				widget_radio* obj=(widget_radio*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				widget_radio* leader=obj->m->leader;
				leader->set_state(obj->m->id);
				
				leader->m->onclick(obj->m->id);
			}
			break;
		}
		case CTID_TEXTBOX:
		{
			if (nmhdr->code==EN_CHANGE)
			{
				widget_textbox* obj=(widget_textbox*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				if (obj->m->invalid)
				{
					obj->m->invalid=false;
					InvalidateRect(obj->m->hwnd, NULL, FALSE);
				}
				if (obj->m->onchange)
				{
					obj->m->onchange(obj->get_text());
				}
			}
			break;
		}
		case CTID_LISTVIEW:
		{
			return listbox_notify(nmhdr);
			break;
		}
	}
	return 0;
}

uintptr_t _window_get_widget_color(unsigned int type, void* handle, void* draw, void* parent)
{
	switch (GetDlgCtrlID((HWND)handle))
	{
		case CTID_TEXTBOX:
		{
			widget_textbox* obj=(widget_textbox*)GetWindowLongPtr((HWND)handle, GWLP_USERDATA);
			if (obj->m->invalid)
			{
				SetBkMode((HDC)draw, TRANSPARENT);
				return (LRESULT)bg_invalid;
			}
			break;
		}
	}
	return DefWindowProcA((HWND)parent, (UINT)type, (WPARAM)draw, (LPARAM)handle);
}
#endif
