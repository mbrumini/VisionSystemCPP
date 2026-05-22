#include "DetailedLogger.h"

#include <QDateTime>
#include <QDir>

bool DetailedLogger::setEnabled(bool enabled, const QString& rootDirectory, QString* errorMessage)
{
  if (!enabled)
  {
    if (m_file.isOpen())
    {
      m_stream.flush();
      m_file.close();
    }
    m_filePath.clear();
    return true;
  }

  if (m_file.isOpen())
  {
    return true;
  }

  QDir dir(rootDirectory);
  if (!dir.exists() && !dir.mkpath("."))
  {
    if (errorMessage)
    {
      *errorMessage = QString("Impossibile creare cartella log: %1").arg(rootDirectory);
    }
    return false;
  }

  const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  m_filePath = dir.filePath(QString("vision_detailed_%1.log").arg(stamp));
  m_file.setFileName(m_filePath);
  if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
  {
    if (errorMessage)
    {
      *errorMessage = QString("Impossibile aprire log dettagliato: %1").arg(m_filePath);
    }
    m_filePath.clear();
    return false;
  }

  m_stream.setDevice(&m_file);
  logLine("=== detailed log enabled ===");
  return true;
}

bool DetailedLogger::enabled() const
{
  return m_file.isOpen();
}

QString DetailedLogger::filePath() const
{
  return m_filePath;
}

void DetailedLogger::logLine(const QString& line)
{
  if (!m_file.isOpen())
  {
    return;
  }

  m_stream << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
           << " | " << line << '\n';
  m_stream.flush();
}
