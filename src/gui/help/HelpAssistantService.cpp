#include "gui/help/HelpAssistantService.h"

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
  return QString::fromUtf8(PROJECT_SOURCE_DIR);
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
{
}

bool HelpAssistantService::isBusy() const
{
  return m_process != nullptr;
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

QList<HelpAssistantService::SourceDocument> HelpAssistantService::loadDocuments(const QString& languageCode) const
{
  QString language = languageCode.isEmpty() ? QStringLiteral("it") : languageCode;
  if (language == m_cachedLanguageCode && !m_cachedDocuments.isEmpty())
  {
    return m_cachedDocuments;
  }

  QDir dir(QDir(projectRoot()).filePath(QStringLiteral("docs/help/%1/functions").arg(language)));
  if (!dir.exists() && language != QStringLiteral("it"))
  {
    dir = QDir(QDir(projectRoot()).filePath(QStringLiteral("docs/help/it/functions")));
    language = QStringLiteral("it");
  }

  QList<SourceDocument> documents;
  const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.txt")}, QDir::Files, QDir::Name);
  for (const QFileInfo& info : files)
  {
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      continue;
    }

    QTextStream stream(&file);
    SourceDocument document;
    document.path = info.absoluteFilePath();
    document.content = stream.readAll().trimmed();
    document.title = info.completeBaseName().replace("_", " ");

    const QStringList lines = document.content.split('\n');
    for (const QString& line : lines.mid(0, 8))
    {
      if (line.toLower().startsWith(QStringLiteral("titolo:")))
      {
        document.title = line.section(':', 1).trimmed();
        break;
      }
    }

    document.tokens = tokenize(document.title + "\n" + document.content);
    if (!document.tokens.isEmpty())
    {
      documents.push_back(document);
    }
  }
  m_cachedLanguageCode = language;
  m_cachedDocuments = documents;
  return documents;
}

QList<HelpAssistantService::SourceDocument> HelpAssistantService::selectSources(const QString& question,
                                                                                const QString& conversationContext,
                                                                                const QList<SourceDocument>& documents) const
{
  const QString retrievalText = conversationContext.trimmed().isEmpty()
                                  ? question
                                  : QStringLiteral("Domanda corrente: %1\nDomanda corrente: %1\n%2").arg(question, conversationContext);
  const QStringList queryTokens = expandQueryTokens(retrievalText, tokenize(retrievalText));
  if (queryTokens.isEmpty())
  {
    return {};
  }

  std::map<QString, int> documentFrequency;
  for (const SourceDocument& document : documents)
  {
    const QSet<QString> uniqueTokens(document.tokens.begin(), document.tokens.end());
    for (const QString& token : uniqueTokens)
    {
      ++documentFrequency[token];
    }
  }

  struct ScoredDocument
  {
    double score = 0.0;
    SourceDocument document;
  };

  QList<ScoredDocument> scored;
  for (const SourceDocument& document : documents)
  {
    std::map<QString, int> counts;
    for (const QString& token : document.tokens)
    {
      ++counts[token];
    }

    double score = 0.0;
    const QString title = normalizeText(document.title);
    for (const QString& token : queryTokens)
    {
      const auto countIt = counts.find(token);
      if (countIt == counts.end())
      {
        continue;
      }

      const int frequency = std::max(1, documentFrequency[token]);
      const double idf = std::log((1.0 + documents.size()) / (1.0 + frequency)) + 1.0;
      const double titleBoost = title.contains(token) ? 2.2 : 1.0;
      score += (1.0 + std::log(countIt->second)) * idf * titleBoost;
    }

    if (score > 0.0)
    {
      scored.push_back({score, document});
    }
  }

  std::sort(scored.begin(), scored.end(), [](const ScoredDocument& a, const ScoredDocument& b) {
    return a.score > b.score;
  });

  QList<SourceDocument> sources;
  const double bestScore = scored.isEmpty() ? 0.0 : scored.front().score;
  for (const ScoredDocument& item : scored)
  {
    if (sources.size() >= kMaxSources)
    {
      break;
    }
    if (bestScore > 0.0 && item.score < bestScore * 0.25)
    {
      continue;
    }
    sources.push_back(item.document);
  }
  return sources;
}

QString HelpAssistantService::buildPrompt(const QString& question,
                                          const QString& machineContext,
                                          const QString& conversationContext,
                                          const QList<SourceDocument>& sources) const
{
  QStringList blocks;
  for (int i = 0; i < sources.size(); ++i)
  {
    const SourceDocument& source = sources[i];
    blocks.push_back(QStringLiteral("[Documento %1: %2]\n%3")
                       .arg(i + 1)
                       .arg(sourceName(source.path))
                       .arg(trimmedContent(source.content)));
  }

  return QStringLiteral(
           "Usa solo il contesto qui sotto per rispondere alla domanda.\n"
           "I documenti sono ordinati per pertinenza: dai piu peso al Documento 1.\n"
           "Rispondi al tema della domanda, senza trasformarla in una checklist generica.\n"
           "Rispondi in modo breve e operativo: massimo 8 righe, salvo richiesta esplicita di dettaglio.\n"
           "Se il contesto macchina contiene camere candidate o limiti camera, usali per dire quali camere sono adatte, possibili o non adatte.\n"
           "Non consigliare una camera come certa se il suo profilo o la configurazione non supportano la richiesta.\n"
           "Se la domanda e' generica, per esempio chiede solo una lunghezza, chiedi prima quali riferimenti o che tipo di quota serve; non elencare camere a caso.\n"
           "Usa il contesto conversazione per interpretare follow-up come 'ora cosa faccio' o correzioni come 'non e' foro-bordo'.\n"
           "Se l'operatore corregge una tua ipotesi, accetta la correzione e non ripetere l'ipotesi sbagliata.\n"
           "Se l'hardware illuminatore non e' dichiarato, dillo chiaramente invece di inventarlo.\n"
           "Se manca una informazione, dillo e indica quale controllo pratico fare.\n"
           "Alla fine aggiungi una riga \"Fonti:\" con i nomi dei file usati.\n\n"
           "Contesto macchina corrente:\n%1\n\n"
           "Contesto conversazione:\n%2\n\n"
           "Contesto manuale:\n%3\n\n"
           "Domanda:\n%4")
    .arg(machineContext.trimmed().isEmpty() ? QStringLiteral("Non disponibile.") : limitBlock(machineContext, kMaxMachineContextChars),
         conversationContext.trimmed().isEmpty() ? QStringLiteral("Nessun messaggio precedente.") : limitBlock(conversationContext, kMaxConversationContextChars),
         blocks.join("\n\n"),
         question.trimmed());
}

QStringList HelpAssistantService::tokenize(const QString& text) const
{
  static const QSet<QString> stopwords = {
    "a", "ad", "al", "alla", "alle", "allo", "anche", "che", "ci", "con", "da", "dei",
    "ciao", "come", "controllare", "controllo", "del", "della", "devo", "defo", "di", "e", "ed", "fare", "faccio", "gli", "ha", "i", "il", "in", "la", "le",
    "lo", "ma", "mi", "ne", "nel", "non", "o", "per", "piu", "puo", "se", "si",
    "su", "sul", "tra", "un", "una", "uno", "uso", "vedi", "vedo"
  };

  QStringList tokens;
  static const QRegularExpression tokenPattern("[a-z0-9_]+");
  QRegularExpressionMatchIterator it = tokenPattern.globalMatch(normalizeText(text));
  while (it.hasNext())
  {
    const QString token = it.next().captured(0);
    if (token.size() > 1 && !stopwords.contains(token))
    {
      tokens.push_back(token);
    }
  }
  return tokens;
}

QStringList HelpAssistantService::expandQueryTokens(const QString& question, const QStringList& tokens) const
{
  QStringList expanded = tokens;
  const QString normalized = normalizeText(QStringLiteral(" ") + question + QStringLiteral(" "));
  const bool hasMeasure = std::any_of(tokens.begin(), tokens.end(), [](const QString& token) {
    return token.startsWith(QStringLiteral("misur")) || token == QStringLiteral("quota") || token == QStringLiteral("quote");
  });
  const bool hasNegative = normalized.contains(QStringLiteral(" non ")) ||
                           normalized.contains(QStringLiteral(" manca")) ||
                           normalized.contains(QStringLiteral(" assente")) ||
                           normalized.contains(QStringLiteral(" sparisce"));

  if (hasMeasure && hasNegative)
  {
    expanded.append({QStringLiteral("misura"), QStringLiteral("disponibile"), QStringLiteral("geometria"),
                     QStringLiteral("sorgente"), QStringLiteral("mancante"), QStringLiteral("quota")});
  }
  if (normalized.contains(QStringLiteral("lunghezza")) ||
      normalized.contains(QStringLiteral("larghezza")) ||
      normalized.contains(QStringLiteral("altezza")) ||
      normalized.contains(QStringLiteral("quota lineare")))
  {
    expanded.append({QStringLiteral("lunghezza"), QStringLiteral("larghezza"), QStringLiteral("altezza"),
                     QStringLiteral("distanza"), QStringLiteral("quota"), QStringLiteral("lineare"),
                     QStringLiteral("punto"), QStringLiteral("linea"), QStringLiteral("bordo"),
                     QStringLiteral("profilo"), QStringLiteral("superficie"), QStringLiteral("riferimento")});
  }
  if (normalized.contains(QStringLiteral("bordo bordo")) ||
      normalized.contains(QStringLiteral("bordo-bordo")) ||
      normalized.contains(QStringLiteral("due bordi")))
  {
    expanded.append({QStringLiteral("larghezza"), QStringLiteral("superficiale"), QStringLiteral("bordo"),
                     QStringLiteral("bordo"), QStringLiteral("due"), QStringLiteral("bordi"),
                     QStringLiteral("linea"), QStringLiteral("linea"), QStringLiteral("distanza"),
                     QStringLiteral("cava"), QStringLiteral("sede"), QStringLiteral("luce"),
                     QStringLiteral("diretta")});
  }
  if (tokens.contains(QStringLiteral("edge")) && hasMeasure)
  {
    expanded.append({QStringLiteral("edge"), QStringLiteral("geometria"), QStringLiteral("trovata"),
                     QStringLiteral("misura"), QStringLiteral("risultati"), QStringLiteral("frame")});
  }
  if (tokens.contains(QStringLiteral("luce")) || tokens.contains(QStringLiteral("illuminazione")))
  {
    expanded.append({QStringLiteral("illuminazione"), QStringLiteral("contrasto"),
                     QStringLiteral("esposizione"), QStringLiteral("gain")});
  }
  if (normalized.contains(QStringLiteral("diametr")) && normalized.contains(QStringLiteral("superficie")))
  {
    expanded.append({QStringLiteral("diametro"), QStringLiteral("superficie"), QStringLiteral("cerchio"),
                     QStringLiteral("foro"), QStringLiteral("luce"), QStringLiteral("diretta"),
                     QStringLiteral("diffusa"), QStringLiteral("coassiale"), QStringLiteral("anulare"),
                     QStringLiteral("edge")});
  }
  if (normalized.contains(QStringLiteral("misur")) && normalized.contains(QStringLiteral("superfic")))
  {
    expanded.append({QStringLiteral("misure"), QStringLiteral("superficiali"), QStringLiteral("faccia"),
                     QStringLiteral("foro"), QStringLiteral("sede"), QStringLiteral("tasca"),
                     QStringLiteral("luce"), QStringLiteral("diretta"), QStringLiteral("geometria"),
                     QStringLiteral("cerchio"), QStringLiteral("linea"), QStringLiteral("camera"),
                     QStringLiteral("candidata"), QStringLiteral("profilo")});
  }
  if (tokens.contains(QStringLiteral("camera")) ||
      tokens.contains(QStringLiteral("telecamera")) ||
      tokens.contains(QStringLiteral("tc")))
  {
    expanded.append({QStringLiteral("camera"), QStringLiteral("candidata"), QStringLiteral("profilo"),
                     QStringLiteral("misura"), QStringLiteral("superficie"), QStringLiteral("dimensionale")});
  }
  if (normalized.contains(QStringLiteral("ross")) ||
      normalized.contains(QStringLiteral("esclusion")) ||
      normalized.contains(QStringLiteral("mascher")))
  {
    expanded.append({QStringLiteral("maschera"), QStringLiteral("rossa"), QStringLiteral("esclusione"),
                     QStringLiteral("localizzazione"), QStringLiteral("ignora")});
  }

  return expanded;
}

QString HelpAssistantService::cleanProcessOutput(const QString& text) const
{
  QString cleaned = text;
  static const QRegularExpression ansiPattern("\x1b\\[[0-?]*[ -/]*[@-~]");
  static const QRegularExpression spinnerPattern("^[\\s\\x{2800}-\\x{28ff}]+$");
  cleaned.remove(ansiPattern);

  QStringList lines;
  for (QString line : cleaned.split('\n'))
  {
    line.remove('\r');
    line = line.trimmed();
    if (!line.isEmpty() && !spinnerPattern.match(line).hasMatch())
    {
      lines.push_back(line);
    }
  }
  return lines.join('\n').trimmed();
}

QString HelpAssistantService::sourceName(const QString& path) const
{
  return QFileInfo(path).fileName();
}

QString HelpAssistantService::limitBlock(const QString& text, qsizetype maxChars) const
{
  QString normalized = text.trimmed();
  if (normalized.size() <= maxChars)
  {
    return normalized;
  }
  return normalized.left(maxChars).trimmed() + QStringLiteral("\n...");
}
