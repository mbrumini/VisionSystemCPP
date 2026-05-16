#pragma once

#include <QJsonObject>
#include <QString>

class TranslationManager
{
public:
  bool loadLanguage(const QString& languageCode, QString* errorMessage = nullptr);

  QString languageCode() const;
  QString text(const QString& key) const;

private:
  QString m_languageCode = "it";
  QJsonObject m_root;
};
