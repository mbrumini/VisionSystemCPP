#pragma once

#include <QFile>
#include <QString>
#include <QTextStream>

class DetailedLogger
{
public:
  bool setEnabled(bool enabled, const QString& rootDirectory, QString* errorMessage = nullptr);
  bool enabled() const;
  QString filePath() const;
  void logLine(const QString& line);

private:
  QFile m_file;
  QTextStream m_stream;
  QString m_filePath;
};
