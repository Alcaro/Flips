//#include<stdint.h>
//#include<time.h>
//static uint64_t nanotime(){
//struct timespec tv;
//clock_gettime(CLOCK_MONOTONIC, &tv);
//return tv.tv_sec*(uint64_t)1000000000 + tv.tv_nsec;}
#include "window.h"
#ifdef ARGUI_GTK3
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

//there is a gtk_tree_view_insert_column_with_data_func, but while it can get rid of
// virtual_list_get_value, it can not get rid of virtual_list_iter_n_children

//because GtkTreeView insists on iterating the entire list and building some silly tree out of it
//there's no need for anything like that; it's a list, you have O(1) to everything
//see gtk_tree_view_build_tree for details
#define MAX_ROWS 100000

//http://scentric.net/tutorial/
static GType M_VIRTUAL_TYPE=0;
#define M_VIRTUAL_LIST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), M_VIRTUAL_TYPE, struct VirtualList))
#define M_IS_VIRTUAL_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), M_VIRTUAL_TYPE))

struct VirtualList
{
	GObject g_parent;//leave this one on top
	
	size_t rows;
	unsigned int columns;
	bool checkboxes;
	
	struct widget_listbox_virtual * subject;
	function<const char * (size_t row, int column)> get_cell;
};

struct VirtualListClass
{
	GObjectClass parent_class;
};

static GtkTreeModelFlags virtual_list_get_flags(GtkTreeModel* tree_model)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), (GtkTreeModelFlags)0);
	return (GtkTreeModelFlags)(GTK_TREE_MODEL_LIST_ONLY|GTK_TREE_MODEL_ITERS_PERSIST);
}

static gint virtual_list_get_n_columns(GtkTreeModel* tree_model)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), 0);
	return M_VIRTUAL_LIST(tree_model)->columns;
}

static GType virtual_list_get_column_type(GtkTreeModel* tree_model, gint index)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), G_TYPE_INVALID);
	g_return_val_if_fail(index>=0 && (unsigned int)index<M_VIRTUAL_LIST(tree_model)->columns, G_TYPE_INVALID);
	
	return G_TYPE_STRING;
}

static gboolean virtual_list_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreePath* path)
{
	g_assert(M_IS_VIRTUAL_LIST(tree_model));
	g_assert(path!=NULL);
	g_assert(gtk_tree_path_get_depth(path)==1);
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	
	uintptr_t n=gtk_tree_path_get_indices(path)[0];
	if (n>=virtual_list->rows || n<0) return FALSE;
	
	iter->stamp=0;
	iter->user_data=(void*)n;
	iter->user_data2=NULL;//we don't need those two
	iter->user_data3=NULL;
	
	return TRUE;
}

static GtkTreePath* virtual_list_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), NULL);
	g_return_val_if_fail(iter!=NULL, NULL);
	
	GtkTreePath* path=gtk_tree_path_new();
	gtk_tree_path_append_index(path, (uintptr_t)iter->user_data);
	
	return path;
}

static void virtual_list_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, gint column, GValue* value)
{
	g_return_if_fail(M_IS_VIRTUAL_LIST(tree_model));
	g_return_if_fail(iter!=NULL);
	g_return_if_fail(column>=0);
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	uintptr_t row=(uintptr_t)iter->user_data;
	
	unsigned int ucolumn=column;
	
	//if (row == MAX_ROWS)
	//{
	//	if (ucolumn == virtual_list->columns)
	//	{
	//		g_value_init(value, G_TYPE_BOOLEAN);
	//		g_value_set_boolean(value, false);
	//		return;
	//	}
	//	else
	//	{
	//		g_value_init(value, G_TYPE_STRING);
	//		//if(0);
	//		//else if (virtual_list->columns==1) g_value_set_string(value, "(sorry, not supported)");
	//		//else if (virtual_list->columns==2 && column==0) g_value_set_string(value, "(sorry, not");
	//		//else if (virtual_list->columns==2 && column==1) g_value_set_string(value, "supported)");
	//		//else if (virtual_list->columns==3 && column==0) g_value_set_string(value, "(sorry,");
	//		//else if (virtual_list->columns==3 && column==1) g_value_set_string(value, "not");
	//		//else if (virtual_list->columns==3 && column==2) g_value_set_string(value, "supported)");
	//		//else g_value_set_string(value, "");
	//		g_value_set_string(value, "...");
	//	}
	//	return;
	//}
	
	if (ucolumn == virtual_list->columns)
	{
		g_value_init(value, G_TYPE_BOOLEAN);
		g_value_set_boolean(value, virtual_list->get_cell(row, -1) ? true : false);
		return;
	}
	
	g_return_if_fail(ucolumn<virtual_list->columns);
	
	
	if (row >= virtual_list->rows) g_return_if_reached();
	
	g_value_init(value, G_TYPE_STRING);
	const char * ret=virtual_list->get_cell(row, ucolumn);
	g_value_set_string(value, ret);
}

static gboolean virtual_list_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), FALSE);
	
	if (!iter) return FALSE;
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	
	uintptr_t id = (uintptr_t)iter->user_data;
	if (id >= virtual_list->rows-1) return FALSE;
	
	iter->stamp = 0;
	iter->user_data = (void*)(id+1);
	
	return TRUE;
}

static gboolean virtual_list_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), FALSE);
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	
	if (parent || virtual_list->rows==0) return FALSE;
	
	iter->stamp=0;
	iter->user_data=(void*)(uintptr_t)0;
	
	return TRUE;
}

static gboolean virtual_list_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
	return FALSE;
}

static gint virtual_list_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), -1);
	g_return_val_if_fail(iter==NULL || iter->user_data!=NULL, FALSE);
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	
	if (iter) return 0;
	return virtual_list->rows;
}

static gboolean virtual_list_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent, gint n)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), FALSE);
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	
	if (parent) return FALSE;
	if (n<0) return FALSE;
	if ((unsigned int)n >= virtual_list->rows) return FALSE;
	
	iter->stamp = 0;
	iter->user_data = (void*)(uintptr_t)n;
	
	return TRUE;
}

static gboolean virtual_list_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* child)
{
	return FALSE;
}

static void virtual_list_tree_model_init (GtkTreeModelIface* iface)
{
	iface->get_flags = virtual_list_get_flags;
	iface->get_n_columns = virtual_list_get_n_columns;
	iface->get_column_type = virtual_list_get_column_type;
	iface->get_iter = virtual_list_get_iter;
	iface->get_path = virtual_list_get_path;
	iface->get_value = virtual_list_get_value;
	iface->iter_next = virtual_list_iter_next;
	iface->iter_children = virtual_list_iter_children;
	iface->iter_has_child = virtual_list_iter_has_child;
	iface->iter_n_children = virtual_list_iter_n_children;
	iface->iter_nth_child = virtual_list_iter_nth_child;
	iface->iter_parent = virtual_list_iter_parent;
}

static void init_widget()
{
	if (M_VIRTUAL_TYPE != 0) return;
	
	static const GTypeInfo virtual_list_info = {
		sizeof(struct VirtualListClass),
		NULL, NULL, NULL, NULL, NULL,
		sizeof(struct VirtualList), 0, NULL };
	static const GInterfaceInfo tree_model_info = { (GInterfaceInitFunc)virtual_list_tree_model_init, NULL, NULL };
	
	M_VIRTUAL_TYPE = g_type_register_static(G_TYPE_OBJECT, "ArlibVirtualList", &virtual_list_info, (GTypeFlags)0);
	g_type_add_interface_static(M_VIRTUAL_TYPE, GTK_TYPE_TREE_MODEL, &tree_model_info);
}





struct widget_listbox_virtual::impl {
	GtkTreeView* tree;
	gint borderheight;
	gint checkwidth;
	GtkCellRenderer* render;
	
	struct VirtualList* vlist;
	
	function<void(size_t row)> onfocus;
	function<void(size_t row)> onactivate;
	function<void(size_t row)> ontoggle;
};

void widget_listbox_virtual::construct(unsigned int numcolumns, const char * * columns)
{
	init_widget();
	
	m = new impl;
	
	widget = gtk_scrolled_window_new(NULL, NULL);
	widthprio = 3;
	heightprio = 3;
	
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	m->tree = GTK_TREE_VIEW(gtk_tree_view_new());
	gtk_container_add(GTK_CONTAINER(widget), GTK_WIDGET(m->tree));
	
	m->vlist = (VirtualList*)g_object_new(M_VIRTUAL_TYPE, NULL);
	m->vlist->subject = this;
	
	m->vlist->columns = numcolumns;
	m->vlist->rows = 0;
	m->vlist->checkboxes = false;
	
	m->render = gtk_cell_renderer_text_new();
	
	for (unsigned int i=0;i<numcolumns;i++)
	{
		gtk_tree_view_insert_column_with_attributes(m->tree, -1, columns[i], m->render, "text", i, NULL);
		GtkTreeViewColumn* col = gtk_tree_view_get_column(m->tree, i);
		gtk_tree_view_column_set_expand(col, true);
		gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
	}
	
	gtk_widget_set_hexpand(GTK_WIDGET(this->widget), true);
	gtk_widget_set_vexpand(GTK_WIDGET(this->widget), true);
	gtk_tree_view_set_fixed_height_mode(m->tree, true);
	
	gtk_widget_show_all(GTK_WIDGET(m->tree));
	
	GtkRequisition size;
	gtk_widget_get_preferred_size(GTK_WIDGET(m->tree), NULL, &size);
	m->borderheight = size.height;
	
	m->checkwidth = 0;
	
	set_size(10, NULL, -1);
}

widget_listbox_virtual::~widget_listbox_virtual()
{
	delete m;
}

//static void widget_listbox_virtual_refresh_row(widget_listbox_virtual* obj, size_t row)
//{
//	GtkTreeIter iter;
//	gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(obj->m->vlist), &iter, NULL, row);
//	GtkTreePath* path=gtk_tree_path_new_from_indices(row, -1);
//	gtk_tree_model_row_changed(GTK_TREE_MODEL(obj->m->vlist), path, &iter);
//	gtk_tree_path_free(path);
//}

widget_listbox_virtual* widget_listbox_virtual::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(m->tree), enable);
	return this;
}

widget_listbox_virtual* widget_listbox_virtual::set_contents(function<const char * (size_t row, int column)> get_cell,
                                                             function<size_t(const char * prefix, size_t start, bool up)> search)
{
	//ignore the search function, there is no valid way for gtk+ to use it
	m->vlist->get_cell=get_cell;
	return this;
}

widget_listbox_virtual* widget_listbox_virtual::set_num_rows(size_t rows)
{
	if (rows > MAX_ROWS) rows = MAX_ROWS;
	
	GtkAdjustment* adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(m->tree));
	double scrollfrac = gtk_adjustment_get_value(adj) / m->vlist->rows;
	
	//this is piss slow for some reason I can't figure out
	gtk_tree_view_set_model(m->tree, GTK_TREE_MODEL(NULL));
	m->vlist->rows = rows;
	gtk_tree_view_set_model(m->tree, GTK_TREE_MODEL(m->vlist));
	
	if (scrollfrac == scrollfrac)
	{
		gtk_adjustment_set_upper(adj, scrollfrac * m->vlist->rows + gtk_adjustment_get_page_size(adj));
		gtk_adjustment_changed(adj);
		gtk_adjustment_set_value(adj, scrollfrac * m->vlist->rows);
		gtk_adjustment_value_changed(adj);//shouldn't it do this by itself?
	}
	return this;
}

size_t widget_listbox_virtual::get_max_rows()
{
	return MAX_ROWS;
}

widget_listbox_virtual* widget_listbox_virtual::refresh()
{
	gtk_widget_queue_draw(GTK_WIDGET(m->tree));
	return this;
}

size_t widget_listbox_virtual::get_active_row()
{
	GList* list = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(m->tree), NULL);
	if (!list) return (size_t)-1;
	size_t ret = gtk_tree_path_get_indices((GtkTreePath*)list->data)[0];
	//if (ret == MAX_ROWS) ret = (size_t)-1;
	g_list_free_full(list, (GDestroyNotify)gtk_tree_path_free);
	return ret;
}

static void listbox_on_focus_change(GtkTreeView* tree_view, gpointer user_data)
{
	widget_listbox_virtual* obj=(widget_listbox_virtual*)user_data;
	GtkTreePath* path;
	gtk_tree_view_get_cursor(obj->m->tree, &path, NULL);
	size_t item = (size_t)-1;
	if (path) item = gtk_tree_path_get_indices(path)[0];
	//if (item == MAX_ROWS) item = (size_t)-1;
	obj->m->onfocus(item);
	if (path) gtk_tree_path_free(path);
}

widget_listbox_virtual* widget_listbox_virtual::set_on_focus_change(function<void(size_t row)> onchange)
{
	m->onfocus = onchange;
	g_signal_connect(m->tree, "cursor-changed", G_CALLBACK(listbox_on_focus_change), this);
	return this;
}

static void listbox_onactivate(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data)
{
	widget_listbox_virtual* obj = (widget_listbox_virtual*)user_data;
	int item = gtk_tree_path_get_indices(path)[0];
	if (item != MAX_ROWS)
	{
		obj->m->onactivate(item);
	}
}

widget_listbox_virtual* widget_listbox_virtual::set_onactivate(function<void(size_t row)> onactivate)
{
	m->onactivate=onactivate;
	g_signal_connect(m->tree, "row-activated", G_CALLBACK(listbox_onactivate), this);
	return this;
}

widget_listbox_virtual* widget_listbox_virtual::set_size(unsigned int height, const char * const * widths, int expand)
{
	gint width_total = 0;
	for (unsigned int i=0;i<m->vlist->columns;i++)
	{
		//figuring out this algorithm took about half an hour of digging in the GTK+ source code.
		//it's fairly simple, but it's spread out all over the place so I can't find the parts I want.
		GtkTreeViewColumn* col = gtk_tree_view_get_column(m->tree, (m->vlist->checkboxes ? 1 : 0) + i);
		
		gint size1;
		gtk_widget_get_preferred_width(gtk_tree_view_column_get_button(col), NULL, &size1);
		
		GtkRequisition size2;
		if (widths && widths[i])
		{
			g_object_set(m->render, "text", widths[i], NULL);
			gtk_cell_renderer_get_preferred_size(m->render, GTK_WIDGET(widget), NULL, &size2);
			
			gint horizontal_separator;
			gtk_widget_style_get(GTK_WIDGET(m->tree), "horizontal-separator", &horizontal_separator, NULL);
			size2.width += horizontal_separator;
		}
		else size2.width = 0;
		//there's also a grid_line_width (gtktreeview.c:6110 or near _gtk_tree_view_column_push_padding),
		//but I don't use grid lines, so I'll ignore that.
		
		gint colwidth = (size1 > size2.width ? size1 : size2.width);
		gtk_tree_view_column_set_fixed_width(col, colwidth);
		width_total += colwidth;
	}
	gtk_widget_set_size_request(GTK_WIDGET(m->tree), width_total + m->checkwidth, -1);
	
	if (height)
	{
		GtkRequisition size;
		g_object_set(m->render, "text", "A", NULL);
		gtk_cell_renderer_get_preferred_size(m->render, GTK_WIDGET(widget), NULL, &size);
		gtk_widget_set_size_request(GTK_WIDGET(widget), -1, m->borderheight + (size.height+2) * height);
	}
	
	for (unsigned int i=0;i<m->vlist->columns;i++)
	{
		gtk_tree_view_column_set_expand(gtk_tree_view_get_column(m->tree, i), (expand == -1) || (i == (unsigned int)expand));
	}
	
	return this;
}

static void widget_listbox_virtual_checkbox_toggle(GtkCellRendererToggle* cell_renderer, gchar* path, gpointer user_data)
{
	widget_listbox_virtual* obj = (widget_listbox_virtual*)user_data;
	unsigned int row = atoi(path);
	obj->m->ontoggle(row);
	obj->refresh();
}

widget_listbox_virtual* widget_listbox_virtual::add_checkboxes(function<void(size_t row)> ontoggle)
{
	m->vlist->checkboxes = true;
	
	GtkCellRenderer* render = gtk_cell_renderer_toggle_new();
	gtk_tree_view_insert_column_with_attributes(m->tree, 0, "", render, "active", m->vlist->columns, NULL);
	
	//gint checkheight;
	//g_object_get(render, "height", &checkheight, NULL);
	//if (checkheight>m->cellheight) m->cellheight=checkheight;
	
	m->ontoggle=ontoggle;
	g_signal_connect(render, "toggled", G_CALLBACK(widget_listbox_virtual_checkbox_toggle), this);
	
	GtkRequisition checksize;
	gtk_cell_renderer_get_preferred_size(render, GTK_WIDGET(widget), NULL, &checksize);
	
	gint horizontal_separator;
	gtk_widget_style_get(GTK_WIDGET(m->tree), "horizontal-separator", &horizontal_separator, NULL);
	
	GtkTreeViewColumn* col = gtk_tree_view_get_column(m->tree, 0);
	gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(col, checksize.width + horizontal_separator);
	
	m->checkwidth = checksize.width + horizontal_separator;
	
	gint origwidth;
	gtk_widget_get_size_request(GTK_WIDGET(m->tree), &origwidth, NULL);
	printf("%i+%i+%i\n",origwidth,checksize.width, horizontal_separator);
	gtk_widget_set_size_request(GTK_WIDGET(m->tree), m->checkwidth + origwidth, -1);
	
	gtk_widget_show_all(GTK_WIDGET(widget));
	return this;
}
#endif
