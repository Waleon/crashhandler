#include "crashhandlerdialog.h"
#include "crashhandler.h"
#include "ui_crashhandlerdialog.h"
#include "utils.h"

#include <QMessageBox>
#include <QClipboard>
#include <QIcon>
#include <QSettings>
#include <QStyle>

CrashHandlerDialog::CrashHandlerDialog(CrashHandler *handler,
                                       const QString &signalName,
                                       const QString &appName,
                                       QWidget *parent)
    : QDialog(parent)
    , m_crashHandler(handler)
    , m_ui(new Ui::CrashHandlerDialog)
{
    m_ui->setupUi(this);
    m_ui->introLabel->setTextFormat(Qt::RichText);
    m_ui->introLabel->setOpenExternalLinks(true);
    m_ui->debugInfoEdit->setReadOnly(true);
    m_ui->progressBar->setMinimum(0);
    m_ui->progressBar->setMaximum(0);
    m_ui->restartAppCheckBox->setText(tr("&Restart %1 on close").arg(appName));

    const QStyle * const style = QApplication::style();
    m_ui->closeButton->setIcon(style->standardIcon(QStyle::SP_DialogCloseButton));

    const int iconSize = style->pixelMetric(QStyle::PM_MessageBoxIconSize, 0);
    QIcon icon = style->standardIcon(QStyle::SP_MessageBoxCritical);
    m_ui->iconLabel->setPixmap(icon.pixmap(iconSize, iconSize));

    connect(m_ui->copyToClipBoardButton, &QAbstractButton::clicked,
            this, &CrashHandlerDialog::copyToClipboardClicked);
    connect(m_ui->reportBugButton, &QAbstractButton::clicked,
            m_crashHandler, &CrashHandler::openBugTracker);
    connect(m_ui->debugAppButton, &QAbstractButton::clicked,
            m_crashHandler, &CrashHandler::debugApplication);
    connect(m_ui->closeButton, &QAbstractButton::clicked, this, &CrashHandlerDialog::close);

    setApplicationInfo(signalName, appName);
}

CrashHandlerDialog::~CrashHandlerDialog()
{
    delete m_ui;
}

bool CrashHandlerDialog::runDebuggerWhileBacktraceNotFinished()
{
    // 询问用户
    const QString title = tr("Run Debugger And Abort Collecting Backtrace?");
    const QString message = tr(
                "<html><head/><body>"
                "<p><b>Run the debugger and abort collecting backtrace?</b></p>"
                "<p>You have requested to run the debugger while collecting the backtrace was not "
                "finished.</p>"
                "</body></html>");

    int result = QMessageBox::question(this,
                                       title,
                                       message,
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::Yes);

    return result == QMessageBox::Yes;
}

void CrashHandlerDialog::setToFinalState()
{
    m_ui->progressBar->hide();
    m_ui->copyToClipBoardButton->setEnabled(true);
    m_ui->reportBugButton->setEnabled(true);
}

void CrashHandlerDialog::disableRestartAppCheckBox()
{
    m_ui->restartAppCheckBox->setDisabled(true);
}

void CrashHandlerDialog::disableDebugAppButton()
{
    m_ui->debugAppButton->setDisabled(true);
}

void CrashHandlerDialog::setApplicationInfo(const QString &signalName, const QString &appName)
{
    const QString title = tr("%1 has closed unexpectedly (Signal \"%2\")").arg(appName, signalName);
    const QString introLabelContents = tr(
                "<p><b>%1.</b></p>"
                "<p>Please file a <a href='%2'>bug report</a> with the debug information provided below.</p>")
            .arg(title, QLatin1String(URL_BUGTRACKER));
    const QString versionInformation = tr("%1, based on Qt %2 (%3 bit)\n")
            .arg(appName,
                 QLatin1String(QT_VERSION_STR),
                 QString::number(QSysInfo::WordSize));

    setWindowTitle(title);
    m_ui->introLabel->setText(introLabelContents);
    m_ui->debugInfoEdit->append(versionInformation);
}

void CrashHandlerDialog::appendDebugInfo(const QString &chunk)
{
    m_ui->debugInfoEdit->append(chunk);
}

void CrashHandlerDialog::selectLineWithContents(const QString &text)
{
    // 移到最后
    QTextCursor cursor = m_ui->debugInfoEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_ui->debugInfoEdit->setTextCursor(cursor);

    // 通过反向搜索找到文本
    m_ui->debugInfoEdit->find(text, QTextDocument::FindCaseSensitively | QTextDocument::FindBackward);

    // 高亮整行
    cursor = m_ui->debugInfoEdit->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    m_ui->debugInfoEdit->setTextCursor(cursor);
}

void CrashHandlerDialog::copyToClipboardClicked()
{
    QApplication::clipboard()->setText(m_ui->debugInfoEdit->toPlainText());
}

void CrashHandlerDialog::close()
{
    if (m_ui->restartAppCheckBox->isEnabled() && m_ui->restartAppCheckBox->isChecked())
        m_crashHandler->restartApplication();

    QCoreApplication::quit();
}
