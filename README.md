Floating IPS is a patcher for IPS and BPS files, aiming for a simple interface yet plenty of power under the hood.

Features:
- Creates the smallest BPS patches of any known tool in existence, and is faster than any other comparable creator
- Creates the smallest IPS patches of any known tool in existence, too (but only yields tiny gains over Lunar IPS)
- Fully-featured GUIs under Windows and GTK+, including command line support; can also be a pure command line program
- Can apply multiple patches to the same ROM, and creates a huge pile of ROMs
- Remembers which ROMs you've used, and will use them again if it thinks it's correct (BPS only)
- Can launch emulators after patching the ROMs (combined with the above, this allows you to double click a BPS to launch its ROM)

Planned:
- Automatically enable ROM guesser (open a message box the first time it guesses a ROM, asking if you want that)
- Different emulators for different filetypes (check the extension)
- Better support for ROM database in CLI; the -d flag, or DB enabling from the GUI (for pure-CLI, existence of flipsdb.bin file beside Flips), will store ROMs in the database; the filename "-" will read from it, and "wrong ROM" will look for the right one (but not automatically use it)

Not planned (if your plans are different, send a PR):
- Non-console OSX support; I don't have the right hardware
- Qt support; my distro uses GNOME, and all distros I've seen can run both GTK+ and Qt (and QString's UTF16 irritates me)
