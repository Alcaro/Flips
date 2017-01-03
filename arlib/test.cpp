#ifdef ARLIB_TESTRUNNER
#ifndef ARLIB_TEST
#define ARLIB_TEST
#endif
#include "test.h"
#include "array.h"
#include "gui/window.h"

struct testlist {
	void(*func)();
	const char * loc;
	const char * name;
	testlist* next;
};

static testlist* g_testlist;

_testdecl::_testdecl(void(*func)(), const char * loc, const char * name)
{
	testlist* next = malloc(sizeof(testlist));
	next->func = func;
	next->loc = loc;
	next->name = name;
	next->next = g_testlist;
	g_testlist = next;
}

int _test_result;

static array<int> callstack;
void _teststack_push(int line) { callstack.append(line); }
void _teststack_pop() { callstack.resize(callstack.size()-1); }
static string stack(int top)
{
	if (top<0) return "";
	
	string ret = " (line "+tostring(top);
	
	for (int i=callstack.size()-1;i>=0;i--)
	{
		ret += ", called from "+tostring(callstack[i]);
	}
	
	return ret+")";
}

static void _testfail(cstring why)
{
	if (!_test_result) puts(why); // discard multiple failures from same test, they're probably caused by same thing
	_test_result = 1;
}

void _testfail(cstring why, int line)
{
	_testfail(why+stack(line));
}

void _testeqfail(cstring name, int line, cstring expected, cstring actual)
{
	if (expected.contains("\n") || actual.contains("\n"))
	{
		_testfail("\nFailed assertion "+name+stack(line)+"\nexpected:\n"+expected+"\nactual:\n"+actual);
	}
	else
	{
		_testfail("\nFailed assertion "+name+stack(line)+": expected "+expected+", got "+actual);
	}
}

void _test_skip(cstring why)
{
	if (!_test_result) puts("skipped: "+why);
	_test_result = 2;
}

#undef main // the real main is #define'd to something stupid on test runs
int main(int argc, char* argv[])
{
#ifndef ARGUI_NONE
	window_init(&argc, &argv);
#else
	_window_init_file();
#endif
	
	int count[3]={0,0,0};
	
	//flip list backwards
	//order of static initializers is implementation defined, but this makes output better under gcc
	testlist* test = g_testlist;
	g_testlist = NULL;
	while (test)
	{
		testlist* next = test->next;
		test->next = g_testlist;
		g_testlist = test;
		test = next;
	}
	
	test = g_testlist;
	while (test)
	{
		testlist* next = test->next;
		if (test->name) printf("Testing %s (%s)...", test->name, test->loc);
		else printf("Testing %s...", test->loc);
		fflush(stdout);
		_test_result = 0;
		callstack.reset();
		test->func();
		count[_test_result]++;
		if (!_test_result) puts(" pass");
		free(test);
		test = next;
	}
	printf("Passed %i, failed %i", count[0], count[1]);
	if (count[2]) printf(", skipped %i", count[2]);
	puts("");
	return 0;
}

#ifdef ARLIB_TEST_ARLIB
test() {}
test() {}
#endif
#endif
