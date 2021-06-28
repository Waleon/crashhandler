QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = demo
TEMPLATE = app
DESTDIR = $$OUT_PWD/../bin/

CONFIG += c++11

#CONFIG += force_debug_info

HEADERS += \
        widget.h \
        $$PWD/../crashhandler/crashhandlersetup.h

SOURCES += \
        main.cpp \
        widget.cpp \
        $$PWD/../crashhandler/crashhandlersetup.cpp
