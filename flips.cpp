#include "flips.h"

/*
a/a.bps b/b.smc         -> -a a/a.bps b/b.smc a/a.smc ("default path", patch path + patch basename + infile extension)
a/a.smc b/b.bps         -> -a b/b.bps a/a.smc b/b.smc
a/a.bps b/b.smc c/c.sfc -> -a a/a.bps b/b.smc c/c.sfc
-c a/a.smc b/b.smc      -> -c a/a.smc b/b.smc b/b.bps
a/a.smc b/b.smc c/c.bps -> -c a/a.smc b/b.smc c/c.bps
a/a.smc b/b.smc         -> -c a/a.smc b/b.smc b/b.bps
a/a.smc                 -> error
a/a.bps b/b.bps         -> error
(anything) -m b/b.txt   -> extract or insert manifest here
a/a.bps -m b/b.txt      -> extract manifest only
a/a.bps .               -> query database
-c . a/a.smc a/a.bps    -> pick best match from database
-c . a/a.smc            -> -c . a/a.smc a/a.bps
-c a/a.smc              -> -c . a/a.smc
(null)                  -> launch gui
a/a.bps                 -> launch patch wizard
anything else           -> error
flips-c a/a.smc         -> -c . a/a.smc a/a.bps

--db                 -> print database
--db a/a.smc b/b.smc -> add to database
--db -a/a.smc        -> remove from database
a/a.smc --db         -> error
-a --db              -> error

-a --apply can handle IPS, UPS or BPS
-c --create can handle IPS or BPS (delta creator only, remove moremem)
-m --manifest -i --info - exists (if there's a manifest but -m is not present, print that)
-s --silent - don't print anything on success, also silence BPS create progress (but do print on failure)
-h -? --help -v --version - exists
--ips --bps - removed, use the correct extensions
--ignore-checksum - allow applying patch to wrong files (DANGEROUS)

replace --exact:
--inhead=512 - discard 512 leading bytes in the infile before sending to patcher
--patchhead=512 - prepend 512 leading 00s before patching, discard afterwards
--outhead=512 - prepend 512 leading 00s after patching
--head=512 - set both inhead and outhead
all three default to 0
if both inhead and patchhead are nonzero, replace the leading min(inhead,patchhead) 00s with data from infile; same for outhead
if none are set, insize modulo 32768 is 512, and file extension is sfc or smc, set in/outhead to 512 and patchhead to 0
if the above happened and patching fails, retry with all headers 0; if success, throw a warning, else return error for unheadered file

database:
on successful BPS application, add without asking, even if that size/sum is already known (but don't add the same file twice, of course)
if file disappears or its checksum changes, delete and try another, if any
on failed application, or successful IPS or UPS application, do nothing
~/.config/flips.cfg
#Floating IPS configuration
#Version 2.00
database crc32=a31bead4 size=524800 path=/home/alcaro/smw.smc
database crc32=b19ed489 size=524288 path=/home/alcaro/smw.smc # SMCs have two entries
database crc32=b19ed489 size=524288 path=/home/alcaro/smw.sfc # duplicates are fine
assoc-target=ask # or auto or auto-exec
create-show-all=true # affects whether Create Patch (GUI) defaults to all files, or only common ROMs; both in and out
create-auto-source=true # if source rom can't be found, asks for that after target

auto pick source rom:
 load first 1MB from source
 for each file in database, except duplicates:
  load first 1MB
  check how many bytes are the same
 if exactly one file has >1/8 matching, and the rest are <1/32, use that
 otherwise error
 behind off-by-default flag

GUI is same as Flips 1.31, except
- no IPS creation
- publish size/checksum of infile info in failure message
- no manifests
- replace settings window with list of db files (including add/delete buttons), and assoc-target config (don't allow disabling db)
- rewrite using Arlib
which means these limits compared to CLI:
- can't create IPS
- no manifests

patch wizard:
first, get infile from DB or user
then, depending on config.assoc, one of:
 ask user for outfile path (default, default to default path)
 write outfile to default path
 write outfile to default path; suppress success message (but not warnings), instead launch outfile using default OS associations
  don't touch stdin/stdout
   on Windows, launcher is GUI and doesn't care; ignore stdio
   on Linux from CLI, act as if running launcher directly; ignore stdio
   on Linux from GUI, stdio are devnull and can safely be ignored
   on every platform is 'do nothing' a valid choice
  don't clean up outfile, let user do that; user is looking at default path, he'll find it

bps spec:
http://wayback.archive.org/web/20110911111128/http://byuu.org/programming/bps/
*/

static void a_apply()
{
	
}

class flipsargs {
public:
	enum { m_default, m_apply, m_create, m_info, m_db } mode = m_default;
	array<string> fnames;
	
	string manifest;
	bool silent = false;
	bool ignorechecksum = false;
	
	bool autohead = true;
	size_t inhead = 0;
	size_t patchhead = 0;
	size_t outhead = 0;
	
#ifdef ARLIB_TEST
	string errormsg;
#endif
	
	void usage(cstring error)
	{
#ifndef ARLIB_TEST
		window_attach_console();
		if (error) puts("error: "+error);
		puts(R"(command line usage:
flips -a|--apply a.bps b.smc [c.sfc]
 apply patch; default output file is a.smc
flips -c|--create a.smc b.smc [c.bps]
 create patch; default output file is b.bps
flips -i|--info a.bps
 print some information about this patch, such as its expected input file
with only filenames, guess either --apply or --create
bps, ips and ups patches can be applied, bps and ips can be created

database:
 if the source file has been used before, it can be shortened to . (bps only)
 usable both when creating and applying
the database can be manipulated with
flips --db
 print list of known inputs
flips --db [-]a.smc [[-]b.smc]
 add or remove files from database

additional options:
-m foo.xml or --manifest=foo.xml: extract or insert bps manifest here
-s --silent: remain silent on success
--ignore-checksum: allow applying a bps patch to wrong input file
--inhead=512: discard the first 512 bytes of the input file
--patchhead=512: prepend 512 bytes before patching, discard afterwards
--outhead=512: prepend 512 bytes to the patch output
--head=512: shortcut for --inhead=512 --outhead=512
prepended bytes are either copied from a previous header, or 00)");
		exit(error ? 1 : 0);
#else
		if (!errormsg) errormsg = error;
#endif
	}
	
	//returns whether 'next' was used
	//calls usage() if arg is unknown
	bool parse(cstring arg, cstring next)
	{
		if(0);
		else if (arg=="help") usage("");
		else if (arg=="version")
		{
			window_attach_console();
			puts("?");
			exit(0);
		}
		else if (arg=="apply") mode=m_apply;
		else if (arg=="create") mode=m_create;
		else if (arg=="info") mode=m_info;
		else if (arg=="db") mode=m_db;
		else if (arg=="manifest")
		{
			manifest=next;
			return true;
		}
		else if (arg=="silent") silent=true;
		else if (arg=="ignorechecksum") ignorechecksum=true;
		else if (arg=="head" || arg=="inhead" || arg=="patchhead" || arg=="outhead")
		{
			size_t size;
			if (!fromstring(next, size)) usage("invalid argument to --"+arg);
			if (arg=="head") outhead=inhead=size;
			if (arg=="inhead") inhead=size;
			if (arg=="patchhead") patchhead=size;
			if (arg=="outhead") outhead=size;
			return true;
		}
		else usage("unknown option --"+arg);
		return false;
	}
	
	string longname(char arg)
	{
		switch (arg)
		{
		case 'h': return "help";
		case '?': return "help";
		case 'v': return "version";
		
		case 'a': return "apply";
		case 'c': return "create";
		case 'i': return "info";
		case 'm': return "manifest";
		case 's': return "silent";
		default: usage("unknown option -"+string(arrayview<char>(&arg, 1))); return "";
		}
	}
	
	//if -f is --foo, all of these yield arg="foo" next="bar":
	//-fbar | -f bar | --foo=bar
	//and these yield next="":
	//-f -b | -f --bar | -f | --foo bar
	void parse(const char * const * argv)
	{
		array<string> args;
		for (int i=1;argv[i];i++) args.append(argv[i]);
		
		for (size_t i=0;i<args.size();i++)
		{
			string arg = args[i];
			if (arg[0]=='-')
			{
				if (arg[1]=='-')
				{
					//long
					array<string> parts = arg.substr(2, ~0).split<1>("=");
					
					bool hasarg = (parts.size()==2);
					bool argused = parse(parts[0], parts[1]);
					if (hasarg && !argused) usage("--"+parts[0]+" does not take an argument");
				}
				else
				{
					//short
					string rest = arg.substr(1, ~0);
					
				next:
					char opt = rest[0];
					rest = rest.substr(1, ~0);
					
					if (rest)
					{
						bool argused = parse(longname(opt), rest);
						if (!argused) goto next;
					}
					else
					{
						bool argused = parse(longname(opt), args[i+1]);
						if (argused) i++;
					}
				}
			}
			else
			{
				//not option
				fnames.append(arg);
				continue;
			}
		}
	}
	
	void setmode()
	{
		
//a/a.bps b/b.smc         -> -a a/a.bps b/b.smc a/a.smc ("default path", patch path + patch basename + infile extension)
//a/a.smc b/b.bps         -> -a b/b.bps a/a.smc b/b.smc
//a/a.bps b/b.smc c/c.sfc -> -a a/a.bps b/b.smc c/c.sfc
//-c a/a.smc b/b.smc      -> -c a/a.smc b/b.smc b/b.bps
//a/a.smc b/b.smc c/c.bps -> -c a/a.smc b/b.smc c/c.bps
//a/a.smc b/b.smc         -> -c a/a.smc b/b.smc b/b.bps
//a/a.smc                 -> error
//a/a.bps b/b.bps         -> error
//(anything) -m b/b.txt   -> extract or insert manifest here
//a/a.bps -m b/b.txt      -> extract manifest only
//a/a.bps .               -> query database
//-c . b/b.smc b/b.bps    -> pick best match from database
//-c . b/b.smc            -> -c . b/b.smc b/b.bps
//-c b/b.smc              -> -c . b/b.smc b/b.bps
//(null)                  -> launch gui
//a/a.bps                 -> launch patch wizard
//anything else           -> error
	}
};

test()
{{
//macros are great to wipe out copypasted boilerplate
#define PARSE(x) } { const char * argv[] = { "flips", x, NULL }; flipsargs args; args.parse(argv)
	PARSE(NULL);
	assert_eq(args.errormsg, "");
	
	PARSE("--foo");
	assert(args.errormsg != "");
	
	PARSE("--apply");
	assert_eq(args.errormsg, "");
	assert(args.mode==flipsargs::m_apply);
	
	PARSE("-a");
	assert_eq(args.errormsg, "");
	assert(args.mode==flipsargs::m_apply);
	
	PARSE("-a", "-m", "foo.smc");
	assert_eq(args.errormsg, "");
	assert(args.mode==flipsargs::m_apply);
	assert_eq(args.manifest, "foo.smc");
	
	PARSE("-a", "--manifest=foo.smc");
	assert_eq(args.errormsg, "");
	assert(args.mode==flipsargs::m_apply);
	assert_eq(args.manifest, "foo.smc");
	
	PARSE("-a", "--manifest", "foo.smc");
	assert(args.errormsg != ""); // invalid way to specify --manifest
	
	PARSE("-am", "foo.smc");
	assert_eq(args.errormsg, "");
	assert(args.mode==flipsargs::m_apply);
	assert_eq(args.manifest, "foo.smc");
	
	PARSE("-amfoo.smc");
	assert_eq(args.errormsg, "");
	assert(args.mode==flipsargs::m_apply);
	assert_eq(args.manifest, "foo.smc");
#undef PARSE
}}

int main(int argc, char* argv[])
{
	bool guiavail = window_try_init(&argc, &argv);
	
	flipsargs args;
	args.parse(argv);
	
	if (guiavail)
	{
		window* wnd = window_create(
			widget_create_layout_grid(2,2, false,
				widget_create_button("Apply Patch")->set_onclick(bind(a_apply)),
				widget_create_button("Create Patch"),
				widget_create_button("Apply and Run"),
				widget_create_button("Settings")));
		wnd->set_title("Flips v" FLIPSVER);
		wnd->set_visible(true);
		while (wnd->is_visible()) window_run_wait();
		return 0;
	}
	
	return -1;
}
