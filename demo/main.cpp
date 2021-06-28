#include "widget.h"
#include "../crashhandler/crashhandlersetup.h"
#include <QApplication>
#include <QtDebug>

namespace {
const QString appName = "demo";
const QString executableDirPath = "";
}

void crash()
{
    int* a = nullptr;
    *a = 1;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    CrashHandlerSetup crashHandler(appName,
                                   CrashHandlerSetup::EnableRestart,
                                   executableDirPath);

    Widget w;
    w.resize(600, 400);
    w.show();

    return a.exec();
}
