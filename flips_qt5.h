#ifndef FLIPS_QT5_H
#define FLIPS_QT5_H

#include <QObject>
#include <QVariant>
#include <QVariantMap>

#include "flips.h"

class flips_qt5 : public QObject
{
    Q_OBJECT
public:
    explicit flips_qt5(QObject *parent = nullptr);

private:
    char * toCharStr(QString qStr) const;

signals:

public slots:
    int cli(int argc, char * argv[]);
    QVariantMap applyPatch(QString patchPath, QString romPath, QString newRomPath);
    QVariantMap createPatch(int patchTypeIndex, QString cleanRomPath, QString hackRomPath, QString newPatchPath);
};

#endif // FLIPS_QT5_H
