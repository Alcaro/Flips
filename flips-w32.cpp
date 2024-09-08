//Module name: Floating IPS, Windows frontend
//Author: Alcaro
//Date: See Git history
//Licence: GPL v3.0 or higher

#include "flips.h"

#ifdef FLIPS_WINDOWS
class file_w32 : public file {
	size_t size;
	HANDLE io;
	
public:
	static file* create(LPCWSTR filename)
	{
		HANDLE io = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (io==INVALID_HANDLE_VALUE) return NULL;
		return new file_w32(io, (size_t)0);
	}
	
private:
	file_w32(HANDLE io, uint32_t sizetsize) : io(io)
	{
		size = GetFileSize(io, NULL);
	}
	
	file_w32(HANDLE io, uint64_t sizetsize) : io(io)
	{
		GetFileSizeEx(io, (PLARGE_INTEGER)&size);
	}
	
public:
	size_t len() { return size; }
	
	bool read(uint8_t* target, size_t start, size_t len)
	{
		OVERLAPPED ov = {0};
		ov.Offset = start;
		ov.OffsetHigh = start>>16>>16;
		DWORD actuallen;
		return (ReadFile(io, target, len, &actuallen, &ov) && len==actuallen);
	}
	
	~file_w32() { CloseHandle(io); }
};

file* file::create(LPCWSTR filename) { return file_w32::create(filename); }
bool file::exists(LPCWSTR filename) { return GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES; }


class filewrite_w32 : public filewrite {
	HANDLE io;
	
public:
	static filewrite* create(LPCWSTR filename)
	{
		HANDLE io = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (!io) return NULL;
		return new filewrite_w32(io);
	}
	
private:
	filewrite_w32(HANDLE io) : io(io) {}
	
public:
	bool append(const uint8_t* data, size_t len)
	{
		DWORD truelen;
		return (WriteFile(io, data, len, &truelen, NULL) && truelen==len);
	}
	
	~filewrite_w32() { CloseHandle(io); }
};

filewrite* filewrite::create(LPCWSTR filename) { return filewrite_w32::create(filename); }


//TODO: implement properly
//also ensure input==output works if implementing this, rather than getting a file sharing violation
//applies even when selecting multiple patches, of which one overwrites input
filemap* filemap::create(LPCWSTR filename) { return filemap::create_fallback(filename); }


HWND hwndMain=NULL;
HWND hwndSettings=NULL;

struct {
	char signature[9];
	unsigned char cfgversion;
	unsigned char lastRomType;
	bool openInEmulatorOnAssoc;
	bool enableAutoRomSelector;
	bool skipChecksum;
	enum patchtype lastPatchType;
	int windowleft;
	int windowtop;
} static state;
#define mycfgversion 2
WCHAR * st_emulator=NULL;
void set_st_emulator_len(LPCWSTR newemu, int len)
{
	free(st_emulator);
	st_emulator=(WCHAR*)malloc((len+1)*sizeof(WCHAR));
	if (newemu) memcpy(st_emulator, newemu, len*sizeof(WCHAR));
	st_emulator[len]='\0';
}
void set_st_emulator(LPCWSTR newemu)
{
	set_st_emulator_len(newemu, wcslen(newemu));
}


HWND hwndProgress;
LRESULT CALLBACK bpsdProgressWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

bool bpsdCancel;

void bpsdeltaBegin()
{
	bpsdCancel=false;
	RECT mainwndpos;
	GetWindowRect(hwndMain, &mainwndpos);
	hwndProgress=CreateWindowA(
						"floatingmunchers", flipsversion,
						WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_BORDER,
						mainwndpos.left+53, mainwndpos.top+27, 101, 39, hwndMain, NULL, GetModuleHandle(NULL), NULL);
	SetWindowLongPtrA(hwndProgress, GWLP_WNDPROC, (LONG_PTR)bpsdProgressWndProc);
	
	ShowWindow(hwndProgress, SW_SHOW);
	EnableWindow(hwndMain, FALSE);
	
	bpsdeltaProgress(NULL, 0, 1);
}

bool bpsdeltaProgress(void* userdata, size_t done, size_t total)
{
	if (!bpsdeltaGetProgress(done, total)) return !bpsdCancel;
	if (hwndProgress) InvalidateRect(hwndProgress, NULL, false);
	MSG Msg;
	while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) DispatchMessage(&Msg);
	return !bpsdCancel;
}

LRESULT CALLBACK bpsdProgressWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ERASEBKGND: return TRUE;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			RECT rc;
			BeginPaint(hwnd, &ps);
			GetClientRect(hwnd, &rc);
			FillRect(ps.hdc, &rc, GetSysColorBrush(COLOR_3DFACE));
			SetBkColor(ps.hdc, GetSysColor(COLOR_3DFACE));
			SelectObject(ps.hdc, (HFONT)GetStockObject(DEFAULT_GUI_FONT));
			DrawTextA(ps.hdc, bpsdProgStr, -1, &rc, DT_CENTER | DT_NOCLIP);
			EndPaint(hwnd, &ps);
		}
		break;
	case WM_CLOSE:
		bpsdCancel=true;
		break;
	default:
		return DefWindowProcA(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

void bpsdeltaEnd()
{
	EnableWindow(hwndMain, TRUE);
	DestroyWindow(hwndProgress);
	hwndProgress=NULL;
}


bool SelectRom(LPWSTR filename, LPCWSTR title, bool output)
{
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize=sizeof(ofn);
	ofn.hwndOwner=hwndMain;
	ofn.lpstrFilter=TEXT("Most Common ROM Files\0*.smc;*.sfc;*.nes;*.gb;*.gbc;*.gba;*.nds;*.vb;*.sms;*.smd;*.md;*.ngp;*.n64;*.z64\0All Files (*.*)\0*.*\0");
	ofn.lpstrFile=filename;
	ofn.nMaxFile=MAX_PATH;
	ofn.nFilterIndex=state.lastRomType;
	ofn.lpstrTitle=title;
	ofn.Flags=OFN_PATHMUSTEXIST|(output?OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
	ofn.lpstrDefExt=TEXT("smc");
	if (!output && !GetOpenFileName(&ofn)) return false;
	if ( output && !GetSaveFileName(&ofn)) return false;
	state.lastRomType=ofn.nFilterIndex;
	return true;
}

UINT mboxtype[]={ MB_OK, MB_OK, MB_OK|MB_ICONWARNING, MB_OK|MB_ICONWARNING, MB_OK|MB_ICONERROR, MB_OK|MB_ICONERROR };

LPCWSTR patchextensions[]={
	NULL,//unused, ty_null
	TEXT("bps"),
	TEXT("ips"),
};

static struct errorinfo error(errorlevel level, const char * text)
{
	struct errorinfo errinf = { level, text };
	return errinf;
}

int a_ApplyPatch(LPCWSTR clipatchname)
{
	WCHAR patchnames[65536];
	patchnames[0]='\0';
	bool multiplePatches;
	if (clipatchname)
	{
		multiplePatches=false;
		wcscpy(patchnames, clipatchname);
	}
	else
	{
		//get patch names
		OPENFILENAME ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize=sizeof(ofn);
		ofn.hwndOwner=hwndMain;
		ofn.lpstrFilter=TEXT("All supported patches (*.ips, *.bps)\0*.ips;*.bps;*.ups\0All files (*.*)\0*.*\0");
		ofn.lpstrFile=patchnames;
		ofn.nMaxFile=65535;
		ofn.lpstrTitle=TEXT("Select Patches to Use");
		ofn.Flags=OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_ALLOWMULTISELECT|OFN_EXPLORER;
		ofn.lpstrDefExt=patchextensions[state.lastPatchType];
		if (!GetOpenFileName(&ofn)) return 0;
		multiplePatches=(ofn.nFileOffset && patchnames[ofn.nFileOffset-1]=='\0');
	}
	
	//get rom name and apply
	
	if (!multiplePatches)
	{
		file* patch = file::create(patchnames);
		if (patch)
		{
		
		WCHAR inromname_buf[MAX_PATH];
		LPCWSTR inromname=NULL;
		if (state.enableAutoRomSelector) inromname=FindRomForPatch(patch, NULL);
		if (!inromname)
		{
			inromname=inromname_buf;
			inromname_buf[0]='\0';
			if (!SelectRom(inromname_buf, TEXT("Select File to Patch"), false)) goto cancel;
		}
		WCHAR outromname[MAX_PATH];
		wcscpy(outromname, inromname);
		LPWSTR outrompath=GetBaseName(outromname);
		LPWSTR patchpath=GetBaseName(patchnames);
		if (outrompath && patchpath)
		{
			wcscpy(outrompath, patchpath);
			LPWSTR outromext=GetExtension(outrompath);
			LPWSTR inromext=GetExtension(inromname);
			if (*inromext && *outromext) wcscpy(outromext, inromext);
		}
		if (!SelectRom(outromname, TEXT("Select Output File"), true)) goto cancel;
		struct errorinfo errinf=ApplyPatchMem(patch, inromname, !state.skipChecksum, outromname, NULL, state.enableAutoRomSelector);
		delete patch;
		MessageBoxA(hwndMain, errinf.description, flipsversion, mboxtype[errinf.level]);
		return errinf.level;
		}
	cancel:
		delete patch;
		return 0;
	}
	else
	{
#define max(a, b) (a > b ? a : b)
		if (state.enableAutoRomSelector)
		{
			LPCWSTR foundRom=NULL;
			bool canUseFoundRom=true;
			bool usingFoundRom=false;
			
		redo: ;
			WCHAR thisFileNameWithPath[MAX_PATH];
			bool anySuccess=false;
			enum { e_none, e_notice, e_warning, e_invalid, e_io_rom_write, e_io_rom_read, e_no_auto, e_io_read_patch } worsterror=e_none;
			LPCSTR messages[8]={
					"The patches were applied successfully!",//e_none
					"The patches were applied successfully!",//e_notice (ignore)
					"The patches were applied, but one or more may be mangled or improperly created...",//e_warning
					"Some patches were applied, but not all of the given patches are valid...",//e_invalid
					"Some patches were applied, but not all of the desired ROMs could be created...",//e_rom_io_write
					"Some patches were applied, but not all of the input ROMs could be read...",//e_io_rom_read
					"Some patches were applied, but not all of the required input ROMs could be located...",//e_no_auto
					"Some patches were applied, but not all of the given patches could be read...",//e_io_read_patch
				};
			
			wcscpy(thisFileNameWithPath, patchnames);
			LPWSTR thisFileName=wcschr(thisFileNameWithPath, '\0');
			*thisFileName='\\';
			thisFileName++;
			
			LPWSTR thisPatchName=wcschr(patchnames, '\0')+1;
			while (*thisPatchName)
			{
				wcscpy(thisFileName, thisPatchName);
				file* patch = file::create(thisFileNameWithPath);
				{
				if (!patch)
				{
					worsterror=max(worsterror, e_io_read_patch);
					canUseFoundRom=false;
					goto multi_auto_next;
				}
				bool possible;
				LPCWSTR romname=FindRomForPatch(patch, &possible);
				if (usingFoundRom)
				{
					if (!romname) romname=foundRom;
					else goto multi_auto_next;
				}
				else
				{
					if (!romname)
					{
						if (possible) canUseFoundRom=false;
						worsterror=max(worsterror, e_no_auto);
						goto multi_auto_next;
					}
				}
				if (!foundRom) foundRom=romname;
				if (foundRom!=romname) canUseFoundRom=false;
				
				wcscpy(GetExtension(thisFileName), GetExtension(romname));
				struct errorinfo errinf=ApplyPatchMem(patch, romname, !state.skipChecksum, thisFileNameWithPath, NULL, true);
				
				if (errinf.level==el_broken) worsterror=max(worsterror, e_invalid);
				if (errinf.level==el_notthis) worsterror=max(worsterror, e_no_auto);
				if (errinf.level==el_warning) worsterror=max(worsterror, e_warning);
				if (errinf.level<el_notthis) anySuccess=true;
				else canUseFoundRom=false;
				}
			multi_auto_next:
				delete patch;
				thisPatchName=wcschr(thisPatchName, '\0')+1;
			}
			if (anySuccess)
			{
				if (worsterror==e_no_auto && foundRom && canUseFoundRom && !usingFoundRom)
				{
					usingFoundRom=true;
					goto redo;
				}
				int severity=(worsterror==e_none ? el_ok : el_warning);
				MessageBoxA(hwndMain, messages[worsterror], flipsversion, mboxtype[severity]);
				return severity;
			}
		}
		WCHAR inromname[MAX_PATH];
		inromname[0]='\0';
		if (!SelectRom(inromname, TEXT("Select Base File"), false)) return 0;
		WCHAR thisFileNameWithPath[MAX_PATH];
		wcscpy(thisFileNameWithPath, patchnames);
		LPWSTR thisFileName=wcschr(thisFileNameWithPath, '\0');
		*thisFileName='\\';
		thisFileName++;
		LPWSTR thisPatchName=wcschr(patchnames, '\0')+1;
		LPCWSTR romExtension=GetExtension(inromname);
		filemap* inrommap=filemap::create(inromname);
		struct mem inrom=inrommap->get();
		bool anySuccess=false;
		enum { e_none, e_notice, e_warning, e_invalid_this, e_invalid, e_io_write, e_io_read, e_io_read_rom } worsterror=e_none;
		enum errorlevel severity[2][8]={
				 { el_ok,  el_ok,  el_warning,el_broken,      el_broken,  el_broken, el_broken, el_broken },
				 { el_ok,  el_ok,  el_warning,el_warning,     el_warning, el_warning,el_warning,el_broken },
			};
		LPCSTR messages[2][8]={
				{
					//no error-free
					NULL,//e_none
					NULL,//e_notice
					NULL,//e_warning
					"None of these are valid patches for this ROM!",//e_invalid_this
					"None of these are valid patches!",//e_invalid
					"Couldn't write any ROMs. Are you on a read-only medium?",//e_io_write
					"Couldn't read any patches. What exactly are you doing?",//e_io_read
					"Couldn't read the input ROM. What exactly are you doing?",//e_io_read_rom
				},{
					//at least one error-free
					"The patches were applied successfully!",//e_none
					"The patches were applied successfully!",//e_notice
					"The patches were applied, but one or more may be mangled or improperly created...",//e_warning
					"Some patches were applied, but not all of the given patches are valid for this ROM...",//e_invalid_this
					"Some patches were applied, but not all of the given patches are valid...",//e_invalid
					"Some patches were applied, but not all of the desired ROMs could be created...",//e_io_write
					"Some patches were applied, but not all of the given patches could be read...",//e_io_read
					NULL,//e_io_read_rom
				},
			};
		if (inrom.ptr)
		{
			bool removeheaders=shouldRemoveHeader(inromname, inrom.len);
			while (*thisPatchName)
			{
				wcscpy(thisFileName, thisPatchName);
				file* patch = file::create(thisFileNameWithPath);
				if (patch)
				{
					LPWSTR patchExtension=GetExtension(thisFileName);
					wcscpy(patchExtension, romExtension);
					struct errorinfo errinf=ApplyPatchMem2(patch, inrom, removeheaders, !state.skipChecksum, thisFileNameWithPath, NULL);
					
					if (errinf.level==el_broken) worsterror=max(worsterror, e_invalid);
					if (errinf.level==el_notthis) worsterror=max(worsterror, e_invalid_this);
					if (errinf.level==el_warning) worsterror=max(worsterror, e_warning);
					if (errinf.level<el_notthis)
					{
						if (state.enableAutoRomSelector && !anySuccess) AddToRomList(patch, inromname);
						anySuccess=true;
					}
					delete patch;
				}
				else worsterror=max(worsterror, e_io_read);
				thisPatchName=wcschr(thisPatchName, '\0')+1;
			}
		}
		else worsterror=e_io_read_rom;
		delete inrommap;
		MessageBoxA(hwndMain, messages[anySuccess][worsterror], flipsversion, mboxtype[severity[anySuccess][worsterror]]);
		return severity[anySuccess][worsterror];
#undef max
	}
}

void a_CreatePatch()
{
	//pick roms
	WCHAR romnames[2][MAX_PATH];
	WCHAR patchname[MAX_PATH];
	
	romnames[0][0]='\0';
	romnames[1][0]='\0';
	if (!SelectRom(romnames[0], TEXT("Select ORIGINAL UNMODIFIED File to Use"), false)) return;
	if (!SelectRom(romnames[1], TEXT("Select NEW MODIFIED File to Use"), false)) return;
	
	if (!wcsicmp(romnames[0], romnames[1]))
	{
		MessageBoxA(hwndMain, "That's the same file! You should really use two different files.", flipsversion, mboxtype[el_broken]);
		return;
	}
	
	//pick patch name and type
	wcscpy(patchname, romnames[1]);
	LPWSTR extension=GetExtension(patchname);
	if (extension) *extension='\0';//wcscpy(extension+1, extensions[state.lastPatchType]);
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize=sizeof(ofn);
	ofn.hwndOwner=hwndMain;
	ofn.lpstrFilter =
		TEXT("BPS Patch File (*.bps)\0*.bps\0")
		//TEXT("BPS Patch File (Favor Creation Speed) (*.bps)\0*.bps\0")
		TEXT("IPS Patch File (*.ips)\0*.ips\0");
	ofn.lpstrFile=patchname;
	ofn.nMaxFile=MAX_PATH;
	ofn.nFilterIndex=state.lastPatchType;
	ofn.lpstrTitle=TEXT("Select File to Save As");
	ofn.Flags=OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt=patchextensions[state.lastPatchType];
	if (!GetSaveFileName(&ofn))
	{
		state.lastPatchType=(enum patchtype)ofn.nFilterIndex;
		return;
	}
	state.lastPatchType=(enum patchtype)ofn.nFilterIndex;
	
	bpsdCancel=false;
	struct errorinfo errinf=CreatePatch(romnames[0], romnames[1], (enum patchtype)ofn.nFilterIndex, NULL, patchname);
	if (!bpsdCancel) MessageBoxA(hwndMain, errinf.description, flipsversion, mboxtype[errinf.level]);
}

bool a_SetEmulator()
{
	WCHAR newemupath[MAX_PATH];
	*newemupath='\0';
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize=sizeof(ofn);
	ofn.hwndOwner=hwndMain;
	ofn.lpstrFilter=TEXT("Emulator Files (*.exe)\0*.exe\0All files (*.*)\0*.*\0");
	ofn.lpstrFile=newemupath;
	ofn.nMaxFile=MAX_PATH;
	ofn.lpstrTitle=TEXT("Select Emulator to Use");
	ofn.Flags=OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt=TEXT("exe");
	if (!GetOpenFileName(&ofn)) return false;
	set_st_emulator(newemupath);
	return true;
}

int a_ApplyRun(LPCWSTR clipatchname)
{
	struct mem rommem={NULL,0};
	struct mem patchedmem={NULL,0};
	
	WCHAR patchpath[MAX_PATH];
	*patchpath='\0';
	if (clipatchname)
	{
		wcscpy(patchpath, clipatchname);
	}
	else
	{
		OPENFILENAME ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize=sizeof(ofn);
		ofn.hwndOwner=hwndMain;
		ofn.lpstrFilter=TEXT("All supported patches (*.bps, *.ips)\0*.bps;*.ips;*.ups\0All files (*.*)\0*.*\0");
		ofn.lpstrFile=patchpath;
		ofn.nMaxFile=MAX_PATH;
		ofn.lpstrTitle=TEXT("Select Patch to Use");
		ofn.Flags=OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt=TEXT("bps");
		if (!GetOpenFileName(&ofn)) return 0;
	}
	
	struct errorinfo errinf;
	file* patch = file::create(patchpath);
	if (!patch)
	{
		errinf=error(el_broken, "Couldn't read input patch. What exactly are you doing?");
		goto error;
	}
	
	LPCWSTR romname;
	romname=NULL;
	if (state.enableAutoRomSelector) romname=FindRomForPatch(patch, NULL);
	WCHAR romname_base[MAX_PATH];
	if (!romname)
	{
		romname_base[0]='\0';
		if (!SelectRom(romname_base, TEXT("Select Base File"), false))
		{
			delete patch;
			return 0;
		}
		romname=romname_base;
	}
	
	if (!*st_emulator && !a_SetEmulator())
	{
		delete patch;
		return 0;
	}
	
	//WCHAR tempfilepath[MAX_PATH];
	//WCHAR tempfilename[MAX_PATH];
	//if (!GetTempPath(MAX_PATH, tempfilepath)) wcscpy(tempfilepath, TEXT("."));
	//if (!GetTempFileName(tempfilepath, TEXT("rom"), 0, tempfilename)) wcscpy(tempfilename, TEXT("temprom.tmp"));
	
	WCHAR outfilename_rel[MAX_PATH];
	wcscpy(outfilename_rel, patchpath);
	wcscpy(GetExtension(outfilename_rel), GetExtension(romname));
	WCHAR outfilename[MAX_PATH];
	GetFullPathName(outfilename_rel, MAX_PATH, outfilename, NULL);
	
	errinf=ApplyPatchMem(patch, romname, !state.skipChecksum, outfilename, NULL, state.enableAutoRomSelector);
error:
	
	if (errinf.level!=el_ok) MessageBoxA(hwndMain, errinf.description, flipsversion, mboxtype[errinf.level]);
	if (errinf.level>=el_notthis) return el_broken;
	
	delete patch;
	if (rommem.ptr) FreeFileMemory(rommem);
	if (patchedmem.ptr) free(patchedmem.ptr);
	
	WCHAR cmdline[1+MAX_PATH+3+MAX_PATH+1+1];
	swprintf(cmdline, 1+MAX_PATH+3+MAX_PATH+1+1, TEXT("\"%ls\" \"%ls\""), st_emulator, outfilename);
	WCHAR * dirend=GetBaseName(patchpath);
	if (dirend) *dirend='\0';
	STARTUPINFO startupinfo;
	ZeroMemory(&startupinfo, sizeof(STARTUPINFO));
	PROCESS_INFORMATION processinformation;
	if (!CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, patchpath, &startupinfo, &processinformation))
	{
		MessageBoxA(hwndMain, "Couldn't open emulator.", flipsversion, mboxtype[el_broken]);
		//DeleteFile(tempfilename);
		return el_broken;
	}
	
	//I don't clean up the temp file when the emulator is done.
	//- It would just force me to keep track of a bunch of state.
	//- It'd force me to not exit when the window is closed.
	//- The bsnes profile selector would confuse it.
	//- The emulator may have created a bunch of other files, for example SRAM.
	//Few other apps clean up anyways.
	CloseHandle(processinformation.hProcess);
	CloseHandle(processinformation.hThread);
	return errinf.level;
}

void a_AssignFileTypes(bool checkonly);

HWND assocText;
HWND assocButton;
void a_ShowSettings()
{
	if (hwndSettings)
	{
		SetActiveWindow(hwndSettings);
		return;
	}
	
	hwndSettings=CreateWindowA(
		"floatingmunchers", flipsversion,
		WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_BORDER|WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 3+6+202+6+3, 21 + 6+23+6+23+3+13+1+17+4+17+6 + 3+17+6 + 3, NULL, NULL, GetModuleHandle(NULL), NULL);
	
	HFONT hfont=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
	HWND item;
	
	int x=6;
	int y=6;
	int lineheight;
	
#define endline(padding) do { x=6; y+=lineheight+padding; } while(0)
#define line(height) lineheight=height
	
#define widget(type, style, text, w, h, action) \
	do { \
		int thisy=y+(lineheight-h)/2; \
		item=CreateWindowA(type, text, WS_CHILD|WS_TABSTOP|WS_VISIBLE|style, \
		                   x, thisy, w, h, hwndSettings, (HMENU)(action), GetModuleHandle(NULL), NULL); \
		SendMessage(item, WM_SETFONT, (WPARAM)hfont, 0); \
		x+=w+6; \
	} while(0)
	
#define firstbutton(text, w, h, action) \
	widget(WC_BUTTONA, WS_GROUP|BS_DEFPUSHBUTTON, text, w, h, action)
	
#define button(text, w, h, action) \
	widget(WC_BUTTONA, BS_PUSHBUTTON, text, w, h, action)
	
#define labelL(text, w, h, action) \
	widget(WC_STATICA, SS_LEFT, text, w, h, action)
#define labelC(text, w, h, action) \
	widget(WC_STATICA, SS_CENTER, text, w, h, action)
	
#define radio(text, w, h, action) \
	widget(WC_BUTTONA, BS_AUTORADIOBUTTON, text, w, h, action)
#define check(text, w, h, action) \
	widget(WC_BUTTONA, BS_AUTOCHECKBOX, text, w, h, action)
	
	line(23);
	firstbutton("Select emulator", 202/*94*/, 23, 101);
	endline(6);
	
	line(23);
	button("Assign file types", 98, 23, 102); assocButton=item;
	labelL("(can not be undone)", 98, 13, 0); assocText=item;
	endline(3);
	
	line(13);
	labelC("When opening through associations:", 175, 13, 0);
	endline(1);
	line(17);
	radio("Create ROM", 79, 17, 103); Button_SetCheck(item, (state.openInEmulatorOnAssoc==false));
	radio("Run in emulator", 95, 17, 104); Button_SetCheck(item, (state.openInEmulatorOnAssoc==true));
	endline(4);
	
	line(17);
	check("Enable automatic ROM selector", 202, 17, 105); Button_SetCheck(item, (state.enableAutoRomSelector));
	endline(3);
	
	line(17);
	check("Ignore checksum when patching", 202, 17, 106); Button_SetCheck(item, (state.skipChecksum));
	endline(3);
	
	ShowWindow(hwndSettings, SW_SHOW);
#undef firstbutton
#undef button
#undef label
#undef radio
	//if (!fileTypesAssigned) button(6,68,200,23, "Assign File Types");
}

void key_core(bool checkonly, LPCWSTR path, LPCWSTR value, bool * p_hasExts, bool * p_refresh)
{
	HKEY hkey;
	DWORD type;
	WCHAR truepath[60];
	wcscpy(truepath, TEXT("Software\\Classes\\"));
	wcscat(truepath, path);
	WCHAR regval[MAX_PATH+30];
	DWORD regvallen;
	bool hasExts=true;
	if (checkonly)
	{
		regvallen=sizeof(regval);
		if (RegOpenKeyEx(HKEY_CURRENT_USER, truepath, 0, KEY_READ, &hkey)!=ERROR_SUCCESS) goto add;
		if (value && RegQueryValueEx(hkey, NULL, NULL, &type, (LPBYTE)regval, &regvallen)!=ERROR_SUCCESS) hasExts=false;
		RegCloseKey(hkey);
		if (!hasExts) goto add;
		if (value && wcsncmp(regval, value, sizeof(regval)/sizeof(*regval))) *p_hasExts=false;
		return;
	}
	else
	{
	add:
		regvallen=sizeof(regval);
		if (RegCreateKeyExW(HKEY_CURRENT_USER, truepath, 0, NULL, 0, KEY_WRITE, NULL, &hkey, NULL)==ERROR_SUCCESS)
		{
			if (value) RegSetValueExW(hkey, NULL, 0, REG_SZ, (BYTE*)value, (wcslen(value)+1)*sizeof(WCHAR));
			RegCloseKey(hkey);
		}
		if (path[0]=='.') *p_refresh=true;
		return;
	}
}

void a_AssignFileTypes(bool checkonly)
{
	WCHAR outstring[MAX_PATH+30];
	outstring[0]='"';
	GetModuleFileNameW(NULL, outstring+1, MAX_PATH);
	LPWSTR outstringend=wcschr(outstring, '\0');
	*outstringend='"';
	outstringend++;
	
	bool hasExts=true;
	bool refresh=false;
#define key(path, value) \
			key_core(checkonly, TEXT(path), TEXT(value), &hasExts, &refresh)
#define key_path(path, value) \
			wcscpy(outstringend, TEXT(value)); key_core(checkonly, TEXT(path), outstring, &hasExts, &refresh)
#define key_touch(path) \
			key_core(checkonly, TEXT(path), NULL, &hasExts, &refresh)
	
	key(".ips", "FloatingIPSFileIPS");
	key("FloatingIPSFileIPS", "Floating IPS File");
	key_path("FloatingIPSFileIPS\\DefaultIcon", ",1");
	key_touch("FloatingIPSFileIPS\\shell");
	key_touch("FloatingIPSFileIPS\\shell\\open");
	key_path("FloatingIPSFileIPS\\shell\\open\\command", " \"%1\"");
	
	key(".bps", "FloatingIPSFileBPS");
	key("FloatingIPSFileBPS", "Floating IPS File");
	key_path("FloatingIPSFileBPS\\DefaultIcon", ",2");
	key_touch("FloatingIPSFileBPS\\shell");
	key_touch("FloatingIPSFileBPS\\shell\\open");
	key_path("FloatingIPSFileBPS\\shell\\open\\command", " \"%1\"");
	
	if (refresh)
	{
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
		
		if (hwndMain)
		{
			RECT wndpos;
			GetWindowRect(hwndMain, &wndpos);
			MoveWindow(hwndMain, wndpos.left, wndpos.top, 218, 93, true);
		}
	}
	if (!checkonly || hasExts)
	{
		SetWindowText(assocText, TEXT("(already done)"));
		Button_Enable(assocButton, false);
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		{
			if (wParam==1) a_ApplyPatch(NULL);
			if (wParam==2) a_CreatePatch();
			if (wParam==3) a_ApplyRun(NULL);
			if (wParam==4) a_ShowSettings();
			
			if (wParam==101) a_SetEmulator();
			if (wParam==102) a_AssignFileTypes(false);
			if (wParam==103) state.openInEmulatorOnAssoc=false;
			if (wParam==104) state.openInEmulatorOnAssoc=true;
			if (wParam==105) state.enableAutoRomSelector^=1;
			if (wParam==106) state.skipChecksum^=1;
		}
		break;
	case WM_CLOSE:
		{
			if (hwnd==hwndMain && !IsIconic(hwnd))
			{
				RECT wndpos;
				GetWindowRect(hwnd, &wndpos);
				state.windowleft=wndpos.left;
				state.windowtop=wndpos.top;
			}
			DestroyWindow(hwnd);
		}
		break;
	case WM_DESTROY:
		{
			if (hwnd==hwndMain) PostQuitMessage(0);
			if (hwnd==hwndSettings) hwndSettings=NULL;
			break;
		}
	default:
		return DefWindowProcA(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

static HFONT try_create_font(const char * name, int size)
{
	return CreateFontA(-size*96/72, 0, 0, 0, FW_NORMAL,
	                   FALSE, FALSE, FALSE, DEFAULT_CHARSET,
	                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE,
	                   name);
}

int ShowMainWindow(HINSTANCE hInstance, int nCmdShow)
{
	WNDCLASSA wc;
	wc.style=0;
	wc.lpfnWndProc=WindowProc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=GetModuleHandle(NULL);
	wc.hIcon=LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(0));
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=GetSysColorBrush(COLOR_3DFACE);//(HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName=NULL;
	wc.lpszClassName="floatingmunchers";
	RegisterClassA(&wc);
	
	MSG msg;
	hwndMain=CreateWindowA(
				"floatingmunchers", flipsversion,
				WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_BORDER|WS_MINIMIZEBOX,
				state.windowleft, state.windowtop, 204, 93, NULL, NULL, GetModuleHandle(NULL), NULL);
	
	HFONT hfont=try_create_font("Segoe UI", 9);
	if (!hfont) hfont=try_create_font("MS Shell Dlg 2", 8);
	if (!hfont) hfont=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
	
	int buttonid=0;
	HWND lastbutton;
#define button(x,y,w,h, text) \
		do { \
			lastbutton=CreateWindowA("BUTTON", text, WS_CHILD|WS_TABSTOP|WS_VISIBLE|(buttonid==0?(BS_DEFPUSHBUTTON|WS_GROUP):(BS_PUSHBUTTON)), \
											x, y, w, h, hwndMain, (HMENU)(uintptr_t)(buttonid+1), GetModuleHandle(NULL), NULL); \
			SendMessage(lastbutton, WM_SETFONT, (WPARAM)hfont, 0); \
			buttonid++; \
		} while(0)
	button(6,  6,  90/*77*/,23, "Apply Patch"); SetActiveWindow(lastbutton);
	button(104,6,  90/*83*/,23, "Create Patch");
	button(6,  37, 90/*90*/,23, "Apply and Run");
	button(104,37, 90/*59*/,23, "Settings");
	
	ShowWindow(hwndMain, nCmdShow);
	
	a_AssignFileTypes(true);
	
	while (GetMessageA(&msg, NULL, 0, 0)>0)
	{
		if (!IsDialogMessageA(hwndMain, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}
	
	return msg.wParam;
}

void GUIClaimConsole()
{
	//this one makes it act like a console app in all cases except it doesn't create a new console if
	// not launched from one (it'd swiftly go away on app exit anyways), and it doesn't like being
	// launched from cmd since cmd wants to run a new command if spawning a gui app (I can't make it
	// not be a gui app because that flashes a console; it acts sanely from batch files)
	
	bool claimstdin=(GetFileType(GetStdHandle(STD_INPUT_HANDLE))==FILE_TYPE_UNKNOWN);
	bool claimstdout=(GetFileType(GetStdHandle(STD_OUTPUT_HANDLE))==FILE_TYPE_UNKNOWN);
	bool claimstderr=(GetFileType(GetStdHandle(STD_ERROR_HANDLE))==FILE_TYPE_UNKNOWN);
	
	if (claimstdin || claimstdout || claimstderr) AttachConsole(ATTACH_PARENT_PROCESS);
	
	if (claimstdin) freopen("CONIN$", "rt", stdin);
	if (claimstdout) freopen("CONOUT$", "wt", stdout);
	if (claimstderr) freopen("CONOUT$", "wt", stderr);
	
	if (claimstdout) fputc('\r', stdout);
	if (claimstderr) fputc('\r', stderr);
}

HINSTANCE hInstance_;
int nCmdShow_;

WCHAR * get_cfgpath()
{
	static WCHAR cfgfname[MAX_PATH+8];
	GetModuleFileNameW(NULL, cfgfname, MAX_PATH);
	WCHAR * ext=GetExtension(cfgfname);
	if (ext) *ext='\0';
	wcscat(cfgfname, TEXT("cfg.bin"));
	return cfgfname;
}

void GUILoadConfig()
{
	memset(&state, 0, sizeof(state));
	struct mem configbin=ReadWholeFile(get_cfgpath());
	void* configbin_org=configbin.ptr;
	if (configbin.len >= sizeof(state))
	{
#define readconfig(target, size) \
	if (size<0 || configbin.len < size) goto badconfig; \
	memcpy(target, configbin.ptr, size); \
	configbin.ptr += size; \
	configbin.len -= size
		
		readconfig(&state, sizeof(state));
		if (memcmp(state.signature, "FlipscfgW", sizeof(state.signature))!=0 || state.cfgversion!=mycfgversion) goto badconfig;
		int emulen;
		readconfig(&emulen, sizeof(emulen));
		set_st_emulator_len(NULL, emulen);
		readconfig(st_emulator, (unsigned)emulen*sizeof(WCHAR));
		SetRomList(configbin);
	}
	else
	{
	badconfig:
		memcpy(state.signature, "FlipscfgW", sizeof(state.signature));
		state.cfgversion=mycfgversion;
		state.lastRomType=0;
		state.openInEmulatorOnAssoc=false;
		state.enableAutoRomSelector=false;
		state.skipChecksum=false;
		state.lastPatchType=ty_bps;
		state.windowleft=CW_USEDEFAULT;
		state.windowtop=CW_USEDEFAULT;
		set_st_emulator(TEXT(""));
	}
	free(configbin_org);
}

int GUIShow(LPCWSTR filename)
{
	GUILoadConfig();
	
	INITCOMMONCONTROLSEX initctrls;
	initctrls.dwSize=sizeof(initctrls);
	initctrls.dwICC=ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&initctrls);
	
	int ret;
	if (filename)
	{
		if (state.openInEmulatorOnAssoc==false) ret=a_ApplyPatch(filename);
		else ret=a_ApplyRun(filename);
	}
	else ret=ShowMainWindow(hInstance_, nCmdShow_);
	
	HANDLE file=CreateFile(get_cfgpath(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
	if (file!=INVALID_HANDLE_VALUE)
	{
		DWORD whocares;
		WriteFile(file, &state, sizeof(state), &whocares, NULL);
		int len=wcslen(st_emulator);
		WriteFile(file, &len, sizeof(len), &whocares, NULL);
		WriteFile(file, st_emulator, sizeof(WCHAR)*wcslen(st_emulator), &whocares, NULL);
		struct mem romlist=GetRomList();
		WriteFile(file, romlist.ptr, romlist.len, &whocares, NULL);
		CloseHandle(file);
	}
	
	return ret;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	hInstance_=hInstance;
	nCmdShow_=nCmdShow;
	int argc;
	wchar_t ** argv=CommandLineToArgvW(GetCommandLineW(), &argc);
	return flipsmain(argc, argv);
}
#endif
