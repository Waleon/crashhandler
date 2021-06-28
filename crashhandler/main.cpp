#include "crashhandler.h"
#include "utils.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QProcess>
#include <QString>
#include <QStyle>
#include <QTextStream>

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
const QString applicationName = "Crash Handler";
const QString parentProcessName = "demo";
}

static void printErrorAndExit()
{
    QTextStream err(stderr);
    err << "This crash handler will be called by main program. "
                               "Do not call this manually.\n";
    exit(EXIT_FAILURE);
}

// 包含了正在进程中运行的程序链接
static bool isParentProcessValid(Q_PID pid)
{
    const QString executable = QFile::symLinkTarget(QString("/proc/%1/exe").arg(pid));

    return executable.contains(parentProcessName);
}

// 由崩溃的应用程序的信号处理程序调用
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(applicationName);
    app.setWindowIcon(QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical));

    // 解析参数
    QCommandLineParser parser;
    parser.addPositionalArgument("signal-name", QString());
    parser.addPositionalArgument("app-name", QString());
    const QCommandLineOption disableRestartOption("disable-restart");
    parser.addOption(disableRestartOption);
    parser.process(app);

    // 检查使用情况
    const QStringList positionalArguments = parser.positionalArguments();
    if (positionalArguments.size() != 2)
        printErrorAndExit();

    // 返回父进程标识
    Q_PID parentPid = getppid();
//    if (!isParentProcessValid(parentPid))
//        printErrorAndExit();

    // 运行
    const QString signalName = positionalArguments.at(0);
    const QString appName = positionalArguments.at(1);
    CrashHandler::RestartCapability restartCap = CrashHandler::EnableRestart;
    if (parser.isSet(disableRestartOption))
        restartCap = CrashHandler::DisableRestart;

    CrashHandler crashHandler(parentPid, signalName, appName, restartCap);
    crashHandler.run();

    return app.exec();
}
