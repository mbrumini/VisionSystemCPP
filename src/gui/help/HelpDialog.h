#pragma once

#include "gui/help/HelpAssistantService.h"

#include <QDialog>

#include <functional>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class HelpDialog : public QDialog
{
public:
  struct Labels
  {
    QString manualTab;
    QString chatTab;
    QString questionPlaceholder;
    QString askButton;
    QString waiting;
    QString assistantUnavailable;
  };

  HelpDialog(const QString& manualText,
             const QString& languageCode,
             const Labels& labels,
             std::function<QString()> machineContextProvider,
             QWidget* parent = nullptr);

private:
  void askQuestion();
  void appendChatLine(const QString& speaker, const QString& text);
  QString conversationContext() const;
  void setBusy(bool busy);

  QString m_languageCode;
  QStringList m_recentTurns;
  Labels m_labels;
  std::function<QString()> m_machineContextProvider;
  HelpAssistantService* m_assistant = nullptr;
  QPlainTextEdit* m_chatHistory = nullptr;
  QLineEdit* m_question = nullptr;
  QPushButton* m_askButton = nullptr;
};
