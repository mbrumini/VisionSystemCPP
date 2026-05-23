#pragma once

#include <QDialog>

class QTextEdit;

class SetupResultsDialog : public QDialog
{
public:
  explicit SetupResultsDialog(QWidget* parent = nullptr);

  void setResultsText(const QString& text);

private:
  QTextEdit* m_text = nullptr;
};
