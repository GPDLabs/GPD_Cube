QT -= gui
QT += bluetooth serialport network core gui-private

CONFIG += c++11 console
CONFIG -= app_bundle


DEFINES += QT_DEPRECATED_WARNINGS


SOURCES += \
    cephes.c \
    checkversion.cpp \
    dfft.c \
    download.cpp \
    globalval.cpp \
    handleziptype.cpp \
    main.cpp \
    matrix.c \
    qrserver.cpp \
    sts.c

HEADERS += \
    checkversion.h \
    download.h \
    globalval.h \
    handleziptype.h \
    qrserver.h \
    sts.h

target.path = /home/quakey/qt_out
INSTALLS += target

DISTFILES += \
    libhash/Makefile

LIBS += -lwiringPi
