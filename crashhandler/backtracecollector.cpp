#include "backtracecollector.h"

#include <QDebug>
#include <QScopedPointer>
#include <QTemporaryFile>

const char GdbBatchCommands[] =
        "set height 0\n"
        "set width 0\n"
        "thread\n"
        "thread apply all backtrace full\n";

class BacktraceCollectorPrivate
{
public:
    BacktraceCollectorPrivate() {}

    bool errorOccurred = false;
    QScopedPointer<QTemporaryFile> commandFile;
    QProcess debugger;
    QString output;
};

BacktraceCollector::BacktraceCollector(QObject *parent)
    : QObject(parent)
    , d_ptr(new BacktraceCollectorPrivate)
{
    Q_D(BacktraceCollector);

    connect(&d->debugger, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &BacktraceCollector::onDebuggerFinished);
    connect(&d->debugger, &QProcess::errorOccurred, this, &BacktraceCollector::onDebuggerError);
    connect(&d->debugger, &QIODevice::readyRead, this, &BacktraceCollector::onDebuggerOutputAvailable);
    d->debugger.setProcessChannelMode(QProcess::MergedChannels);
}

BacktraceCollector::~BacktraceCollector()
{

}

void BacktraceCollector::run(Q_PID pid)
{
    Q_D(BacktraceCollector);

    d->debugger.start(QLatin1String("gdb"), QStringList({
        "--nw",    // Do not use a window interface.
        "--nx",    // Do not read .gdbinit file.
        "--batch", // Exit after processing options.
        "--command", createTemporaryCommandFile(), "--pid", QString::number(pid) })
    );
}

bool BacktraceCollector::isRunning() const
{
    Q_D(const BacktraceCollector);

    return d->debugger.state() == QProcess::Running;
}

void BacktraceCollector::kill()
{
    Q_D(BacktraceCollector);

    d->debugger.kill();
}

void BacktraceCollector::onDebuggerFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_D(BacktraceCollector);

    Q_UNUSED(exitStatus);

    if (d->errorOccurred) {
        emit error(QLatin1String("QProcess: ") + d->debugger.errorString());
        return;
    }

    if (exitCode != 0) {
        emit error(QString::fromLatin1("Debugger exited with code %1.").arg(exitCode));
        return;
    }

    emit backtrace(d->output);
}

void BacktraceCollector::onDebuggerError(QProcess::ProcessError error)
{
    Q_D(BacktraceCollector);

    Q_UNUSED(error);

    d->errorOccurred = true;
}

QString BacktraceCollector::createTemporaryCommandFile()
{
    Q_D(BacktraceCollector);

    d->commandFile.reset(new QTemporaryFile);

    if (!d->commandFile->open()) {
        emit error(QLatin1String("Error: Could not create temporary command file."));
        return QString();
    }

    if (d->commandFile->write(GdbBatchCommands) == -1) {
        emit error(QLatin1String("Error: Could not write temporary command file."));
        return QString();
    }

    d->commandFile->close();
    return d->commandFile->fileName();
}

void BacktraceCollector::onDebuggerOutputAvailable()
{
    Q_D(BacktraceCollector);

    const QString newChunk = QString::fromLocal8Bit(d->debugger.readAll());
    d->output.append(newChunk);

    emit backtraceChunk(newChunk);
}
