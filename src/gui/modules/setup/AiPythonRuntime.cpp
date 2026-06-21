#include "gui/modules/setup/AiPythonRuntime.h"

#include "config/RecipeJsonUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

QString aiProjectPath(const QString& relativePath)
{
  return RecipeJsonUtils::appPath(relativePath);
}

QString aiPythonProgram()
{
  const QString venvPython = aiProjectPath(".venv-ai/Scripts/python.exe");
  if (QFileInfo::exists(venvPython))
  {
    return QDir::toNativeSeparators(venvPython);
  }
  return "py";
}

QStringList aiPythonArguments(const QStringList& scriptArguments)
{
  if (aiPythonProgram() == "py")
  {
    QStringList args = {"-3.11"};
    args.append(scriptArguments);
    return args;
  }
  return scriptArguments;
}

QStringList aiPythonUnbufferedArguments(const QStringList& scriptArguments)
{
  if (aiPythonProgram() == "py")
  {
    QStringList args = {"-3.11", "-u"};
    args.append(scriptArguments);
    return args;
  }

  QStringList args = {"-u"};
  args.append(scriptArguments);
  return args;
}

QString aiPowerShellQuote(const QString& text)
{
  QString escaped = QDir::toNativeSeparators(text);
  escaped.replace("'", "''");
  return "'" + escaped + "'";
}

void configureHiddenProcess(QProcess* process)
{
#ifdef Q_OS_WIN
  if (!process)
  {
    return;
  }
  process->setCreateProcessArgumentsModifier(
    [](QProcess::CreateProcessArguments* arguments) {
      arguments->flags |= CREATE_NO_WINDOW;
      arguments->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
      arguments->startupInfo->wShowWindow = SW_HIDE;
    });
#else
  Q_UNUSED(process);
#endif
}
