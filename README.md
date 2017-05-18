Floating IPS is a patcher for IPS and BPS files, aiming for a simple interface yet plenty of power under the hood.

Features:
- Creates the smallest BPS patches of any known tool in existence, and is faster than any other comparable creator
- Creates the smallest IPS patches of any known tool in existence, too (but only a little smaller, Lunar IPS is quite good)
- Fully-featured GUIs under Windows and GTK+, including command line support; can also be a pure command line program
- Can apply multiple patches to the same ROM, creating a huge pile of ROMs (GUI only)
- Remembers which ROMs you've used, and will use them again if it thinks it's correct (BPS only, GUI only)
- Can launch other programs after patching the ROMs, allowing you to double click a BPS to launch an emulator (GUI only)

Planned:
- Automatically enable ROM guesser (open a message box the first time it guesses a ROM, asking if you want that)
- Different emulators for different filetypes (check the extension, or default to the OS default handler?)
- Better support for ROM database in CLI; the -d flag, or DB enabling from the GUI (for pure-CLI, existence of flipsdb.bin file beside Flips), will store ROMs in the database; the filename "." will read from it, and "wrong ROM" will look for the right one (but not automatically use it)
- Use path of patch as output filename, not the ROM path
- Better autodetection for command line; if the first two files have same extension, create, else apply

Not planned (if your plans are different, send a PR):
- OSX GUI support; I don't have the right hardware
- Qt GUI support; my distro prefers GTK+, and all distros I've seen can run both GTK+ and Qt, not worth the effort (and QString's UTF-16 irritates me)
