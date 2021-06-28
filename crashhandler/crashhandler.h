#pragma once

#include <QObject>

class ApplicationInfo;
class CrashHandlerPrivate;

class CrashHandler : public QObject
{
    Q_OBJECT

public:
    enum RestartCapability { EnableRestart, DisableRestart };

    explicit CrashHandler(pid_t pid,
                          const QString &signalName,
                          const QString &appName,
                          RestartCapability restartCap = EnableRestart,
                          QObject *parent = Q_NULLPTR);
    ~CrashHandler();

public Q_SLOTS:
    void run();
    void onError(const QString &errorMessage);
    void onBacktraceChunk(const QString &chunk);
    void onBacktraceFinished(const QString &backtrace);
    void openBugTracker();
    void restartApplication();
    void debugApplication();

private:
    enum WaitMode { WaitForExit, DontWaitForExit };

    bool collectRestartAppData();
    static void runCommand(QStringList commandLine, QStringList environment, WaitMode waitMode);

    QScopedPointer<CrashHandlerPrivate> d_ptr;
    Q_DECLARE_PRIVATE_D(d_ptr, CrashHandler)
};
