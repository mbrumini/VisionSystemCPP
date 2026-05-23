#include "gui/help/HelpAssistantService.h"

#include "gui/help/HelpModelBootstrapper.h"

#include <QCoreApplication>
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
  const QString appDocs = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("docs/help"));
  if (QDir(appDocs).exists())
  {
    return QCoreApplication::applicationDirPath();
  }
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
  const QString normalizedRetrieval = normalizeText(retrievalText);
  const bool mentionsCircularReference = normalizedRetrieval.contains(QStringLiteral(" foro")) ||
                                         normalizedRetrieval.contains(QStringLiteral(" fori")) ||
                                         normalizedRetrieval.contains(QStringLiteral(" cerchio")) ||
                                         normalizedRetrieval.contains(QStringLiteral(" cerchi")) ||
                                         normalizedRetrieval.contains(QStringLiteral(" centro")) ||
                                         normalizedRetrieval.contains(QStringLiteral(" centri")) ||
                                         normalizedRetrieval.contains(QStringLiteral(" sede")) ||
                                         normalizedRetrieval.contains(QStringLiteral(" diametr"));
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
    const QString title = normalizeText(item.document.title);
    const QString path = normalizeText(item.document.path);
    const QString keywords = normalizeText(item.document.content.left(240));
    const bool circularDocument = title.contains(QStringLiteral("foro")) ||
                                  title.contains(QStringLiteral("fori")) ||
                                  title.contains(QStringLiteral("cerchio")) ||
                                  title.contains(QStringLiteral("diametro")) ||
                                  path.contains(QStringLiteral("foro")) ||
                                  path.contains(QStringLiteral("fori")) ||
                                  path.contains(QStringLiteral("cerchio")) ||
                                  path.contains(QStringLiteral("diametro")) ||
                                  keywords.contains(QStringLiteral("foro")) ||
                                  keywords.contains(QStringLiteral("fori")) ||
                                  keywords.contains(QStringLiteral("cerchio")) ||
                                  keywords.contains(QStringLiteral("diametro"));
    if (!mentionsCircularReference && circularDocument)
    {
      continue;
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
    blocks.push_back(QStringLiteral("[Contesto tecnico %1]\n%2")
                       .arg(i + 1)
                       .arg(trimmedContent(source.content)));
  }

  return QStringLiteral(
           "Usa solo il contesto qui sotto per rispondere alla domanda.\n"
           "I blocchi di contesto sono ordinati per pertinenza: dai piu peso al primo blocco.\n"
           "Rispondi al tema della domanda, senza trasformarla in una checklist generica.\n"
           "Rispondi in modo breve e operativo: massimo 8 righe, salvo richiesta esplicita di dettaglio.\n"
           "Tieni un tono umano e disponibile. Se ci sta, puoi aggiungere una battuta leggera e breve, ma la risposta tecnica deve restare chiara.\n"
           "Se la domanda e' solo un saluto o un ringraziamento, rispondi naturalmente senza ripetere procedure.\n"
           "Non citare documenti, blocchi, contesti tecnici o numeri di documento nella risposta all'operatore.\n"
           "Segui il contesto della richiesta: non introdurre oggetti non citati dall'operatore o dai documenti, per esempio fori, cerchi, camere o misure specifiche.\n"
           "Se manca un dettaglio, chiedilo o indica una verifica pratica invece di completare la storia da solo.\n"
           "Se il contesto macchina contiene camere candidate o limiti camera, usali per dire quali camere sono adatte, possibili o non adatte.\n"
           "Non consigliare una camera come certa se il suo profilo o la configurazione non supportano la richiesta.\n"
           "Se la domanda parla di superficie, faccia del pezzo, foro cieco, sede, tasca o incisione, non proporre camere BN dimensionali come candidate principali.\n"
           "Le camere BN dimensionali vanno proposte per profilo esterno, silhouette o foro passante visto in silhouette solo quando questo e' esplicito o chiaramente richiesto.\n"
           "Non introdurre fori, cerchi o centri se l'operatore parla solo di un punto o un bordo e non cita fori, cerchi o sedi circolari.\n"
           "Se la domanda e' generica, per esempio chiede solo una lunghezza, chiedi prima quali riferimenti o che tipo di quota serve; non elencare camere a caso.\n"
           "Usa il contesto conversazione per interpretare follow-up come 'ora cosa faccio' o correzioni come 'non e' foro-bordo'.\n"
           "Se l'operatore corregge una tua ipotesi, accetta la correzione e non ripetere l'ipotesi sbagliata.\n"
           "Se l'hardware illuminatore non e' dichiarato, dillo chiaramente invece di inventarlo.\n"
           "Se manca una informazione, dillo e indica quale controllo pratico fare.\n"
           "Non mostrare i nomi dei file o una sezione Fonti nella risposta all'operatore.\n\n"
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
  if ((normalized.contains(QStringLiteral("punto")) && normalized.contains(QStringLiteral("superficie"))) ||
      (normalized.contains(QStringLiteral("punto")) &&
       (normalized.contains(QStringLiteral("muov")) ||
        normalized.contains(QStringLiteral("instabil")) ||
        normalized.contains(QStringLiteral("stabilizz")) ||
        normalized.contains(QStringLiteral("staqbilizz")))))
  {
    expanded.append({QStringLiteral("punto"), QStringLiteral("edge"), QStringLiteral("instabile"),
                     QStringLiteral("muove"), QStringLiteral("superficie"), QStringLiteral("bordo"),
                     QStringLiteral("scansione"), QStringLiteral("riflesso"), QStringLiteral("texture"),
                     QStringLiteral("sensibilita"), QStringLiteral("transizione"), QStringLiteral("first"),
                     QStringLiteral("best"), QStringLiteral("last"), QStringLiteral("fascia"),
                     QStringLiteral("contrasto"), QStringLiteral("coassiale"), QStringLiteral("diffusa"),
                     QStringLiteral("geometria"), QStringLiteral("costruita")});
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
  if (normalized.contains(QStringLiteral("rifless")))
  {
    expanded.append({QStringLiteral("riflesso"), QStringLiteral("riflessi"),
                     QStringLiteral("edge"), QStringLiteral("bordo"), QStringLiteral("falso"),
                     QStringLiteral("superficie"), QStringLiteral("lucida"), QStringLiteral("diffusa"),
                     QStringLiteral("coassiale"), QStringLiteral("angolo"), QStringLiteral("luce")});
  }
  if (normalized.contains(QStringLiteral("localizz")) ||
      normalized.contains(QStringLiteral("posa")) ||
      (normalized.contains(QStringLiteral("pezzo")) && normalized.contains(QStringLiteral("difficile"))))
  {
    expanded.append({QStringLiteral("localizzazione"), QStringLiteral("localizzare"),
                     QStringLiteral("pezzo"), QStringLiteral("difficile"), QStringLiteral("instabile"),
                     QStringLiteral("posa"), QStringLiteral("centro"), QStringLiteral("angolo"),
                     QStringLiteral("area"), QStringLiteral("ricerca"), QStringLiteral("roi"),
                     QStringLiteral("esclusioni"), QStringLiteral("maschera"), QStringLiteral("rossa"),
                     QStringLiteral("riflessi"), QStringLiteral("contrasto"), QStringLiteral("illuminazione"),
                     QStringLiteral("meccanica"), QStringLiteral("battuta")});
  }
  if (normalized.contains(QStringLiteral("diametr")) && normalized.contains(QStringLiteral("superficie")))
  {
    expanded.append({QStringLiteral("diametro"), QStringLiteral("superficie"), QStringLiteral("cerchio"),
                     QStringLiteral("foro"), QStringLiteral("luce"), QStringLiteral("diretta"),
                     QStringLiteral("diffusa"), QStringLiteral("coassiale"), QStringLiteral("anulare"),
                     QStringLiteral("edge"), QStringLiteral("camera"), QStringLiteral("candidata"),
                     QStringLiteral("grigi"), QStringLiteral("measurement"), QStringLiteral("surface"),
                     QStringLiteral("bn"), QStringLiteral("dimensionale"), QStringLiteral("silhouette")});
  }
  if (normalized.contains(QStringLiteral("diametr")) &&
      (normalized.contains(QStringLiteral("esterno")) ||
       normalized.contains(QStringLiteral("profilo")) ||
       normalized.contains(QStringLiteral("silhouette"))))
  {
    expanded.append({QStringLiteral("diametro"), QStringLiteral("esterno"), QStringLiteral("profilo"),
                     QStringLiteral("silhouette"), QStringLiteral("retroilluminazione"),
                     QStringLiteral("backlight"), QStringLiteral("bn"), QStringLiteral("dimensionale"),
                     QStringLiteral("edge"), QStringLiteral("cerchio"), QStringLiteral("contorno"),
                     QStringLiteral("bordo")});
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

bool HelpAssistantService::tryBuildDirectReply(const QString& question,
                                               const QString& conversationContext,
                                               HelpAssistantReply& reply) const
{
  const QString normalized = normalizeText(question.trimmed());
  QString cleaned = normalized;
  static const QRegularExpression punctuationPattern(QStringLiteral("[^a-z0-9_\\s]+"));
  cleaned.replace(punctuationPattern, QStringLiteral(" "));
  const QString compacted = cleaned.simplified();
  QStringList operatorLines;
  for (const QString& line : conversationContext.split('\n'))
  {
    if (line.trimmed().startsWith(QStringLiteral("Operatore:"), Qt::CaseInsensitive))
    {
      operatorLines.push_back(line);
    }
  }
  QString contextCleaned = normalizeText(operatorLines.join(QStringLiteral(" ")));
  contextCleaned.replace(punctuationPattern, QStringLiteral(" "));
  const QString compactedOperatorContext = contextCleaned.simplified();
  if (compacted.isEmpty())
  {
    return false;
  }

  static const QSet<QString> thanksMessages = {
    QStringLiteral("grazie"),
    QStringLiteral("ok grazie"),
    QStringLiteral("perfetto grazie"),
    QStringLiteral("bene grazie"),
    QStringLiteral("ottimo grazie"),
    QStringLiteral("grazie mille"),
    QStringLiteral("ti ringrazio")
  };
  const QStringList words = compacted.split(' ', Qt::SkipEmptyParts);
  const bool asksIdentity = (words.contains(QStringLiteral("chi")) && words.contains(QStringLiteral("sei"))) ||
                            compacted.contains(QStringLiteral("come puoi aiutarmi")) ||
                            compacted.contains(QStringLiteral("cosa puoi fare")) ||
                            compacted.contains(QStringLiteral("a cosa servi"));
  if (asksIdentity)
  {
    reply.answer = QStringLiteral(
      "Sono l'aiuto di VisionSystemCPP. Posso guidarti su setup, localizzazione, edge, geometrie, misure, tolleranze, illuminazione e diagnosi dei problemi. Dimmi cosa vedi sul pezzo e ci ragioniamo passo passo.");
    return true;
  }

  const bool hasThanks = words.contains(QStringLiteral("grazie")) ||
                         (words.contains(QStringLiteral("ringrazio")) && words.size() <= 3);
  if (thanksMessages.contains(compacted) || (hasThanks && words.size() <= 4))
  {
    reply.answer = QStringLiteral("Figurati. Quando vuoi, ripartiamo dal pezzo senza fare teatro.");
    return true;
  }

  static const QSet<QString> greetingMessages = {
    QStringLiteral("ciao"),
    QStringLiteral("buongiorno"),
    QStringLiteral("buonasera"),
    QStringLiteral("salve")
  };
  if (greetingMessages.contains(compacted))
  {
    reply.answer = QStringLiteral("Ciao. Dimmi cosa devi controllare e provo a guidarti passo passo.");
    return true;
  }

  const bool asksDiameter = compacted.contains(QStringLiteral("diametr"));
  const bool hasDiameterContext = compacted.contains(QStringLiteral("superficie")) ||
                                  compacted.contains(QStringLiteral("superficiale")) ||
                                  compacted.contains(QStringLiteral("profilo")) ||
                                  compacted.contains(QStringLiteral("esterno")) ||
                                  compacted.contains(QStringLiteral("silhouette")) ||
                                  compacted.contains(QStringLiteral("foro")) ||
                                  compacted.contains(QStringLiteral("fori")) ||
                                  compacted.contains(QStringLiteral("sede")) ||
                                  compacted.contains(QStringLiteral("tasca")) ||
                                  compacted.contains(QStringLiteral("cieco")) ||
                                  compacted.contains(QStringLiteral("passante"));
  if (asksDiameter && !hasDiameterContext && words.size() <= 4)
  {
    reply.answer = QStringLiteral(
      "Ok, diametro. Prima devo capire quale: diametro esterno in profilo/silhouette, foro passante, oppure diametro visibile sulla superficie come sede, foro cieco o incisione? Cambia camera, luce e tipo di edge.");
    return true;
  }

  const QString combined = QStringLiteral(" ") + compactedOperatorContext + QStringLiteral(" ") + compacted + QStringLiteral(" ");
  const bool asksLinearMeasure = combined.contains(QStringLiteral(" lunghezza ")) ||
                                 combined.contains(QStringLiteral(" larghezza ")) ||
                                 combined.contains(QStringLiteral(" altezza ")) ||
                                 combined.contains(QStringLiteral(" quota lineare "));
  const bool externalProfile = combined.contains(QStringLiteral(" esterno ")) ||
                               combined.contains(QStringLiteral(" profilo ")) ||
                               combined.contains(QStringLiteral(" profilo bn ")) ||
                               combined.contains(QStringLiteral(" silhouette ")) ||
                               combined.contains(QStringLiteral(" bn "));
  const bool twoReferences = combined.contains(QStringLiteral(" 2 linee ")) ||
                             combined.contains(QStringLiteral(" due linee ")) ||
                             combined.contains(QStringLiteral(" 2 bordi ")) ||
                             combined.contains(QStringLiteral(" due bordi ")) ||
                             combined.contains(QStringLiteral(" linea linea ")) ||
                             combined.contains(QStringLiteral(" bordo bordo "));

  if (asksLinearMeasure && externalProfile && twoReferences && words.size() <= 4)
  {
    reply.answer = QStringLiteral(
      "Ok, adesso il caso e' chiaro: lunghezza esterna su profilo BN tra due bordi.\n"
      "Usa una camera dimensionale BN con contrasto netto o retroilluminazione, poi crea due edge linea sui due bordi esterni.\n"
      "In Setup controlla che le due linee restino stabili su piu' pezzi, senza riflessi o soglia ballerina.\n"
      "Quando le linee sono verdi e ripetibili, crea la misura linea-linea e imposta nominale e tolleranze.");
    return true;
  }

  if (compacted == QStringLiteral("lunghezza") ||
      compacted == QStringLiteral("larghezza") ||
      compacted == QStringLiteral("altezza"))
  {
    reply.answer = QStringLiteral(
      "Ok. Prima dimmi che tipo di quota e': profilo esterno BN/silhouette, distanza tra due bordi sulla superficie, foro-bordo, oppure punto-punto?");
    return true;
  }

  if (asksLinearMeasure && externalProfile &&
      (compacted == QStringLiteral("esterno") ||
       compacted == QStringLiteral("profilo") ||
       compacted == QStringLiteral("profilo bn") ||
       compacted == QStringLiteral("bn")))
  {
    reply.answer = QStringLiteral(
      "Perfetto, quindi parliamo di profilo esterno BN. Se la quota e' tra due lati del pezzo, il passo successivo e' trovare i due bordi con edge linea e poi fare una misura linea-linea.");
    return true;
  }

  return false;
}

QString HelpAssistantService::cleanProcessOutput(const QString& text) const
{
  QString cleaned = text;
  static const QRegularExpression ansiPattern("\x1b\\[[0-?]*[ -/]*[@-~]");
  static const QRegularExpression spinnerPattern("^[\\s\\x{2800}-\\x{28ff}]+$");
  static const QRegularExpression sourcesPattern(
    QStringLiteral("(?im)^\\s*(fonti|fonti usate|sorgenti)\\s*:\\s*[\\s\\S]*$"));
  static const QRegularExpression documentReferencePattern(
    QStringLiteral("\\b(documento|documenti|contesto tecnico|blocco)\\s+\\d+\\b"), QRegularExpression::CaseInsensitiveOption);
  cleaned.remove(ansiPattern);
  cleaned.remove(sourcesPattern);

  QStringList lines;
  for (QString line : cleaned.split('\n'))
  {
    line.remove('\r');
    line = line.trimmed();
    if (documentReferencePattern.match(line).hasMatch())
    {
      continue;
    }
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
