#ifndef FLIPS_QT5_H
#define FLIPS_QT5_H

#include <QObject>

class flips_qt5 : public QObject
{
    Q_OBJECT
public:
    explicit flips_qt5(QObject *parent = nullptr);

    enum PatchType { PatchType_BPS = 0, PatchType_IPS };

signals:

public slots:
    int cli(int argc, char * argv[]);
    bool applyPatch(QString patchPath, bool exact, QString romPath, QString newRomPath);
    bool createPatch(int patchType, bool exact, QString cleanRomPath, QString hackRomPath, QString newPatchPath);
};

#endif // FLIPS_QT5_H
