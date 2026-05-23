#include "gui/help/HelpDialog.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <utility>

HelpDialog::HelpDialog(const QString& manualText,
                       const QString& languageCode,
                       const Labels& labels,
                       std::function<QString()> machineContextProvider,
                       QWidget* parent)
  : QDialog(parent)
  , m_languageCode(languageCode)
  , m_labels(labels)
  , m_machineContextProvider(std::move(machineContextProvider))
{
  setAttribute(Qt::WA_DeleteOnClose);
  resize(820, 660);

  m_assistant = new HelpAssistantService(this);

  auto* layout = new QVBoxLayout(this);
  auto* tabs = new QTabWidget(this);

  auto* manual = new QTextBrowser(tabs);
  manual->setPlainText(manualText);
  tabs->addTab(manual, m_labels.manualTab);

  auto* chatPage = new QWidget(tabs);
  auto* chatLayout = new QVBoxLayout(chatPage);
  m_chatHistory = new QPlainTextEdit(chatPage);
  m_chatHistory->setReadOnly(true);
  m_chatHistory->setPlaceholderText(m_labels.assistantUnavailable);
  chatLayout->addWidget(m_chatHistory, 1);

  auto* inputLayout = new QHBoxLayout();
  m_question = new QLineEdit(chatPage);
  m_question->setPlaceholderText(m_labels.questionPlaceholder);
  m_askButton = new QPushButton(m_labels.askButton, chatPage);
  inputLayout->addWidget(m_question, 1);
  inputLayout->addWidget(m_askButton);
  chatLayout->addLayout(inputLayout);
  tabs->addTab(chatPage, m_labels.chatTab);

  layout->addWidget(tabs);

  connect(m_askButton, &QPushButton::clicked, this, [this]() { askQuestion(); });
  connect(m_question, &QLineEdit::returnPressed, this, [this]() { askQuestion(); });
}

void HelpDialog::askQuestion()
{
  const QString question = m_question->text().trimmed();
  if (question.isEmpty())
  {
    return;
  }

  appendChatLine(QStringLiteral("Tu"), question);
  m_question->clear();
  setBusy(true);
  appendChatLine(QStringLiteral("Vision Help"), m_labels.waiting);

  m_assistant->ask(
    question,
    m_languageCode,
    m_machineContextProvider ? m_machineContextProvider() : QString(),
    conversationContext(),
    [this, question](const HelpAssistantReply& reply) {
      QString answer = reply.answer;
      if (!reply.sources.isEmpty() && !answer.contains(QStringLiteral("Fonti:"), Qt::CaseInsensitive))
      {
        answer += QStringLiteral("\n\nFonti: ") + reply.sources.join(QStringLiteral(", "));
      }
      appendChatLine(QStringLiteral("Vision Help"), answer);
      m_recentTurns.push_back(QStringLiteral("Operatore: %1").arg(question));
      m_recentTurns.push_back(QStringLiteral("Assistente: %1").arg(answer.left(900)));
      while (m_recentTurns.size() > 8)
      {
        m_recentTurns.removeFirst();
      }
      setBusy(false);
    },
    [this](const QString& error) {
      appendChatLine(QStringLiteral("Vision Help"), error);
      setBusy(false);
    });
}

QString HelpDialog::conversationContext() const
{
  if (m_recentTurns.isEmpty())
  {
    return {};
  }
  return m_recentTurns.join(QStringLiteral("\n"));
}

void HelpDialog::appendChatLine(const QString& speaker, const QString& text)
{
  QString current = m_chatHistory->toPlainText();
  if (!current.isEmpty())
  {
    current += QStringLiteral("\n\n");
  }
  current += QStringLiteral("%1:\n%2").arg(speaker, text.trimmed());
  m_chatHistory->setPlainText(current);
  m_chatHistory->verticalScrollBar()->setValue(m_chatHistory->verticalScrollBar()->maximum());
}

void HelpDialog::setBusy(bool busy)
{
  m_question->setEnabled(!busy);
  m_askButton->setEnabled(!busy);
}
