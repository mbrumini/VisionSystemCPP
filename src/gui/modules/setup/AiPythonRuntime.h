#pragma once

#include <QString>
#include <QStringList>

class QProcess;

QString aiProjectPath(const QString& relativePath);
QString aiPythonProgram();
QStringList aiPythonArguments(const QStringList& scriptArguments);
QStringList aiPythonUnbufferedArguments(const QStringList& scriptArguments);
QString aiPowerShellQuote(const QString& text);
void configureHiddenProcess(QProcess* process);
