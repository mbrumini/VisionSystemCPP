#include "RecipeManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace
{
QString recipesRoot()
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("recipes");
}

QString appSettingsPath()
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("config/app_settings.json");
}

QJsonObject rectToJson(const QRect& rect)
{
  QJsonObject result;
  result["x"] = rect.x();
  result["y"] = rect.y();
  result["width"] = rect.width();
  result["height"] = rect.height();
  return result;
}

QRect rectFromJson(const QJsonObject& object)
{
  return QRect(
    object.value("x").toInt(),
    object.value("y").toInt(),
    object.value("width").toInt(),
    object.value("height").toInt());
}
}

QString RecipeManager::recipesRootPath()
{
  return recipesRoot();
}

QString RecipeManager::normalizeRecipeId(const QString& text)
{
  QString result = text.trimmed().toLower();
  result.replace(QRegularExpression("[^a-z0-9_-]+"), "_");
  result.replace(QRegularExpression("_+"), "_");
  result.remove(QRegularExpression("^_+|_+$"));
  return result;
}

QStringList RecipeManager::availableRecipes()
{
  QDir root(recipesRoot());
  const QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  QStringList result;

  for (const QFileInfo& entry : entries)
  {
    if (QFile::exists(QDir(entry.absoluteFilePath()).filePath("recipe.json")))
    {
      result.append(entry.fileName());
    }
  }

  return result;
}

bool RecipeManager::createRecipe(const QString& recipeId, QString* errorMessage)
{
  const QString normalizedId = normalizeRecipeId(recipeId);

  if (normalizedId.isEmpty())
  {
    if (errorMessage)
    {
      *errorMessage = "ID ricetta non valido.";
    }

    return false;
  }

  QDir root(recipesRoot());

  if (!root.exists() && !root.mkpath("."))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare directory ricette: " + root.absolutePath();
    }

    return false;
  }

  const QString recipePath = root.filePath(normalizedId);

  if (QFileInfo::exists(recipePath))
  {
    if (errorMessage)
    {
      *errorMessage = "Ricetta gia' esistente: " + normalizedId;
    }

    return false;
  }

  QDir recipeDir;

  if (!recipeDir.mkpath(recipePath + "/cameras"))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare ricetta: " + recipePath;
    }

    return false;
  }

  return writeRecipeMetadata(normalizedId, QDir(recipePath).filePath("recipe.json"), errorMessage);
}

bool RecipeManager::duplicateRecipe(const QString& sourceRecipeId, const QString& newRecipeId, QString* errorMessage)
{
  const QString normalizedId = normalizeRecipeId(newRecipeId);
  const QString sourcePath = QDir(recipesRoot()).filePath(sourceRecipeId);
  const QString destinationPath = QDir(recipesRoot()).filePath(normalizedId);

  if (normalizedId.isEmpty() || !QFileInfo::exists(sourcePath))
  {
    if (errorMessage)
    {
      *errorMessage = "Duplicazione ricetta non valida.";
    }

    return false;
  }

  if (QFileInfo::exists(destinationPath))
  {
    if (errorMessage)
    {
      *errorMessage = "Ricetta gia' esistente: " + normalizedId;
    }

    return false;
  }

  if (!copyDirectory(sourcePath, destinationPath, errorMessage))
  {
    return false;
  }

  return writeRecipeMetadata(normalizedId, QDir(destinationPath).filePath("recipe.json"), errorMessage);
}

bool RecipeManager::importRecipeDirectory(const QString& sourceDirectory, QString* importedRecipeId, QString* errorMessage)
{
  QFileInfo sourceInfo(sourceDirectory);

  if (!sourceInfo.isDir())
  {
    if (errorMessage)
    {
      *errorMessage = "Directory ricetta non valida: " + sourceDirectory;
    }

    return false;
  }

  QString recipeId = normalizeRecipeId(sourceInfo.fileName());

  if (recipeId.isEmpty())
  {
    recipeId = "imported_recipe";
  }

  QString destinationPath = QDir(recipesRoot()).filePath(recipeId);
  int suffix = 2;

  while (QFileInfo::exists(destinationPath))
  {
    recipeId = normalizeRecipeId(sourceInfo.fileName()) + "_" + QString::number(suffix);
    destinationPath = QDir(recipesRoot()).filePath(recipeId);
    suffix++;
  }

  if (!copyDirectory(sourceDirectory, destinationPath, errorMessage))
  {
    return false;
  }

  if (!writeRecipeMetadata(recipeId, QDir(destinationPath).filePath("recipe.json"), errorMessage))
  {
    return false;
  }

  if (importedRecipeId)
  {
    *importedRecipeId = recipeId;
  }

  return true;
}

bool RecipeManager::exportRecipeDirectory(const QString& recipeId, const QString& destinationDirectory, QString* errorMessage)
{
  const QString sourcePath = QDir(recipesRoot()).filePath(recipeId);
  const QString destinationPath = QDir(destinationDirectory).filePath(recipeId);

  if (!QFileInfo::exists(sourcePath))
  {
    if (errorMessage)
    {
      *errorMessage = "Ricetta non trovata: " + recipeId;
    }

    return false;
  }

  if (QFileInfo::exists(destinationPath))
  {
    if (errorMessage)
    {
      *errorMessage = "Destinazione gia' esistente: " + destinationPath;
    }

    return false;
  }

  return copyDirectory(sourcePath, destinationPath, errorMessage);
}

QString RecipeManager::loadActiveRecipeId()
{
  QFile file(appSettingsPath());

  if (!file.open(QIODevice::ReadOnly))
  {
    return "default";
  }

  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  const QString recipeId = document.object().value("activeRecipe").toString("default");

  if (availableRecipes().contains(recipeId))
  {
    return recipeId;
  }

  return "default";
}

bool RecipeManager::saveActiveRecipeId(const QString& recipeId, QString* errorMessage)
{
  QFile file(appSettingsPath());
  QJsonObject root;

  if (file.open(QIODevice::ReadOnly))
  {
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());

    if (document.isObject())
    {
      root = document.object();
    }
  }

  root["activeRecipe"] = recipeId;

  QDir settingsDir = QFileInfo(appSettingsPath()).dir();

  if (!settingsDir.exists() && !settingsDir.mkpath("."))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare directory configurazione: " + settingsDir.absolutePath();
    }

    return false;
  }

  QFile outputFile(appSettingsPath());

  if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile salvare configurazione applicazione: " + appSettingsPath();
    }

    return false;
  }

  outputFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
}

RecipeManager::RecipeManager(QString recipeId)
  : m_recipeId(std::move(recipeId))
{
}

QString RecipeManager::recipeId() const
{
  return m_recipeId;
}

void RecipeManager::setRecipeId(const QString& recipeId)
{
  m_recipeId = normalizeRecipeId(recipeId);
}

bool RecipeManager::loadLocalizationRoi(const QString& cameraId, QRect& roi) const
{
  QFile file(cameraRecipePath(cameraId));

  if (!file.open(QIODevice::ReadOnly))
  {
    return false;
  }

  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());

  if (!document.isObject())
  {
    return false;
  }

  const QJsonObject localization = document.object()
    .value("tools").toObject()
    .value("localization").toObject();
  const QJsonObject searchRoi = localization.value("searchRoi").toObject();

  if (searchRoi.isEmpty())
  {
    return false;
  }

  roi = rectFromJson(searchRoi);
  return roi.isValid();
}

bool RecipeManager::saveLocalizationRoi(const QString& cameraId, const QRect& roi, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QDir directory = QFileInfo(path).dir();

  if (!directory.exists() && !directory.mkpath("."))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare directory ricetta: " + directory.absolutePath();
    }

    return false;
  }

  QJsonObject root;
  QFile inputFile(path);

  if (inputFile.open(QIODevice::ReadOnly))
  {
    const QJsonDocument existingDocument = QJsonDocument::fromJson(inputFile.readAll());

    if (existingDocument.isObject())
    {
      root = existingDocument.object();
    }
  }

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject localization = tools.value("localization").toObject();
  localization["enabled"] = true;
  localization["method"] = "bw_dark_on_light";
  localization["searchRoi"] = rectToJson(roi.normalized());
  tools["localization"] = localization;
  root["tools"] = tools;

  QFile outputFile(path);

  if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile salvare ricetta camera: " + path;
    }

    return false;
  }

  outputFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
}

QString RecipeManager::cameraRecipePath(const QString& cameraId) const
{
  return QDir(recipesRoot()).filePath(m_recipeId + "/cameras/" + cameraId + ".json");
}

bool RecipeManager::copyDirectory(const QString& sourceDirectory, const QString& destinationDirectory, QString* errorMessage)
{
  QDir source(sourceDirectory);

  if (!source.exists())
  {
    if (errorMessage)
    {
      *errorMessage = "Directory sorgente non trovata: " + sourceDirectory;
    }

    return false;
  }

  QDir destination;

  if (!destination.mkpath(destinationDirectory))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare directory destinazione: " + destinationDirectory;
    }

    return false;
  }

  const QFileInfoList entries = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

  for (const QFileInfo& entry : entries)
  {
    const QString targetPath = QDir(destinationDirectory).filePath(entry.fileName());

    if (entry.isDir())
    {
      if (!copyDirectory(entry.absoluteFilePath(), targetPath, errorMessage))
      {
        return false;
      }
    }
    else if (!QFile::copy(entry.absoluteFilePath(), targetPath))
    {
      if (errorMessage)
      {
        *errorMessage = "Impossibile copiare file: " + entry.absoluteFilePath();
      }

      return false;
    }
  }

  return true;
}

bool RecipeManager::writeRecipeMetadata(const QString& recipeId, const QString& path, QString* errorMessage)
{
  QJsonObject root;
  QFile inputFile(path);

  if (inputFile.open(QIODevice::ReadOnly))
  {
    const QJsonDocument existingDocument = QJsonDocument::fromJson(inputFile.readAll());

    if (existingDocument.isObject())
    {
      root = existingDocument.object();
    }
  }

  root["id"] = recipeId;

  if (!root.contains("displayName"))
  {
    root["displayName"] = recipeId;
  }

  QFile outputFile(path);

  if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile scrivere metadati ricetta: " + path;
    }

    return false;
  }

  outputFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
}
