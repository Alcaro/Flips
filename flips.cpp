#include "flips.h"

/*
a/a.bps b/b.smc         -> -a a/a.bps b/b.smc a/a.smc ("default path", patch path + patch basename + infile extension)
a/a.smc b/b.bps         -> -a b/b.bps a/a.smc b/b.smc (if no console then auto-gui)
a/a.bps b/b.smc c/c.sfc -> -a a/a.bps b/b.smc c/c.sfc
-c a/a.smc b/b.smc      -> -c a/a.smc b/b.smc b/b.bps
a/a.smc b/b.smc c/c.bps -> -c a/a.smc b/b.smc c/c.bps
a/a.smc b/b.smc         -> -c a/a.smc b/b.smc b/b.bps (if no console then auto-gui, sorting files by mod date)
a/a.bps b/b.bps         -> error
(anything) -m b/b.txt   -> extract or insert manifest here
a/a.bps -m b/b.txt      -> extract manifest only
a/a.bps .               -> query database
-a a/a.bps              -> query database
. a/a.smc a/a.bps       -> pick best match from database
. a/a.smc               -> -c . a/a.smc a/a.bps
-c a/a.smc              -> -c . a/a.smc
(null)                  -> (gui) main window
a/a.bps                 -> (gui) apply patch
a/a.smc                 -> (gui) create patch, per assoc-can-create
anything else           -> error

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
if file disappears or its checksum changes, silently delete and try another, if any
on failed application, or successful IPS or UPS application, do nothing
if create-auto-source, creating a patch also adds source file to database
~/.config/flips.cfg
#Floating IPS configuration
#Version 2.00
database crc32=a31bead4 size=524800 path=/home/alcaro/smw.smc
database crc32=b19ed489 size=524288 path=/home/alcaro/smw.smc # SMCs have two entries
database crc32=b19ed489 size=524288 path=/home/alcaro/smw.sfc # duplicates are fine
assoc-apply-exec=false   # if true, dropping a bps on flips.exe will suppress success messages and instead launch emulator
assoc-can-create=unset   # true -> dropping rom on exe creates; false -> not a patch error; unset -> error but ask, with don't ask again
create-show-all=false    # affects whether Create Patch (GUI) defaults to all files, or only common ROMs; both in and out
create-auto-source=false # if source rom can't be found, asks for that after target

auto pick source rom:
 load first 1MB from source
 for each file in database, except duplicates:
  load first 1MB
  check how many bytes are the same
 if exactly one file has >1/3 matching, and the rest are <1/16, use that
 otherwise error
 behind off-by-default flag

GUI limits compared to CLI:
- can't create IPS
- no manifests
- checksum is mandatory

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

class flipsargs {
public:
	//most of these are set by parse()
	//flipsfile::{f, exists} are set by fillfiles
	enum mode_t { m_default, m_apply, m_create, m_info, m_db } mode = m_default;
	bool canusegui;        // window_try_init
	bool usegui;           // !window_console_avail && no other args
	bool forcegui = false; // --gui
	
	struct flipsfile {
		string path;
		file f;
		enum { t_auto, t_patch, t_file } type;
	};
	array<flipsfile> files;
	
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
	
	void error(cstring error)
	{
#ifndef ARLIB_TEST
		window_attach_console();
		puts("error: "+error);
		exit(1);
#else
		if (!errormsg) errormsg = "error: "+error;
#endif
	}
	
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
--gui: use GUI even if launched from command line
--inhead=512: discard the first 512 bytes of the input file
--patchhead=512: prepend 512 bytes before patching, discard afterwards
--outhead=512: prepend 512 bytes to the patch output
--head=512: shortcut for --inhead=512 --outhead=512
prepended bytes are either copied from a previous header, or 00)");
		exit(error ? 1 : 0);
#else
		if (!errormsg) errormsg = "error: "+error;
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
			window_console_attach();
			puts("Flips v" FLIPSVER);
			exit(0);
		}
		else if (arg=="apply") mode=m_apply;
		else if (arg=="create") mode=m_create;
		else if (arg=="info") mode=m_info;
		else if (arg=="db") error("--db must be first if present");
		else if (arg=="manifest")
		{
			manifest=next;
			return true;
		}
		else if (arg=="silent") silent=true;
		else if (arg=="ignorechecksum") ignorechecksum=true;
		else if (arg=="gui") forcegui=true;
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
		
		if (args[0]=="--db")
		{
			mode = m_db;
			for (size_t i=1;i<args.size();i++)
			{
				files[i-1].path = args[i];
			}
			return;
		}
		
		for (size_t i=0;i<args.size();i++)
		{
			string arg = args[i];
			if (arg[0]=='-')
			{
				usegui = false;
				
				if (arg[1]=='-')
				{
					//long
					array<string> parts = arg.substr(2, ~0).split<1>("=");
					
					bool hasarg = (parts.size()==2);
					bool argused = parse(parts[0], parts[1]);
					//this breaks for optional arguments, but I don't have any of those
					if (hasarg && !argused) usage("--"+parts[0]+" does not take an argument");
					if (!hasarg && argused) usage("--"+parts[0]+" requires an argument");
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
						bool hasarg = (args.size() > i+1 && args[i+1][0]!='-');
						bool argused = parse(longname(opt), (hasarg ? args[i+1] : ""));
						if (!hasarg && argused) usage("--"+longname(opt)+" requires an argument");
						if (argused) i++;
					}
				}
			}
			else
			{
				//not option
				files.append().path = arg;
				continue;
			}
		}
	}
	
	void setfiles()
	{
		if (forcegui) usegui=true;
		if (!canusegui) usegui=false;
		if (forcegui && !canusegui) error("gui requested but not available");
		
		if (files.size() > 3) usage("too many files");
		for (flipsfile& f : files)
		{
			if (f.path==".") f.type = flipsfile::t_auto;
			else
			{
				f.f.open(f.path);
				
				bool ispatch;
				if (f.f) ispatch = (patch::identify(f.f) != patch::t_unknown);
				else ispatch = (patch::identify_ext(f.path) != patch::t_unknown);
				
				if (ispatch) f.type = flipsfile::t_patch;
				else f.type = flipsfile::t_file;
			}
		}
		
		if (!usegui && files.size()==0) usage("");
		if (usegui && files.size() > 2) usage("too many files");
		if (mode == m_default)
		{
			if (files.size()>=2 && files[0].type!=flipsfile::t_patch && files[1].type==flipsfile::t_file) mode = m_create;
			else if (usegui && files.size()==2 && (files[0].type==flipsfile::t_file || files[1].type==flipsfile::t_file)) mode = m_create;
			else mode = m_apply;
		}
		if (mode == m_apply)
		{
			if (files.size()==1) files.append().path = ".";
			if (files.size()==2) files.append().path = ".";
			if (files[1].type==flipsfile::t_patch && files[0].type!=flipsfile::t_patch)
			{
				std::swap(files[0], files[1]);
			}
			if (files[0].type != flipsfile::t_patch) usage("unknown patch format");
			
			//applying a patch to a patch requires explicit output filename
			//applying a patch to an auto can't have a patch as output
			bool haspatchpatch = (files[1].type==flipsfile::t_patch || files[2].type==flipsfile::t_patch);
			bool hasauto       = (files[1].type==flipsfile::t_auto  || files[2].type==flipsfile::t_auto);
			if (haspatchpatch && hasauto) usage("attempt to apply a patch to a patch\n if you want that, give three filenames");
		}
		if (mode == m_create)
		{
			if (files.size()==1) files.insert(0).path = ".";
			if (files.size()==2) files.append().path = ".";
			if (files[1].type==flipsfile::t_auto) usage("can't auto detect target file for patch");
			
			//creating a patch using patches as input requires three filenames
			bool haspatchsrc = (files[0].type==flipsfile::t_patch || files[1].type==flipsfile::t_patch);
			bool hasauto = (files[0].type==flipsfile::t_auto || files[1].type==flipsfile::t_auto || files[2].type==flipsfile::t_auto);
			if (haspatchsrc && hasauto) usage("attempt to create a patch that applies to a patch\n if you want that, give three filenames");
			//if (hasauto && files[1].type!=flipsfile::t_file) usage("moo");
			if (files[2].type==flipsfile::t_file) usage("unknown patch format");
			
			if (usegui)
			{
				error("fixme: sort files by date");
			}
		}
	}
	
	void execute()
	{
		
	}
};

test("flipsargs::parse")
{{
//macros are great for wiping out copypasted boilerplate
#define PARSE_BASE(...) } { const char * argv[] = { "flips", __VA_ARGS__, NULL }; flipsargs args; args.parse(argv)
#define PARSE(...) PARSE_BASE(__VA_ARGS__); assert_eq(args.errormsg, "")
#define PARSE_ERR(...) PARSE_BASE(__VA_ARGS__); assert(args.errormsg != "")
	PARSE(NULL);
	
	PARSE_ERR("--foo");
	
	PARSE("--apply");
	assert_eq(args.mode, flipsargs::m_apply);
	
	PARSE("-a");
	assert_eq(args.mode, flipsargs::m_apply);
	
	PARSE("-a", "-m", "foo.smc");
	assert_eq(args.mode, flipsargs::m_apply);
	assert_eq(args.manifest, "foo.smc");
	
	PARSE("-a", "--manifest=foo.smc");
	assert_eq(args.mode, flipsargs::m_apply);
	assert_eq(args.manifest, "foo.smc");
	
	PARSE_ERR("-a", "--manifest", "foo.smc"); // --manifest doesn't work that way
	
	PARSE("-am", "foo.smc");
	assert_eq(args.mode, flipsargs::m_apply);
	assert_eq(args.manifest, "foo.smc");
	
	PARSE("-amfoo.smc");
	assert_eq(args.mode, flipsargs::m_apply);
	assert_eq(args.manifest, "foo.smc");
#undef PARSE_BASE
#undef PARSE
#undef PARSE_ERR
}}

static void testsetfiles(int numfiles, int fnames, char exp_a, char exp_c)
{
	static const char * fnamebase[]={".", "a.bps", "a.smc"};
	const char * f1 = fnamebase[fnames/3/3%3];
	const char * f2 = fnamebase[fnames/3%3];
	const char * f3 = fnamebase[fnames%3];
	
	for (int pass=0;pass<3;pass++)
	{
		flipsargs args;
		args.canusegui=false;
		if (numfiles>=3) args.files.append().path = f1;
		if (numfiles>=2) args.files.append().path = f2;
		if (numfiles>=1) args.files.append().path = f3;
		args.mode = (flipsargs::mode_t)pass;
		args.setfiles();
		
		int result;
		if (args.errormsg!="") result = 0;
		else
		{
			assert(args.mode != flipsargs::m_default);
			result = args.mode;
		}
		
		if (pass==flipsargs::m_default)
		{
			if (result==0 && (exp_a=='A' || exp_c=='C')) goto fail;
			if (exp_a=='A' && result!=flipsargs::m_apply) goto fail;
			if (exp_c=='C' && result!=flipsargs::m_create) goto fail;
		}
		if (pass==flipsargs::m_apply)
		{
			if (result==0 && exp_a!='-') goto fail;
			if (result!=0 && exp_a=='-') goto fail;
		}
		if (pass==flipsargs::m_create)
		{
			if (result==0 && exp_c!='-') goto fail;
			if (result!=0 && exp_c=='-') goto fail;
		}
		continue;
		
	fail:
		string exp_s;
		exp_s[0]=exp_a;
		exp_s[1]=exp_c;
		if (numfiles<3) f1="";
		if (numfiles<2) f2="";
		if (numfiles<1) f3="";
		string err = (string)f1+" "+f2+" "+f3+" pass "+tostring(pass)+" exp "+exp_s+": unexpected ";
		if (result!=0) err += tostring(result);
		if (result==0) err += args.errormsg;
		assert_fail_nostack(err);
	}
}

test("flipsargs::setfiles")
{
	//each combination is 2 chars; first is apply, second is create
	//- = reject this, ac = allow if -a/-c is given, AC = auto select this mode
	static const char * expected1 =
		"--A--c" // auto, patch, rom
		;
	static const char * expected2 =
		"--A--C" // auto,  *
		"A---A-" // patch, *
		"--A--C" // rom,   *
		;
	static const char * expected3 =
		"------" // auto,  auto,  *
		"A---A-" // auto,  patch, *
		"-C-C--" // auto,  rom,   *
		"A---A-" // patch, auto,  *
		"--aca-" // patch, patch, *
		"A-acA-" // patch, rom,   *
		"------" // rom,   auto,  *
		"A-acA-" // rom,   patch, *
		"-C-C--" // rom,   rom,   *
		;
	for (int i=0;i<3    ;i++) testcall(testsetfiles(1, i, expected1[i*2], expected1[i*2+1]));
	for (int i=0;i<3*3  ;i++) testcall(testsetfiles(2, i, expected2[i*2], expected2[i*2+1]));
	for (int i=0;i<3*3*3;i++) testcall(testsetfiles(3, i, expected3[i*2], expected3[i*2+1]));
}

int main(int argc, char* argv[])
{
	bool guiavail = window_try_init(&argc, &argv);
	
	flipsargs args;
	args.canusegui = guiavail;
	args.usegui = !window_console_avail();
	args.parse(argv);
	if (args.mode == flipsargs::m_db)
	{
		//args.executedb();
	}
	//args.setfiles();
	//args.querydb();
	//args.execute();
	
	//if (guiavail)
	//{
	//	window* wnd = window_create(
	//		widget_create_layout_grid(2,2, false,
	//			widget_create_button("Apply Patch")->set_onclick(bind(a_apply)),
	//			widget_create_button("Create Patch"),
	//			widget_create_button("Apply and Run"),
	//			widget_create_button("Settings")));
	//	wnd->set_title("Flips v" FLIPSVER);
	//	wnd->set_visible(true);
	//	while (wnd->is_visible()) window_run_wait();
	//	return 0;
	//}
	
	return -1;
}
