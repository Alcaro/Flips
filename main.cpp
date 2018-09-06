#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "flips_qt5.h"

int main(int argc, char *argv[])
{
    //Cheack for commmand line arguments
    if(argc > 1){
        //Run program in console mode

        flips_qt5 flipsController;

        return flipsController.cli(argc, argv);
    } else {
        //Run program as gui app

        //Create application
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        QGuiApplication app(argc, argv);

        //Register Types
        qmlRegisterUncreatableType<flips_qt5>("FlipsLib", 1, 0, "flipsController",
                    QStringLiteral("flips_qt5 should not be created in QML"));

        //Create QML engine instance
        QQmlApplicationEngine engine;

        //Load context properties
        flips_qt5 flipsController;
        engine.rootContext()->setContextProperty(QStringLiteral("flipsController"), &flipsController);

        //Start QML engine with main qml file
        engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
        if (engine.rootObjects().isEmpty())
            return -1;

        return app.exec();
    }
}
