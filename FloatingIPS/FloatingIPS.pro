# Set binary and data targets for all platforms
linux {
    isEmpty(PREFIX) {
        PREFIX = /usr/local
    }
    BINDIR = $$PREFIX/bin
    DATADIR = $$PREFIX/share
}


macx {
    isEmpty(PREFIX) {
        PREFIX = ~
    }
    BINDIR = $$PREFIX/Applications
    DATADIR = $$PREFIX/Library/Application\ Support/FloatingIPS
}

# Configure compiler
CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 4): QT += quick

TARGET = FloatingIPS
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS += \
    crc32.h \
    flips.h \
    libbps.h \
    libips.h \
    libups.h \
    global.h \
    divsufsort.h \
    flips_qt5.h

SOURCES += \
        main.cpp \
    crc32.cpp \
    flips.cpp \
    libbps.cpp \
    libips.cpp \
    libups.cpp \
    libbps-suf.cpp \
    divsufsort.c \
    flips_qt5.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
DISTFILES +=

linux {
    appdata.files = ../dist/linux/FloatingIPS.appdata.xml
    appdata.path = $$DATADIR/appdata/

    desktop.files = ../dist/linux/FloatingIPS.desktop
    desktop.path = $$DATADIR/applications/

    pixmap.files = ../dist/linux/FloatingIPS.png
    pixmap.path = $$DATADIR/pixmaps/

    INSTALLS += appdata desktop pixmap
}

macx {
    ICON = ../dist/macos/FloatingIPS.icns
}

windows {
    RC_FILE = ../dist/windows/FloatingIPS.ico
}

target.path = $$BINDIR
INSTALLS += target
