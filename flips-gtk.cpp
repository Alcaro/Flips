//Module name: Floating IPS, GTK+ frontend
//Author: Alcaro
//Date: See Git history
//Licence: GPL v3.0 or higher

//List of assumptions made whose correctness is not guaranteed by GTK+:
//The character '9' is as wide as the widest of '0' '1' '2' '3' '4' '5' '6' '7' '8' '9'.
// Failure leads to: The BPS delta creation progress window being a little too small.
// Fixable: Not hard, but unlikely to be worth it.
//g_spawn_async does not write to argv or its pointed-to strings.
// Failure leads to: Corrupting the configuration.
// Fixable: Not hard, but unlikely to be worth it.

#include "flips.h"

#ifdef FLIPS_GTK
#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_10
//I'd prefer enabling this, but that makes the GTK headers throw about 500 warnings about GtkFileChooserNative. probably missing ifdef
//#define GDK_VERSION_MAX_ALLOWED GDK_VERSION_3_10
#include <gtk/gtk.h>

class file_gtk : public file {
	size_t size;
	GFileInputStream* io;
	
public:
	static file* create(const char * filename)
	{
		GFile* file = g_file_new_for_commandline_arg(filename);
		GFileInputStream* io=g_file_read(file, NULL, NULL);
		g_object_unref(file);
		if (!io) return NULL;
		return new file_gtk(io);
	}
	
private:
	file_gtk(GFileInputStream* io) : io(io)
	{
		GFileInfo* info = g_file_input_stream_query_info(io, G_FILE_ATTRIBUTE_STANDARD_SIZE, NULL, NULL);
		size = g_file_info_get_size(info);
		g_object_unref(info);
	}
	
public:
	size_t len() { return size; }
	
	bool read(uint8_t* target, size_t start, size_t len)
	{
		g_seekable_seek(G_SEEKABLE(io), start, G_SEEK_SET, NULL, NULL);
		gsize actualsize;
		return (g_input_stream_read_all(G_INPUT_STREAM(io), target, len, &actualsize, NULL, NULL) && actualsize == len);
	}
	
	~file_gtk() { g_object_unref(io); }
};

file* file::create(const char * filename) { return file_gtk::create(filename); }
bool file::exists(const char * filename)
{
	GFile* file = g_file_new_for_commandline_arg(filename);
	bool ret = g_file_query_exists(file, NULL);
	g_object_unref(file);
	return ret;
}


class filewrite_gtk : public filewrite {
	GOutputStream* io;
	
public:
	static filewrite* create(const char * filename)
	{
		GFile* file = g_file_new_for_commandline_arg(filename);
		if (!file) return NULL;
		GFileOutputStream* io = g_file_replace(file, NULL, false, G_FILE_CREATE_NONE, NULL, NULL);
		g_object_unref(file);
		if (!io) return NULL;
		return new filewrite_gtk(G_OUTPUT_STREAM(io));
	}
	
private:
	filewrite_gtk(GOutputStream* io) : io(io) {}
	
public:
	bool append(const uint8_t* data, size_t len)
	{
		return g_output_stream_write_all(io, data, len, NULL, NULL, NULL);
	}
	
	~filewrite_gtk() { g_object_unref(io); }
};

filewrite* filewrite::create(const char * filename)
{
	filewrite* ret = filewrite_gtk::create(filename); // g_file_create tries to create <dir>/.goutputstream-asdfghjkl
	if (!ret) ret = filewrite::create_libc(filename); // if it fails (for example /dev/.goutputstream-foobar), try this instead
	return ret;
}


class filemap_gtk : public filemap {
public:
	GMappedFile* mapfile;
	
	static filemap* create(const char * filename)
	{
		GMappedFile* mapfile = g_mapped_file_new(filename, false, NULL);
		if (!mapfile) return NULL;
		return new filemap_gtk(mapfile);
	}
	
	filemap_gtk(GMappedFile* mapfile) : mapfile(mapfile) {}
	
	size_t len() { return g_mapped_file_get_length(mapfile); }
	const uint8_t * ptr() { return (uint8_t*)g_mapped_file_get_contents(mapfile); }
	
	~filemap_gtk() { g_mapped_file_unref(mapfile); }
};
filemap* filemap::create(const char * filename)
{
	filemap* ret = filemap_gtk::create(filename);
	if (!ret) ret = filemap::create_fallback(filename);
	return ret;
}



static bool canShowGUI;
static GtkWidget* window;
static bool isFlatpak;

//struct {
//	char signature[9];
//	unsigned int lastPatchType;
//	bool createFromAllFiles;
//	bool openInEmulatorOnAssoc;
//	bool autoSelectRom;
//	gchar * emulator;
//} static state;
//#define cfgversion 5

static GtkWidget* windowBpsd;
static GtkWidget* labelBpsd;
static bool bpsdCancel;

//static void bpsdeltaCancel(GtkWindow* widget, gpointer user_data)
//{
//	bpsdCancel=true;
//}

void bpsdeltaBegin()
{
	bpsdCancel=false;
	windowBpsd=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	if (window)
	{
		gtk_window_set_modal(GTK_WINDOW(windowBpsd), true);
		gtk_window_set_transient_for(GTK_WINDOW(windowBpsd), GTK_WINDOW(window));
	}
	gtk_window_set_title(GTK_WINDOW(windowBpsd), flipsversion);
	
	labelBpsd=gtk_label_new("Please wait... 99.9%");
	gtk_container_add(GTK_CONTAINER(windowBpsd), labelBpsd);
	GtkRequisition size;
	gtk_widget_get_preferred_size(labelBpsd, NULL, &size);
	gtk_label_set_text(GTK_LABEL(labelBpsd), "Please wait... 0.0%");
	gtk_widget_set_size_request(labelBpsd, size.width, size.height);
	gtk_window_set_resizable(GTK_WINDOW(windowBpsd), false);
	
	gtk_misc_set_alignment(GTK_MISC(labelBpsd), 0.0f, 0.5f);
	
	gtk_widget_show_all(windowBpsd);
}

bool bpsdeltaProgress(void* userdata, size_t done, size_t total)
{
	if (bpsdeltaGetProgress(done, total))
	{
		gtk_label_set_text(GTK_LABEL(labelBpsd), bpsdProgStr);
	}
	gtk_main_iteration_do(false);
	return !bpsdCancel;
}

void bpsdeltaEnd()
{
	if (!bpsdCancel) gtk_widget_destroy(windowBpsd);
}

//'force' sets the filename even if the file doesn't exist
static void setoutpath(GtkFileChooser* dialog, const char * name, bool force)
{
	if (!name) return;
	
	gtk_file_chooser_set_uri(dialog, name);
	if (!force) return;
	gchar* filename = g_filename_from_uri(name, NULL, NULL);
	if (filename)
	{
		gchar* basename = g_path_get_basename(filename);
		gtk_file_chooser_set_current_name(dialog, basename ? basename : filename);
		g_free(filename);
		g_free(basename);
	}
}

static char * SelectRom(const char * defaultname, const char * title, bool isForSaving)
{
	GtkWidget* dialog;
	if (!isForSaving)
	{
		dialog = gtk_file_chooser_dialog_new(title, GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN,
		                                     "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
		setoutpath(GTK_FILE_CHOOSER(dialog), defaultname, false);
	}
	else
	{
		dialog = gtk_file_chooser_dialog_new(title, GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE,
		                                     "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
		setoutpath(GTK_FILE_CHOOSER(dialog), defaultname, true);
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), true);
	}
	
	GtkFileFilter* filterRom = gtk_file_filter_new();
	gtk_file_filter_set_name(filterRom, "Most Common ROM Files");
	gtk_file_filter_add_pattern(filterRom, "*.smc");
	gtk_file_filter_add_pattern(filterRom, "*.sfc");
	gtk_file_filter_add_pattern(filterRom, "*.nes");
	gtk_file_filter_add_pattern(filterRom, "*.gb");
	gtk_file_filter_add_pattern(filterRom, "*.gbc");
	gtk_file_filter_add_pattern(filterRom, "*.gba");
	gtk_file_filter_add_pattern(filterRom, "*.nds");
	gtk_file_filter_add_pattern(filterRom, "*.vb");
	gtk_file_filter_add_pattern(filterRom, "*.sms");
	gtk_file_filter_add_pattern(filterRom, "*.smd");
	gtk_file_filter_add_pattern(filterRom, "*.md");
	gtk_file_filter_add_pattern(filterRom, "*.ngp");
	gtk_file_filter_add_pattern(filterRom, "*.n64");
	gtk_file_filter_add_pattern(filterRom, "*.z64");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filterRom);
	
	GtkFileFilter* filterAll = gtk_file_filter_new();
	gtk_file_filter_set_name(filterAll, "All files");
	gtk_file_filter_add_pattern(filterAll, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filterAll);
	
	if (cfg.getint("allfiles")) gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filterAll);
	else gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filterRom);
	
	char * ret=NULL;
	if (gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT)
	{
		ret=gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
	}
	
	GtkFileFilter* thisfilter=gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(dialog));
	cfg.setint("allfiles", (thisfilter==filterAll));
	
	gtk_widget_destroy(dialog);
	return ret;
}

//returns path if demandLocal, else URI
static GSList * SelectPatches(bool allowMulti, bool demandLocal)
{
	GtkWidget* dialog=gtk_file_chooser_dialog_new(allowMulti?"Select Patches to Use":"Select Patch to Use", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN,
	                                              "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), allowMulti);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), demandLocal);
	
	GtkFileFilter* filter;
	
	filter=gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "All supported patches (*.bps, *.ips)");
	gtk_file_filter_add_pattern(filter, "*.bps");
	gtk_file_filter_add_pattern(filter, "*.ips");
	gtk_file_filter_add_pattern(filter, "*.ups");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	//apparently the file chooser takes ownership of the filter. would be nice to document that in gtk_file_chooser_set_filter...
	
	filter=gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "All files");
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	
	if (gtk_dialog_run(GTK_DIALOG(dialog))!=GTK_RESPONSE_ACCEPT)
	{
		gtk_widget_destroy(dialog);
		return NULL;
	}
	
	GSList * ret;
	if (demandLocal) ret=gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
	else ret=gtk_file_chooser_get_uris(GTK_FILE_CHOOSER(dialog));
	gtk_widget_destroy(dialog);
	return ret;
}

static void ShowMessage(struct errorinfo errinf)
{
	GtkMessageType errorlevels[]={ GTK_MESSAGE_OTHER, GTK_MESSAGE_OTHER, GTK_MESSAGE_WARNING, GTK_MESSAGE_WARNING, GTK_MESSAGE_ERROR, GTK_MESSAGE_ERROR };
	GtkWidget* dialog=gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, errorlevels[errinf.level], GTK_BUTTONS_CLOSE, "%s",errinf.description);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}



enum worsterrorauto { ea_none, ea_warning, ea_invalid, ea_io_rom_write, ea_io_rom_read, ea_no_auto, ea_io_read_patch };

struct multiapplystateauto {
	enum worsterrorauto error;
	bool anySuccess;
	
	const char * foundRom;
	bool canUseFoundRom;
	bool usingFoundRom;
};

static void ApplyPatchMultiAutoSub(gpointer data, gpointer user_data)
{
#define max(a,b) ((a)>(b)?(a):(b))
#define error(which) do { state->error=max(state->error, which); } while(0)
	gchar * patchpath=(gchar*)data;
	struct multiapplystateauto * state=(struct multiapplystateauto*)user_data;
	
	file* patch = file::create(patchpath);
	if (!patch)
	{
		state->canUseFoundRom=false;
		error(ea_io_read_patch);
		return;
	}
	
	bool possible;
	const char * rompath=FindRomForPatch(patch, &possible);
	if (state->usingFoundRom)
	{
		if (!rompath) rompath=state->foundRom;
		else goto cleanup;
	}
	else
	{
		if (!rompath)
		{
			if (possible) state->canUseFoundRom=false;
			error(ea_no_auto);
			goto cleanup;
		}
	}
	if (!state->foundRom) state->foundRom=rompath;
	if (state->foundRom!=rompath) state->canUseFoundRom=false;
	
	{
	const char * romext=GetExtension(rompath);
	gchar * outrompath=g_strndup(patchpath, strlen(patchpath)+strlen(romext)+1);
	strcpy(GetExtension(outrompath), romext);
	
	struct errorinfo errinf=ApplyPatchMem(patch, rompath, !cfg.getint("skipchecksum"), outrompath, NULL, true);
	if (errinf.level==el_broken) error(ea_invalid);
	if (errinf.level==el_notthis) error(ea_no_auto);
	if (errinf.level==el_warning) error(ea_warning);
	if (errinf.level<el_notthis) state->anySuccess=true;
	else state->canUseFoundRom=false;
	g_free(outrompath);
	}
	
cleanup:
	delete patch;
#undef max
#undef error
}

static bool ApplyPatchMultiAuto(GSList * filenames)
{
	struct multiapplystateauto state;
	state.error=ea_none;
	state.anySuccess=false;
	
	state.foundRom=NULL;
	state.canUseFoundRom=true;
	state.usingFoundRom=false;
	
	g_slist_foreach(filenames, ApplyPatchMultiAutoSub, &state);
	if (state.error==ea_no_auto && state.foundRom && state.canUseFoundRom)
	{
		state.usingFoundRom=true;
		state.error=ea_none;
		g_slist_foreach(filenames, ApplyPatchMultiAutoSub, &state);
	}
	
	if (state.anySuccess)
	{
		struct errorinfo messages[8]={
				{ el_ok, "The patches were applied successfully!" },//ea_none
				{ el_warning, "The patches were applied, but one or more may be mangled or improperly created..." },//ea_warning
				{ el_warning, "Some patches were applied, but not all of the given patches are valid..." },//ea_invalid
				{ el_warning, "Some patches were applied, but not all of the desired ROMs could be created..." },//ea_rom_io_write
				{ el_warning, "Some patches were applied, but not all of the input ROMs could be read..." },//ea_io_rom_read
				{ el_warning, "Some patches were applied, but not all of the required input ROMs could be located..." },//ea_no_auto
				{ el_warning, "Some patches were applied, but not all of the given patches could be read..." },//ea_io_read_patch
				{ el_broken, NULL },//ea_no_found
			};
		ShowMessage(messages[state.error]);
		return true;
	}
	return false;
}



enum worsterror { e_none, e_warning_notthis, e_warning, e_invalid_this, e_invalid, e_io_write, e_io_read, e_io_read_rom };

struct multiapplystate {
	const gchar * romext;
	struct mem rommem;
	bool anySuccess;
	bool removeHeaders;
	enum worsterror worsterror;
};

static void ApplyPatchMulti(gpointer data, gpointer user_data)
{
	char * patchname=(char*)data;
	struct multiapplystate * state=(struct multiapplystate*)user_data;
#define max(a,b) ((a)>(b)?(a):(b))
#define error(which) do { state->worsterror=max(state->worsterror, which); } while(0)
	
	file* patch = file::create(patchname);
	if (patch)
	{
		char * outromname=g_strndup(patchname, strlen(patchname)+strlen(state->romext)+1);
		char * outromext=GetExtension(outromname);
		strcpy(outromext, state->romext);
		
		struct errorinfo errinf=ApplyPatchMem2(patch, state->rommem, state->removeHeaders, !cfg.getint("skipchecksum"), outromname, NULL);
		if (errinf.level==el_broken) error(e_invalid);
		if (errinf.level==el_notthis) error(e_invalid_this);
		if (errinf.level==el_warning) error(e_warning);
		if (errinf.level==el_unlikelythis) error(e_warning_notthis);
		if (errinf.level<el_notthis) state->anySuccess=true;
		
		delete patch;
		g_free(outromname);
	}
	else error(e_io_read);
	g_free(data);
#undef max
#undef error
}

static void a_ApplyPatch(GtkButton* widget, gpointer user_data)
{
	gchar * filename=(gchar*)user_data;
	GSList * filenames=NULL;
	if (!filename)
	{
		filenames=SelectPatches(true, false);
		if (!filenames) return;
		if (!filenames->next) filename=(gchar*)filenames->data;
	}
	if (filename)//do not change to else, this is set if the user picks only one file
	{
		struct errorinfo errinf;
		file* patchfile = file::create(filename);
		if (!patchfile)
		{
			errinf=(struct errorinfo){ el_broken, "Couldn't read input patch. What exactly are you doing?" };
			ShowMessage(errinf);
			return;
		}
		
		char * inromname=NULL;
		if (cfg.getint("autorom")) inromname=g_strdup(FindRomForPatch(patchfile, NULL)); // g_strdup(NULL) is NULL
		if (!inromname) inromname=SelectRom(filename, "Select File to Patch", false);
		if (!inromname) goto cleanup;
		
		{
		const char * inromext=GetExtension(inromname);
		if (!inromext) inromext="";
		
		char * outromname_d=g_strndup(filename, strlen(filename)+strlen(inromext)+1);
		char * ext=GetExtension(outromname_d);
		strcpy(ext, inromext);
		
		char * outromname=SelectRom(outromname_d, "Select Output File", true);
		if (outromname)
		{
			struct errorinfo errinf=ApplyPatchMem(patchfile, inromname, !cfg.getint("skipchecksum"), outromname, NULL, cfg.getint("autorom"));
			ShowMessage(errinf);
		}
		g_free(inromname);
		g_free(outromname_d);
		g_free(outromname);
		}
		
	cleanup:
		delete patchfile;
	}
	else
	{
		if (cfg.getint("autorom"))
		{
			if (ApplyPatchMultiAuto(filenames))
			{
				g_slist_free_full(filenames, g_free);
				return;
			}
		}
		
		struct multiapplystate state;
		//picking one at random isn't always correct, but it's better than nothing
		char * inromname=SelectRom((gchar*)filenames->data, "Select Base File", false);
		if (!inromname) return;
		state.romext=GetExtension(inromname);
		if (!*state.romext) state.romext=".sfc";
		filemap* map=filemap::create(inromname);
		state.rommem=map->get();
		state.removeHeaders=shouldRemoveHeader(inromname, state.rommem.len);
		state.worsterror=e_none;
		state.anySuccess=false;
		g_slist_foreach(filenames, ApplyPatchMulti, &state);
		g_free(inromname);
		delete map;
		struct errorinfo errormessages[2][8]={
			{
				//no error-free
				{ el_ok, NULL },//e_none
				{ el_warning, NULL},//e_warning_notthis
				{ el_warning, NULL},//e_warning
				{ el_broken, "None of these are valid patches for this ROM!" },//e_invalid_this
				{ el_broken, "None of these are valid patches!" },//e_invalid
				{ el_broken, "Couldn't write any ROMs. Are you on a read-only medium?" },//e_io_write
				{ el_broken, "Couldn't read any patches. What exactly are you doing?" },//e_io_read
				{ el_broken, "Couldn't read the input ROM. What exactly are you doing?" },//e_io_read_rom
			},{
				//at least one error-free
				{ el_ok, "The patches were applied successfully!" },//e_none
				{ el_warning, "The patches were applied, but one or more is unlikely to be intended for this ROM..." },//e_warning_notthis
				{ el_warning, "The patches were applied, but one or more may be mangled or improperly created..." },//e_warning
				{ el_warning, "Some patches were applied, but not all of the given patches are valid for this ROM..." },//e_invalid_this
				{ el_warning, "Some patches were applied, but not all of the given patches are valid..." },//e_invalid
				{ el_warning, "Some patches were applied, but not all of the desired ROMs could be created..." },//e_io_write
				{ el_warning, "Some patches were applied, but not all of the given patches could be read..." },//e_io_read
				{ el_broken, NULL,//e_io_read_rom
			},
		}};
		ShowMessage(errormessages[state.anySuccess][state.worsterror]);
	}
	g_slist_free(filenames);
}

static void a_CreatePatch(GtkButton* widget, gpointer user_data)
{
	char * inrom=NULL;
	char * outrom=NULL;
	char * patchname=NULL;
	
	inrom=SelectRom(NULL, "Select ORIGINAL UNMODIFIED File to Use", false);
	if (!inrom) goto cleanup;
	outrom=SelectRom(inrom, "Select NEW MODIFIED File to Use", false);
	if (!outrom) goto cleanup;
	if (!strcmp(inrom, outrom))
	{
		ShowMessage((struct errorinfo){ el_broken, "That's the same file! You should really use two different files." });
		goto cleanup;
	}
	
	struct {
		const char * filter;
		const char * description;
	} static const typeinfo[]={
		{ "*.bps", "BPS Patch File" },
		{ "*.ips", "IPS Patch File" },
	};
	static const int numtypeinfo = sizeof(typeinfo)/sizeof(*typeinfo);
	
	int lasttype;
	lasttype = cfg.getint("lasttype", ty_bps);
	{
		char * defpatchname=g_strndup(outrom, strlen(outrom)+4+1);
		char * ext=GetExtension(defpatchname);
		strcpy(ext, typeinfo[lasttype-1].filter+1);
		
		GtkWidget* dialog=gtk_file_chooser_dialog_new("Select File to Save As", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE,
		                                              "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
		setoutpath(GTK_FILE_CHOOSER(dialog), defpatchname, true);
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), true);
		
		GtkFileFilter* filters[numtypeinfo];
		for (size_t i=0;i<numtypeinfo;i++)
		{
			GtkFileFilter* filter=gtk_file_filter_new();
			filters[i]=filter;
			gtk_file_filter_set_name(filter, typeinfo[i].description);
			gtk_file_filter_add_pattern(filter, typeinfo[i].filter);
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
		}
		
		gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filters[lasttype-1]);
		if (gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT)
		{
			patchname=gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
		}
		
		GtkFileFilter* filter=gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(dialog));
		for (int i=0;i<numtypeinfo;i++)
		{
			if (filter==filters[i])
			{
				if (patchname && lasttype!=i && !strcmp(GetExtension(patchname), typeinfo[lasttype-1].filter+1))
				{
					strcpy(GetExtension(patchname), typeinfo[i].filter+1);
				}
				cfg.setint("lasttype", i+1);
			}
		}
		
		gtk_widget_destroy(dialog);
	}
	if (!patchname) goto cleanup;
	
	bpsdCancel=false;
	struct errorinfo errinf;
	errinf=CreatePatch(inrom, outrom, (patchtype)lasttype, NULL, patchname);
	if (!bpsdCancel) ShowMessage(errinf);
	
cleanup:
	g_free(inrom);
	g_free(outrom);
	g_free(patchname);
}

static void a_SetEmulatorFor(GtkButton* widget, gpointer user_data);
static void a_ApplyRun(GtkButton* widget, gpointer user_data)
{
	gchar * patchname=(gchar*)user_data;
	if (!patchname)
	{
		GSList * patchnames=SelectPatches(false, true);
		if (!patchnames) return;
		patchname=(gchar*)patchnames->data;
		g_slist_free(patchnames);
	}
	
	file* patchfile = file::create(patchname);
	gchar * romname=NULL;
	{
	if (!patchfile)
	{
		ShowMessage((struct errorinfo){ el_broken, "Couldn't read input patch. What exactly are you doing?" });
		goto cleanup;
	}
	
	if (cfg.getint("autorom")) romname=g_strdup(FindRomForPatch(patchfile, NULL)); // g_strdup(NULL) is NULL
	if (!romname)
	{
		char* patch_uri = g_filename_to_uri(patchname, NULL, NULL);
		romname=SelectRom(patch_uri, "Select Base File", false);
		g_free(patch_uri);
	}
	if (!romname) goto cleanup;
	
	if (!GetEmuFor(romname)) a_SetEmulatorFor(NULL, romname);
	if (!GetEmuFor(romname)) goto cleanup;
	
	//gchar * outromname;
	//gint fd=g_file_open_tmp("flipsXXXXXX.smc", &outromname, NULL);
	
	gchar * outromname_rel=g_strndup(patchname, strlen(patchname)+4+1);
	strcpy(GetExtension(outromname_rel), GetExtension(romname));
	
	GFile* outrom_file=g_file_new_for_commandline_arg(outromname_rel);
	g_free(outromname_rel);
	gchar * outromname;
	if (g_file_is_native(outrom_file)) outromname=g_file_get_path(outrom_file);
	else outromname=g_file_get_uri(outrom_file);
	g_object_unref(outrom_file);
	
	struct errorinfo errinf=ApplyPatchMem(patchfile, romname, !cfg.getint("skipchecksum"), outromname, NULL, cfg.getint("autorom"));
	if (errinf.level!=el_ok) ShowMessage(errinf);
	if (errinf.level>=el_notthis) goto cleanup;
	
	gchar * patchend=GetBaseName(patchname);
	*patchend='\0';
	
	const gchar * argv[3];
	argv[0]=GetEmuFor(romname);
	argv[1]=outromname;
	argv[2]=NULL;
	
	GPid pid;
	GError* error=NULL;
	if (!g_spawn_async(*patchname ? patchname : NULL, (gchar**)argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, &pid, &error))
	{
		//g_unlink(tempname);//apparently this one isn't in the headers.
		ShowMessage((struct errorinfo){ el_broken, error->message });
		g_error_free(error);
	}
	else g_spawn_close_pid(pid);
	g_free(outromname);
	//close(fd);
	}
	
cleanup:
	delete patchfile;
	g_free(patchname);
	g_free(romname);
}

static void flatpakDisable(GtkWidget* widget)
{
	if (isFlatpak)
	{
		gtk_widget_set_sensitive(widget, false);
		gtk_widget_set_tooltip_text(widget, "Running in emulators is not available in Flatpak");
	}
}

static void a_SetEmulator(GtkButton* widget, gpointer user_data);
static void a_ShowSettings(GtkButton* widget, gpointer user_data)
{
	//used mnemonics:
	//E - Select Emulator
	//M - Create ROM
	//U - Run in Emulator
	//A - Enable automatic ROM selector
	
	GtkWidget* settingswindow=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(settingswindow), flipsversion);
	gtk_window_set_resizable(GTK_WINDOW(settingswindow), false);
	gtk_window_set_modal(GTK_WINDOW(settingswindow), true);
	gtk_window_set_transient_for(GTK_WINDOW(settingswindow), GTK_WINDOW(window));
	g_signal_connect(settingswindow, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	
	GtkGrid* grid=GTK_GRID(gtk_grid_new());
	gtk_grid_set_row_spacing(grid, 3);
	
	GtkWidget* button=gtk_button_new_with_mnemonic("Select _Emulator");
	g_signal_connect(button, "clicked", G_CALLBACK(a_SetEmulator), settingswindow);
	gtk_grid_attach(grid, button, 0,0, 1,1);
	flatpakDisable(button);
	
	GtkWidget* text=gtk_label_new("When opening through associations:");
	gtk_grid_attach(grid, text, 0,1, 1,1);
	
	GtkGrid* radioGrid=GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_homogeneous(radioGrid, true);
	GtkWidget* emuAssoc;
	emuAssoc=gtk_radio_button_new_with_mnemonic(NULL, "Create RO_M");
	gtk_grid_attach(radioGrid, emuAssoc, 0,0, 1,1);
	emuAssoc=gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(emuAssoc), "R_un in Emulator");
	gtk_grid_attach(radioGrid, emuAssoc, 1,0, 1,1);
	g_object_ref(emuAssoc);//otherwise it, and its value, gets eaten when I close the window, before I can save its value anywhere
	flatpakDisable(emuAssoc);
	if (cfg.getint("assocemu")) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(emuAssoc), true);
	gtk_grid_attach(grid, GTK_WIDGET(radioGrid), 0,2, 1,1);
	
	GtkWidget* autoRom;
	autoRom=gtk_check_button_new_with_mnemonic("Enable _automatic ROM selector");
	if (cfg.getint("autorom")) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoRom), true);
	g_object_ref(autoRom);
	gtk_grid_attach(grid, autoRom, 0,3, 1,1);
	flatpakDisable(autoRom);
	
	GtkWidget* skipChecksum;
	skipChecksum=gtk_check_button_new_with_mnemonic("Ignore checksum when patching");
	if (cfg.getint("skipchecksum")) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(skipChecksum), true);
	g_object_ref(skipChecksum);
	gtk_grid_attach(grid, skipChecksum, 0,4, 1,1);
	flatpakDisable(skipChecksum);
	
	gtk_container_add(GTK_CONTAINER(settingswindow), GTK_WIDGET(grid));
	
	gtk_widget_show_all(settingswindow);
	gtk_main();
	
	cfg.setint("assocemu", (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(emuAssoc))));
	cfg.setint("autorom", (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(autoRom))));
	cfg.setint("skipchecksum", (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(skipChecksum))));
	g_object_unref(emuAssoc);
	g_object_unref(autoRom);
	g_object_unref(skipChecksum);
}

static gboolean filterExecOnly(const GtkFileFilterInfo* filter_info, gpointer data)
{
	GFile* file=g_file_new_for_uri(filter_info->uri);
	GFileInfo* info=g_file_query_info(file, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	bool ret=g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);
	g_object_unref(file);
	g_object_unref(info);
	return ret;
}

static void a_SetEmulatorFor(GtkButton* widget, gpointer user_data)
{
	GtkWidget* dialog=gtk_file_chooser_dialog_new("Select Emulator to Use", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN,
		                                            "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), true);
	
	GtkFileFilter* filter=gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Executable files");
	gtk_file_filter_add_custom(filter, GTK_FILE_FILTER_URI, filterExecOnly, NULL, NULL);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	
	const char * emu_path = GetEmuFor((const char*)user_data);
	char* emu_uri = emu_path ? g_filename_to_uri(emu_path, NULL, NULL) : NULL;
	setoutpath(GTK_FILE_CHOOSER(dialog), emu_uri, false);
	g_free(emu_uri);
	
	if (gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT)
	{
		SetEmuFor((const char*)user_data, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));
	}
	
	gtk_widget_destroy(dialog);
}

static void SetEmuActivate(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data)
{
	GtkListStore* list = GTK_LIST_STORE(gtk_tree_view_get_model(tree_view));
	
	GtkTreeModel* model = gtk_tree_view_get_model(tree_view);
	GtkTreeIter iter;
	gtk_tree_model_get_iter(model, &iter, path);
	
	char* ext;
	gtk_tree_model_get(model, &iter, 0,&ext, -1);
	char* name = g_strdup_printf(".%s", ext);
	
	a_SetEmulatorFor(NULL, name);
	gtk_list_store_set(list, &iter, 1,GetEmuFor(name), -1);
	
	g_free(name);
	g_free(ext);
}

static void SetEmuDelete(GtkButton* widget, gpointer user_data)
{
	GtkTreeView* listview = GTK_TREE_VIEW(user_data);
	GList* list = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(listview), NULL);
	if (!list) return;
	
	GtkTreeModel* model = gtk_tree_view_get_model(listview);
	GtkTreeIter it;
	gtk_tree_model_get_iter(model, &it, (GtkTreePath*)list->data);
	
	char* ext;
	gtk_tree_model_get(model, &it, 0,&ext, -1);
	char* name = g_strdup_printf("emu.%s", ext);
	cfg.set(name, NULL);
	g_free(name);
	g_free(ext);
	
	gtk_list_store_remove(GTK_LIST_STORE(model), &it);
	g_list_free_full(list, (GDestroyNotify)gtk_tree_path_free);
}

static void a_SetEmulator(GtkButton* widget, gpointer user_data)
{
	GtkWidget* emuwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(emuwindow), flipsversion);
	gtk_window_set_modal(GTK_WINDOW(emuwindow), true);
	gtk_window_set_transient_for(GTK_WINDOW(emuwindow), GTK_WINDOW(user_data));
	gtk_window_set_default_size (GTK_WINDOW(emuwindow), 300, 200);
	g_signal_connect(emuwindow, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	
	GtkGrid* grid = GTK_GRID(gtk_grid_new());
	gtk_grid_set_row_spacing(grid, 3);
	
	GtkListStore* list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	for (size_t i=0;i<cfg.getcount();i++)
	{
		const char * name = cfg.getnamebyid(i);
		const char * value = cfg.getvaluebyid(i);
		
		if (strncmp(name, "emu.", strlen("emu.")) != 0) continue;
		if (value==NULL) continue;
		
		GtkTreeIter iter;
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0,name+strlen("emu."), 1,value, -1);
		//gtk_list_store_set(list, &iter, 0,name, 1,value, -1);
	}
	
	const char * columns[]={"Type", "Emulator"};
	
	//I just remembered why I hate this specific widget. It's so complex.
	GtkTreeView* listview = GTK_TREE_VIEW(gtk_tree_view_new());
	
	GtkCellRenderer* render=gtk_cell_renderer_text_new();
	for (int i=0;i<2;i++)
	{
		gtk_tree_view_insert_column_with_attributes(listview, -1, columns[i], render, "text", i, NULL);
		
		GtkTreeViewColumn* col=gtk_tree_view_get_column(listview, i);
		int width = gtk_tree_view_column_get_width(col);
		gtk_tree_view_column_set_fixed_width(col, width*2);
		if (i==1) gtk_tree_view_column_set_expand(col, true);
	}
	gtk_tree_view_set_model(listview, GTK_TREE_MODEL(list));
	g_signal_connect(listview, "row-activated", G_CALLBACK(SetEmuActivate), NULL);
	
	GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(listview));
	
	gtk_widget_set_hexpand(scroll, true);
	gtk_widget_set_vexpand(scroll, true);
	
	gtk_grid_attach(grid, scroll, 0,0, 1,1);
	
	GtkWidget* btnDelete = gtk_button_new_with_label("Delete");
	g_signal_connect(btnDelete, "clicked", G_CALLBACK(SetEmuDelete), listview);
	gtk_grid_attach(grid, btnDelete, 0,1, 1,1);
	
	gtk_container_add(GTK_CONTAINER(emuwindow), GTK_WIDGET(grid));
	
	gtk_widget_show_all(emuwindow);
	gtk_main();
	
	//TODO: pull values back from the ListStore
	//discard everything in the config, then add new values from the store
	//those stores are a bit easier to work with if I don't insist they interoperate with non-GTK things
}


//gchar * get_cfgpath()
//{
//	static gchar * cfgpath=NULL;
//	if (!cfgpath) cfgpath=g_build_filename(g_get_user_config_dir(), "flipscfg", NULL);
//	return cfgpath;
//}
//
//void GUILoadConfig()
//{
//	if (!canShowGUI) return;
//	
//	state.lastPatchType = atoi(cfg.get(TEXT("lasttype")));
//	state.createFromAllFiles = atoi(cfg.get(TEXT("allfiles")));
//	state.openInEmulatorOnAssoc = atoi(cfg.get(TEXT("assocemu")));
//	state.autoSelectRom = atoi(cfg.get(TEXT("autorom")));
//	
//	//const char * 
//	//state.autoSelectRom = atoi(cfg.get(TEXT("autorom")));
//	
//	struct mem cfgin = file::read(get_cfgpath());
//	if (cfgin.len>=10+1+1+1+1+4+4 && !memcmp(cfgin.ptr, "FlipscfgG", 9) && cfgin.ptr[9]==cfgversion)
//	{
//		state.lastPatchType=cfgin.ptr[10];
//		state.createFromAllFiles=cfgin.ptr[11];
//		state.openInEmulatorOnAssoc=cfgin.ptr[12];
//		state.autoSelectRom=cfgin.ptr[13];
//		int len=0;
//		len|=cfgin.ptr[14]<<24;
//		len|=cfgin.ptr[15]<<16;
//		len|=cfgin.ptr[16]<<8;
//		len|=cfgin.ptr[17]<<0;
//		if (len==0) state.emulator=NULL;
//		else
//		{
//			state.emulator=(gchar*)g_malloc(len+1);
//			memcpy(state.emulator, cfgin.ptr+22, len);
//			state.emulator[len]=0;
//		}
//		struct mem romlist={cfgin.ptr+22+len, 0};
//		romlist.len|=cfgin.ptr[18]<<24;
//		romlist.len|=cfgin.ptr[19]<<16;
//		romlist.len|=cfgin.ptr[20]<<8;
//		romlist.len|=cfgin.ptr[21]<<0;
//		SetRomList(romlist);
//	}
//	else
//	{
//		memset(&state, 0, sizeof(state));
//		state.lastPatchType=ty_bps;
//	}
//	free(cfgin.ptr);
//}

int GUIShow(const char * filename)
{
	if (!canShowGUI)
	{
		g_warning("couldn't parse command line arguments, what are you doing?");
		usage();
	}
	
	//copied a few lines from libgtk, I don't want to call gtk_init if I'm not going to poke the GUI
	GdkDisplay* display=gdk_display_open(gdk_get_display_arg_name());
	if (!display) display=gdk_display_get_default();
	if (!display)
	{
		g_warning("couldn't connect to display, fix it or use command line");
		usage();
	}
	gdk_display_manager_set_default_display(gdk_display_manager_get(), display);
	
	if (filename)
	{
		window=NULL;
		if (cfg.getint("assocemu")==false) a_ApplyPatch(NULL, g_strdup(filename));
		else a_ApplyRun(NULL, g_strdup(filename));
		return 0;
	}
	
	window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), flipsversion);
	gtk_window_set_resizable(GTK_WINDOW(window), false);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	
	GtkGrid* grid=GTK_GRID(gtk_grid_new());
	gtk_grid_set_row_homogeneous(grid, true);
	gtk_grid_set_column_homogeneous(grid, true);
	gtk_grid_set_row_spacing(grid, 5);
	gtk_grid_set_column_spacing(grid, 5);
	GtkWidget* button;
#define button(x, y, text, function) \
		button=gtk_button_new_with_mnemonic(text); \
		g_signal_connect(button, "clicked", function, NULL); \
		gtk_grid_attach(grid, button, x, y, 1, 1);
	button(0,0, "_Apply Patch", G_CALLBACK(a_ApplyPatch));
	button(1,0, "_Create Patch", G_CALLBACK(a_CreatePatch));
	button(0,1, "Apply and _Run", G_CALLBACK(a_ApplyRun));
	flatpakDisable(button);
	button(1,1, "_Settings", G_CALLBACK(a_ShowSettings));
#undef button
	
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(grid));
	
	gtk_widget_show_all(window);
	gtk_main();
	
	//int emulen=state.emulator?strlen(state.emulator):0;
	//struct mem romlist=GetRomList();
	//struct mem cfgout=(struct mem){ NULL, 10+1+1+1+1+4+4+emulen+romlist.len };
	//cfgout.ptr=(uint8_t*)g_malloc(cfgout.len);
	//memcpy(cfgout.ptr, "FlipscfgG", 9);
	//cfgout.ptr[9]=cfgversion;
	//cfgout.ptr[10]=state.lastPatchType;
	//cfgout.ptr[11]=state.createFromAllFiles;
	//cfgout.ptr[12]=state.openInEmulatorOnAssoc;
	//cfgout.ptr[13]=state.autoSelectRom;
	//cfgout.ptr[14]=emulen>>24;
	//cfgout.ptr[15]=emulen>>16;
	//cfgout.ptr[16]=emulen>>8;
	//cfgout.ptr[17]=emulen>>0;
	//cfgout.ptr[18]=romlist.len>>24;
	//cfgout.ptr[19]=romlist.len>>16;
	//cfgout.ptr[20]=romlist.len>>8;
	//cfgout.ptr[21]=romlist.len>>0;
	//memcpy(cfgout.ptr+22, state.emulator, emulen);
	//memcpy(cfgout.ptr+22+emulen, romlist.ptr, romlist.len);
	//filewrite::write(get_cfgpath(), cfgout);
	
	return 0;
}

int main(int argc, char * argv[])
{
	g_set_prgname("com.github.Alcaro.Flips");
	canShowGUI = gtk_parse_args(&argc, &argv);
	isFlatpak = (access("/.flatpak-info", F_OK) == 0);
	cfg.init_file(g_build_filename(g_get_user_config_dir(), "flipscfg", NULL));
	return flipsmain(argc, argv);
}
#endif
