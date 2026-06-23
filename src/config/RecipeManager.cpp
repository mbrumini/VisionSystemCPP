#include "RecipeManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPolygon>
#include <QRegularExpression>

#include "RecipeJsonUtils.h"

using namespace RecipeJsonUtils;

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
  clearRecipeCache();
}

void RecipeManager::clearRecipeCache() const
{
  m_measurementsCache.clear();
  m_constructedGeometriesCache.clear();
}

void RecipeManager::invalidateCameraRecipeCache(const QString& cameraId) const
{
  m_measurementsCache.remove(cameraId);
  m_constructedGeometriesCache.remove(cameraId);
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

LocalizationSettings RecipeManager::loadLocalizationSettings(const QString& cameraId) const
{
  LocalizationSettings settings;
  QFile file(cameraRecipePath(cameraId));

  if (!file.open(QIODevice::ReadOnly))
  {
    return settings;
  }

  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());

  if (!document.isObject())
  {
    return settings;
  }

  const QJsonObject threshold = document.object()
    .value("tools").toObject()
    .value("localization").toObject()
    .value("threshold").toObject();

  if (threshold.isEmpty())
  {
    return settings;
  }

  settings.thresholdFactor = threshold.value("factor").toDouble(settings.thresholdFactor);
  settings.thresholdOffset = threshold.value("offset").toDouble(settings.thresholdOffset);
  return settings;
}

QVector<QRect> RecipeManager::loadLocalizationExclusionRects(const QString& cameraId) const
{
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return {};
  }

  const QJsonArray exclusions = root.value("tools").toObject()
    .value("localization").toObject()
    .value("exclusionRects").toArray();
  QVector<QRect> result;

  for (const QJsonValue& value : exclusions)
  {
    const QRect rect = rectFromJson(value.toObject()).normalized();

    if (rect.isValid())
    {
      result.append(rect);
    }
  }

  return result;
}

bool RecipeManager::saveLocalizationRoi(const QString& cameraId, const QRect& roi, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject localization = tools.value("localization").toObject();
  localization["enabled"] = true;
  localization["method"] = "bw_dark_on_light";
  localization["searchRoi"] = rectToJson(roi.normalized());

  QJsonObject threshold = localization.value("threshold").toObject();

  if (!threshold.contains("factor"))
  {
    threshold["factor"] = LocalizationSettings().thresholdFactor;
  }

  if (!threshold.contains("offset"))
  {
    threshold["offset"] = LocalizationSettings().thresholdOffset;
  }

  localization["threshold"] = threshold;
  tools["localization"] = localization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::addLocalizationExclusionRect(const QString& cameraId, const QRect& rect, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject localization = tools.value("localization").toObject();
  localization["enabled"] = true;
  localization["method"] = "bw_dark_on_light";

  QJsonArray exclusions = localization.value("exclusionRects").toArray();
  exclusions.append(rectToJson(rect.normalized()));
  localization["exclusionRects"] = exclusions;
  tools["localization"] = localization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveLocalizationExclusionRects(const QString& cameraId, const QVector<QRect>& rects, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject localization = tools.value("localization").toObject();
  localization["enabled"] = true;
  localization["method"] = "bw_dark_on_light";

  QJsonArray exclusions;
  for (const QRect& rect : rects)
  {
    const QRect normalized = rect.normalized();
    if (normalized.isValid())
    {
      exclusions.append(rectToJson(normalized));
    }
  }

  localization["exclusionRects"] = exclusions;
  tools["localization"] = localization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::clearLocalizationExclusionRects(const QString& cameraId, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject localization = tools.value("localization").toObject();
  localization["exclusionRects"] = QJsonArray();
  tools["localization"] = localization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
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
