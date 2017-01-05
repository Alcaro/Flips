#pragma once
#include "../global.h"
#include "../string.h"
#include <string.h>

class window;
class windowmenu_menu;
class widget_base;

#if defined(ARGUI_WINDOWS)
#define ARGUI_MANUAL_LAYOUT
#endif

//This must be called before calling any other window_*, before creating any interface that does any I/O, before calling anything from
// the malloc() family, and before using argc/argv; basically, before doing anything else. It should be the first thing main() does.
//It does the following actions, in whatever order makes sense:
//- Initialize the window system, if needed; on failure, terminates the process
//- Read off any arguments it recognizes (if any), and delete them; for example, it takes care of --display and a few others on GTK+
//- Convert argv[0] to the standard path format, if needed (hi Windows)
void window_init(int * argc, char * * argv[]);

//Returns false if the window system couldn't be initialized, rather than terminating.
bool window_try_init(int * argc, char * * argv[]);

//On Windows, attaches stdout/stderr to the console of the launching process. On Linux, does nothing.
//On both, returns whether the process is currently in a terminal. Returns true if I/O is redirected.

//Returns whether the process was launched from a console.
//If yes, calling window_console_attach will connect stdout/stderr to something (stdin not guaranteed to work).
bool window_console_avail();
bool window_console_attach(); // Returns whether it worked.

//On Windows, the program is assumed portable, so it returns the program directory.
//On Linux, the program is assumed installed, so it returns the user's config directory.
string window_config_path();

//window toolkit is not choosable at runtime
//It is safe to interact with this window while inside its callbacks, with the exception that you may not free it.
//You may also not use window_run_*().
class window {
public:
	//Marks the window as a popup dialog. This makes it act differently in some ways.
	//For example, poking Escape will close it, and it may or may not get a thinner window border.
	//Must be called before the first call to set_visible(). Can't be undone, and can't be called multiple times.
	virtual void set_is_dialog() = 0;
	
	//Sets which window created this one. This can, for example, center it on top of the parent.
	//Should generally be combined with set_is_popup.
	//Must be called before the first call to set_visible(). Can't be undone, and can't be called multiple times.
	virtual void set_parent(window* parent) = 0;
	
	//Blocks interacting with other windows in the program while this one is visible.
	//It is undefined behaviour to have two modal windows visible simultaneously.
	virtual void set_modal(bool modal) = 0;
	
	//newwidth and newheight are the content size, excluding menus/toolbars/etc.
	//If there is any widget whose size is unknown inside, then the sizes may only be used in resize(), and for relative measurements.
	//It is allowed to call resize() on unresizable windows, but changing the size of
	// any contents (changing a label text, for example) will resize it to minimum.
	//If resizable, the resize callback is called after the window is resized and everything is set to the new sizes.
	virtual void resize(unsigned int width, unsigned int height) = 0;
	virtual void set_resizable(bool resizable, function<void(unsigned int newwidth, unsigned int newheight)> onresize) = 0;
	
	virtual void get_pos(int * x, int * y) = 0;
	//Do not move to position (0,0) - it puts the window border outside the screen.
	virtual void set_pos(int x, int y) = 0;
	virtual void set_onmove(function<void(int x, int y)> onmove) = 0;
	
	virtual void set_title(const char * title) = 0;
	
	//The callback tells whether the close request should be honored; true for close, false for keep.
	//The window is only hidden, not deleted; you can use show() again later.
	//It is safe to free this structure from within this callback; if you do this, return true for close.
	virtual void set_onclose(function<bool()> onclose) = 0;
	
	//Appends a menu bar to the top of the window. If the window has a menu already, it's replaced. NULL removes the menu.
	//But there's no real reason to replace it. Just change it.
	//Must be created by windowmenu_menu::create_top.
	virtual void set_menu(windowmenu_menu* menu) = 0;
	
	////Creates a status bar at the bottom of the window. It is undefined what happens if numslots equals or exceeds 32.
	////align is how each string is aligned; 0 means touch the left side, 1 means centered, 2 means touch the right side.
	////dividerpos is in 240ths of the window size. Values 0 and 240, as well as
	//// a divider position to the left of the previous one, yield undefined behaviour.
	////dividerpos[numslots-1] is ignored; the status bar always covers the entire width of the window.
	////It is implementation defined whether the previous status bar strings remain, or if you must use statusbar_set again.
	////It is implementation defined whether dividers will be drawn. However, it is guaranteed
	//// that the implementation will look like the rest of the operating system, as far as that's feasible.
	////It is implementation defined what exactly happens if a string is too
	//// long to fit; however, it is guaranteed to show as much as it can.
	////To remove the status bar, set numslots to 0.
	//virtual void statusbar_create(int numslots, const int * align, const int * dividerpos) = 0;
	////Sets a string on the status bar. The index is zero-based. All strings are initially blank.
	//virtual void statusbar_set(int slot, const char * text) = 0;
	
	//This replaces the contents of a window.
	virtual void replace_contents(widget_base* contents) = 0;
	
	//Setting a window visible while it already is will do nothing.
	virtual void set_visible(bool visible) = 0;
	virtual bool is_visible() = 0;
	
	//Call only after making the window visible.
	virtual void focus() = 0;
	
	//If the menu is active, the window is considered not active.
	//If the menu doesn't exist, it is considered not active.
	//If the window is hidden, results are undefined.
	virtual bool is_active() = 0;
	virtual bool menu_active() = 0;
	
	//This will also remove the window from the screen, if it's visible.
	virtual ~window() = 0;
	
	
	//Only usable by the implementation, don't call them yourself. Not guaranteed to be implemented at all.
	//Returns a native handle to the window.
	virtual uintptr_t _get_handle() { return 0; }
	//Recomputes the window content layout.
	//If return value is false, the reflow will be done later and the old sizes are still present.
	virtual bool _reflow() { return false; };
};
inline window::~window(){}
window* window_create(widget_base* contents);



//Each widget is owned by the layout or window it's put in (layouts can own more layouts). Deleting the parent deletes the children.
//Each widget has a few shared base functions that can be called without knowing what
// type of widget this is. However, they should all be seen as implementation details.
//It is undefined behaviour to query a widget's state before it's placed inside a window.
//Any pointers given during widget creation must be valid until the widget is placed inside a window.
//Most functions return the object it's called on, so object state can be set while creating the object.
class widget_base : nocopy {
public:
#ifdef ARGUI_MANUAL_LAYOUT
	//measure() returns no value, but sets the width and height. The sizes are undefined if the last
	// function call on the widget was not measure(); widgets may choose to update their sizes in
	// response to anything that resizes them, and leave measure() blank.
	//If multiple widgets want the space equally much, they get equal fractions, in addition to their base demand.
	//If a widget gets extra space and doesn't want it, it should add some padding in any direction.
	//The widget should, if needed by the window manager, forward all plausible events to its parent window,
	// unless the widget wants the events. (For example, a button will want mouse events, but not file drop events.)
	//The window handles passed around are implementation defined.
	//The return value from init() is the number of child windows involved, from the window manager's point of view.
	virtual unsigned int init(window * parent, uintptr_t parenthandle) = 0;
	virtual void measure() = 0;
	unsigned int width;
	unsigned int height;
	virtual void place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height) = 0;
	
	//this one acts roughly like Q_OBJECT
#define WIDGET_BASE \
		unsigned int init(window * parent, uintptr_t parenthandle); \
		void measure(); \
		void place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height);
#else
	void * widget;
	#define WIDGET_BASE /* */
#endif
	//The priorities mean:
	//0 - Widget has been assigned a certain size; it must get exactly that. (Canvas, viewport)
	//1 - Widget wants a specific size; will only grudgingly accept more. (Most of them)
	//2 - Widget has orders to consume extra space if there's any left over and nothing really wants it. (Padding)
	//3 - Widget will look better if given extra space. (Textbox, listbox)
	//4 - Widget is ordered to be resizable. (Canvas, viewport)
	uint8_t widthprio;
	uint8_t heightprio;
	virtual ~widget_base() {};
};


class widget_padding : public widget_base { WIDGET_BASE
public:
	widget_padding(bool vertical);
	~widget_padding();
	
public:
	struct impl;
	impl * m;
};
static inline widget_padding* widget_create_padding_horz() { return new widget_padding(false); }
static inline widget_padding* widget_create_padding_vert() { return new widget_padding(true); }


class widget_layout : public widget_base { WIDGET_BASE
protected:
	void construct(unsigned int numchildren, widget_base* * children,
	              unsigned int totwidth,  unsigned int * widths,  bool uniformwidths,
	              unsigned int totheight, unsigned int * heights, bool uniformheights);
	widget_layout() {}
	
public:
//The lists are terminated with a NULL. It shouldn't be empty.
	widget_layout(bool vertical, bool uniform, widget_base* firstchild, ...);
#define widget_create_layout_horz(...) (new widget_layout(false, false, __VA_ARGS__))
#define widget_create_layout_vert(...) (new widget_layout(true,  false, __VA_ARGS__))
	
	//This one allows some widgets to take up multiple boxes of the grid. They're still stored row by
	// row, except that there is no entry for slots that are already used.
	//It is undefined behaviour if a widget does not fit where it belongs, if it overlaps another widget,
	// or if it's size 0 in either direction.
	widget_layout(unsigned int totwidth,   unsigned int totheight,   bool uniformwidths, bool uniformheights,
	              unsigned int firstwidth, unsigned int firstheight, widget_base* firstchild, ...);
#define widget_create_layout(...) (new widget_layout(__VA_ARGS__))
	
	//In this one, the widths/heights arrays can be NULL, which is treated as being filled with 1s.
	//But if you want that, you should probably use the grid constructor instead. (Though it's useful for the constructors themselves.)
	widget_layout(unsigned int numchildren, widget_base* * children,
	              unsigned int totwidth,  unsigned int * widths,  bool uniformwidths,
	              unsigned int totheight, unsigned int * heights, bool uniformheights)
	{
		construct(numchildren, children, totwidth, widths, uniformwidths, totheight, heights, uniformheights);
	}
	
	~widget_layout();
	
public:
	struct impl;
	impl * m;
};
//The widgets are stored row by row. There is no NULL terminator, because the size is known from the arguments already.
//Uniform sizes mean that every row has the same height, and every column has the same width.
widget_layout* widget_create_layout_grid(unsigned int width, unsigned int height, bool uniformsizes, widget_base* firstchild, ...);


class widget_label : public widget_base { WIDGET_BASE
public:
	widget_label(const char * text = "");
	~widget_label();
	
	//Disabling a label does nothing, but may change how it looks.
	//Useful it if it's attached to another widget, and this widget is disabled.
	widget_label* set_enabled(bool enable);
	
	widget_label* set_text(const char * text);
	widget_label* set_ellipsize(bool ellipsize);//Defaults to false.
	//Alignment 0 means touch the left side, 1 means centered, 2 means touch the right side. Defaults to left.
	widget_label* set_alignment(int align);
	
public:
	struct impl;
	impl * m;
};
#define widget_create_label(...) new widget_label(__VA_ARGS__)


class widget_button : public widget_base { WIDGET_BASE
public:
	widget_button(const char * text = "");
	~widget_button();
	
	widget_button* set_enabled(bool enable);
	widget_button* set_text(const char * text);
	widget_button* set_onclick(function<void()> onclick);
	
public:
	struct impl;
	impl * m;
};
#define widget_create_button(...) (new widget_button(__VA_ARGS__))


class widget_checkbox : public widget_base { WIDGET_BASE
public:
	widget_checkbox(const char * text = "");
	~widget_checkbox();
	
	widget_checkbox* set_enabled(bool enable);
	widget_checkbox* set_text(const char * text);
	bool get_state();
	widget_checkbox* set_state(bool checked);
	widget_checkbox* set_onclick(function<void(bool checked)> onclick);
	
public:
	struct impl;
	impl * m;
};
#define widget_create_checkbox(...) (new widget_checkbox(__VA_ARGS__))


class widget_radio : public widget_base { WIDGET_BASE
public:
	widget_radio(const char * text = "");
	~widget_radio();
	
	widget_radio* set_enabled(bool enable);
	
	widget_radio* set_text(const char * text);
	
	//The button this function is called on becomes the group leader. The leader must be the first in the group.
	//It is undefined behaviour to attempt to redefine a group.
	//It is undefined behaviour to set the onclick handler, or set or get the state, for anything except the group leader.
	//The window may not be shown before grouping them.
	widget_radio* group(unsigned int numitems, widget_radio* * group);
	
	//Returns which one is active. The group leader is 0.
	unsigned int get_state();
	
	//State values are the same as get_state().
	widget_radio* set_state(unsigned int state);
	
	//Called whenever the state changes. It is allowed to set the state in response to this.
	//It is undefined whether the callback can fire for the previously active state, for example due to clicking the button twice.
	//Must be set only for the group leader.
	widget_radio* set_onclick(function<void(unsigned int state)> onclick);
	
public:
	struct impl;
	impl * m;
};
#define widget_create_radio(...) (new widget_radio(__VA_ARGS__))

//This one wraps them in a horizontal or vertical layout, and groups them.
//It's just a convenience; you can create them and group them manually and get the same results.
//The first one will expect a set of radio buttons. The second will expect a set of radio button texts, and will put the group leader in the pointer.
widget_layout* widget_create_radio_group(bool vertical, widget_radio* leader, ...);
widget_layout* widget_create_radio_group(bool vertical, widget_radio* * leader, const char * firsttext, ...);
#define widget_create_radio_group_horz(...) widget_create_radio_group(false, __VA_ARGS__)
#define widget_create_radio_group_vert(...) widget_create_radio_group(true, __VA_ARGS__)


class widget_textbox : public widget_base { WIDGET_BASE
public:
	widget_textbox();
	~widget_textbox();
	
	widget_textbox* set_enabled(bool enable);
	widget_textbox* focus();
	
	//The return value is guaranteed valid until the next call to any function
	// on this object, or the next window_run[_iter], whichever comes first.
	const char * get_text();
	widget_textbox* set_text(const char * text);
	//If the length is 0, it's unlimited.
	widget_textbox* set_length(unsigned int maxlen);
	//How many instances of the letter 'X' should fit in the textbox without scrolling. Defaults to 5.
	widget_textbox* set_width(unsigned int xs);
	
	//Highlights the widget as containing invalid data. Can paint the background red, focus it, and various other stuff.
	//The invalidity highlight is removed as soon as the contents are changed, but may be restored on the onchange event.
	//Making a widget invalid and disabled simultaneously is undefined behaviour.
	widget_textbox* set_invalid(bool invalid);
	
	//Called whenever the text changes.
	//Note that it is not guaranteed to fire only if the text has changed; it may, for example,
	// fire if the user selects an E and types another E on top. Or for no reason at all.
	//Also note that 'text' is invalidated under the same conditions as get_text is.
	widget_textbox* set_onchange(function<void(const char * text)> onchange);
	//Called if the user hits Enter while this widget is focused. [TODO: Doesn't that activate the default button instead?]
	widget_textbox* set_onactivate(function<void(const char * text)> onactivate);
	
public:
	struct impl;
	impl * m;
};
#define widget_create_textbox(...) (new widget_textbox(__VA_ARGS__))


////A canvas is a simple image. It's easy to work with, but performance is poor and it can't vsync, so it shouldn't be used for video.
//class widget_canvas : public widget_base { WIDGET_BASE
//public:
//	widget_canvas(unsigned int width, unsigned int height);
//	~widget_canvas();
//	//can't disable this
//	
//	widget_canvas* resize(unsigned int width, unsigned int height);
//	uint32_t * (*draw_begin)();
//	void draw_end();
//	
//	//TODO
//	////Whether to hide the cursor while it's on top of this widget.
//	////The mouse won't instantly hide; if it's moving, it will be visible. The exact details are up to the implementation,
//	//// but it will be similar to "the mouse is visible if it has moved within the last 1000 milliseconds".
//	//widget_canvas* set_hide_cursor(bool hide);
//	//
//	////This must be called before the window is shown, and only exactly once.
//	////All given filenames are invalidated once the callback returns.
//	//widget_canvas* set_support_drop(function<void(const char * const * filenames)> on_file_drop);
//	
//public:
//	struct impl;
//	impl * m;
//};
//#define widget_create_canvas(width, height) (new widget_canvas(width, height))


//A viewport fills the same purpose as a canvas, but the tradeoffs go the opposite way.
class widget_viewport : public widget_base { WIDGET_BASE
public:
	widget_viewport(unsigned int width, unsigned int height);
	~widget_viewport();
	
	widget_viewport* resize(unsigned int width, unsigned int height);
	
	//There's no single way to render high-performance graphics (especially 3d), so a separate video driver is required.
	//Many video drivers (especially OpenGL-based ones) can't render to arbitrary windows, but must create their own windows;
	// therefore, this widget doesn't create its own window, but expects to be given one by the video driver.
	// The driver retains ownership and is expected to delete it.
	//The driver's created window should be a child of this one:
	uintptr_t get_parent();
	
	//As this widget is resizable, it needs a way to report size changes.
	//This is done via this function. Call it and the widget will move and resize the window to whereever this widget is located.
	//If the widget changes size, this will be reported to onresize(). Guaranteed to only be called if actually changed.
	//This callback will be called if the widget is altered by resize(). The rest of Arlib avoids calling callbacks for API-sourced calls,
	// but the video driver isn't the one who called resize().
	//The driver must return the new window after the resize. This may be the same as the old one. If different,
	// the driver is responsible for destroying the old one.
	//If the program wants to destroy the video driver, it must call set_contents(0, NULL, NULL) before doing so.
	//If the driver doesn't need the resize callback, it may return 0. However, set_contents() is still required.
	//ondestroy is called whenever the viewport is destroyed, if the viewport isn't disconnected first.
	void set_child(uintptr_t windowhandle, function<void(unsigned int width, unsigned int height)> onresize, function<void()> ondestroy);
	
	//TODO
	////See documentation of canvas for these.
	//widget_viewport* set_hide_cursor(bool hide);
	//widget_viewport* set_support_drop(function<void(const char * const * filenames)> on_file_drop);
	
	//TODO
	////Keycodes are from libretro; 0 if unknown. Scancodes are implementation defined and always present.
	//widget_viewport* set_kb_callback(function<void(unsigned int keycode, unsigned int scancode)> keyboard_cb);
	
public:
	struct impl;
	impl * m;
};
#define widget_create_viewport(width, height) (new widget_viewport(width, height))


class widget_listbox_virtual : public widget_base { WIDGET_BASE
private:
	void construct(unsigned int numcolumns, const char * * columns);
	
public:
	widget_listbox_virtual(unsigned int numcolumns, const char * * columns) { construct(numcolumns, columns); }
	template<typename... Args>
	widget_listbox_virtual(Args... cols)
	{
		const char * cols_up[] = { cols... };
		construct(sizeof...(cols), cols_up);
	}
	~widget_listbox_virtual();
	
	widget_listbox_virtual* set_enabled(bool enable);
	
	//Column -1 is the checkboxes, if they exist; NULL for unchecked, non-NULL (not necessarily a valid pointer) for checked.
	//The search callback should return the row ID closest to 'start' in the given direction where the first column starts with 'str'.
	//If 'start' itself starts with 'prefix', it should be returned.
	//If there is none in that direction, loop around. If still no match, return (size_t)-1.
	//It's optional, but recommended for better performance.
	//(GTK+ is stupid and doesn't let me use it.)
	widget_listbox_virtual* set_contents(function<const char * (size_t row, int column)> get_cell,
	                                     function<size_t(const char * prefix, size_t start, bool up)> search);
	
	//On Windows, the limit is 100 million; if more than that, it puts in 0.
	// Probably because it's a nice round number, and the listbox row height (19) times 100 million is fairly close to 2^31.
	//On GTK+, it's 100000; it's slow on huge lists, since it allocates memory for each row,
	// even when using gtk_tree_view_set_fixed_height_mode and similar.
	static size_t get_max_rows();
	
	//If more than get_max_rows(), it's capped to that.
	widget_listbox_virtual* set_num_rows(size_t rows);
	
	//Call this after changing anything. It's fine to change multiple rows before calling this.
	widget_listbox_virtual* refresh();
	
	//If the active row changes, set_focus_change will fire. However, onactivate will likely not.
	//The exact conditions under which a listbox entry is activated is platform dependent, but double
	// click and Enter are likely. It is guaranteed to be possible.
	//Returns (size_t)-1 if no row is active.
	size_t get_active_row();
	widget_listbox_virtual* set_on_focus_change(function<void(size_t row)> onchange);
	widget_listbox_virtual* set_onactivate(function<void(size_t row)> onactivate);
	
	//This is the size on the screen. The height is how many items show up below the header;
	// the widths are the longest string that should comfortably fit (or the column header, whichever is wider).
	//0 in height means "use some sensible default"; NULL in widths means "only check the column".
	//'expand' is which column should get all extra size, if the widget is given more space than it asked for; -1 for even distribution.
	//Defaults to 10, column headers, and -1.
	//TODO: do this on Windows.
	widget_listbox_virtual* set_size(unsigned int height, const char * const * widths, int expand);
	
	//It is implementation defined how the checkboxes are represented. They can be prepended to the
	// first column, on a column of their own, or something weirder. The position relative to the
	// other columns is not guaranteed, though it is likely to be the leftmost column.
	//The toggle callback does not contain the current nor former state; the user is expected to keep track of that.
	widget_listbox_virtual* add_checkboxes(function<void(size_t row)> ontoggle);
	
	//TODO (maybe): make columns editable; for windows, it's LVN_BEGINLABELEDIT
	
public:
	struct impl;
	impl * m;
};
#define widget_create_listbox_virtual(...) (new widget_listbox_virtual(__VA_ARGS__))


//Easier to use than the virtual listbox, but slower. Should be preferred for most usecases.
class widget_listbox : public widget_listbox_virtual
{
	size_t numcols;
	size_t numrows;
	char * * * cells; // [row][col] is a string
	
	const char * get_cell(size_t row, int column) { return cells[row][column]; }
	
	void init(int numcols)
	{
		this->numcols = numcols;
		this->numrows = 0;
		this->cells = NULL;
		widget_listbox_virtual::set_contents(bind_this(&widget_listbox::get_cell), NULL);
	}
	
public:
	widget_listbox(unsigned int numcolumns, const char * * columns) : widget_listbox_virtual(numcolumns, columns)
	{
		init(numcolumns);
	}
	
	template<typename... Args>
	widget_listbox(Args... cols) : widget_listbox_virtual(cols...)
	{
		init(sizeof...(cols));
	}
	
	~widget_listbox()
	{
		for (size_t row=0;row<numrows;row++)
		{
			for (size_t col=0;col<numcols;col++)
			{
				free(cells[row][col]);
			}
			free(cells[row]);
		}
		free(cells);
	}
	
	size_t rows() { return numrows; }
	
	widget_listbox* add_row(const char * const * cols) { insert_row(numrows, cols); return this; }
	widget_listbox* add_row(const char * * cols) { add_row((const char * const *)cols); return this; }
	template<typename... Args>
	widget_listbox* add_row(Args... cols)
	{
		const char * cols_up[] = { cols... };
		add_row(cols_up);
		return this;
	}
	
	widget_listbox* insert_row(size_t before, const char * const * cols)
	{
		cells = realloc(cells, sizeof(char**)*(numrows+1));
		memmove(cells+before+1, cells+before, sizeof(char**)*(numrows-before));
		numrows++;
		cells[before] = malloc(sizeof(char*)*numcols);
		for (size_t col=0;col<numcols;col++)
		{
			cells[before][col] = strdup(cols[col]);
		}
		widget_listbox_virtual::set_num_rows(numrows);
		return this;
	}
	widget_listbox* insert_row(size_t before, const char * * cols) { insert_row(before, (const char * const *)cols); return this; }
	template<typename... Args>
	widget_listbox* insert_row(size_t before, Args... cols)
	{
		const char * cols_up[] = { cols... };
		insert_row(before, cols_up);
		return this;
	}
	
	widget_listbox* delete_row(size_t row)
	{
		for (size_t col=0;col<numcols;col++)
		{
			free(cells[row][col]);
		}
		numrows--;
		memmove(cells+row, cells+row+1, sizeof(char**)*(numrows-row));
		widget_listbox_virtual::set_num_rows(numrows);
		return this;
	}
	
	widget_listbox* replace_row(size_t row, const char * const * cols)
	{
		for (size_t col=0;col<numcols;col++)
		{
			free(cells[row][col]);
			cells[row][col] = strdup(cols[col]);
		}
		widget_listbox_virtual::refresh();
		return this;
	}
	
	widget_listbox* replace_row(size_t row, const char ** cols) { replace_row(row, (const char * const *)cols); return this; }
	template<typename... Args>
	widget_listbox* replace_row(size_t row, Args... cols)
	{
		const char * cols_up[] = { cols... };
		replace_row(row, cols_up);
		return this;
	}
	
	widget_listbox* replace_cell(size_t row, size_t col, const char * text)
	{
		free(cells[row][col]);
		cells[row][col] = strdup(text);
		widget_listbox_virtual::refresh();
		return this;
	}
	
private:
	//Calling these will screw up the internal state. Yes, you can do it, but it'll break if you try.
	
	widget_listbox_virtual* set_contents(function<const char * (size_t row, int column)> get_cell,
	                             function<size_t(const char * prefix, size_t start, bool up)> search);
	widget_listbox_virtual* set_num_rows(size_t rows);
	widget_listbox_virtual* refresh(size_t row);
	widget_listbox_virtual* add_checkboxes(function<void(size_t row)> ontoggle);
};
#define widget_create_listbox(...) (new widget_listbox(__VA_ARGS__))


//A decorative frame around a widget, to group them together. The widget can be a layout (and probably should, otherwise you're adding a box to a single widget).
class widget_frame : public widget_base { WIDGET_BASE
public:
	widget_frame(const char * text, widget_base* contents);
	~widget_frame();
	
	//can't disable this (well okay, we can, but it's pointless to disable a widget that does nothing)
	widget_frame* set_text(const char * text);
	
public:
	struct impl;
	impl * m;
};
#define widget_create_frame(...) (new widget_frame(__VA_ARGS__))



//The only thing that may be done with a menu item before it's in a window, is passing it into another menu constructor.
//The text pointers, if any, must be also be valid until added to the parent.
//Destruction is not allowed.
class windowmenu : nocopy {
public:
	virtual ~windowmenu() = 0;
public:
	struct impl;
	impl * m;
};
inline windowmenu::~windowmenu(){}

class windowmenu_item : public windowmenu {
public:
	void set_enabled(bool enable);
	
	static windowmenu_item* create(const char * text, function<void(void)> onactivate);
	~windowmenu_item();
	
public:
	struct impl;
	impl * mu;
};
class windowmenu_check : public windowmenu {
public:
	void set_enabled(bool enable);
	bool get_checked();
	void set_checked(bool checked);
	
	static windowmenu_check* create(const char * text, function<void(bool checked)> onactivate);
	~windowmenu_check();
	
public:
	struct impl;
	impl * mu;
};
class windowmenu_radio : public windowmenu {
public:
	void set_enabled(bool enable);
	//If the new state is out of range, it's undefined behaviour.
	unsigned int get_state();
	void set_state(unsigned int state);
	
	static windowmenu_radio* create(function<void(unsigned int state)> onactivate, const char * firsttext, ...);
	static windowmenu_radio* create(unsigned int numitems, const char * const * texts, function<void(unsigned int state)> onactivate);
	~windowmenu_radio();
	
public:
	struct impl;
	impl * mu;
};
class windowmenu_separator : public windowmenu {
public:
	static windowmenu_separator* create();
	~windowmenu_separator();
	
public:
	struct impl;
	impl * mu;
};
class windowmenu_menu : public windowmenu {
public:
	void insert_child(unsigned int pos, windowmenu* child);
	void remove_child(windowmenu* child);
	
	static windowmenu_menu* create(const char * text, windowmenu* firstchild, ...);
	static windowmenu_menu* create(const char * text, unsigned int numchildren, windowmenu* const * children);
	
	//This one goes in a window. Anything else only goes in other menues.
	static windowmenu_menu* create_top(windowmenu_menu* firstchild, ...);
	static windowmenu_menu* create_top(unsigned int numchildren, windowmenu_menu* const * children);
	
	~windowmenu_menu();
	
public:
	struct impl;
	impl * mu;
};



//Tells the window manager to handle recent events and fire whatever callbacks are relevant.
//Neither of them are allowed while inside any callback of any kind.
//Some other functions may call these two.
void window_run_iter();//Returns as soon as possible. Use if, for example, you're displaying an animation.
void window_run_wait();//Returns only after doing something. Use while idling. It will return if any
                       // state (other than the time) has changed or if any callback has fired.
                       //It may also return due to uninteresting events, as often as it wants;
                       // however, repeatedly calling it will leave the CPU mostly idle.

////Shows a message box. You can do that by creating a label and some buttons, but it gives inferior results.
////Returns true for OK and Yes, and false for Cancel/No/close window.
////The title may or may not be ignored.
//enum mbox_sev { mb_info, mb_warn, mb_err };
//enum mbox_btns { mb_ok, mb_okcancel, mb_yesno };
//bool window_message_box(const char * text, const char * title, enum mbox_sev severity, enum mbox_btns buttons);

////Usable for both ROMs and dylibs. If dylib is true, the returned filenames are for the system's
//// dynamic linker; this will disable gvfs-like systems the dynamic linker can't understand, and may
//// hide files not marked executable, if this makes sense. If false, only file_read/etc is guaranteed
//// to work.
////If multiple is true, multiple files may be picked; if not, only one can be picked. Should
//// generally be true for dylibs and false for ROMs, but not guaranteed.
////The parent window will be disabled while the dialog is active.
////Both extensions and return value have the format { "smc", ".sfc", NULL }. Extensions are optional.
////Return value is full paths, zero or more. Duplicates are allowed in both input and output.
////The return value is valid until the next call to window_file_picker() or window_run_*(), whichever comes first.
//const char * const * window_file_picker(window * parent,
//                                        const char * title,
//                                        const char * const * extensions,
//                                        const char * extdescription,
//                                        bool dylib,
//                                        bool multiple);

////Returns the number of microseconds since an undefined start time.
////The start point doesn't change while the program is running, but need not be the same across reboots, nor between two processes.
////It can be program launch, system boot, the Unix epoch, or whatever.
//uint64_t window_get_time();

//Implementation details, don't touch.
void _window_init_inner();
void _window_init_misc();
void _window_init_shell();
uintptr_t _window_notify_inner(void* notification);
uintptr_t _window_get_widget_color(unsigned int type, void* handle, void* draw, void* parent);

//This one can be used if the one calling widget_listbox_virtual->set_contents doesn't provide a search function.
size_t _widget_listbox_search(function<const char *(size_t row, int column)> get_cell, size_t rows,
                              const char * prefix, size_t start, bool up);

#ifdef ARGUIPROT_X11
//Returns the display and screen we should use.
//The concept of screens only exists on X11, so this should not be used elsewhere.
//Only I/O drivers should have any reason to use this.
struct _XDisplay;
typedef struct _XDisplay Display;
struct window_x11_info {
	Display* display;
	int screen;
	unsigned long root; //The real type is Window aka XID.
};
extern struct window_x11_info window_x11;
#endif

//TODO: If porting to Qt, use https://woboq.com/blog/verdigris-qt-without-moc.html
//Windows resources are bad enough, but at least they have a reason to exist -
// they have to be available without executing the program. moc has no such excuse.
