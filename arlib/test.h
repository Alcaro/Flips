#pragma once
#include "global.h"
#include "stringconv.h"

#undef assert

#ifdef ARLIB_TEST

class _test_maybeptr {
	const char * data;
public:
	_test_maybeptr() : data(NULL) {}
	_test_maybeptr(const char * data) : data(data) {}
	
	operator const char *() { return data; }
};
class _testdecl {
public:
	_testdecl(void(*func)(), const char * loc, const char * name);
};

void _testfail(cstring name, int line);
void _testeqfail(cstring name, int line, cstring expected, cstring actual);

void _teststack_push(int line);
void _teststack_pop();

#define TESTFUNCNAME JOIN(_testfunc, __LINE__)
#define test(...) \
	static void TESTFUNCNAME(); \
	static _testdecl JOIN(_testdecl, __LINE__)(TESTFUNCNAME, __FILE__ ":" STR(__LINE__), _test_maybeptr(__VA_ARGS__)); \
	static void TESTFUNCNAME()
#define assert_ret(x, ret) do { if (!(x)) { _testfail("\nFailed assertion " #x, __LINE__); return ret; } } while(0)
#define assert(x) assert_ret(x,)
#define assert_eq(actual,expected) do { \
		if ((actual) != (expected)) \
		{ \
			_testeqfail(#actual " == " #expected, __LINE__, tostring(expected), tostring(actual)); \
			return; \
		} \
	} while(0)
#define testcall(x) _teststack_push(__LINE__),x,_teststack_pop()

#else

#define test(...) static void MAYBE_UNUSED JOIN(_testfunc_, __LINE__)()
#define assert(x)
#define assert_eq(x,y)
#define testcall(x) x

#endif
