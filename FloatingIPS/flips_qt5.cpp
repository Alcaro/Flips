#include <QFile>

#include "flips_qt5.h"

file* file::create(const char * filename) { return file::create_libc(filename); }
filewrite* filewrite::create(const char * filename) { return filewrite::create_libc(filename); }
filemap* filemap::create(const char * filename) { return filemap::create_fallback(filename); }

flips_qt5::flips_qt5(QObject *parent) : QObject(parent)
{

}

char * flips_qt5::toCharStr(QString qStr) const
{
    QByteArray qStrByteArray = qStr.toLatin1();
    char * c_qStr = qStrByteArray.data();
    return c_qStr;
}

int flips_qt5::cli(int argc, char *argv[])
{
    return flipsmain(argc, argv);
}

QVariantMap flips_qt5::applyPatch(QString patchPath, QString romPath, QString newRomPath)
{
    // Set some defaults
    bool ignoreChecksum = true;

    struct manifestinfo manifestInfo;
    manifestInfo.use = false;
    manifestInfo.required = false;
    manifestInfo.name = nullptr;

    // Convert parameter string(s)
    QByteArray patchPathByteArray = patchPath.toLatin1();
    LPCWSTR c_patchPath = patchPathByteArray.data();

    // Run patch application
    struct errorinfo errInf = ApplyPatch(
                c_patchPath, //Not sure why this paramter is unable to accept toCharStr method output
                toCharStr(romPath),
                ignoreChecksum,
                toCharStr(newRomPath),
                &manifestInfo,
                false);

    // Return result
    QVariantMap map;
    map.insert("success", (errInf.level == 0));
    map.insert("errorMessage", errInf.description);
    return map;
}

QVariantMap flips_qt5::createPatch(int patchTypeIndex, QString cleanRomPath, QString hackRomPath, QString newPatchPath)
{
    // Set some defaults
    struct manifestinfo manifestInfo;
    manifestInfo.use = false;
    manifestInfo.required = false;
    manifestInfo.name = nullptr;

    // Convert parameter string(s)
    QByteArray cleanRomPathByteArray = cleanRomPath.toLatin1();
    const char *c_cleanRomPath = cleanRomPathByteArray.data();

    QByteArray hackRomPathByteArray = hackRomPath.toLatin1();
    const char *c_hackRomPath = hackRomPathByteArray.data();

    QByteArray newPatchPathByteArray = newPatchPath.toLatin1();
    const char *c_newPatchPath = newPatchPathByteArray.data();

    // Set patch type
    enum patchtype createThisPatchType;
    switch(patchTypeIndex){
    case 0:
        createThisPatchType = ty_bps;
        break;
    case 1:
        createThisPatchType = ty_ips;
        break;
    case 2:
        createThisPatchType = ty_ups;
        break;
    default:
        createThisPatchType = ty_bps;
        break;
    }

    // Run patch creation
    struct errorinfo errInf = CreatePatch(
                c_cleanRomPath,
                c_hackRomPath,
                createThisPatchType,
                &manifestInfo,
                c_newPatchPath
                );

    // Return result
    QVariantMap map;
    map.insert("success", (errInf.level == 0));
    map.insert("errorMessage", errInf.description);
    return map;
}
