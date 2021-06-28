#include "utils.h"

#include <QFile>
#include <QDebug>

QByteArray fileContents(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Warning: Could not open '%s'.", qPrintable(filePath));
        return QByteArray();
    }

    return file.readAll();
}
