Floating IPS (or Flips) is a patcher for IPS and BPS files, aiming for a simple interface yet plenty of power under the hood.

Features:
- Creates the smallest BPS patches of any known tool in existence, and is faster than any other comparable creator
- Creates the smallest IPS patches of any known tool in existence, too (but only a little smaller, Lunar IPS is quite good)
- Fully-featured GUIs under Windows and GTK+, including command line support; can also be a pure command line program
- Can apply multiple patches to the same ROM, creating a huge pile of ROMs (GUI only)
- Can remembers which ROMs you've used, and use them again if it thinks it's correct (BPS only, GUI only)
- Can launch other programs after patching the ROMs; together with the above, this allows you to double click a BPS to launch an emulator (GUI only)

Floating IPS is in maintenance mode. Bug reports and pull requests will be processed, but feature requests are unlikely to be implemented. There will be no formal releases within any forseeable future; users should consider every commit a release.

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
- Install [mingw-w64](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/8.1.0/threads-win32/seh/), or similar
- `mingw32-make CFLAGS=-O3` (GUI is automatically enabled), or extract the appropriate optimization commands from make.sh
- Alternatively, [here's a binary](https://www.smwcentral.net/?p=section&a=details&id=11474) (though it's quite outdated).

Usage:
- If GUI is enabled, just run the program. File pickers' window titles tell what they want; Enable automatic ROM picker and Run in Emulator can cause file pickers to appear and disappear depending on the chosen patch, making usage less predictable, so it's disabled by default.
- For CLI use, see `./flips --help`. Available both with or without GUI support (though Windows CLI use from interactive prompts is slightly screwy, due to OS limitations).
