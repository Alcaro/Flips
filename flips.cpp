#include "arlib.h"
#include "flips.h"

static void a_apply()
{
	
}

/*
a/a.bps                 -> ask for in rom (opt), ask for out rom (opt), launch emu (opt)
a/a.bps b/b.smc         -> -a a/a.bps b/b.smc a/a.smc
a/a.smc b/b.bps         -> -a b/b.bps a/a.smc b/b.smc
a/a.bps b/b.smc c/c.sfc -> -a a/a.bps b/b.smc c/c.sfc
a/a.smc                 -> error
a/a.smc b/b.smc         -> error
a/a.smc b/b.smc c/c.bps -> -c a/a.smc b/b.smc c/c.bps
multiple patches -> error
anything else -> error

allow -a and -c for overriding this
allow applying IPS, UPS and BPS, but only create BPS in GUI; CLI supports everything (maybe an advanced mode that exposes everything?)
only use the delta creator; moremem is worthless, remove it

also need this new feature:
--romhead=512 - discard 512 leading bytes in the ROM before sending to patcher
--patchhead=512 - prepend 512 leading 00s before patching, discard afterwards
--outhead=512 - prepend 512 leading 00s after patching
if both romhead and patchhead are nonzero, replace the leading min(romhead,patchhead) 00s with data from the rom; same for outhead
if patchhead or romhead isn't set, it's 0
if outhead is not set, it's romhead
if none are set, romsize modulo 32768 is 512, and file extension is sfc or smc, set rom/outhead to 512 and patchhead to 0
if patching fails, retry with all headers 0 and throw a warning
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
