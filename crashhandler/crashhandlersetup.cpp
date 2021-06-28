#include "crashhandlersetup.h"

#include <QtGlobal>

#if defined(Q_OS_LINUX) // && !defined(QT_NO_DEBUG)
#define BUILD_CRASH_HANDLER
#endif

#ifdef BUILD_CRASH_HANDLER

#include <QApplication>
#include <QString>

#include <stdlib.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/prctl.h>

// 旧库中没有 PR_SET_PTRACER，所以需要定义一下，在下面会用到。
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif

#ifdef Q_WS_X11
#include <qx11info_x11.h>
#include <X11/Xlib.h>
#endif

static const char *appNameC = nullptr;
static const char *disableRestartOptionC = nullptr;
static const char *crashHandlerPathC = nullptr;
static void *signalHandlerStack = nullptr;

namespace {
const QString execName = "crashhandler";
}

extern "C" void signalHandler(int signal)
{
#ifdef Q_WS_X11
    // Kill window since it's frozen anyway.
    if (QX11Info::display())
        close(ConnectionNumber(QX11Info::display()));
#endif
    pid_t pid = fork();
    switch (pid) {
    case -1: // error
        break;
    case 0: // child
        if (disableRestartOptionC) {
            execl(crashHandlerPathC, crashHandlerPathC, strsignal(signal), appNameC,
                  disableRestartOptionC, nullptr);
        } else {
            execl(crashHandlerPathC, crashHandlerPathC, strsignal(signal), appNameC, nullptr);
        }
        _exit(EXIT_FAILURE);
    default: // parent
        prctl(PR_SET_PTRACER, pid, 0, 0, 0);
        waitpid(pid, nullptr, 0);
        _exit(EXIT_FAILURE);
        break;
    }
}
#endif // BUILD_CRASH_HANDLER

CrashHandlerSetup::CrashHandlerSetup(const QString &appName,
                                     RestartCapability restartCap,
                                     const QString &executableDirPath)
{
#ifdef BUILD_CRASH_HANDLER
    appNameC = qstrdup(qPrintable(appName));

    if (restartCap == DisableRestart)
        disableRestartOptionC = "--disable-restart";

    const QString execDirPath = executableDirPath.isEmpty()
            ? QCoreApplication::applicationDirPath()
            : executableDirPath;
    const QString crashHandlerPath = execDirPath + "/" + execName;
    crashHandlerPathC = qstrdup(qPrintable(crashHandlerPath));

    // 为信号处理程序设置一个替代堆栈，这样就可以处理 SIGSEGV 了，即使正常的进程堆栈已经耗尽。
    stack_t ss;
    ss.ss_sp = signalHandlerStack = malloc(SIGSTKSZ);
    if (ss.ss_sp == nullptr) {
        qWarning("Warning: Could not allocate space for alternative signal stack (%s).", Q_FUNC_INFO);
        return;
    }
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, nullptr) == -1) {
        qWarning("Warning: Failed to set alternative signal stack (%s).", Q_FUNC_INFO);
        return;
    }

    // 安装信号处理程序来调用崩溃处理程序
    struct sigaction sa;
    if (sigemptyset(&sa.sa_mask) == -1) {
        qWarning("Warning: Failed to empty signal set (%s).", Q_FUNC_INFO);
        return;
    }
    sa.sa_handler = &signalHandler;
    // SA_RESETHAND - 在信号处理程序被调用后，将信号动作恢复为默认值
    // SA_NODEFER - 在信号被触发后不要阻塞它（否则阻塞信号将通过 fork() 和 execve() 继承），没有信号将不能重启主程序。
    // SA_ONSTACK - 使用替代堆栈
    sa.sa_flags = SA_RESETHAND | SA_NODEFER | SA_ONSTACK;

    // 不要在这里添加 SIGPIPE, QProcess 和 QTcpSocket 使用它。
    const int signalsToHandle[] = {SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGBUS, 0};
    for (int i = 0; signalsToHandle[i]; ++i) {
        if (sigaction(signalsToHandle[i], &sa, nullptr) == -1 ) {
            qWarning("Warning: Failed to install signal handler for signal \"%s\" (%s).",
                strsignal(signalsToHandle[i]), Q_FUNC_INFO);
        }
    }
#else
    Q_UNUSED(appName);
    Q_UNUSED(restartCap);
    Q_UNUSED(executableDirPath);
#endif // BUILD_CRASH_HANDLER
}

CrashHandlerSetup::~CrashHandlerSetup()
{
#ifdef BUILD_CRASH_HANDLER
    delete[] crashHandlerPathC;
    delete[] appNameC;
    free(signalHandlerStack);
#endif
}
