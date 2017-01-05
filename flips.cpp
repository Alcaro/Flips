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


class flipscfg {
	class smcwrap : public file::implrd {
		file inner;
	public:
		smcwrap(file f) : inner(std::move(f)) {}
		
		size_t size() { return inner.size()-512; }
		size_t read(arrayvieww<byte> target, size_t start) { return inner.read(target, start+512); }
		arrayview<byte> mmap(size_t start, size_t len) { return inner.mmap(start+512, len); }
		void unmap(arrayview<byte> data) { inner.unmap(data); }
	};
	
	struct dbentry {
		string path;
		size_t size;
		uint32_t crc32;
		
		template<typename T> void serialize(T& s)
		{
			s.hex("crc32", crc32);
			s("size", size);
			s("path", path);
		}
	};
	array<dbentry> database;
	
public:
	bool assoc_apply_exec;
	bool assoc_can_create; // TODO: maybe<bool>
	bool create_show_all;
	bool create_auto_source;
	
	template<typename T> void serialize(T& s)
	{
		s.comment("Floating IPS configuration");
		s.comment("Version " FLIPSVER);
		s("database", database);
		s("assoc-apply-exec", assoc_apply_exec);
		s("assoc-can-create", assoc_can_create);
		s("create-show-all", create_show_all);
		s("create-auto-source", create_auto_source);
	}
	
	string findrombycrc(size_t size, uint32_t crc32)
	{
		for (const dbentry& e : database)
		{
			if (e.size==size && e.crc32==crc32) return e.path;
		}
		return "";
	}
	
	void addfile(cstring path, size_t size, uint32_t crc32)
	{
		for (const dbentry& e : database)
		{
			if (e.path==path && e.size==size && e.crc32==crc32) return;
		}
		dbentry& e = database.append();
		e.path=path;
		e.size=size;
		e.crc32=crc32;
	}
	
	void addfile(cstring path)
	{
		file f;
		if (!f.open(path)) return;
		arrayview<byte> bytes = f.mmap();
		addfile(path, bytes.size(), crc32(bytes));
		f.unmap(bytes);
	}
	
	void removefile(cstring path)
	{
		for (size_t i=0;i<database.size();i++)
		{
			if (database[i].path == path)
			{
				database.remove(i);
				i--;
			}
		}
	}
	
	
	static file fopen_smc(cstring path)
	{
		file f;
		f.open(path);
		if ((path.endswith(".sfc") || path.endswith(".smc")) && f.size()%32768 == 512)
		{
			return file::wrap(new smcwrap(std::move(f)));
		}
		else return f;
	}
	
	//usable only if in/patch/outhead are default or 0/0/0
	//f must be unheadered
	string findsimilarfile(const file& f, bool smcheader)
	{
		size_t matchsize = min(f.size(), 1024*1024);
		autommap bytesmatch(f, 0, matchsize);
		arrayview<uint32_t> match = bytesmatch.cast<uint32_t>();
		
		size_t idxfound = -1;
		for (size_t i=0;i<database.size();i++)
		{
			//multiple copies of same file? ignore that
			if (idxfound != (size_t)-1 &&
			    database[i].size  == database[idxfound].size &&
			    database[i].crc32 == database[idxfound].crc32)
			{
				continue;
			}
			
			file dbf;
			if (smcheader) dbf = fopen_smc(database[i].path);
			else dbf.open(database[i].path);
			
			if (!dbf)
			{
				//can't access file? then discard it
				database.remove(i);
				i--;
				continue;
			}
			
			size_t thismatchsize = min(dbf.size(), matchsize);
			if (database[i].size != dbf.size()) continue; // file messed up, or smc header found when told to ignore that
			if (thismatchsize < 4096) continue; // too small to be reliable
			
			autommap dbbytesmatch(dbf, 0, thismatchsize);
			arrayview<uint32_t> dbmatch = dbbytesmatch.cast<uint32_t>();
			
			thismatchsize /= sizeof(uint32_t);
			size_t nummatch = 0;
			for (size_t j=0;j<thismatchsize;j++)
			{
				if (match[j]==dbmatch[j]) nummatch++;
			}
			
			//in my tests, the right clean rom is ~70% match, while incorrect files are ~2.5% (probably mostly long strings of 00 or FF)
//puts(tostring(i)+":"+database[i].path+":"+tostring((float)nummatch/thismatchsize));
			if ((float)nummatch/thismatchsize < 1.0/8) continue; // irrelevant file
			if ((float)nummatch/thismatchsize > 1.0/3) // this looks good
			{
				if (idxfound != (size_t)-1) return ""; // multiple matches, abort mission
				idxfound = i;
			}
		}
		if (idxfound != (size_t)-1) return database[idxfound].path;
		else return "";
	}
};


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
		window_console_attach();
		puts("error: "+error);
		exit(1);
#else
		if (!errormsg) errormsg = "error: "+error;
#endif
	}
	
	void usage(cstring error)
	{
#ifndef ARLIB_TEST
		window_console_attach();
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
		else if (arg=="apply") mode = m_apply;
		else if (arg=="create") mode = m_create;
		else if (arg=="info") mode = m_info;
		else if (arg=="db") error("--db must be first if present");
		else if (arg=="manifest")
		{
			manifest=next;
			return true;
		}
		else if (arg=="silent") silent = true;
		else if (arg=="ignorechecksum") ignorechecksum = true;
		else if (arg=="gui") forcegui = true;
		else if (arg=="head" || arg=="inhead" || arg=="patchhead" || arg=="outhead")
		{
			autohead = false;
			size_t size;
			if (!fromstring(next, size)) usage("invalid argument to --"+arg);
			if (arg=="head") outhead = inhead = size;
			if (arg=="inhead") inhead = size;
			if (arg=="patchhead") patchhead = size;
			if (arg=="outhead") outhead = size;
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
				if (f.f) ispatch = (patch::identify(f.f) != patch::ty_unknown);
				else ispatch = (patch::identify_ext(f.path) != patch::ty_unknown);
				
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
	
	void fillautos(flipscfg& cfg)
	{
		if (mode == m_apply) // patch, in, out
		{
			if (files[1].type==flipsfile::t_auto)
			{
				autommap bytes(files[0].f);
				string foundpath;
				string expectedrom;
				switch (patch::identify(bytes))
				{
				case patch::ty_bps:
				{
					patch::bps::info inf;
					if (inf.parse(bytes) != patch::e_ok) break; // broken patch is unlikely, just throw a generic error
					foundpath = findrombycrc(inf.size_in, inf.crc_in);
					if (!foundpath) expectedrom="size "+tostring(inf.size_in)+" crc32 "+tostring(inf.crc_in);
				}
				default: ;
				}
				if (foundpath)
				{
					files[1].path = foundpath;
					files[1].type = flipsfile::t_file;
				}
				else if (!usegui)
				{
					if (expectedrom) error("can't find input file, nothing in database matching "+expectedrom);
					else error("missing input file");
				}
			}
			if (files[2].type==flipsfile::t_auto)
			{
				string dir = file::dirname(files[0].path);
				array<string> parts = file::dirname(files[0].path);
			}
		}
	}
	
	void execute()
	{
		
	}
};

#ifndef ARLIB_TEST
int main(int argc, char* argv[])
{
	bool guiavail = window_try_init(&argc, &argv);
	
	file cfgf;
	cfgf.open(window_config_path()+"flips.cfg", file::m_write);
	flipscfg cfg = bmlunserialize<flipscfg>(string(cfgf.read()));
	
	flipsargs args;
	args.canusegui = guiavail;
	args.usegui = !window_console_avail();
	args.parse(argv);
	if (args.mode == flipsargs::m_db)
	{
		//args.executedb(cfg);
	}
	args.setfiles();
	args.fillautos(cfg);
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
#endif
