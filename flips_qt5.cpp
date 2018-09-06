#include <QFile>

#include "flips_qt5.h"
#include "flips.h"

file* file::create(const char * filename) { return file::create_libc(filename); }
filewrite* filewrite::create(const char * filename) { return filewrite::create_libc(filename); }
filemap* filemap::create(const char * filename) { return filemap::create_fallback(filename); }

flips_qt5::flips_qt5(QObject *parent) : QObject(parent)
{

}

int flips_qt5::cli(int argc, char *argv[])
{
    return flipsmain(argc, argv);
}

bool flips_qt5::applyPatch(QString patchPath, bool exact, QString romPath, QString newRomPath)
{
    // Make a copy of the original ROM file
    // and rename it to the same name as the patch
    QFile::copy(romPath, newRomPath);

    // Create command
    QByteArray patchPathByteArray = patchPath.toLatin1();
    const char *c_patchPath = patchPathByteArray.data();

    QByteArray newRomPathByteArray = newRomPath.toLatin1();
    const char *c_newRomPath = newRomPathByteArray.data();

    std::vector<std::string> arguments;

    if(exact)
        arguments = {"--apply", "--exact", c_patchPath, c_newRomPath};
    else
        arguments = {"--apply", c_patchPath, c_newRomPath};

    // Create argv for CLI
    std::vector<char*> argv;
    for (const auto& arg : arguments)
        argv.push_back((char*)arg.data());
    argv.push_back(nullptr);

    // Run Command
    int result = flipsmain(argv.size() - 1, argv.data());

    // Return result
    if(result == 0)
      return true;
    else
      return false;
}

bool flips_qt5::createPatch(int patchType, bool exact, QString cleanRomPath, QString hackRomPath, QString newPatchPath)
{
    // Create command
    QByteArray cleanRomPathByteArray = cleanRomPath.toLatin1();
    const char *c_cleanRomPath = cleanRomPathByteArray.data();

    QByteArray hackRomPathByteArray = hackRomPath.toLatin1();
    const char *c_hackRomPath = hackRomPathByteArray.data();

    QByteArray newPatchPathByteArray = newPatchPath.toLatin1();
    const char *c_newPatchPath = newPatchPathByteArray.data();

    std::vector<std::string> arguments;

    switch (patchType) {
    case PatchType_BPS:
        if(exact)
            arguments = {"--create", "--exact", "--bps", c_cleanRomPath, c_hackRomPath, c_newPatchPath};
        else
            arguments = {"--create", "--bps", c_cleanRomPath, c_hackRomPath, c_newPatchPath};
        break;
    case PatchType_IPS:
        if(exact)
            arguments = {"--create", "--exact", "--ips", c_cleanRomPath, c_hackRomPath, c_newPatchPath};
        else
            arguments = {"--create", "--ips", c_cleanRomPath, c_hackRomPath, c_newPatchPath};
        break;
    }

    // Create argv for CLI
    std::vector<char*> argv;
    for (const auto& arg : arguments)
        argv.push_back((char*)arg.data());
    argv.push_back(nullptr);

    // Run Command
    int result = flipsmain(argv.size() - 1, argv.data());

    // Return result
    if(result == 0)
      return true;
    else
      return false;
}
