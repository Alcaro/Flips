Floating IPS (or Flips) is a patcher for IPS and BPS files, aiming for a simple interface yet plenty of power under the hood.

Features:
- Creates the smallest BPS patches of any known tool in existence, and is faster than any other comparable creator
- Creates the smallest IPS patches of any known tool in existence, too (but only a little smaller, Lunar IPS is quite good)
- Fully-featured GUIs under Windows and GTK+, including command line support; can also be a pure command line program
- Can apply multiple patches to the same ROM, creating a huge pile of ROMs (GUI only)
- Can remembers which ROMs you've used, and use them again if it thinks it's correct (BPS only, GUI only)
- Can launch other programs after patching the ROMs; together with the above, this allows you to double click a BPS to launch an emulator (GUI only)

Planned features:
- Better autodetection for CLI apply/create; if the first one is a patch, apply, else create
- Better support for ROM database in CLI; the -d flag, or DB enabling from the GUI (for pure-CLI, existence of flipsdb.bin file beside Flips), will store ROMs in the database; the filename "." will read from it, and "wrong ROM" will look for the right one (but not automatically use it)
- Use path of patch as output filename, not the ROM path
- Less automatic UI, so it's clearer what's happening
- Remove emulator chooser, use OS file associations instead
- Automatic source ROM chooser for creation

Not planned (feel free to fork Flips, I'll list it below; I don't have suitable hardware, so I won't be able to merge most of those):
- OSX GUI support; Flips/Windows works in Wine, and there are several other OSX patchers. I can't test them, so I can't endorse anything.
- Qt GUI support; my distro prefers GTK+, and all distros I've seen can run both GTK+ and Qt, not worth the effort (and QString's UTF-16 irritates me)
- Android/iOS/etc GUI support; I have no interest in phone gaming

Third-party forks, or separate tools, covering usecases this version doesn't (this only acknowledges their existence, and is not an endorsement; I haven't used most of them):
- [Floating IPS](https://github.com/Alcaro/Flips); the original Floating IPS, in case you're currently looking at a fork
- [MultiPatch](https://projects.sappharad.com/tools/multipatch.html), OSX, applies BPS/IPS/UPS/PPF/Xdelta/bsdiff/Ninja2, creates BPS/IPS/XDelta/bsdiff
- [QtFloatingIPS](https://github.com/covarianttensor/QtFloatingIPS), Flips port to OSX (may work on others too)
- [Wh0ba Floating IPS](https://wh0ba.github.io/repo/), Flips port to iOS/Cydia
- [CPS](https://media.smwcentral.net/Alcaro/bps/), JavaScript, applies BPS, creates nothing
- There are many tools that offer a strict subset of Flips functionality (Lunar IPS, beat, etc). I'm not listing them here.

Compilation - Linux (Debian/Ubuntu):
- `sudo apt-get install g++ build-essential`; for GUI support, also `sudo apt-get install libgtk-3-dev pkg-config`
- `./make.sh` to build an optimized binary; for development, use `make CFLAGS=-g` (ignore make-maintainer.sh)

Compilation - other Linux:
- Install make and g++; for GUI support, also install pkg-config and GTK+ 3 headers; for profiling support, also install curl or wget
- `./make.sh` to build an optimized binary; for development, use `make CFLAGS=-g` (ignore make-maintainer.sh)

Compilation - OSX, other Unix, or anything else with a C++ compiler:
- Install a C++ compiler
- `clang++ *.c *.cpp -O3 -o flips` (with changes as appropriate)
- For GTK GUI support, use some suitable variant of `clang++ *.c *.cpp -O3 -o flips -DFLIPS_GTK -lgtk3`
- For better optimizations (profiling/etc), extract the appropriate commands from Makefile and make.sh

Compilation - Windows:
- Install [mingw-w64](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/7.2.0/threads-win32/seh/)
- `mingw32-make CFLAGS=-O3` (GUI is automatically enabled), or extract the appropriate optimization commands from make.sh
- Alternatively, [here's a binary](https://www.smwcentral.net/?p=section&a=details&id=11474) (though it's slightly outdated).

Usage:
- If GUI is enabled, just run the program. File pickers' window titles tell what they want; Enable automatic ROM picker and Run in Emulator can cause file pickers to appear and disappear depending on the chosen patch, making usage less predictable, so it's disabled by default.
- For CLI use, see `./flips --help`. Available both with or without GUI support (though Windows CLI use from interactive prompts is slightly screwy, due to OS limitations).
