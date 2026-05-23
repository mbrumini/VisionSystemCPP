#include "gui/modules/setup/SetupResultsDialog.h"

#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>

SetupResultsDialog::SetupResultsDialog(QWidget* parent)
  : QDialog(parent)
{
  setAttribute(Qt::WA_DeleteOnClose);
  resize(520, 640);

  auto* layout = new QVBoxLayout(this);
  m_text = new QTextEdit(this);
  m_text->setReadOnly(true);
  m_text->setLineWrapMode(QTextEdit::WidgetWidth);
  layout->addWidget(m_text);
}

void SetupResultsDialog::setResultsText(const QString& text)
{
  if (!m_text)
  {
    return;
  }

  m_text->setPlainText(text);
  if (m_text->horizontalScrollBar())
  {
    m_text->horizontalScrollBar()->setValue(0);
  }
  if (m_text->verticalScrollBar())
  {
    m_text->verticalScrollBar()->setValue(0);
  }
}
