#pragma once

#include <QByteArray>
#include <QString>

const char URL_BUGTRACKER[] = "https://bugreports.qt.io/";

QByteArray fileContents(const QString &filePath);
