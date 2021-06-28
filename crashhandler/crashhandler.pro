QT += core gui widgets

TARGET = crashhandler
TEMPLATE = app
DESTDIR = $$OUT_PWD/../bin/

HEADERS += \
    backtracecollector.h \
    crashhandlerdialog.h \
    crashhandler.h \
    utils.h

SOURCES += \
    main.cpp \
    backtracecollector.cpp \
    crashhandlerdialog.cpp \
    crashhandler.cpp \
    utils.cpp

FORMS += \
    crashhandlerdialog.ui
