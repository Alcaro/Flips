#include "arlib.h"
#include "flips.h"

/*
a/a.bps b/b.smc         -> -a a/a.bps b/b.smc a/a.smc ("default path", patch path + patch basename + infile extension)
a/a.smc b/b.bps         -> -a b/b.bps a/a.smc b/b.smc
a/a.bps b/b.smc c/c.sfc -> -a a/a.bps b/b.smc c/c.sfc
-c a/a.smc b/b.smc      -> -c a/a.smc b/b.smc b/b.bps
a/a.smc b/b.smc c/c.bps -> -c a/a.smc b/b.smc c/c.bps
a/a.smc                 -> error
a/a.smc b/b.smc         -> error
a/a.bps b/b.bps         -> error
a/a.bps -m b/b.txt      -> extract manifest only
a/a.bps $               -> query database
(null)                  -> launch gui
a/a.bps                 -> launch patch wizard
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
database crc32=b19ed489 size=524288 path=/home/alcaro/smw.sfc
database crc32=a31bead4 size=524800 path=/home/alcaro/smw.smc
assoc-target=ask # or auto or auto-exec
create-show-all=true # affects whether Create Patch (GUI) defaults to all files, or only common ROMs

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

//static void usage()
//{
//	//GUIClaimConsole();
//	puts(
//	// 12345678901234567890123456789012345678901234567890123456789012345678901234567890
//		"usage:\n"
//		"   "
//#ifndef FLIPS_CLI
//       "flips\n"
//		"or flips patch.ips\n"
//		"or "
//#endif
//		   "flips [--apply] [--exact] patch.bps rom.smc [outrom.smc]\n"
//		"or flips [--create] [--exact] [--bps | --bps-linear | --ips] clean.smc\n"
//		"  hack.smc [patch.bps]\n"
//#ifndef FLIPS_CLI
//		"(for scripting, only the latter two are sensible)\n"
//#endif
//		"(patch.ips is valid in all cases patch.bps is)\n"
//		"\n"
//	// 12345678901234567890123456789012345678901234567890123456789012345678901234567890
//		"options:\n"
//		"-a --apply: apply patch (default if given two arguments)\n"
//		"-c --create: create patch (default if given three arguments)\n"
//		"-I --info: BPSes contain information about input and output roms, print it\n"
//		//"  also estimates how much of the source file is retained\n"
//		//"  anything under 400 is fine, anything over 600 should be treated with suspicion\n"
//		"-i --ips, -b -B --bps --bps-delta, --bps-linear, --bps-delta-moremem:\n"
//		"  create this patch format instead of guessing based on file extension\n"
//		"  ignored when applying\n"
//		" bps formats:\n"
//		"  delta is the recommended one; it's a good balance between creation time and\n"
//		"    patch size\n"
//		"    -b and -B both refer to this, for backwards compatibility reasons\n"
//		"  delta-moremem is usually slightly faster than delta, but uses about twice\n"
//		"    as much memory; it gives identical patches to delta\n"
//		"  linear is the fastest, but tends to give pretty big patches\n"
//		"--exact: do not remove SMC headers when applying or creating a BPS patch\n"
//		"  (ignored for IPS)\n"
//		"--ignore-checksum: accept checksum mismatches when applying a BPS patch\n"
//		"-m or --manifest: emit or insert a manifest file as romname.bml\n"
//		"  (valid only for BPS)\n"
//		"-mfilename or --manifest=filename: emit or insert a manifest file exactly here\n"
//		"-h -? --help: show this information\n"
//		"-v --version: show application version\n"
//	// 12345678901234567890123456789012345678901234567890123456789012345678901234567890
//		);
//	exit(0);
//}

static void a_apply()
{
	
}

int main(int argc, char* argv[])
{
	window_init(&argc, &argv);
	window* wnd = window_create(
		widget_create_layout_grid(2,2, false,
			widget_create_button("Apply Patch")->set_onclick(bind(a_apply)),
			widget_create_button("Create Patch"),
			widget_create_button("Apply and Run"),
			widget_create_button("Settings")));
	wnd->set_title("Flips v" FLIPSVER);
	wnd->set_visible(true);
	while (wnd->is_visible()) window_run_wait();
}
