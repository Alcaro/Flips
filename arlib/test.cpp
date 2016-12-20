#ifdef ARLIB_TEST
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

static bool thisfail;

static array<int> callstack;
void _teststack_push(int line) { callstack.append(line); }
void _teststack_pop() { callstack.resize(callstack.size()-1); }
static string stack(int top)
{
	string ret = " (line "+tostring(top);
	
	for (int i=callstack.size()-1;i>=0;i--)
	{
		ret += ", called from "+tostring(callstack[i]);
	}
	
	return ret+")";
}

static void _testfail(cstring why)
{
	if (!thisfail) puts(why); // discard multiple failures from same test, they're probably caused by same thing
	thisfail = true;
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

#undef main // the real main is #define'd to something stupid on test runs
int main(int argc, char* argv[])
{
#ifndef ARGUI_NONE
	window_init(&argc, &argv);
#else
	_window_init_file();
#endif
	
	int count[2]={0,0};
	
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
		thisfail = false;
		callstack.reset();
		test->func();
		count[thisfail]++;
		if (!thisfail) puts(" pass");
		free(test);
		test = next;
	}
	printf("Passed %i, failed %i\n", count[0], count[1]);
	return 0;
}

test() {}
test() {}
#endif
