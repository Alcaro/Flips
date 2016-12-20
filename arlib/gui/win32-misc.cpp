#include "window.h"
#include "../file.h"
#include "../os.h"
#ifdef ARGUI_WINDOWS
#undef bind
#include <windows.h>
#include <commdlg.h>
#define bind bind_func
#ifdef ARLIB_WUTF
#include "../wutf/wutf.h"
#endif

//Number of ugly hacks: 5
//If a status bar item is right-aligned, a space is appended.
//The status bar is created with WS_DISABLED.
//WM_SYSCOMMAND is sometimes ignored.
//I have to keep track of the mouse position so I can ignore various bogus instances of WM_MOUSEMOVE.
//I have to undefine 'bind' before including any Windows header. I suspect something is including winsock.

//Incompatibility levels:
//Level 0 - a feature works as intended
//Level 1 - a feature is usable, but behaves weirdly
//Level 2 - attempting to use a feature throws an error box, or reports failure in a way the program can and does handle
//Level 3 - attempting to use a feature reports success internally, but nothing happens
//Level 4 - attempting to use a feature crashes the program
//Level 5 - program won't start
//Maximum allowed incompatibility level:
//XP SP2 and older: 5
//XP SP3:
//  1 after December 8, 2013
//  2 after April 8, 2014
//  3 after August 8, 2014
//  4 after December 8, 2014
//  5 after April 8, 2015
//Vista SP0 and higher: 0
//List:
//Level 0: SetDllDirectory demands XP SP1 or higher. (But anything below SP3 is, for all intents and purposes, dead.)
//Level 1: LVCFMT_FIXED_WIDTH on the listbox is ignored before Vista
//Danger list (likely to hit):
//Level 4: printf dislikes z (size_t) size specifiers; they must be behind #ifdef DEBUG, or turned into "I" via #define
// NOTE: This is present on Vista too. z requires 7 or higher.
//Level 5: 64-bit programs dislike XP (there are 32bit Vista/7/8, but Vista is practically dead, as is 32bit 7+)
//Level 5: SRWLOCK is Vista+.

//static LARGE_INTEGER timer_freq;

void window_init(int * argc, char * * argv[])
{
#ifdef ARLIB_WUTF
	WuTF_enable_args(argc, argv);
#endif
	
	for (unsigned int i=0;(*argv)[0][i];i++)
	{
		if ((*argv)[0][i]=='\\') (*argv)[0][i]='/';
	}
	
	_window_init_file();
	_window_init_shell();
	_window_init_inner();
	
	//QueryPerformanceFrequency(&timer_freq);
}

#if 0
file* file::create(const char * filename)
{
	//sorry Windows, no fancy features for you, you suck
	return create_fs(filename);
}

bool window_message_box(const char * text, const char * title, enum mbox_sev severity, enum mbox_btns buttons)
{
	UINT sev[3]={ 0, MB_ICONWARNING, MB_ICONERROR };
	UINT btns[3]={ 0, MB_OKCANCEL, MB_YESNO };
	int ret=MessageBox(NULL, text, title, sev[severity]|btns[buttons]|MB_TASKMODAL);
	return (ret==IDOK || ret==IDYES);
}

const char * const * window_file_picker(window * parent,
                                        const char * title,
                                        const char * const * extensions,
                                        const char * extdescription,
                                        bool dylib,
                                        bool multiple)
{
	//there is no reasonable way to use the dylib flag; windows has nothing gvfs-like (okay, maybe IShellItem, but I can't get that from GetOpenFileName).
	static char * * ret=NULL;
	if (ret)
	{
		char * * del=ret;
		while (*del)
		{
			free(*del);
			del++;
		}
		free(ret);
		ret=NULL;
	}
	
	unsigned int filterlen=strlen(extdescription)+1+0+1-1+strlen("All files")+1+strlen("*.*")+1+1;
	for (unsigned int i=0;extensions[i];i++) filterlen+=2+strlen(extensions[i])+1;
	char * filter=malloc(filterlen);
	
	char * filterat=filter;
	strcpy(filterat, extdescription);
	filterat+=strlen(extdescription)+1;
	for (unsigned int i=0;extensions[i];i++)
	{
		unsigned int thislen=strlen(extensions[i]);
		filterat[0]='*';
		filterat[1]='.';
		if (*extensions[i]=='.') filterat--;
		memcpy(filterat+2, extensions[i], thislen);
		filterat[2+thislen]=';';
		filterat+=2+thislen+1;
	}
	memcpy(filterat-1, "\0All files\0*.*\0", 1+strlen("All files")+1+strlen("*.*")+1+1);
	
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize=sizeof(ofn);
	ofn.hwndOwner=(parent?(HWND)parent->_get_handle():NULL);
	ofn.lpstrFilter=(extensions[0] ? filter : "All files (*.*)\0*.*\0");
	char * filenames=malloc(65536);
	*filenames='\0';
	ofn.lpstrFile=filenames;
	ofn.nMaxFile=65536;
	ofn.lpstrTitle=title;
	ofn.Flags=OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER|(multiple?OFN_ALLOWMULTISELECT:0);
	ofn.lpstrDefExt=NULL;
	
	WCHAR cwd[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, cwd);
	BOOL ok=GetOpenFileName(&ofn);
	SetCurrentDirectoryW(cwd);
	
	free(filter);
	if (!ok)
	{
		free(filenames);
		return NULL;
	}
	
	bool ismultiple=(ofn.nFileOffset && filenames[ofn.nFileOffset-1]=='\0');
	if (!ismultiple)
	{
		ret=malloc(sizeof(char*)*2);
		ret[0]=window_get_absolute_path(window_get_cwd(), filenames, true);
		ret[1]=NULL;
		return (const char * const *)ret;
	}
	filenames[ofn.nFileOffset-1]='\\';
	
	unsigned int numfiles=0;
	char * filename=filenames+ofn.nFileOffset;
	while (*filename)
	{
		numfiles++;
		filename+=strlen(filename)+1;
	}
	
	ret=malloc(sizeof(char*)*(numfiles+1));
	filename=filenames+ofn.nFileOffset;
	char * * retout=ret;
	while (*filename)
	{
		unsigned int thislen=strlen(filename);
		memcpy(filenames+ofn.nFileOffset, filename, thislen+1);
		*retout=window_get_absolute_path(window_get_cwd(), filenames, true);
		retout++;
		filename+=thislen+1;
	}
	free(filenames);
	ret[numfiles] = NULL;
	
	return (const char * const *)ret;
}

char * window_get_absolute_path(const char * basepath, const char * path, bool allow_up)
{
	return _window_native_get_absolute_path(basepath, path, allow_up);
}



bool file_read(const char * filename, void* * data, size_t * len)
{
	if (!filename) return false;
	HANDLE file=CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file==INVALID_HANDLE_VALUE) return false;
	DWORD readlen=GetFileSize(file, NULL);
	DWORD truelen;
	char* truedata=malloc(readlen+1);
	ReadFile(file, truedata, readlen, &truelen, NULL);
	truedata[readlen]='\0';
	*data=truedata;
	CloseHandle(file);
	if (truelen!=readlen)
	{
		free(truedata);
		return false;
	}
	if (len) *len=truelen;
	return true;
}

bool file_write(const char * filename, const anyptr data, size_t len)
{
	if (!filename) return false;
	if (!len) return true;
	HANDLE file=CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file==INVALID_HANDLE_VALUE) return false;
	DWORD truelen;
	WriteFile(file, data, len, &truelen, NULL);
	CloseHandle(file);
	return (truelen==len);
}

bool file_read_to(const char * filename, anyptr data, size_t len)
{
	if (!filename) return false;
	if (!len) return true;
	HANDLE file=CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file==INVALID_HANDLE_VALUE) return false;
	DWORD readlen=GetFileSize(file, NULL);
	if (readlen!=len)
	{
		CloseHandle(file);
		return false;
	}
	DWORD truelen;
	ReadFile(file, data, len, &truelen, NULL);
	CloseHandle(file);
	return (len==truelen);
}


//this could be made far cleaner if . or .. was guaranteed to be first.
struct finddata {
	HANDLE h;
	WIN32_FIND_DATA file;
	bool first;
};

void* file_find_create(const char * path)
{
	if (!path) return NULL;
	int pathlen=strlen(path);
	char * pathcopy=malloc(pathlen+3);
	memcpy(pathcopy, path, pathlen);
	pathcopy[pathlen+0]='\\';
	pathcopy[pathlen+1]='*';
	pathcopy[pathlen+2]='\0';
	struct finddata * find=malloc(sizeof(struct finddata));
	find->h=FindFirstFile(pathcopy, &find->file);
	free(pathcopy);
	find->first=true;
	if (find->h==INVALID_HANDLE_VALUE)
	{
		free(find);
		return NULL;
	}
	return find;
}

bool file_find_next(void* find_, char * * path, bool * isdir)
{
	if (!find_) return false;
	struct finddata * find=(struct finddata*)find_;
nextfile:;
	bool ok=true;
	if (find->first) find->first=false;
	else ok=FindNextFile(find->h, &find->file);
	if (!ok) return false;
	if (!strcmp(find->file.cFileName, ".")) goto nextfile;
	if (!strcmp(find->file.cFileName, "..")) goto nextfile;
	*path=strdup(find->file.cFileName);
	*isdir=(find->file.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY);
	return true;
}

void file_find_close(void* find_)
{
	if (!find_) return;
	struct finddata * find=(struct finddata*)find_;
	FindClose(find->h);
	free(find);
}
#endif
#endif
