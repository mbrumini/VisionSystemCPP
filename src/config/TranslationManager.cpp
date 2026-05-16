#include "TranslationManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>

namespace
{
QString translationPath(const QString& languageCode)
{
  const QString fileName = "translations/" + languageCode + ".json";
  const QString runtimePath = QDir(QCoreApplication::applicationDirPath()).filePath(fileName);

  if (QFile::exists(runtimePath))
  {
    return runtimePath;
  }

  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(fileName);
}
}

bool TranslationManager::loadLanguage(const QString& languageCode, QString* errorMessage)
{
  QFile file(translationPath(languageCode));

  if (!file.open(QIODevice::ReadOnly))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile aprire file traduzione: " + languageCode;
    }

    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    if (errorMessage)
    {
      *errorMessage = "File traduzione non valido: " + parseError.errorString();
    }

    return false;
  }

  m_languageCode = languageCode;
  m_root = document.object();
  return true;
}

QString TranslationManager::languageCode() const
{
  return m_languageCode;
}

QString TranslationManager::text(const QString& key) const
{
  QJsonValue value = m_root;

  for (const QString& part : key.split('.'))
  {
    if (!value.isObject())
    {
      return key;
    }

    value = value.toObject().value(part);
  }

  if (!value.isString())
  {
    return key;
  }

  return value.toString();
}
