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

//The above license applies only to this file, not the entire Arlib.
#if 0
//You don't need this. It's just a bunch of tests for WuTF itself.
//To run: Flip the above #if, then
// $ g++ *.cpp -std=c++11 && ./a.out
// C:\> g++ *.cpp -std=c++11 -lcomdlg32 && a.exe
//The tests require C++11, for char16_t. WuTF itself is plain C.
#include "wutf.h"
#include <stdio.h>

static bool test816_core(int flags, const char* utf8, const char16_t* utf16_exp, int inlen=0, int outlen_e=0)
{
	int iflags = flags & WUTF_INVALID_MASK;
	
	if ((flags&WUTF_WTF8) && iflags==WUTF_INVALID_DCXX) return true;
	
	if (!inlen)
	{
		while (utf8[inlen]) inlen++;
		inlen++;
	}
	if (!outlen_e)
	{
		while (utf16_exp[outlen_e]) outlen_e++;
		outlen_e++;
	}
	
	uint16_t utf16_act[128];
	int outlen_a = WuTF_utf8_to_utf16(flags, utf8, inlen, utf16_act, 128);
	
	int outpos_a=0;
	int outpos_e=0;
	
	int outlen_a2 = WuTF_utf8_to_utf16(flags, utf8, inlen, NULL, 0);
	if (outlen_a != outlen_a2) { printf("Expected length %i, got %i\n", outlen_a, outlen_a2); goto fail; }
	
	if (iflags == WUTF_INVALID_ABORT && outlen_a < 0)
	{
		for (int i=0;i<outlen_e;i++)
		{
//printf("[%i]%.4X\n",i,utf16_exp[i]);
			if ((utf16_exp[i]&0xFC00) == 0xDC00) return true;
			if ((utf16_exp[i]&0xFC00) == 0xD800) i++;
		}
	}
	
	while (outpos_e < outlen_e && outpos_a < outlen_a)
	{
		uint16_t exp = utf16_exp[outpos_e++];
		uint16_t act = utf16_act[outpos_a++];
		if ((flags & WUTF_WTF8) == 0)
		{
			if ((exp&0xFC00) == 0xDC00 && (utf16_exp[outpos_e-2]&0xFC00) != 0xD800)
			{
				if (iflags == WUTF_INVALID_FFFD && act == 0xFFFD) continue;
				if (iflags == WUTF_INVALID_DROP) { outpos_a--; continue; }
			}
		}
		if (exp!=act) goto fail;
	}
	if (outpos_e != outlen_e || outpos_a != outlen_a)
	{
	fail:
		puts(utf8);
		printf("E "); for (int i=0;i<outlen_e;i++) printf("%.4X ", utf16_exp[i]); puts("");
		printf("A "); for (int i=0;i<outlen_a;i++) printf("%.4X ", utf16_act[i]); puts("");
		return false;
	}
	return true;
}

static void test816f(int flags, const char* utf8, const char16_t* utf16_exp, int inlen=0, int outlen_e=0)
{
	test816_core(flags|WUTF_INVALID_DCXX, utf8, utf16_exp, inlen, outlen_e) &&
	test816_core(flags|WUTF_INVALID_FFFD, utf8, utf16_exp, inlen, outlen_e) &&
	test816_core(flags|WUTF_INVALID_DROP, utf8, utf16_exp, inlen, outlen_e) &&
	test816_core(flags|WUTF_INVALID_ABORT, utf8, utf16_exp, inlen, outlen_e) &&
	false;
}

static void test816(const char* utf8, const char16_t* utf16_exp, int inlen=0, int outlen_e=0)
{
	test816f(0, utf8, utf16_exp, inlen, outlen_e);
}

static void test168f(int flags, const char16_t* utf16, const char* utf8_exp, int inlen=0, int outlen_e=0)
{
	if (!inlen)
	{
		while (utf16[inlen]) inlen++;
		inlen++;
	}
	if (!outlen_e)
	{
		while (utf8_exp[outlen_e]) outlen_e++;
		outlen_e++;
	}
	
	char utf8_act[128];
	int outlen_a = WuTF_utf16_to_utf8(flags, (const uint16_t*)utf16, inlen, utf8_act, 128);
	
	int outpos_a=0;
	int outpos_e=0;
	
	int outlen_a2 = WuTF_utf16_to_utf8(flags, (const uint16_t*)utf16, inlen, NULL, 0);
	if (outlen_a != outlen_a2) { printf("Expected length %i, got %i\n", outlen_a, outlen_a2); goto fail; }
	
	while (outpos_e < outlen_e && outpos_a < outlen_a)
	{
		if (utf8_exp[outpos_e++] != utf8_act[outpos_a++]) goto fail;
	}
	if (outpos_e != outlen_e || outpos_a != outlen_a)
	{
	fail:
		for (int i=0;i<inlen;i++) printf("%.4X ", utf16[i]); puts("");
		printf("E "); for (int i=0;i<outlen_e;i++) printf("%.2X ", utf8_exp[i]&255); puts("");
		printf("A "); for (int i=0;i<outlen_a;i++) printf("%.2X ", utf8_act[i]&255); puts("");
	}
}

static void test168(const char16_t* utf16, const char* utf8_exp, int inlen=0, int outlen_e=0)
{
	test168f(0, utf16, utf8_exp, inlen, outlen_e);
}

void WuTF_test_encoder()
{
	test816("a", u"a");
	test816("smörgåsräka", u"smörgåsräka");
	test816("♩♪♫♬", u"♩♪♫♬");
	test816("𝄞♩♪♫♬", u"𝄞♩♪♫♬");
	
	//http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
	//bugs found in this implementation:
	//- I treated 0xF8 as 0xF0 rather than illegal
	//- the test for surrogate or not is <= 0xFFFF, not <
	
	//1  Some correct UTF-8 text
	test816("#\xce\xba\xe1\xbd\xb9\xcf\x83\xce\xbc\xce\xb5#", u"#\x03BA\x1F79\x03C3\x03BC\x03B5#");
	
	//2  Boundary condition test cases
	test816("#\0#",                       u"#\0#", 4,4);
	test816("#\xc2\x80#",                 u"#\x0080#");
	test816("#\xe0\xa0\x80#",             u"#\x0800#");
	test816("#\xf0\x90\x80\x80#",         u"#\xD800\xDC00#");
	test816("#\xf8\x88\x80\x80\x80#",     u"#\xDCF8\xDC88\xDC80\xDC80\xDC80#");
	test816("#\xfc\x84\x80\x80\x80\x80#", u"#\xDCFC\xDC84\xDC80\xDC80\xDC80\xDC80#");
	
	test816("#\x7f#",                     u"#\x007F#");
	test816("#\xdf\xbf#",                 u"#\x07FF#");
	test816("#\xef\xbf\xbf#",             u"#\xFFFF#");
	test816("#\xf7\xbf\xbf\xbf#",         u"#\xDCF7\xDCBF\xDCBF\xDCBF#");
	test816("#\xfb\xbf\xbf\xbf\xbf#",     u"#\xDCFB\xDCBF\xDCBF\xDCBF\xDCBF#");
	test816("#\xfd\xbf\xbf\xbf\xbf\xbf#", u"#\xDCFD\xDCBF\xDCBF\xDCBF\xDCBF\xDCBF#");
	
	test816("#\xed\x9f\xbf#",     u"#\xD7FF#");
	test816("#\xee\x80\x80#",     u"#\xE000#");
	test816("#\xef\xbf\xbd#",     u"#\xFFFD#");
	test816("#\xf4\x8f\xbf\xbf#", u"#\xDBFF\xDFFF#");
	test816("#\xf4\x90\x80\x80#", u"#\xDCF4\xDC90\xDC80\xDC80#");
	
	//3  Malformed sequences
	test816("#\x80#",                         u"#\xDC80#");
	test816("#\xbf#",                         u"#\xDCBF#");
	
	test816("#\x80\xbf#",                     u"#\xDC80\xDCBF#");
	test816("#\x80\xbf\x80#",                 u"#\xDC80\xDCBF\xDC80#");
	test816("#\x80\xbf\x80\xbf#",             u"#\xDC80\xDCBF\xDC80\xDCBF#");
	test816("#\x80\xbf\x80\xbf\x80#",         u"#\xDC80\xDCBF\xDC80\xDCBF\xDC80#");
	test816("#\x80\xbf\x80\xbf\x80\xbf#",     u"#\xDC80\xDCBF\xDC80\xDCBF\xDC80\xDCBF#");
	test816("#\x80\xbf\x80\xbf\x80\xbf\x80#", u"#\xDC80\xDCBF\xDC80\xDCBF\xDC80\xDCBF\xDC80#");
	
	test816("#\x80\x81\x82\x83\x84\x85\x86\x87#", u"#\xDC80\xDC81\xDC82\xDC83\xDC84\xDC85\xDC86\xDC87#");
	test816("#\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f#", u"#\xDC88\xDC89\xDC8A\xDC8B\xDC8C\xDC8D\xDC8E\xDC8F#");
	test816("#\x90\x91\x92\x93\x94\x95\x96\x97#", u"#\xDC90\xDC91\xDC92\xDC93\xDC94\xDC95\xDC96\xDC97#");
	test816("#\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f#", u"#\xDC98\xDC99\xDC9A\xDC9B\xDC9C\xDC9D\xDC9E\xDC9F#");
	test816("#\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7#", u"#\xDCA0\xDCA1\xDCA2\xDCA3\xDCA4\xDCA5\xDCA6\xDCA7#");
	test816("#\xa8\xa9\xaa\xab\xac\xad\xae\xaf#", u"#\xDCA8\xDCA9\xDCAA\xDCAB\xDCAC\xDCAD\xDCAE\xDCAF#");
	test816("#\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7#", u"#\xDCB0\xDCB1\xDCB2\xDCB3\xDCB4\xDCB5\xDCB6\xDCB7#");
	test816("#\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf#", u"#\xDCB8\xDCB9\xDCBA\xDCBB\xDCBC\xDCBD\xDCBE\xDCBF#");
	
	test816("#\xc0\x20\xc1\x20\xc2\x20\xc3\x20#", u"#\xDCC0 \xDCC1 \xDCC2 \xDCC3 #");
	test816("#\xc4\x20\xc5\x20\xc6\x20\xc7\x20#", u"#\xDCC4 \xDCC5 \xDCC6 \xDCC7 #");
	test816("#\xc8\x20\xc9\x20\xca\x20\xcb\x20#", u"#\xDCC8 \xDCC9 \xDCCA \xDCCB #");
	test816("#\xcc\x20\xcd\x20\xce\x20\xcf\x20#", u"#\xDCCC \xDCCD \xDCCE \xDCCF #");
	test816("#\xd0\x20\xd1\x20\xd2\x20\xd3\x20#", u"#\xDCD0 \xDCD1 \xDCD2 \xDCD3 #");
	test816("#\xd4\x20\xd5\x20\xd6\x20\xd7\x20#", u"#\xDCD4 \xDCD5 \xDCD6 \xDCD7 #");
	test816("#\xd8\x20\xd9\x20\xda\x20\xdb\x20#", u"#\xDCD8 \xDCD9 \xDCDA \xDCDB #");
	test816("#\xdc\x20\xdd\x20\xde\x20\xdf\x20#", u"#\xDCDC \xDCDD \xDCDE \xDCDF #");
	
	test816("#\xe0\x20\xe1\x20\xe2\x20\xe3\x20#", u"#\xDCE0 \xDCE1 \xDCE2 \xDCE3 #");
	test816("#\xe4\x20\xe5\x20\xe6\x20\xe7\x20#", u"#\xDCE4 \xDCE5 \xDCE6 \xDCE7 #");
	test816("#\xe8\x20\xe9\x20\xea\x20\xeb\x20#", u"#\xDCE8 \xDCE9 \xDCEA \xDCEB #");
	test816("#\xec\x20\xed\x20\xee\x20\xef\x20#", u"#\xDCEC \xDCED \xDCEE \xDCEF #");
	
	test816("#\xf0\x20\xf1\x20\xf2\x20\xf3\x20#", u"#\xDCF0 \xDCF1 \xDCF2 \xDCF3 #");
	test816("#\xf4\x20\xf5\x20\xf6\x20\xf7\x20#", u"#\xDCF4 \xDCF5 \xDCF6 \xDCF7 #");
	
	test816("#\xf8\x20\xf9\x20\xfa\x20\xfb\x20#", u"#\xDCF8 \xDCF9 \xDCFA \xDCFB #");
	
	test816("#\xfc\x20\xfd\x20#",                 u"#\xDCFC \xDCFD #");
	
	test816("#\xc0#",                 u"#\xDCC0#");
	test816("#\xe0\x80#",             u"#\xDCE0\xDC80#");
	test816("#\xf0\x80\x80#",         u"#\xDCF0\xDC80\xDC80#");
	test816("#\xf8\x80\x80\x80#",     u"#\xDCF8\xDC80\xDC80\xDC80#");
	test816("#\xfc\x80\x80\x80\x80#", u"#\xDCFC\xDC80\xDC80\xDC80\xDC80#");
	
	test816("#\xdf#",                 u"#\xDCDF#");
	test816("#\xef\xbf#",             u"#\xDCEF\xDCBF#");
	test816("#\xf7\xbf\xbf#",         u"#\xDCF7\xDCBF\xDCBF#");
	test816("#\xfb\xbf\xbf\xbf#",     u"#\xDCFB\xDCBF\xDCBF\xDCBF#");
	test816("#\xfd\xbf\xbf\xbf\xbf#", u"#\xDCFD\xDCBF\xDCBF\xDCBF\xDCBF#");
	
	test816("#\xc0\xe0\x80\xf0\x80\x80\xf8\x80\x80\x80\xfc\x80\x80\x80\x80#",
	        u"#\xDCC0\xDCE0\xDC80\xDCF0\xDC80\xDC80\xDCF8\xDC80\xDC80\xDC80\xDCFC\xDC80\xDC80\xDC80\xDC80#");
	test816("#\xdf\xef\xbf\xf7\xbf\xbf\xfb\xbf\xbf\xbf\xfd\xbf\xbf\xbf\xbf#",
	        u"#\xDCDF\xDCEF\xDCBF\xDCF7\xDCBF\xDCBF\xDCFB\xDCBF\xDCBF\xDCBF\xDCFD\xDCBF\xDCBF\xDCBF\xDCBF#");
	
	test816("#\xfe#",             u"#\xDCFE#");
	test816("#\xff#",             u"#\xDCFF#");
	test816("#\xfe\xfe\xff\xff#", u"#\xDCFE\xDCFE\xDCFF\xDCFF#");
	
	//4  Overlong sequences
	test816("#\xc0\xaf#",                 u"#\xDCC0\xDCAF#");
	test816("#\xe0\x80\xaf#",             u"#\xDCE0\xDC80\xDCAF#");
	test816("#\xf0\x80\x80\xaf#",         u"#\xDCF0\xDC80\xDC80\xDCAF#");
	test816("#\xf8\x80\x80\x80\xaf#",     u"#\xDCF8\xDC80\xDC80\xDC80\xDCAF#");
	test816("#\xfc\x80\x80\x80\x80\xaf#", u"#\xDCFC\xDC80\xDC80\xDC80\xDC80\xDCAF#");

	test816("#\xc1\xbf#",                 u"#\xDCC1\xDCBF#");
	test816("#\xe0\x9f\xbf#",             u"#\xDCE0\xDC9F\xDCBF#");
	test816("#\xf0\x8f\xbf\xbf#",         u"#\xDCF0\xDC8F\xDCBF\xDCBF#");
	test816("#\xf8\x87\xbf\xbf\xbf#",     u"#\xDCF8\xDC87\xDCBF\xDCBF\xDCBF#");
	test816("#\xfc\x83\xbf\xbf\xbf\xbf#", u"#\xDCFC\xDC83\xDCBF\xDCBF\xDCBF\xDCBF#");

	test816("#\xc0\x80#",                 u"#\xDCC0\xDC80#");
	test816("#\xe0\x80\x80#",             u"#\xDCE0\xDC80\xDC80#");
	test816("#\xf0\x80\x80\x80#",         u"#\xDCF0\xDC80\xDC80\xDC80#");
	test816("#\xf8\x80\x80\x80\x80#",     u"#\xDCF8\xDC80\xDC80\xDC80\xDC80#");
	test816("#\xfc\x80\x80\x80\x80\x80#", u"#\xDCFC\xDC80\xDC80\xDC80\xDC80\xDC80#");
	
	//5  Illegal code positions
	test816("#\xed\xa0\x80#", u"#\xDCED\xDCA0\xDC80#");
	test816("#\xed\xad\xbf#", u"#\xDCED\xDCAD\xDCBF#");
	test816("#\xed\xae\x80#", u"#\xDCED\xDCAE\xDC80#");
	test816("#\xed\xaf\xbf#", u"#\xDCED\xDCAF\xDCBF#");
	test816("#\xed\xb0\x80#", u"#\xDCED\xDCB0\xDC80#");
	test816("#\xed\xbe\x80#", u"#\xDCED\xDCBE\xDC80#");
	test816("#\xed\xbf\xbf#", u"#\xDCED\xDCBF\xDCBF#");
	test816f(WUTF_WTF8, "#\xed\xa0\x80#", u"#\xD800#");
	test816f(WUTF_WTF8, "#\xed\xad\xbf#", u"#\xDB7F#");
	test816f(WUTF_WTF8, "#\xed\xae\x80#", u"#\xDB80#");
	test816f(WUTF_WTF8, "#\xed\xaf\xbf#", u"#\xDBFF#");
	test816f(WUTF_WTF8, "#\xed\xb0\x80#", u"#\xDC00#");
	test816f(WUTF_WTF8, "#\xed\xbe\x80#", u"#\xDF80#");
	test816f(WUTF_WTF8, "#\xed\xbf\xbf#", u"#\xDFFF#");
	
	test816("#\xed\xa0\x80\xed\xb0\x80#", u"#\xDCED\xDCA0\xDC80\xDCED\xDCB0\xDC80#");
	test816("#\xed\xa0\x80\xed\xbf\xbf#", u"#\xDCED\xDCA0\xDC80\xDCED\xDCBF\xDCBF#");
	test816("#\xed\xad\xbf\xed\xb0\x80#", u"#\xDCED\xDCAD\xDCBF\xDCED\xDCB0\xDC80#");
	test816("#\xed\xad\xbf\xed\xbf\xbf#", u"#\xDCED\xDCAD\xDCBF\xDCED\xDCBF\xDCBF#");
	test816("#\xed\xae\x80\xed\xb0\x80#", u"#\xDCED\xDCAE\xDC80\xDCED\xDCB0\xDC80#");
	test816("#\xed\xae\x80\xed\xbf\xbf#", u"#\xDCED\xDCAE\xDC80\xDCED\xDCBF\xDCBF#");
	test816("#\xed\xaf\xbf\xed\xb0\x80#", u"#\xDCED\xDCAF\xDCBF\xDCED\xDCB0\xDC80#");
	test816("#\xed\xaf\xbf\xed\xbf\xbf#", u"#\xDCED\xDCAF\xDCBF\xDCED\xDCBF\xDCBF#");
	test816f(WUTF_CESU8, "#\xed\xa0\x80\xed\xb0\x80#", u"#\xD800\xDC00#");
	test816f(WUTF_CESU8, "#\xed\xa0\x80\xed\xbf\xbf#", u"#\xD800\xDFFF#");
	test816f(WUTF_CESU8, "#\xed\xad\xbf\xed\xb0\x80#", u"#\xDB7F\xDC00#");
	test816f(WUTF_CESU8, "#\xed\xad\xbf\xed\xbf\xbf#", u"#\xDB7F\xDFFF#");
	test816f(WUTF_CESU8, "#\xed\xae\x80\xed\xb0\x80#", u"#\xDB80\xDC00#");
	test816f(WUTF_CESU8, "#\xed\xae\x80\xed\xbf\xbf#", u"#\xDB80\xDFFF#");
	test816f(WUTF_CESU8, "#\xed\xaf\xbf\xed\xb0\x80#", u"#\xDBFF\xDC00#");
	test816f(WUTF_CESU8, "#\xed\xaf\xbf\xed\xbf\xbf#", u"#\xDBFF\xDFFF#");
	
	test816("#\xef\xb7\x90\xef\xb7\x91\xef\xb7\x92\xef\xb7\x93#", u"#\xFDD0\xFDD1\xFDD2\xFDD3#");
	test816("#\xef\xb7\x94\xef\xb7\x95\xef\xb7\x96\xef\xb7\x97#", u"#\xFDD4\xFDD5\xFDD6\xFDD7#");
	test816("#\xef\xb7\x98\xef\xb7\x99\xef\xb7\x9a\xef\xb7\x9b#", u"#\xFDD8\xFDD9\xFDDA\xFDDB#");
	test816("#\xef\xb7\x9c\xef\xb7\x9d\xef\xb7\x9e\xef\xb7\x9f#", u"#\xFDDC\xFDDD\xFDDE\xFDDF#");
	test816("#\xef\xb7\xa0\xef\xb7\xa1\xef\xb7\xa2\xef\xb7\xa3#", u"#\xFDE0\xFDE1\xFDE2\xFDE3#");
	test816("#\xef\xb7\xa4\xef\xb7\xa5\xef\xb7\xa6\xef\xb7\xa7#", u"#\xFDE4\xFDE5\xFDE6\xFDE7#");
	test816("#\xef\xb7\xa8\xef\xb7\xa9\xef\xb7\xaa\xef\xb7\xab#", u"#\xFDE8\xFDE9\xFDEA\xFDEB#");
	test816("#\xef\xb7\xac\xef\xb7\xad\xef\xb7\xae\xef\xb7\xaf#", u"#\xFDEC\xFDED\xFDEE\xFDEF#");
	
	test816("#\xf0\x9f\xbf\xbe\xf0\x9f\xbf\xbf\xf0\xaf\xbf\xbe#", u"#\xD83F\xDFFE\xD83F\xDFFF\xD87F\xDFFE#");
	test816("#\xf0\xaf\xbf\xbf\xf0\xbf\xbf\xbe\xf0\xbf\xbf\xbf#", u"#\xD87F\xDFFF\xD8BF\xDFFE\xD8BF\xDFFF#");
	test816("#\xf1\x8f\xbf\xbe\xf1\x8f\xbf\xbf\xf1\x9f\xbf\xbe#", u"#\xD8FF\xDFFE\xD8FF\xDFFF\xD93F\xDFFE#");
	test816("#\xf1\x9f\xbf\xbf\xf1\xaf\xbf\xbe\xf1\xaf\xbf\xbf#", u"#\xD93F\xDFFF\xD97F\xDFFE\xD97F\xDFFF#");
	test816("#\xf1\xbf\xbf\xbe\xf1\xbf\xbf\xbf\xf2\x8f\xbf\xbe#", u"#\xD9BF\xDFFE\xD9BF\xDFFF\xD9FF\xDFFE#");
	test816("#\xf2\x8f\xbf\xbf\xf2\x9f\xbf\xbe\xf2\x9f\xbf\xbf#", u"#\xD9FF\xDFFF\xDA3F\xDFFE\xDA3F\xDFFF#");
	test816("#\xf2\xaf\xbf\xbe\xf2\xaf\xbf\xbf\xf2\xbf\xbf\xbe#", u"#\xDA7F\xDFFE\xDA7F\xDFFF\xDABF\xDFFE#");
	test816("#\xf2\xbf\xbf\xbf\xf3\x8f\xbf\xbe\xf3\x8f\xbf\xbf#", u"#\xDABF\xDFFF\xDAFF\xDFFE\xDAFF\xDFFF#");
	test816("#\xf3\x9f\xbf\xbe\xf3\x9f\xbf\xbf\xf3\xaf\xbf\xbe#", u"#\xDB3F\xDFFE\xDB3F\xDFFF\xDB7F\xDFFE#");
	test816("#\xf3\xaf\xbf\xbf\xf3\xbf\xbf\xbe\xf3\xbf\xbf\xbf#", u"#\xDB7F\xDFFF\xDBBF\xDFFE\xDBBF\xDFFF#");
	test816("#\xf4\x8f\xbf\xbe\xf4\x8f\xbf\xbf#",                 u"#\xDBFF\xDFFE\xDBFF\xDFFF#");
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//some of these are the above backwards, some are other edge cases
	//either way, 16->8 is way less tested than 8->16, and 8<->32 isn't tested at all
	test168(u"a", "a");
	test168(u"smörgåsräka", "smörgåsräka");
	test168(u"♩♪♫♬", "♩♪♫♬");
	test168(u"𝄞♩♪♫♬", "𝄞♩♪♫♬");
	
	test168(u"#\x0000#",       "#\x00#", 4,4);
	test168(u"#\x0080#",       "#\xc2\x80#");
	test168(u"#\x0800#",       "#\xe0\xa0\x80#");
	test168(u"#\xD800\xDC00#", "#\xf0\x90\x80\x80#");
	
	test168(u"#\x007F#",       "#\x7f#");
	test168(u"#\x07FF#",       "#\xdf\xbf#");
	test168(u"#\xFFFF#",       "#\xef\xbf\xbf#");
	
	test168(u"#\xD7FF#",       "#\xed\x9f\xbf#");
	test168(u"#\xE000#",       "#\xee\x80\x80#");
	test168(u"#\xFFFD#",       "#\xef\xbf\xbd#");
	test168(u"#\xDBFF\xDFFF#", "#\xf4\x8f\xbf\xbf#");
	
	test168f(WUTF_WTF8, u"#\xD800#",       "#\xed\xa0\x80#");
	test168f(WUTF_WTF8, u"#\xDBFF#",       "#\xed\xaf\xbf#");
	test168f(WUTF_WTF8, u"#\xDC00#",       "#\xed\xb0\x80#");
	test168f(WUTF_WTF8, u"#\xDFFF#",       "#\xed\xbf\xbf#");
}




#ifdef _WIN32
#define SMR "sm\xC3\xB6rg\xC3\xA5sr\xC3\xA4ka"
#define SMR_W L"sm\x00F6rg\x00E5sr\x00E4ka"
#define SMR3 "\xC3\xA5\xC3\xA4\xC3\xB6"
#define SMR3_W L"\x00E5\x00E4\x00F6"
#include <windows.h>
 
static void testOFN();
//To pass,
//(1) "(1) PASS" must show up in the console.
//(2) "(2) PASS" must show up in the console.
//(3) The five .åäö files must show up in the filename chooser dialog.
//(3) you_shouldnt_see_this.txt must NOT show up.
//(3) The instructions (file type field) must be ungarbled.
//(3) You must follow the filetype instructions.
//(3) "(3) PASS" must show up in the console.
//(4) The text on the button must be correct.
//(4) The text on the button window's title must be correct.
//(5) The message box title must be correct.
//(6) The message box title must be correct.
//Explanation:
//(1) CreateFileA(); a simple thing
//(2) GetWindowTextA(); it returns strings, rather than taking them
//(3) GetOpenFileNameA(); a complex thing
//(4) Test text on a button; also on the window title, but (5) also tests that
//(5) Message box title
//(6) Message box body, and a string that's longer than the 260 limit
//4, 5 and 6 should be verified before dismissing the message box.
void WuTF_test_ansiutf()
{
	WuTF_enable();
	
	DWORD ignore;
	HANDLE h;
	h = CreateFileW(SMR_W L".txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	WriteFile(h, "pokemon", 8, &ignore, NULL);
	CloseHandle(h);
	
	h = CreateFileA(SMR ".txt", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (h != INVALID_HANDLE_VALUE)
	{
		char p[8];
		ReadFile(h, p, 42, &ignore, NULL);
		if (!strcmp(p, "pokemon")) puts("(1) PASS");
		else puts("(1) FAIL: Wrong contents");
		CloseHandle(h);
	}
	else
	{
		printf("(1) FAIL: Couldn't open file (errno %lu)", GetLastError());
	}
	DeleteFileW(SMR_W L".txt");
	
	HWND wnd = CreateWindowA("BUTTON", "(4) CHECK: " SMR, WS_OVERLAPPEDWINDOW|WS_VISIBLE,
	                         CW_USEDEFAULT, CW_USEDEFAULT, 200, 60,
	                         NULL, NULL, NULL, NULL);
	WCHAR expect[42];
	GetWindowTextW(wnd, expect, 42);
	if (!wcscmp(expect, L"(4) CHECK: " SMR_W)) puts("(2) PASS");
	else puts("(2) PASS");
	
	testOFN();
	
	//this one takes two string arguments, one of which can be way longer than 260
#define PAD "Stretch string to 260 characters."
#define PAD2 PAD " " PAD
#define PAD8 PAD2 "\r\n" PAD2 "\r\n" PAD2 "\r\n" PAD2
	MessageBoxA(NULL, PAD8 "\r\n(6) CHECK: " SMR, "(5) CHECK: " SMR, MB_OK);
}

static void testOFN()
{
	CreateDirectoryA(SMR, NULL);
	CloseHandle(CreateFileW(SMR_W L"/" SMR_W L"1." SMR3_W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
	CloseHandle(CreateFileW(SMR_W L"/" SMR_W L"2." SMR3_W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
	CloseHandle(CreateFileW(SMR_W L"/" SMR_W L"3." SMR3_W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
	CloseHandle(CreateFileW(SMR_W L"/" SMR_W L"4." SMR3_W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
	CloseHandle(CreateFileW(SMR_W L"/" SMR_W L"5." SMR3_W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
	CloseHandle(CreateFileW(SMR_W L"/you_shouldnt_see_this.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
	
	OPENFILENAME ofn;
	char ofnret[65536];
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = "Select the " SMR3 " files\0*." SMR3 "\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = ofnret;
	ofn.nMaxFile = 65536;
	ofn.lpstrInitialDir = SMR;
	ofn.Flags = OFN_ALLOWMULTISELECT|OFN_EXPLORER;
	
	GetOpenFileNameA(&ofn);
	
	char* filenames = ofnret;
	int numcorrect = 0;
	while (*filenames)
	{
		puts(filenames);
		if (strlen(filenames)==strlen(SMR "?." SMR3))
		{
			filenames[strlen(SMR)]='?';
			if (!strcmp(filenames, SMR "?." SMR3)) numcorrect++;
			else numcorrect = -1000;
		}
		filenames += strlen(filenames)+1;
	}
	if (numcorrect == 5)
	{
		puts("(3) PASS");
	}
	else
	{
		puts("(3) FAIL");
	}
}
#endif

int main()
{
	WuTF_test_encoder();
#ifdef _WIN32
	WuTF_test_ansiutf();
#endif
}
#endif
