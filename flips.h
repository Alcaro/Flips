//Module name: Floating IPS, header for all frontends
//Author: Alcaro
//Date: June 18, 2015
//Licence: GPL v3.0 or higher

//Preprocessor switch documentation:
//
//FLIPS_WINDOWS
//FLIPS_GTK
//FLIPS_CLI
//  Picks which frontend to use for Flips. You can pick one manually, or let Flips choose
//  automatically depending on the platform (Windows -> FLIPS_WINDOWS, Linux -> FLIPS_GTK, anything
//  else -> FLIPS_CLI). FLIPS_WINDOWS and FLIPS_CLI can be compiled under both C99 and C++98;
//  FLIPS_GTK is only tested under C99.
//  Note that picking the platform native frontend will bring a few advantages even if you only
//  intend to use Flips from the command line; Windows gains access to filenames outside the 8bit
//  charset, and GTK+ will gain the ability to handle files on URIs and not the local file system.
//
//All of these must be defined globally, or Flips will behave erratically.

#if defined(FLIPS_WINDOWS) || defined(FLIPS_GTK) || defined(FLIPS_CLI)
//already picked
#elif defined(_WIN32)
#define FLIPS_WINDOWS
#elif defined(__linux__)
#define FLIPS_GTK
#else
#define FLIPS_CLI
#endif

//#ifdef __cplusplus
//#define EXTERN_C extern "C"
//#else
//#define EXTERN_C
//#endif

#define flipsversion "Flips v1.31"


#if defined(FLIPS_WINDOWS)
#define UNICODE
//# define _UNICODE
//#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0600

//#define _WIN32_IE 0x0600
//#define __MSVCRT_VERSION__ 0x0601
#define NOMINMAX // this seems automatically on in C++ - crazy.
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <wchar.h>
#include <stdio.h>
#include <commctrl.h>

#define wcsicmp _wcsicmp//wcsicmp deprecated? fuck that, I use what I want. I do not add underlines to a few randomly chosen functions.
#define wcsdup _wcsdup
//EXTERN_C int _wcsicmp(const wchar_t * string1, const wchar_t * string2);
//EXTERN_C int swprintf(wchar_t * buffer, const wchar_t * format, ...);//also tdm quit having outdated and/or incomplete headers.


#else
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

//Flips uses Windows types internally, since it's easier to #define them to Linux types than
//defining "const char *" to anything else, and since I use char* at some places (mainly libips/etc)
//and really don't want to try to split them. Inventing my own typedefs seems counterproductive as
//well; they would bring no advantage over Windows types except not being Windows types, and I don't
//see that as a valid argument.
#define LPCWSTR const char *
#define LPWSTR char *
#define WCHAR char
#define wcscpy strcpy
#define wcscat strcat
#define wcschr strchr
#define wcslen strlen
#define wcsdup strdup
#define wcsrchr strrchr
#define wcscmp strcmp
#define wcsncmp strncmp
#define wcsicmp strcasecmp
//#define wcsnicmp strncasecmp
#define wprintf printf
#define TEXT(text) text
//EXTERN_C int strcasecmp(const char *s1, const char *s2);
#define ClaimConsole() // all other platforms have consoles already

#define strdup strdup_flips
static inline char* strdup(const char * in)
{
	size_t len=strlen(in);
	char * ret=(char*)malloc(len+1);
	strcpy(ret, in);
	return ret;
}
#endif

#include "libbps.h"
#include "libips.h"
#include "libups.h"

#ifndef __cplusplus
#include <stdbool.h>//If this file does not exist, remove it and uncomment the following three lines.
//#define bool int
//#define true 1
//#define false 0
#endif


//provided by Flips core
#include "global.h"

enum patchtype {
	ty_null,
	ty_bps,
	ty_ips,
	
	//non-recommended formats
	ty_bps_linear,
	ty_bps_moremem,
	ty_ups,
	
	ty_shut_up_gcc
};

enum errorlevel {
	el_ok,
	el_notice,
	el_unlikelythis,
	el_warning,
	el_notthis,
	el_broken,
	el_shut_up_gcc
};

struct errorinfo {
	enum errorlevel level;
	const char * description;
};

struct manifestinfo {
	bool use;
	bool required;
	LPCWSTR name;
};

class file;
class filewrite;

LPWSTR GetExtension(LPCWSTR fname);
LPWSTR GetBaseName(LPCWSTR fname);
bool shouldRemoveHeader(LPCWSTR romname, size_t romlen);

struct mem GetRomList();
void SetRomList(struct mem data);
LPCWSTR FindRomForPatch(file* patch, bool * possibleToFind);
void AddToRomList(file* patch, LPCWSTR path);
void DeleteRomFromList(LPCWSTR path);

struct errorinfo ApplyPatchMem2(file* patch, struct mem inrom, bool removeheader, bool verifyinput,
                                LPCWSTR outromname, struct manifestinfo * manifestinfo);
struct errorinfo ApplyPatchMem(file* patch, LPCWSTR inromname, bool verifyinput,
                               LPCWSTR outromname, struct manifestinfo * manifestinfo, bool update_rom_list);
struct errorinfo ApplyPatch(LPCWSTR patchname, LPCWSTR inromname, bool verifyinput,
                            LPCWSTR outromname, struct manifestinfo * manifestinfo, bool update_rom_list);
//struct errorinfo CreatePatchToMem(file* inrom, file* outrom, enum patchtype patchtype,
                                  //struct manifestinfo * manifestinfo, struct mem * patchmem);
//struct errorinfo CreatePatch(file* inrom, file* outrom, enum patchtype patchtype,
                             //struct manifestinfo * manifestinfo, LPCWSTR patchname);
struct errorinfo CreatePatchToMem(LPCWSTR inromname, LPCWSTR outromname, enum patchtype patchtype,
                                  struct manifestinfo * manifestinfo, struct mem * patchmem);
struct errorinfo CreatePatch(LPCWSTR inromname, LPCWSTR outromname, enum patchtype patchtype,
                             struct manifestinfo * manifestinfo, LPCWSTR patchname);

extern char bpsdProgStr[24];
extern int bpsdLastPromille;
bool bpsdeltaGetProgress(size_t done, size_t total);

int flipsmain(int argc, WCHAR * argv[]);
void usage();//does not return


//provided by the OS port
//several functions of file:: and filewrite:: also belong to the OS port

//TODO: delete
struct mem ReadWholeFile(LPCWSTR filename);
bool WriteWholeFile(LPCWSTR filename, struct mem data);
bool WriteWholeFileWithHeader(LPCWSTR filename, struct mem header, struct mem data);
void FreeFileMemory(struct mem mem);

void bpsdeltaBegin();
bool bpsdeltaProgress(void* userdata, size_t done, size_t total);
void bpsdeltaEnd();

int ShowGUI(LPCWSTR filename);
#ifdef FLIPS_WINDOWS
void ClaimConsole();
#endif

//the OS port is responsible for main()
