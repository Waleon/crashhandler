#include "crashhandler.h"
#include "crashhandlerdialog.h"
#include "backtracecollector.h"
#include "utils.h"

#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QTextStream>
#include <QUrl>
#include <QVector>

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

namespace {
const QString fileDistroInformation = "/etc/lsb-release";
const QString fileKernelVersion = "/proc/version";
}

static QString collectLinuxDistributionInfo()
{
    return QString::fromLatin1(fileContents(fileDistroInformation));
}

static QString collectKernelVersionInfo()
{
    return QString::fromLatin1(fileContents(fileKernelVersion));
}

// 方便与 exec() 函数族交互的类
class CExecList : public QVector<char *>
{
public:
    CExecList(const QStringList &list)
    {
        foreach (const QString &item, list)
            append(qstrdup(item.toLatin1().data()));
        append(0);
    }

    ~CExecList()
    {
        for (int i = 0; i < size(); ++i)
            delete[] value(i);
    }
};

class CrashHandlerPrivate
{
public:
    CrashHandlerPrivate(pid_t pid,
                        const QString &signalName,
                        const QString &appName,
                        CrashHandler *crashHandler)
        : pid(pid)
        , dialog(crashHandler, signalName, appName) {}

    const pid_t pid;
    const QString creatorInPath; // 备份 debugger

    BacktraceCollector backtraceCollector;
    CrashHandlerDialog dialog;

    QStringList restartAppCommandLine;
    QStringList restartAppEnvironment;
};

CrashHandler::CrashHandler(pid_t pid,
                           const QString &signalName,
                           const QString &appName,
                           RestartCapability restartCap,
                           QObject *parent)
    : QObject(parent)
    , d_ptr(new CrashHandlerPrivate(pid, signalName, appName, this))
{
    Q_D(CrashHandler);

    connect(&d->backtraceCollector, &BacktraceCollector::error, this, &CrashHandler::onError);
    connect(&d->backtraceCollector, &BacktraceCollector::backtraceChunk, this, &CrashHandler::onBacktraceChunk);
    connect(&d->backtraceCollector, &BacktraceCollector::backtrace, this, &CrashHandler::onBacktraceFinished);

    d->dialog.appendDebugInfo(collectKernelVersionInfo());
    d->dialog.appendDebugInfo(collectLinuxDistributionInfo());

    if (restartCap == DisableRestart || !collectRestartAppData()) {
        d->dialog.disableRestartAppCheckBox();
        if (d->creatorInPath.isEmpty())
            d->dialog.disableDebugAppButton();
    }

    d->dialog.show();
}

CrashHandler::~CrashHandler()
{

}

void CrashHandler::run()
{
    Q_D(CrashHandler);

    d->backtraceCollector.run(d->pid);
}

void CrashHandler::onError(const QString &errorMessage)
{
    Q_D(CrashHandler);

    d->dialog.setToFinalState();

    QTextStream(stderr) << errorMessage;
    const QString text = QLatin1String("A problem occurred providing the backtrace. "
                                       "Please make sure to have the debugger \"gdb\" installed.\n");
    d->dialog.appendDebugInfo(text);
    d->dialog.appendDebugInfo(errorMessage);
}

void CrashHandler::onBacktraceChunk(const QString &chunk)
{
    Q_D(CrashHandler);

    d->dialog.appendDebugInfo(chunk);
}

void CrashHandler::onBacktraceFinished(const QString &backtrace)
{
    Q_D(CrashHandler);

    d->dialog.setToFinalState();

    // 选择相关线程的第一行
    QRegExp rx("\\[Current thread is (\\d+)");
    const int pos = rx.indexIn(backtrace);
    if (pos == -1)
        return;

    const QString threadNumber = rx.cap(1);
    const QString textToSelect = QString("Thread %1").arg(threadNumber);
    d->dialog.selectLineWithContents(textToSelect);
}

void CrashHandler::openBugTracker()
{
    QDesktopServices::openUrl(QUrl(QLatin1String(URL_BUGTRACKER)));
}

bool CrashHandler::collectRestartAppData()
{
    Q_D(CrashHandler);

    const QString procDir = QString("/proc/%1").arg(d->pid);

    // 根据进程号获取进程的命令行参数，常规的做法是读取 /proc/{PID}/cmdline，
    // 并用 '\0' 分割其中的字符串得到进程的 args[]，最后一个字符串后面还有一个空字节。
    const QString procCmdFileName = procDir + "/cmdline";
    QList<QByteArray> commandLine = fileContents(procCmdFileName).split('\0');
    if (commandLine.size() < 2) {
        qWarning("%s: Unexpected format in file '%s'.\n", Q_FUNC_INFO, qPrintable(procCmdFileName));
        return false;
    }
    commandLine.removeLast();
    foreach (const QByteArray &item, commandLine)
        d->restartAppCommandLine.append(QString::fromLatin1(item));

    // /proc/[pid]/environ 包含了可用进程环境变量的列表，
    // 条目由空字节('\0')分隔，末尾可能有一个空字节。
    const QString procEnvFileName = procDir + "/environ";
    QList<QByteArray> environment = fileContents(procEnvFileName).split('\0');
    if (environment.isEmpty()) {
        qWarning("%s: Unexpected format in file '%s'.\n", Q_FUNC_INFO, qPrintable(procEnvFileName));
        return false;
    }
    if (environment.last().isEmpty())
        environment.removeLast();

    foreach (const QByteArray &item, environment)
        d->restartAppEnvironment.append(QString::fromLatin1(item));

    return true;
}

void CrashHandler::runCommand(QStringList commandLine, QStringList environment, WaitMode waitMode)
{
    // TODO: QTBUG-2284 QProcess::startDetached 不支持为新进程设置环境。
    // 如果解决了这个 Bug，就可以正常使用它了。因此，这里先使用 fork-exec。

    pid_t pid = fork();
    switch (pid) {
    case -1: // error
        qFatal("%s: fork() failed.", Q_FUNC_INFO);
        break;
    case 0: { // child
        CExecList argv(commandLine);
        CExecList envp(environment);
        qDebug("Running\n");
        for (int i = 0; argv[i]; ++i)
            qDebug("   %s", argv[i]);
        if (!environment.isEmpty()) {
            qDebug("\nwith environment:\n");
            for (int i = 0; envp[i]; ++i)
                qDebug("   %s", envp[i]);
        }

        // 标准管道必须是打开的，否则一旦使用了这些标准管道，应用程序就会收到一个 SIGPIPE。
        if (freopen("/dev/null", "r", stdin) == 0)
            qFatal("%s: freopen() failed for stdin: %s.\n", Q_FUNC_INFO, strerror(errno));
        if (freopen("/dev/null", "w", stdout) == 0)
            qFatal("%s: freopen() failed for stdout: %s.\n", Q_FUNC_INFO, strerror(errno));
        if (freopen("/dev/null", "w", stderr) == 0)
            qFatal("%s: freopen() failed for stderr: %s.\n.", Q_FUNC_INFO, strerror(errno));

        if (environment.isEmpty())
            execvp(argv[0], argv.data());
        else
            execvpe(argv[0], argv.data(), envp.data());
        _exit(EXIT_FAILURE);
    } default: // parent
        if (waitMode == WaitForExit) {
            while (true) {
                int status;
                if (waitpid(pid, &status, 0) == -1) {
                    if (errno == EINTR) //  用于 SIGCHLD 的 QProcess 信号处理程序被触发
                        continue;
                    perror("waitpid() failed unexpectedly");
                }
                if (WIFEXITED(status)) {
                    qDebug("Child exited with exit code %d.", WEXITSTATUS(status));
                    break;
                } else if (WIFSIGNALED(status)) {
                    qDebug("Child terminated by signal %d.", WTERMSIG(status));
                    break;
                }
            }
        }
        break;
    }
}

void CrashHandler::restartApplication()
{
    Q_D(CrashHandler);

    runCommand(d->restartAppCommandLine, d->restartAppEnvironment, DontWaitForExit);
}

void CrashHandler::debugApplication()
{
    Q_D(CrashHandler);

    // 当调试器正在运行时，用户请求调试应用程序。
    if (d->backtraceCollector.isRunning()) {
        if (!d->dialog.runDebuggerWhileBacktraceNotFinished())
            return;
        if (d->backtraceCollector.isRunning()) {
            d->backtraceCollector.disconnect();
            d->backtraceCollector.kill();
            d->dialog.setToFinalState();
            d->dialog.appendDebugInfo(tr("\n\nCollecting backtrace aborted by user."));
            QCoreApplication::processEvents(); // 立即显示最后附加的输出
        }
    }

    // 准备命令
    QString executable = d->creatorInPath;
    if (executable.isEmpty() && !d->restartAppCommandLine.isEmpty())
        executable = d->restartAppCommandLine.at(0);
    const QStringList commandLine = QStringList({executable, "-debug", QString::number(d->pid)});
    QStringList environment;
    if (!d->restartAppEnvironment.isEmpty())
        environment = d->restartAppEnvironment;

    // UI 无论如何都是阻塞/冻结的，所以在调试时隐藏对话框。
    d->dialog.hide();
    runCommand(commandLine, environment, WaitForExit);
    d->dialog.show();
}
