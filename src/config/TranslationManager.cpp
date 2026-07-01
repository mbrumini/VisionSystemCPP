#include "TranslationManager.h"

#include "RecipeJsonUtils.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QString translationPath(const QString& languageCode)
{
  const QString fileName = "translations/" + languageCode + ".json";
  return RecipeJsonUtils::appPath(fileName);
}

QJsonObject fallbackMenuTranslations(const QString& languageCode)
{
  QJsonObject menu;
  QJsonObject commands;

  if (languageCode == "en")
  {
    menu["recipes"] = "Recipes";
    menu["selectRecipe"] = "Select recipe";
    menu["newRecipe"] = "New recipe";
    menu["duplicateRecipe"] = "Duplicate recipe";
    menu["deleteRecipe"] = "Delete recipe";
    menu["importRecipe"] = "Import recipe";
    menu["exportRecipe"] = "Export recipe";
    menu["configurations"] = "Configurations";
    menu["cameras"] = "Cameras";
    menu["access"] = "Access";
    menu["configureCameras"] = "Configure cameras";
    menu["calibrateCheckerboard"] = "Calibrate checkerboard";
    menu["paths"] = "Paths";
    menu["diagnostics"] = "Diagnostics";
    menu["language"] = "Language";
    menu["help"] = "Help";
    menu["system"] = "System";

    commands["start"] = "Start";
    commands["stop"] = "Stop";
    commands["reloadConfig"] = "Reload config";
    commands["gridView"] = "Grid view";
    commands["toggleFullScreen"] = "Fullscreen / window";
    commands["setMaxThreads"] = "Set max threads";
    commands["help"] = "Help";
    commands["exit"] = "Exit";
    commands["showSetupDetails"] = "Show setup details";
    commands["enableDetailedLog"] = "Enable detailed log";
  }
  else
  {
    menu["recipes"] = "Ricette";
    menu["selectRecipe"] = "Seleziona ricetta";
    menu["newRecipe"] = "Nuova ricetta";
    menu["duplicateRecipe"] = "Duplica ricetta";
    menu["deleteRecipe"] = "Elimina ricetta";
    menu["importRecipe"] = "Importa ricetta";
    menu["exportRecipe"] = "Esporta ricetta";
    menu["configurations"] = "Configurazioni";
    menu["cameras"] = "Camere";
    menu["access"] = "Accesso";
    menu["configureCameras"] = "Configura telecamere";
    menu["calibrateCheckerboard"] = "Calibra checkerboard";
    menu["paths"] = "Percorsi";
    menu["diagnostics"] = "Diagnostica";
    menu["language"] = "Lingua";
    menu["help"] = "Aiuto";
    menu["system"] = "Sistema";

    commands["start"] = "Avvia";
    commands["stop"] = "Stop";
    commands["reloadConfig"] = "Ricarica configurazione";
    commands["gridView"] = "Vista griglia";
    commands["toggleFullScreen"] = "Schermo intero / finestra";
    commands["setMaxThreads"] = "Imposta thread massimi";
    commands["help"] = "Aiuto";
    commands["exit"] = "Esci";
    commands["showSetupDetails"] = "Mostra dettagli setup";
    commands["enableDetailedLog"] = "Attiva log dettagliato";
  }

  QJsonObject root;
  root["menu"] = menu;
  root["commands"] = commands;
  return root;
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

    m_languageCode = languageCode;
    m_root = fallbackMenuTranslations(languageCode);
    return true;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    if (errorMessage)
    {
      *errorMessage = "File traduzione non valido: " + parseError.errorString();
    }

    m_languageCode = languageCode;
    m_root = fallbackMenuTranslations(languageCode);
    return true;
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
  QJsonObject object = m_root;

  for (const QString& part : key.split('.'))
  {
    const QJsonValue value = object.value(part);
    if (value.isString())
    {
      return value.toString();
    }

    if (!value.isObject())
    {
      return key;
    }

    object = value.toObject();
  }

  return key;
}
