#include "gui/help/HelpAssistantService.h"

#include "config/RecipeJsonUtils.h"
#include "gui/help/HelpModelBootstrapper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <map>

namespace
{
constexpr int kMaxSources = 3;
constexpr qsizetype kMaxSourceChars = 900;
constexpr qsizetype kMaxMachineContextChars = 2200;
constexpr qsizetype kMaxConversationContextChars = 1200;

QString projectRoot()
{
  return RecipeJsonUtils::appRootPath();
}

QString normalizeText(QString text)
{
  text = text.toLower();
  text.replace(QChar(0x00E0), "a");
  text.replace(QChar(0x00E1), "a");
  text.replace(QChar(0x00E8), "e");
  text.replace(QChar(0x00E9), "e");
  text.replace(QChar(0x00EC), "i");
  text.replace(QChar(0x00ED), "i");
  text.replace(QChar(0x00F2), "o");
  text.replace(QChar(0x00F3), "o");
  text.replace(QChar(0x00F9), "u");
  text.replace(QChar(0x00FA), "u");
  return text;
}

QString trimmedContent(QString content)
{
  static const QRegularExpression blankLines("\n{3,}");
  content = content.trimmed().replace(blankLines, "\n\n");
  if (content.size() <= kMaxSourceChars)
  {
    return content;
  }
  return content.left(kMaxSourceChars).trimmed() + "\n...";
}
}

HelpAssistantService::HelpAssistantService(QObject* parent)
  : QObject(parent)
  , m_bootstrapper(new HelpModelBootstrapper(this))
{
}

bool HelpAssistantService::isBusy() const
{
  return m_busy || m_process != nullptr || (m_bootstrapper && m_bootstrapper->isBusy());
}

void HelpAssistantService::ask(const QString& question,
                               const QString& languageCode,
                               const QString& machineContext,
                               const QString& conversationContext,
                               ReplyHandler onReply,
                               ErrorHandler onError)
{
  if (isBusy())
  {
    onError(QStringLiteral("Assistente gia' occupato. Attendere la risposta corrente."));
    return;
  }

  HelpAssistantReply directReply;
  if (tryBuildDirectReply(question, conversationContext, directReply))
  {
    onReply(directReply);
    return;
  }

  const QList<SourceDocument> documents = loadDocuments(languageCode);
  if (documents.isEmpty())
  {
    onError(QStringLiteral("Nessun file help trovato in docs/help."));
    return;
  }

  const QList<SourceDocument> sources = selectSources(question, conversationContext, documents);
  if (sources.isEmpty())
  {
    onError(QStringLiteral("Nessun documento pertinente trovato. Prova con termini piu' specifici."));
    return;
  }

  const QString prompt = buildPrompt(question, machineContext, conversationContext, sources);
  m_busy = true;
  m_bootstrapper->ensureReady(
    [this, prompt, sources, onReply, onError]() {
      runPrompt(prompt, sources, onReply, onError);
    },
    [this, onError](const QString& error) {
      m_busy = false;
      onError(error);
    });
}

void HelpAssistantService::runPrompt(const QString& prompt,
                                     QList<SourceDocument> sources,
                                     ReplyHandler onReply,
                                     ErrorHandler onError)
{
  auto* process = new QProcess(this);
  m_process = process;

  connect(process, &QProcess::started, this, [process, prompt]() {
    process->write(prompt.toUtf8());
    process->closeWriteChannel();
  });

  connect(process, &QProcess::errorOccurred, this, [this, process, onError](QProcess::ProcessError error) {
    if (process != m_process)
    {
      return;
    }
    m_process = nullptr;
    m_busy = false;
    process->deleteLater();
    if (error == QProcess::FailedToStart)
    {
      onError(QStringLiteral("Ollama non e' disponibile nel PATH o non e' avviato."));
      return;
    }
    onError(QStringLiteral("Errore durante la chiamata a Ollama."));
  });

  connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, process, sources, onReply, onError](int exitCode, QProcess::ExitStatus exitStatus) {
    if (process != m_process)
    {
      return;
    }
    m_process = nullptr;
    m_busy = false;
    const QString stdoutText = cleanProcessOutput(QString::fromUtf8(process->readAllStandardOutput()));
    const QString stderrText = cleanProcessOutput(QString::fromUtf8(process->readAllStandardError()));
    process->deleteLater();

    if (exitStatus != QProcess::NormalExit || exitCode != 0)
    {
      QString message = stderrText.isEmpty() ? stdoutText : stderrText;
      if (message.contains("model", Qt::CaseInsensitive) && message.contains("not found", Qt::CaseInsensitive))
      {
        message = QStringLiteral("Modello Ollama 'vision-help' non trovato. Crearlo con: ollama create vision-help -f ollama/vision-help/Modelfile");
      }
      if (message.isEmpty())
      {
        message = QStringLiteral("Ollama non ha restituito una risposta valida.");
      }
      onError(message);
      return;
    }

    QStringList sourceNames;
    for (const SourceDocument& source : sources)
    {
      sourceNames.push_back(sourceName(source.path));
    }

    HelpAssistantReply reply;
    reply.answer = stdoutText.isEmpty() ? QStringLiteral("Nessuna risposta generata.") : stdoutText;
    reply.sources = sourceNames;
    onReply(reply);
  });

  process->setProgram(QStringLiteral("ollama"));
  process->setArguments({QStringLiteral("run"),
                         QStringLiteral("--nowordwrap"),
                         QStringLiteral("--think=false"),
                         QStringLiteral("--keepalive"),
                         QStringLiteral("30m"),
                         QStringLiteral("vision-help")});
  process->start();
}

