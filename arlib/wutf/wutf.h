// MIT License
//
// Copyright (c) 2016 Alfred Agrell
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//The above license applies only to the WuTF directory, not the entire Arlib.

// It is well known that Windows supports two flavors of every function that
//  takes or returns strings(*): A and W. The A ones take strings in the local
//  user's codepage; W uses UTF-16.
//  (*) With a few exceptions, for example CommandLineToArgvW, CharNextExA and GetProcAddress.
// It is also fairly well known that the local codepage can not be set to UTF-8,
//  despite users' repeated requests.
// It is less well known that the A functions convert their arguments and then
//  call the W functions.
// It is even less well known (though easy to guess) that there are only a few
//  conversion functions in the entire Windows.
//
// It is not very well known that you can mark memory executable using
//  VirtualProtect, and use that to create your own functions.
// It is even less well known that you can mark existing functions writable.
//
// Combining those lead to an evil idea: What if we replace the A->W conversion
//  function with an UTF8->UTF16 converter?
// And this is exactly what I did.
//
//Limitations:
//- IMMEDIATELY VOIDS YOUR WARRANTY
//- Possibly makes antivirus software panic.
//- Will crash if this code is in a DLL that's unloaded, possibly including program shutdown.
//- Affects the entire process; don't do it unless you know the process wants it this way.
//   I believe most processes actually wants it this way; everything I've seen either expects ASCII
//    only, uses the W APIs, or is console output (which is another codepage). I am not aware of
//    anything that actually expects the ANSI codepage.
//- Disables support for non-UTF8 code pages in MultiByteToWideChar and WideCharToMultiByte and
//    treats them as UTF-8, even if explicitly requested otherwise.
//- Console input and output remains ANSI. Consoles are very strangely implemented in Windows;
//    judging by struct CHAR_INFO in WriteConsoleOutput's arguments, the consoles don't support
//    UTF-16, but only UCS-2. (The rest of WuTF supports non-BMP characters, of course.)
//   A more technical explanation: The most common ways to write to the console (the ones in msvcrt)
//    end up in _write, then WriteFile. I can't replace either of them; I need to call them, and
//    there's no reasonably sane and reliable way to redirect a function while retaining the ability
//    to use the original.
//   _setmode(_O_U8TEXT) doesn't help; it makes puts() convert from UTF-16 to UTF-8. I need the
//    other direction.
//   SetConsoleOutputCP(CP_UTF8) seems more promising, but it reports success and does nothing on
//    Windows XP and 7. I'm not sure what it actually does.
//- CharNextA/etc are unchanged and still expect the ANSI code page. (Does anything ever use them?)
//- SetFileApisToOEM is untested. I don't know if it's ignored or if it actually does set them to
//    OEM. Either way, the fix is easy: don't use it.
//- Windows filenames are limited to ~260 characters; but I believe functions that return filenames
//    will count the UTF-8 bytes. (The ones taking filename inputs should work up to 260 UTF-16
//    codepoints.)
//- According to Larry Osterman <https://blogs.msdn.microsoft.com/larryosterman/2007/03/20/other-fun-things-to-do-with-the-endpointvolume-interfaces/>,
//    "all new APIs are unicode only" (aka UTF-16).
//- The UTF8/16 converter is not identical to MultiByteToWideChar(CP_UTF8):
//  - While it does support UTF-16 surrogate pairs, it's perfectly happy to encode lone surrogate
//    characters, as per WTF-8 <https://simonsapin.github.io/wtf-8/>. MBtWC rejects them.
//  - It supports decoding lone surrogates, too - or even paired surrogates (also known as CESU-8).
//  - If given invalid input (and MB_ERR_INVALID_CHARS is absent), it emits one or more U+FFFD
//    REPLACEMENT CHARACTER, rather than dropping them or creating question marks.
//   It does reject overlong encodings, and processes surrogate pairs correctly.
//- Did I mention it voids your warranty?
//
// Keywords:
//  Windows UTF-8
//  make Windows use UTF-8
//  set codepage to UTF-8
//  set codepage to CP_UTF8
//  convert Windows A functions to UTF-8

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifdef _WIN32
//Main function; this one does the actual magic. Call this as early as possible.
void WuTF_enable();

//Converts argc/argv to UTF-8. Uses only documented functions, so it has zero chance of blowing up.
//However, it does leak memory, so don't call it more than once. (The leaks are cleaned up on process exit anyways.)
void WuTF_args(int* argc, char** * argv);

//DO NOT USE THIS UNLESS YOU'RE INSANE
//This replaces the 'victim' function such that all calls instead go to 'replacement'.
//Make sure their signatures, including calling convention, are identical.
typedef void(*WuTF_funcptr)();
void WuTF_redirect_function(WuTF_funcptr victim, WuTF_funcptr replacement);

#else
//Other OSes already use UTF-8.
static inline void WuTF_enable() {}
static inline void WuTF_args(int* argc, char** * argv) {}
#endif

//This one just combines the above.
static inline void WuTF_enable_args(int* argc, char** * argv) { WuTF_enable(); WuTF_args(argc, argv); }


//Lengths are in code units, and include the NUL terminator.
//-1 is valid for the input length, and means 'use strlen()+1'.
//Return value is number of code units emitted.
//If the output parameters are NULL/0, it discards the output, and only returns the required number of code units.

//If input is not valid,
#define WUTF_INVALID_ABORT 0x00 // return error (default)
#define WUTF_INVALID_DROP  0x01 // ignore the bad codepoints
#define WUTF_INVALID_FFFD  0x02 // replace each bad byte with U+FFFD
#define WUTF_INVALID_DCXX  0x03 // encode the invalid bytes as U+DC00 plus the bad byte (lossless)
#define WUTF_INVALID_MASK  0x03 // (used internally)

#define WUTF_TRUNCATE 0x04 // If the output string doesn't fit, truncate it. Without this flag, truncation yields WUTF_E_TRUNCATE.

#define WUTF_CESU8 0x08 // If the input UTF-8 contains paired UTF-16 surrogates, decode it to a single codepoint. utf8_to only.
#define WUTF_WTF8  0x10 // If the input contains unpaired UTF-16 surrogates, treat as normal codepoints. Incompatible with INVALID_DCXX.

#define WUTF_E_TRUNCATE -2
#define WUTF_E_INVALID -1

int WuTF_utf8_to_utf32(int flags, const char* utf8, int utf8_len, uint32_t* utf32, int utf32_len);
int WuTF_utf32_to_utf8(int flags, const uint32_t* utf32, int utf32_len, char* utf8, int utf8_len);

//Used internally in WuTF. It's STRONGLY RECOMMENDED to not use these; use UTF-32 instead.
int WuTF_utf8_to_utf16(int flags, const char* utf8, int utf8_len, uint16_t* utf16, int utf16_len);
int WuTF_utf16_to_utf8(int flags, const uint16_t* utf16, int utf16_len, char* utf8, int utf8_len);

#ifdef __cplusplus
}
#endif
