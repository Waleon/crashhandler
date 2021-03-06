#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
class QString;
namespace Ui { class CrashHandlerDialog; }
QT_END_NAMESPACE

class CrashHandler;

class CrashHandlerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CrashHandlerDialog(CrashHandler *handler,
                                const QString &signalName,
                                const QString &appName,
                                QWidget *parent = Q_NULLPTR);
    ~CrashHandlerDialog();

public:
    void setApplicationInfo(const QString &signalName, const QString &appName);
    void appendDebugInfo(const QString &chunk);
    void selectLineWithContents(const QString &text);
    void setToFinalState();
    void disableRestartAppCheckBox();
    void disableDebugAppButton();
    bool runDebuggerWhileBacktraceNotFinished();

private:
    void copyToClipboardClicked();
    void close();

    CrashHandler *m_crashHandler;
    Ui::CrashHandlerDialog *m_ui;
};
