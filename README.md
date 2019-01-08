Floating IPS is a patcher for IPS and BPS files, aiming for a simple interface yet plenty of power under the hood.

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
- Replace Settings button with Advanced, which switches to a different UI
- Remove emulator chooser, use OS file associations instead
- Automatic source ROM chooser for creation

Not planned (if your plans are different, send a PR):
- OSX GUI support; there's a third-party fork [here](https://github.com/covarianttensor/QtFloatingIPS), and MultiPatch exists and the Windows version works in Wine. I can't test OSX stuff well enough to merge that fork, so pick your favorite.
- Qt GUI support; my distro prefers GTK+, and all distros I've seen can run both GTK+ and Qt, not worth the effort (and QString's UTF-16 irritates me)

Compilation - Linux (Debian/Ubuntu):
- `sudo apt-get install g++ build-essential`; for GUI support, also `sudo apt-get install libgtk-3-dev pkg-config`
- `./make.sh` to build an optimized binary; for development, use `make CFLAGS=-g`

Compilation - other Linux:
- Install make and g++; for GUI support, also install pkg-config and GTK+ 3 headers; for profiling support, also install curl or wget
- `./make.sh` to build an optimized binary; for development, use `make CFLAGS=-g`

Compilation - OSX and other Unix:
- Install a C++ compiler
- `clang++ *.c *.cpp -O3 -o flips` (with changes as appropriate)
- For GTK GUI support, use some suitable variant of `clang++ *.c *.cpp -O3 -o flips -DFLIPS_GTK -lgtk3`
- For better optimizations (profiling/etc), extract the appropriate commands from Makefile and make.sh

Compilation - Windows:
- Install [mingw-w64](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/7.2.0/threads-win32/seh/)
- `mingw32-make CFLAGS=-O3` (GUI is automatically enabled), or extract the appropriate optimization commands from make.sh
- Alternatively, [here's a binary](https://www.smwcentral.net/?p=section&a=details&id=11474).

Usage:
- If GUI is enabled, just run the program. File pickers' window titles tell what they want; Enable automatic ROM picker and Run in Emulator can cause file pickers to appear and disappear depending on the chosen patch, making usage less predictable, so it's disabled by default.
- For CLI use, see `./flips --help`. Available both with or without GUI support (though Windows CLI use from interactive prompts is slightly screwy, due to OS limitations).
