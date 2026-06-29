#include "gui/help/HelpModelBootstrapper.h"

#include "config/RecipeJsonUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include <utility>

namespace
{
QString sourceRoot()
{
  return RecipeJsonUtils::appRootPath();
}
}

HelpModelBootstrapper::HelpModelBootstrapper(QObject* parent)
  : QObject(parent)
{
}

bool HelpModelBootstrapper::isBusy() const
{
  return m_process != nullptr;
}

void HelpModelBootstrapper::ensureReady(ReadyHandler onReady, ErrorHandler onError)
{
  if (m_ready)
  {
    onReady();
    return;
  }
  if (isBusy())
  {
    onError(QStringLiteral("Preparazione di Ollama gia' in corso. Attendere qualche secondo."));
    return;
  }

  m_onReady = std::move(onReady);
  m_onError = std::move(onError);
  m_startAttempts = 0;

  if (ollamaProgram().isEmpty())
  {
    fail(QStringLiteral("Ollama non e' installato o non e' disponibile nel PATH."));
    return;
  }

  checkModels();
}

void HelpModelBootstrapper::checkModels()
{
  auto* process = new QProcess(this);
  m_process = process;
  connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
    if (process != m_process)
    {
      return;
    }
    m_process = nullptr;
    const QString output = cleanOutput(QString::fromUtf8(process->readAllStandardOutput())) + QStringLiteral("\n") +
                           cleanOutput(QString::fromUtf8(process->readAllStandardError()));
    process->deleteLater();

    if (exitStatus != QProcess::NormalExit || exitCode != 0)
    {
      if (m_startAttempts == 0)
      {
        tryStartServer();
        return;
      }
      fail(QStringLiteral("Ollama e' installato, ma il servizio non risponde. Aprire Ollama o verificare 'ollama serve'."));
      return;
    }

    if (outputContainsModel(output))
    {
      finishReady();
      return;
    }

    createModel();
  });
  connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
    if (process != m_process)
    {
      return;
    }
    m_process = nullptr;
    process->deleteLater();
    if (error == QProcess::FailedToStart)
    {
      fail(QStringLiteral("Ollama non e' installato o non e' disponibile nel PATH."));
      return;
    }
    fail(QStringLiteral("Errore durante il controllo dei modelli Ollama."));
  });
  process->setProgram(ollamaProgram());
  process->setArguments({QStringLiteral("list")});
  process->start();
}

void HelpModelBootstrapper::createModel()
{
  const QString modelfile = modelfilePath();
  if (modelfile.isEmpty())
  {
    fail(QStringLiteral("Cartella modello vision-help non trovata. Copiare ollama/vision-help accanto all'eseguibile."));
    return;
  }

  auto* process = new QProcess(this);
  m_process = process;
  connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
    if (process != m_process)
    {
      return;
    }
    m_process = nullptr;
    const QString message = cleanOutput(QString::fromUtf8(process->readAllStandardError()));
    process->deleteLater();

    if (exitStatus != QProcess::NormalExit || exitCode != 0)
    {
      fail(message.isEmpty()
             ? QStringLiteral("Creazione del modello Ollama vision-help non riuscita.")
             : message);
      return;
    }
    finishReady();
  });
  process->setProgram(ollamaProgram());
  process->setArguments({QStringLiteral("create"), QStringLiteral("vision-help"), QStringLiteral("-f"), modelfile});
  process->start();
}

void HelpModelBootstrapper::tryStartServer()
{
  ++m_startAttempts;
  QProcess::startDetached(ollamaProgram(), {QStringLiteral("serve")});
  QTimer::singleShot(1800, this, [this]() {
    if (!m_ready && !isBusy())
    {
      checkModels();
    }
  });
}

void HelpModelBootstrapper::finishReady()
{
  m_ready = true;
  auto ready = std::move(m_onReady);
  m_onReady = {};
  m_onError = {};
  if (ready)
  {
    ready();
  }
}

void HelpModelBootstrapper::fail(const QString& message)
{
  auto error = std::move(m_onError);
  m_onReady = {};
  m_onError = {};
  if (error)
  {
    error(message);
  }
}

QString HelpModelBootstrapper::ollamaProgram() const
{
  return QStandardPaths::findExecutable(QStringLiteral("ollama"));
}

QString HelpModelBootstrapper::modelDirectory() const
{
  const QString sourceModelDir = QDir(sourceRoot()).filePath(QStringLiteral("ollama/vision-help"));
  if (QFileInfo::exists(QDir(sourceModelDir).filePath(QStringLiteral("Modelfile"))))
  {
    return sourceModelDir;
  }
  return {};
}

QString HelpModelBootstrapper::modelfilePath() const
{
  const QString dir = modelDirectory();
  return dir.isEmpty() ? QString() : QDir(dir).filePath(QStringLiteral("Modelfile"));
}

bool HelpModelBootstrapper::outputContainsModel(const QString& output) const
{
  static const QRegularExpression modelLine(QStringLiteral("(^|\\n)vision-help(:|\\s)"));
  return modelLine.match(output).hasMatch();
}

QString HelpModelBootstrapper::cleanOutput(const QString& text) const
{
  QString cleaned = text;
  static const QRegularExpression ansiPattern("\x1b\\[[0-?]*[ -/]*[@-~]");
  cleaned.remove(ansiPattern);
  return cleaned.trimmed();
}
