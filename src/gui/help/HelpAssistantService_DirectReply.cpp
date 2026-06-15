#include "gui/help/HelpAssistantService.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

namespace
{
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
  const bool diameterContext = combined.contains(QStringLiteral(" diametro ")) ||
                               combined.contains(QStringLiteral(" diametri ")) ||
                               combined.contains(QStringLiteral(" diametr"));
  const bool externalDiameter = diameterContext &&
                                (combined.contains(QStringLiteral(" esterno ")) ||
                                 combined.contains(QStringLiteral(" profilo ")) ||
                                 combined.contains(QStringLiteral(" silhouette "))) &&
                                !combined.contains(QStringLiteral(" superficie ")) &&
                                !combined.contains(QStringLiteral(" superficiale ")) &&
                                !combined.contains(QStringLiteral(" cieco ")) &&
                                !combined.contains(QStringLiteral(" sede ")) &&
                                !combined.contains(QStringLiteral(" tasca "));
  if (externalDiameter && words.size() <= 5)
  {
    reply.answer = QStringLiteral(
      "Ok: diametro esterno in silhouette. Qui la strada giusta e' una camera dimensionale BN, con profilo netto del pezzo.\n"
      "Preferisci retroilluminazione o comunque contrasto forte pezzo/sfondo; non usare camere grigi/superficie per questo caso.\n"
      "Crea un edge cerchio o un controllo profilo sul contorno esterno, verifica in Setup che il bordo resti stabile su piu' pezzi, poi imposta misura diametro e tolleranze.");
    return true;
  }

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
