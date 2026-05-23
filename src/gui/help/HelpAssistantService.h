#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class HelpModelBootstrapper;
class QProcess;

struct HelpAssistantReply
{
  QString answer;
  QStringList sources;
};

class HelpAssistantService : public QObject
{
public:
  using ReplyHandler = std::function<void(const HelpAssistantReply&)>;
  using ErrorHandler = std::function<void(const QString&)>;

  explicit HelpAssistantService(QObject* parent = nullptr);

  bool isBusy() const;
  void ask(const QString& question,
           const QString& languageCode,
           const QString& machineContext,
           const QString& conversationContext,
           ReplyHandler onReply,
           ErrorHandler onError);

private:
  struct SourceDocument
  {
    QString path;
    QString title;
    QString content;
    QStringList tokens;
  };

  QList<SourceDocument> loadDocuments(const QString& languageCode) const;
  QList<SourceDocument> selectSources(const QString& question, const QString& conversationContext, const QList<SourceDocument>& documents) const;
  QString buildPrompt(const QString& question, const QString& machineContext, const QString& conversationContext, const QList<SourceDocument>& sources) const;
  QStringList tokenize(const QString& text) const;
  QStringList expandQueryTokens(const QString& question, const QStringList& tokens) const;
  bool tryBuildDirectReply(const QString& question, const QString& conversationContext, HelpAssistantReply& reply) const;
  QString cleanProcessOutput(const QString& text) const;
  QString sourceName(const QString& path) const;
  QString limitBlock(const QString& text, qsizetype maxChars) const;
  void runPrompt(const QString& prompt,
                 QList<SourceDocument> sources,
                 ReplyHandler onReply,
                 ErrorHandler onError);

  QProcess* m_process = nullptr;
  HelpModelBootstrapper* m_bootstrapper = nullptr;
  bool m_busy = false;
  mutable QString m_cachedLanguageCode;
  mutable QList<SourceDocument> m_cachedDocuments;
};
