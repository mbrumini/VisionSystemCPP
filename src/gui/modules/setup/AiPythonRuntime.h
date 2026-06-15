#pragma once

#include <QString>
#include <QStringList>

QString aiProjectPath(const QString& relativePath);
QString aiPythonProgram();
QStringList aiPythonArguments(const QStringList& scriptArguments);
QStringList aiPythonUnbufferedArguments(const QStringList& scriptArguments);
QString aiPowerShellQuote(const QString& text);
