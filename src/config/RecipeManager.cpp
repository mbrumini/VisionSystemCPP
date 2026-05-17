#include "RecipeManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

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

QJsonObject circleToJson(const QPoint& center, int radius)
{
  QJsonObject result;
  result["centerX"] = center.x();
  result["centerY"] = center.y();
  result["radius"] = radius;
  return result;
}

QJsonObject pointToJson(const QPoint& point)
{
  QJsonObject result;
  result["x"] = point.x();
  result["y"] = point.y();
  return result;
}

QPoint pointFromJson(const QJsonObject& object)
{
  return QPoint(object.value("x").toInt(), object.value("y").toInt());
}

QJsonObject pointFToJson(const QPointF& point)
{
  QJsonObject result;
  result["x"] = point.x();
  result["y"] = point.y();
  return result;
}

QPointF pointFFromJson(const QJsonObject& object)
{
  return QPointF(object.value("x").toDouble(), object.value("y").toDouble());
}

QStringList imageNameFilters()
{
  return {"*.bmp", "*.jpg", "*.jpeg", "*.png", "*.tif", "*.tiff"};
}

QString firstImageInDirectory(const QString& path)
{
  QDir directory(path);
  if (!directory.exists())
  {
    return {};
  }

  const QFileInfoList entries = directory.entryInfoList(imageNameFilters(), QDir::Files, QDir::Name);
  if (entries.isEmpty())
  {
    return {};
  }

  return entries.first().absoluteFilePath();
}

bool isSupportedImageFile(const QString& path)
{
  const QString suffix = QFileInfo(path).suffix().toLower();
  return suffix == "bmp" || suffix == "jpg" || suffix == "jpeg" || suffix == "png" || suffix == "tif" || suffix == "tiff";
}

bool loadJsonObject(const QString& path, QJsonObject& root)
{
  QFile inputFile(path);

  if (!inputFile.open(QIODevice::ReadOnly))
  {
    return false;
  }

  const QJsonDocument existingDocument = QJsonDocument::fromJson(inputFile.readAll());

  if (!existingDocument.isObject())
  {
    return false;
  }

  root = existingDocument.object();
  return true;
}

bool saveJsonObject(const QString& path, const QJsonObject& root, QString* errorMessage)
{
  QDir directory = QFileInfo(path).dir();

  if (!directory.exists() && !directory.mkpath("."))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare directory ricetta: " + directory.absolutePath();
    }

    return false;
  }

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

bool RecipeManager::loadSurfaceDefectRoi(const QString& cameraId, QRect& roi) const
{
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return false;
  }

  const QJsonObject searchRoi = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
    .value("searchRoi").toObject();

  if (searchRoi.isEmpty())
  {
    return false;
  }

  roi = rectFromJson(searchRoi);
  return roi.isValid();
}

SurfaceDefectSettings RecipeManager::loadSurfaceDefectSettings(const QString& cameraId) const
{
  SurfaceDefectSettings settings;
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return settings;
  }

  const QJsonObject threshold = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
    .value("threshold").toObject();

  if (threshold.isEmpty())
  {
    return settings;
  }

  settings.thresholdMin = threshold.value("min").toInt(settings.thresholdMin);
  settings.thresholdMax = threshold.value("max").toInt(settings.thresholdMax);
  return settings;
}

QVector<QRect> RecipeManager::loadSurfaceDefectExclusionRects(const QString& cameraId) const
{
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return {};
  }

  const QJsonArray exclusions = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
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

bool RecipeManager::saveSurfaceDefectRoi(const QString& cameraId, const QRect& roi, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceDefects = tools.value("surfaceLocalization").toObject();
  surfaceDefects["enabled"] = true;
  surfaceDefects["method"] = surfaceDefects.value("method").toString("threshold");
  surfaceDefects["searchRoi"] = rectToJson(roi.normalized());

  QJsonObject threshold = surfaceDefects.value("threshold").toObject();

  if (!threshold.contains("min"))
  {
    threshold["min"] = SurfaceDefectSettings().thresholdMin;
  }

  if (!threshold.contains("max"))
  {
    threshold["max"] = SurfaceDefectSettings().thresholdMax;
  }

  surfaceDefects["threshold"] = threshold;
  tools["surfaceLocalization"] = surfaceDefects;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::addSurfaceDefectExclusionRect(const QString& cameraId, const QRect& rect, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceDefects = tools.value("surfaceLocalization").toObject();
  surfaceDefects["enabled"] = true;
  surfaceDefects["method"] = surfaceDefects.value("method").toString("threshold");

  QJsonArray exclusions = surfaceDefects.value("exclusionRects").toArray();
  exclusions.append(rectToJson(rect.normalized()));
  surfaceDefects["exclusionRects"] = exclusions;
  tools["surfaceLocalization"] = surfaceDefects;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceDefectExclusionRects(const QString& cameraId, const QVector<QRect>& rects, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceDefects = tools.value("surfaceLocalization").toObject();
  surfaceDefects["enabled"] = true;
  surfaceDefects["method"] = surfaceDefects.value("method").toString("threshold");

  QJsonArray exclusions;
  for (const QRect& rect : rects)
  {
    const QRect normalized = rect.normalized();
    if (normalized.isValid())
    {
      exclusions.append(rectToJson(normalized));
    }
  }

  surfaceDefects["exclusionRects"] = exclusions;
  tools["surfaceLocalization"] = surfaceDefects;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::clearSurfaceDefectExclusionRects(const QString& cameraId, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceDefects = tools.value("surfaceLocalization").toObject();
  surfaceDefects["exclusionRects"] = QJsonArray();
  tools["surfaceLocalization"] = surfaceDefects;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::clearSurfaceLocalizationGeometry(const QString& cameraId, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["exclusionRects"] = QJsonArray();
  surfaceLocalization.remove("annulus");

  QJsonObject edge = surfaceLocalization.value("edge").toObject();
  edge.remove("circle");
  surfaceLocalization["edge"] = edge;

  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

SurfaceLocalizationStrategyConfig RecipeManager::loadSurfaceLocalizationStrategy(const QString& cameraId) const
{
  SurfaceLocalizationStrategyConfig config;
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return config;
  }

  const QJsonObject strategy = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
    .value("strategy").toObject();

  if (strategy.isEmpty())
  {
    return config;
  }

  config.name = strategy.value("name").toString(config.name);
  config.origin = strategy.value("origin").toString(config.origin);

  const QJsonObject xAxis = strategy.value("xAxis").toObject();
  config.xAxisFrom = xAxis.value("from").toString();
  config.xAxisTo = xAxis.value("to").toString();

  const QJsonArray features = strategy.value("features").toArray();

  for (const QJsonValue& value : features)
  {
    const QJsonObject featureObject = value.toObject();
    SurfaceStrategyFeatureConfig feature;
    feature.id = featureObject.value("id").toString();
    feature.polarity = featureObject.value("polarity").toString(feature.polarity);
    feature.searchRoi = rectFromJson(featureObject.value("searchRoi").toObject()).normalized();

    const QJsonObject threshold = featureObject.value("threshold").toObject();
    feature.thresholdMin = threshold.value("min").toInt(feature.thresholdMin);
    feature.thresholdMax = threshold.value("max").toInt(feature.thresholdMax);

    const QJsonObject expectedRadius = featureObject.value("expectedRadius").toObject();
    feature.expectedRadiusMin = expectedRadius.value("min").toDouble(feature.expectedRadiusMin);
    feature.expectedRadiusMax = expectedRadius.value("max").toDouble(feature.expectedRadiusMax);

    if (!feature.id.isEmpty() && feature.searchRoi.isValid())
    {
      config.features.append(feature);
    }
  }

  return config;
}

SurfaceAnnulusLocalizationConfig RecipeManager::loadSurfaceAnnulusLocalization(const QString& cameraId) const
{
  SurfaceAnnulusLocalizationConfig config;
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return config;
  }

  const QJsonObject surfaceLocalization = root.value("tools").toObject()
    .value("surfaceLocalization").toObject();
  config.method = surfaceLocalization.value("method").toString(config.method);

  if (config.method == "grayscale_threshold" || config.method == "grayscale_annulus_threshold")
  {
    config.method = "threshold";
  }

  if (config.method == "grayscale_annulus_edge")
  {
    config.method = "edge";
  }

  const QJsonObject annulus = surfaceLocalization.value("annulus").toObject();

  const QJsonObject outer = annulus.value("outerCircle").toObject();
  const QJsonObject inner = annulus.value("innerCircle").toObject();

  if (!outer.isEmpty())
  {
    config.hasOuterCircle = true;
    config.center = QPoint(outer.value("centerX").toInt(), outer.value("centerY").toInt());
    config.outerRadius = outer.value("radius").toInt();
  }

  if (!inner.isEmpty())
  {
    config.hasInnerCircle = true;
    config.innerRadius = inner.value("radius").toInt();

    if (!config.hasOuterCircle)
    {
      config.center = QPoint(inner.value("centerX").toInt(), inner.value("centerY").toInt());
    }
  }

  const QJsonObject threshold = surfaceLocalization.value("threshold").toObject();
  config.thresholdMin = threshold.value("min").toInt(config.thresholdMin);
  config.thresholdMax = threshold.value("value").toInt(threshold.value("max").toInt(config.thresholdMax));

  const QJsonObject edge = surfaceLocalization.value("edge").toObject();
  config.edgeSensitivity = edge.value("sensitivity").toInt(config.edgeSensitivity);
  config.edgeFitMaxError = edge.value("fitMaxError").toInt(config.edgeFitMaxError);

  const QJsonObject edgeCircle = edge.value("circle").toObject();

  if (!edgeCircle.isEmpty())
  {
    config.hasEdgeCircle = true;
    config.edgeCenter = QPoint(edgeCircle.value("centerX").toInt(), edgeCircle.value("centerY").toInt());
    config.edgeRadius = edgeCircle.value("radius").toInt();
  }

  const QJsonObject band = edge.value("band").toObject();
  config.edgeBandInner = band.value("inner").toInt(config.edgeBandInner);
  config.edgeBandOuter = band.value("outer").toInt(config.edgeBandOuter);
  return config;
}

bool RecipeManager::saveSurfaceAnnulusCircle(const QString& cameraId, bool outerCircle, const QPoint& center, int radius, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  surfaceLocalization["method"] = surfaceLocalization.value("method").toString("threshold");

  QJsonObject annulus = surfaceLocalization.value("annulus").toObject();
  annulus[outerCircle ? "outerCircle" : "innerCircle"] = circleToJson(center, radius);
  surfaceLocalization["annulus"] = annulus;

  QJsonObject threshold = surfaceLocalization.value("threshold").toObject();

  if (!threshold.contains("min"))
  {
    threshold["min"] = SurfaceDefectSettings().thresholdMin;
  }

  if (!threshold.contains("max"))
  {
    threshold["max"] = SurfaceDefectSettings().thresholdMax;
  }

  if (!threshold.contains("value"))
  {
    threshold["value"] = threshold.value("max").toInt(SurfaceDefectSettings().thresholdMax);
  }

  surfaceLocalization["threshold"] = threshold;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceAnnulusThreshold(const QString& cameraId, int minValue, int maxValue, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  surfaceLocalization["method"] = "threshold";

  QJsonObject threshold;
  threshold["min"] = qBound(0, minValue, 255);
  threshold["max"] = qBound(threshold.value("min").toInt(), maxValue, 255);
  threshold["value"] = threshold.value("max").toInt();
  surfaceLocalization["threshold"] = threshold;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceLocalizationMethod(const QString& cameraId, const QString& method, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  surfaceLocalization["method"] = method == "edge" || method == "edgePca" ? method : "threshold";
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceEdgeCircle(const QString& cameraId, const QPoint& center, int radius, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  surfaceLocalization["method"] = surfaceLocalization.value("method").toString("threshold");

  QJsonObject edge = surfaceLocalization.value("edge").toObject();
  edge["circle"] = circleToJson(center, qMax(1, radius));

  QJsonObject band = edge.value("band").toObject();

  if (!band.contains("inner"))
  {
    band["inner"] = SurfaceAnnulusLocalizationConfig().edgeBandInner;
  }

  if (!band.contains("outer"))
  {
    band["outer"] = SurfaceAnnulusLocalizationConfig().edgeBandOuter;
  }

  edge["band"] = band;
  surfaceLocalization["edge"] = edge;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceEdgeSensitivity(const QString& cameraId, int sensitivity, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  const QString currentMethod = surfaceLocalization.value("method").toString("edge");
  surfaceLocalization["method"] = currentMethod == "edgePca" ? currentMethod : "edge";

  QJsonObject edge = surfaceLocalization.value("edge").toObject();
  edge["sensitivity"] = qBound(0, sensitivity, 255);
  surfaceLocalization["edge"] = edge;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceEdgeBand(const QString& cameraId, int innerWidth, int outerWidth, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  const QString currentMethod = surfaceLocalization.value("method").toString("edge");
  surfaceLocalization["method"] = currentMethod == "edgePca" ? currentMethod : "edge";

  QJsonObject edge = surfaceLocalization.value("edge").toObject();
  QJsonObject band;
  band["inner"] = qBound(1, innerWidth, 200);
  band["outer"] = qBound(1, outerWidth, 200);
  edge["band"] = band;
  surfaceLocalization["edge"] = edge;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceEdgeFitMaxError(const QString& cameraId, int maxError, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  const QString currentMethod = surfaceLocalization.value("method").toString("edge");
  surfaceLocalization["method"] = currentMethod == "edgePca" ? currentMethod : "edge";

  QJsonObject edge = surfaceLocalization.value("edge").toObject();
  edge["fitMaxError"] = qBound(1, maxError, 50);
  surfaceLocalization["edge"] = edge;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

SurfaceModelConfig RecipeManager::loadSurfaceModel(const QString& cameraId) const
{
  SurfaceModelConfig config;
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return config;
  }

  const QJsonObject model = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
    .value("model").toObject();

  if (model.isEmpty())
  {
    return config;
  }

  config.searchRoi = rectFromJson(model.value("searchRoi").toObject()).normalized();
  config.templateImagePath = model.value("templateImagePath").toString();
  config.edgeSensitivity = model.value("edgeSensitivity").toInt(config.edgeSensitivity);
  config.maxShapeDistance = model.value("maxShapeDistance").toDouble(config.maxShapeDistance);
  config.minTemplateScore = model.value("minTemplateScore").toDouble(config.minTemplateScore);

  const QJsonObject angleRange = model.value("angleRange").toObject();
  config.angleStartDegrees = angleRange.value("start").toDouble(config.angleStartDegrees);
  config.angleEndDegrees = angleRange.value("end").toDouble(config.angleEndDegrees);
  config.angleStepDegrees = angleRange.value("step").toDouble(config.angleStepDegrees);

  const QJsonArray contour = model.value("contour").toArray();
  for (const QJsonValue& value : contour)
  {
    config.contour.append(pointFromJson(value.toObject()));
  }

  config.hasModel = config.searchRoi.isValid() && !config.contour.isEmpty() && !config.templateImagePath.isEmpty();
  return config;
}

bool RecipeManager::saveSurfaceModel(const QString& cameraId, const QRect& searchRoi, const QVector<QPoint>& contour, const QString& templateImagePath, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  surfaceLocalization["method"] = "model";

  QJsonObject model = surfaceLocalization.value("model").toObject();
  model["searchRoi"] = rectToJson(searchRoi.normalized());
  model["templateImagePath"] = templateImagePath;
  model["edgeSensitivity"] = model.value("edgeSensitivity").toInt(SurfaceModelConfig().edgeSensitivity);
  model["maxShapeDistance"] = model.value("maxShapeDistance").toDouble(SurfaceModelConfig().maxShapeDistance);
  model["minTemplateScore"] = model.value("minTemplateScore").toDouble(SurfaceModelConfig().minTemplateScore);

  QJsonObject angleRange = model.value("angleRange").toObject();
  if (!angleRange.contains("start"))
  {
    angleRange["start"] = SurfaceModelConfig().angleStartDegrees;
  }
  if (!angleRange.contains("end"))
  {
    angleRange["end"] = SurfaceModelConfig().angleEndDegrees;
  }
  if (!angleRange.contains("step"))
  {
    angleRange["step"] = SurfaceModelConfig().angleStepDegrees;
  }
  model["angleRange"] = angleRange;

  QJsonArray contourArray;
  for (const QPoint& point : contour)
  {
    contourArray.append(pointToJson(point));
  }
  model["contour"] = contourArray;

  surfaceLocalization["model"] = model;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceModelEdgeSensitivity(const QString& cameraId, int sensitivity, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;
  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  QJsonObject model = surfaceLocalization.value("model").toObject();
  model["edgeSensitivity"] = qBound(1, sensitivity, 255);
  surfaceLocalization["model"] = model;
  surfaceLocalization["enabled"] = true;
  surfaceLocalization["method"] = "model";
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceModelMaxShapeDistance(const QString& cameraId, double distance, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;
  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  QJsonObject model = surfaceLocalization.value("model").toObject();
  model["maxShapeDistance"] = qBound(0.001, distance, 5.0);
  surfaceLocalization["model"] = model;
  surfaceLocalization["enabled"] = true;
  surfaceLocalization["method"] = "model";
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceModelMinTemplateScore(const QString& cameraId, double score, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;
  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  QJsonObject model = surfaceLocalization.value("model").toObject();
  model["minTemplateScore"] = qBound(0.0, score, 1.0);
  surfaceLocalization["model"] = model;
  surfaceLocalization["enabled"] = true;
  surfaceLocalization["method"] = "model";
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceModelAngleRange(const QString& cameraId, double startDegrees, double endDegrees, double stepDegrees, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;
  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  QJsonObject model = surfaceLocalization.value("model").toObject();
  QJsonObject angleRange;
  angleRange["start"] = qBound(-180.0, startDegrees, 180.0);
  angleRange["end"] = qBound(-180.0, endDegrees, 180.0);
  angleRange["step"] = qBound(1.0, stepDegrees, 45.0);
  model["angleRange"] = angleRange;
  surfaceLocalization["model"] = model;
  surfaceLocalization["enabled"] = true;
  surfaceLocalization["method"] = "model";
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

QString RecipeManager::surfaceModelTemplateImagePath(const QString& cameraId) const
{
  return QDir(recipesRoot()).filePath(m_recipeId + "/assets/" + cameraId + "_surface_model_template.png");
}

QVector<GeometryLineRecipeConfig> RecipeManager::loadGeometryLines(const QString& cameraId) const
{
  QVector<GeometryLineRecipeConfig> configs;

  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return configs;
  }

  const QJsonArray lines = root.value("tools").toObject()
    .value("geometries").toObject()
    .value("lines").toArray();

  for (const QJsonValue& value : lines)
  {
    const QJsonObject line = value.toObject();
    GeometryLineRecipeConfig config;
    config.id = line.value("id").toString(config.id);
    if (config.id.isEmpty())
    {
      continue;
    }

    config.enabled = line.value("enabled").toBool(false);
    config.partStart = pointFFromJson(line.value("partStart").toObject());
    config.partEnd = pointFFromJson(line.value("partEnd").toObject());
    config.bandHalfWidth = line.value("bandHalfWidth").toInt(config.bandHalfWidth);
    config.edgeSensitivity = line.value("edgeSensitivity").toInt(config.edgeSensitivity);
    config.edgeCleanupDerivative = line.value("edgeCleanupDerivative").toInt(config.edgeCleanupDerivative);
    config.edgeStatisticalFilter = line.value("edgeStatisticalFilter").toInt(config.edgeStatisticalFilter);
    config.useSubpixel = line.value("useSubpixel").toBool(config.useSubpixel);
    config.scanDirection = line.value("scanDirection").toString(config.scanDirection);
    config.transition = line.value("transition").toString(config.transition);
    config.pickMode = line.value("pickMode").toString(config.pickMode);
    configs.append(config);
  }

  return configs;
}

GeometryLineRecipeConfig RecipeManager::loadGeometryLine(const QString& cameraId, const QString& lineId) const
{
  const QVector<GeometryLineRecipeConfig> configs = loadGeometryLines(cameraId);
  for (const GeometryLineRecipeConfig& config : configs)
  {
    if (config.id == lineId)
    {
      return config;
    }
  }

  GeometryLineRecipeConfig config;
  config.id = lineId;
  return config;
}

bool RecipeManager::saveGeometryLines(const QString& cameraId, const QVector<GeometryLineRecipeConfig>& configs, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject geometries = tools.value("geometries").toObject();
  geometries["enabled"] = true;
  QJsonArray lines;
  for (const GeometryLineRecipeConfig& config : configs)
  {
    QJsonObject line;
    line["enabled"] = config.enabled;
    line["id"] = config.id.isEmpty() ? QString("line_%1").arg(lines.size() + 1) : config.id;
    line["type"] = "edge_line";
    line["coordinateSpace"] = "part";
    line["partStart"] = pointFToJson(config.partStart);
    line["partEnd"] = pointFToJson(config.partEnd);
    line["bandHalfWidth"] = qBound(2, config.bandHalfWidth, 500);
    line["edgeSensitivity"] = qBound(1, config.edgeSensitivity, 255);
    line["edgeCleanupDerivative"] = qBound(0, config.edgeCleanupDerivative, 100);
    line["edgeStatisticalFilter"] = qBound(0, config.edgeStatisticalFilter, 100);
    line["useSubpixel"] = config.useSubpixel;
    line["scanDirection"] = config.scanDirection == "normal_negative" ? "normal_negative" : "normal_positive";
    line["transition"] = config.transition == "dark_to_light" ? "dark_to_light" : "light_to_dark";
    line["pickMode"] = config.pickMode == "last" || config.pickMode == "best" ? config.pickMode : "first";
    lines.append(line);
  }

  geometries["lines"] = lines;
  tools["geometries"] = geometries;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveGeometryLine(const QString& cameraId, const GeometryLineRecipeConfig& config, QString* errorMessage) const
{
  QVector<GeometryLineRecipeConfig> configs = loadGeometryLines(cameraId);
  bool replaced = false;
  for (GeometryLineRecipeConfig& existing : configs)
  {
    if (existing.id == config.id)
    {
      existing = config;
      replaced = true;
      break;
    }
  }

  if (!replaced)
  {
    configs.append(config);
  }

  return saveGeometryLines(cameraId, configs, errorMessage);
}

GeometryPointRecipeConfig RecipeManager::loadGeometryPoint(const QString& cameraId, const QString& pointId) const
{
  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    GeometryPointRecipeConfig config;
    config.id = pointId;
    return config;
  }

  const QJsonArray points = root.value("tools").toObject()
    .value("geometries").toObject()
    .value("points").toArray();

  for (const QJsonValue& value : points)
  {
    const QJsonObject point = value.toObject();
    const QString id = point.value("id").toString("point_1");
    if (id != pointId)
    {
      continue;
    }

    GeometryPointRecipeConfig config;
    config.id = id;
    config.enabled = point.value("enabled").toBool(false);
    config.partStart = pointFFromJson(point.value("partStart").toObject());
    config.partEnd = pointFFromJson(point.value("partEnd").toObject());
    config.edgeSensitivity = point.value("edgeSensitivity").toInt(config.edgeSensitivity);
    config.useSubpixel = point.value("useSubpixel").toBool(config.useSubpixel);
    config.transition = point.value("transition").toString(config.transition);
    config.pickMode = point.value("pickMode").toString(config.pickMode);
    return config;
  }

  GeometryPointRecipeConfig config;
  config.id = pointId;
  return config;
}

bool RecipeManager::saveGeometryPoint(const QString& cameraId, const GeometryPointRecipeConfig& config, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject geometries = tools.value("geometries").toObject();
  geometries["enabled"] = true;

  QJsonArray points = geometries.value("points").toArray();
  bool replaced = false;
  for (int i = 0; i < points.size(); ++i)
  {
    QJsonObject point = points[i].toObject();
    if (point.value("id").toString() != config.id)
    {
      continue;
    }

    point["enabled"] = config.enabled;
    point["id"] = config.id.isEmpty() ? "point_1" : config.id;
    point["type"] = "edge_point";
    point["coordinateSpace"] = "part";
    point["partStart"] = pointFToJson(config.partStart);
    point["partEnd"] = pointFToJson(config.partEnd);
    point["edgeSensitivity"] = qBound(1, config.edgeSensitivity, 255);
    point["useSubpixel"] = config.useSubpixel;
    point["transition"] = config.transition == "dark_to_light" ? "dark_to_light" : "light_to_dark";
    point["pickMode"] = config.pickMode == "last" || config.pickMode == "best" ? config.pickMode : "first";
    points[i] = point;
    replaced = true;
    break;
  }

  if (!replaced)
  {
    QJsonObject point;
    point["enabled"] = config.enabled;
    point["id"] = config.id.isEmpty() ? "point_1" : config.id;
    point["type"] = "edge_point";
    point["coordinateSpace"] = "part";
    point["partStart"] = pointFToJson(config.partStart);
    point["partEnd"] = pointFToJson(config.partEnd);
    point["edgeSensitivity"] = qBound(1, config.edgeSensitivity, 255);
    point["useSubpixel"] = config.useSubpixel;
    point["transition"] = config.transition == "dark_to_light" ? "dark_to_light" : "light_to_dark";
    point["pickMode"] = config.pickMode == "last" || config.pickMode == "best" ? config.pickMode : "first";
    points.append(point);
  }

  geometries["points"] = points;
  tools["geometries"] = geometries;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

QString RecipeManager::cameraSampleImagesPath(const QString& cameraId) const
{
  return cameraImagesPath(cameraId, "sample");
}

QString RecipeManager::cameraTestImagesPath(const QString& cameraId) const
{
  return cameraImagesPath(cameraId, "test");
}

QString RecipeManager::firstCameraSampleImagePath(const QString& cameraId) const
{
  return firstImageInDirectory(cameraSampleImagesPath(cameraId));
}

QString RecipeManager::firstCameraTestImagePath(const QString& cameraId) const
{
  return firstImageInDirectory(cameraTestImagesPath(cameraId));
}

bool RecipeManager::ensureCameraImageFolders(const QString& cameraId, QString* errorMessage) const
{
  QDir directory;
  for (const QString& path : {cameraSampleImagesPath(cameraId), cameraTestImagesPath(cameraId)})
  {
    if (!directory.mkpath(path))
    {
      if (errorMessage)
      {
        *errorMessage = "Impossibile creare cartella immagini ricetta: " + path;
      }
      return false;
    }
  }

  return true;
}

bool RecipeManager::importCameraSampleImage(const QString& cameraId, const QString& sourceFilePath, QString* errorMessage) const
{
  if (!isSupportedImageFile(sourceFilePath))
  {
    if (errorMessage)
    {
      *errorMessage = "Immagine campione non valida: " + sourceFilePath;
    }
    return false;
  }

  const QString destinationDirectory = cameraSampleImagesPath(cameraId);
  if (!QDir().mkpath(destinationDirectory))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare cartella sample: " + destinationDirectory;
    }
    return false;
  }

  QDir sampleDir(destinationDirectory);
  const QFileInfo source(sourceFilePath);
  const QString destinationPath = sampleDir.filePath(source.fileName());
  const QString sourceAbsolutePath = source.absoluteFilePath();
  const QString destinationAbsolutePath = QFileInfo(destinationPath).absoluteFilePath();
  for (const QFileInfo& file : sampleDir.entryInfoList(imageNameFilters(), QDir::Files))
  {
    if (file.absoluteFilePath() == sourceAbsolutePath)
    {
      continue;
    }
    sampleDir.remove(file.fileName());
  }

  if (sourceAbsolutePath == destinationAbsolutePath)
  {
    return true;
  }

  if (!QFile::copy(sourceFilePath, destinationPath))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile copiare immagine campione: " + sourceFilePath;
    }
    return false;
  }

  return true;
}

bool RecipeManager::importCameraTestImages(const QString& cameraId, const QString& sourceDirectory, QString* errorMessage) const
{
  QDir source(sourceDirectory);
  if (!source.exists())
  {
    if (errorMessage)
    {
      *errorMessage = "Cartella immagini test non trovata: " + sourceDirectory;
    }
    return false;
  }

  const QString destinationDirectory = cameraTestImagesPath(cameraId);
  if (!QDir().mkpath(destinationDirectory))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare cartella test: " + destinationDirectory;
    }
    return false;
  }

  QDir destination(destinationDirectory);
  int copied = 0;
  for (const QFileInfo& file : source.entryInfoList(imageNameFilters(), QDir::Files, QDir::Name))
  {
    const QString destinationPath = destination.filePath(file.fileName());
    if (file.absoluteFilePath() == QFileInfo(destinationPath).absoluteFilePath())
    {
      copied += 1;
      continue;
    }
    if (QFile::exists(destinationPath))
    {
      QFile::remove(destinationPath);
    }
    if (QFile::copy(file.absoluteFilePath(), destinationPath))
    {
      copied += 1;
    }
  }

  if (copied == 0)
  {
    if (errorMessage)
    {
      *errorMessage = "Nessuna immagine test copiata da: " + sourceDirectory;
    }
    return false;
  }

  return true;
}

QString RecipeManager::cameraRecipePath(const QString& cameraId) const
{
  return QDir(recipesRoot()).filePath(m_recipeId + "/cameras/" + cameraId + ".json");
}

QString RecipeManager::cameraImagesPath(const QString& cameraId, const QString& kind) const
{
  return QDir(recipesRoot()).filePath(m_recipeId + "/images/" + cameraId + "/" + kind);
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
