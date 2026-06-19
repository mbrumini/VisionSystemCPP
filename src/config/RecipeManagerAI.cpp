#include "RecipeManager.h"

#include "RecipeJsonUtils.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

using namespace RecipeJsonUtils;

namespace
{
QString numberedAiFolderName(int id, const QString& displayName, const QString& fallbackName)
{
  const QString name = RecipeManager::normalizeRecipeId(displayName);
  return QString("%1_%2")
    .arg(qMax(1, id), 3, 10, QChar('0'))
    .arg(name.isEmpty() ? fallbackName : name);
}

QString classFolderName(const AiClassificationClassConfig& classConfig)
{
  return numberedAiFolderName(classConfig.id, classConfig.name, "class");
}

QString featureFolderName(const AiSegmentationFeatureConfig& featureConfig)
{
  return numberedAiFolderName(featureConfig.id, featureConfig.name, "feature");
}
}

QString RecipeManager::cameraAiClassificationRawImagesPath(const QString& cameraId) const
{
  return cameraImagesPath(cameraId, "ai/classification/raw");
}

QString RecipeManager::cameraAiLocalizationRawImagesPath(const QString& cameraId) const
{
  return cameraImagesPath(cameraId, "ai/localization_segmentation/raw");
}

QString RecipeManager::cameraAiClassificationClassImagesPath(
  const QString& cameraId,
  const AiClassificationClassConfig& classConfig) const
{
  return cameraImagesPath(cameraId, "ai/classification/classes/" + classFolderName(classConfig));
}

QString RecipeManager::cameraAiSegmentationRawImagesPath(const QString& cameraId) const
{
  return cameraImagesPath(cameraId, "ai/segmentation/raw");
}

QString RecipeManager::cameraAiSegmentationFeatureImagesPath(
  const QString& cameraId,
  const AiSegmentationFeatureConfig& featureConfig) const
{
  return cameraImagesPath(cameraId, "ai/segmentation/features/" + featureFolderName(featureConfig) + "/images");
}

QString RecipeManager::cameraAiSegmentationFeatureMasksPath(
  const QString& cameraId,
  const AiSegmentationFeatureConfig& featureConfig) const
{
  return cameraImagesPath(cameraId, "ai/segmentation/features/" + featureFolderName(featureConfig) + "/masks");
}

QString RecipeManager::cameraAiSegmentationFeatureLabelsPath(
  const QString& cameraId,
  const AiSegmentationFeatureConfig& featureConfig) const
{
  return cameraImagesPath(cameraId, "ai/segmentation/features/" + featureFolderName(featureConfig) + "/labels");
}

QVector<AiClassificationClassConfig> RecipeManager::loadAiClassificationClasses(const QString& cameraId) const
{
  QVector<AiClassificationClassConfig> classes;
  QJsonObject root;
  loadJsonObject(cameraRecipePath(cameraId), root);

  const QJsonArray items = root.value("tools").toObject()
    .value("aiClassification").toObject()
    .value("classes").toArray();
  for (const QJsonValue& value : items)
  {
    const QJsonObject object = value.toObject();
    AiClassificationClassConfig classConfig;
    classConfig.id = object.value("id").toInt();
    classConfig.name = object.value("name").toString().trimmed();
    if (classConfig.id > 0 && !classConfig.name.isEmpty())
    {
      classes.append(classConfig);
    }
  }

  return classes;
}

QVector<AiSegmentationFeatureConfig> RecipeManager::loadAiSegmentationFeatures(const QString& cameraId) const
{
  QVector<AiSegmentationFeatureConfig> features;
  QJsonObject root;
  loadJsonObject(cameraRecipePath(cameraId), root);

  const QJsonArray items = root.value("tools").toObject()
    .value("aiSegmentation").toObject()
    .value("features").toArray();
  for (const QJsonValue& value : items)
  {
    const QJsonObject object = value.toObject();
    AiSegmentationFeatureConfig featureConfig;
    featureConfig.id = object.value("id").toInt();
    featureConfig.name = object.value("name").toString().trimmed();
    if (featureConfig.id > 0 && !featureConfig.name.isEmpty())
    {
      features.append(featureConfig);
    }
  }

  return features;
}

bool RecipeManager::addAiClassificationClass(
  const QString& cameraId,
  const QString& className,
  AiClassificationClassConfig* createdClass,
  QString* errorMessage) const
{
  const QString normalizedName = className.trimmed();
  if (normalizedName.isEmpty())
  {
    if (errorMessage)
    {
      *errorMessage = "Nome classe AI non valido.";
    }
    return false;
  }

  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);
  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject ai = tools.value("aiClassification").toObject();
  ai["enabled"] = true;

  QJsonArray classes = ai.value("classes").toArray();
  int nextId = 1;
  for (const QJsonValue& value : classes)
  {
    const QJsonObject object = value.toObject();
    nextId = qMax(nextId, object.value("id").toInt() + 1);
    if (object.value("name").toString().compare(normalizedName, Qt::CaseInsensitive) == 0)
    {
      if (errorMessage)
      {
        *errorMessage = "Classe AI gia' presente: " + normalizedName;
      }
      return false;
    }
  }

  AiClassificationClassConfig classConfig;
  classConfig.id = nextId;
  classConfig.name = normalizedName;

  QJsonObject object;
  object["id"] = classConfig.id;
  object["name"] = classConfig.name;
  classes.append(object);

  ai["classes"] = classes;
  tools["aiClassification"] = ai;
  root["tools"] = tools;

  if (!saveJsonObject(path, root, errorMessage))
  {
    return false;
  }

  QDir().mkpath(cameraAiClassificationClassImagesPath(cameraId, classConfig));
  if (createdClass)
  {
    *createdClass = classConfig;
  }
  return true;
}

bool RecipeManager::addAiSegmentationFeature(
  const QString& cameraId,
  const QString& featureName,
  AiSegmentationFeatureConfig* createdFeature,
  QString* errorMessage) const
{
  const QString normalizedName = featureName.trimmed();
  if (normalizedName.isEmpty())
  {
    if (errorMessage)
    {
      *errorMessage = "Nome feature segmentazione non valido.";
    }
    return false;
  }

  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);
  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject ai = tools.value("aiSegmentation").toObject();
  ai["enabled"] = true;

  QJsonArray features = ai.value("features").toArray();
  int nextId = 1;
  for (const QJsonValue& value : features)
  {
    const QJsonObject object = value.toObject();
    nextId = qMax(nextId, object.value("id").toInt() + 1);
    if (object.value("name").toString().compare(normalizedName, Qt::CaseInsensitive) == 0)
    {
      if (errorMessage)
      {
        *errorMessage = "Feature segmentazione gia' presente: " + normalizedName;
      }
      return false;
    }
  }

  AiSegmentationFeatureConfig featureConfig;
  featureConfig.id = nextId;
  featureConfig.name = normalizedName;

  QJsonObject object;
  object["id"] = featureConfig.id;
  object["name"] = featureConfig.name;
  features.append(object);

  ai["features"] = features;
  tools["aiSegmentation"] = ai;
  root["tools"] = tools;

  if (!saveJsonObject(path, root, errorMessage))
  {
    return false;
  }

  QDir directory;
  directory.mkpath(cameraAiSegmentationFeatureImagesPath(cameraId, featureConfig));
  directory.mkpath(cameraAiSegmentationFeatureMasksPath(cameraId, featureConfig));
  directory.mkpath(cameraAiSegmentationFeatureLabelsPath(cameraId, featureConfig));
  if (createdFeature)
  {
    *createdFeature = featureConfig;
  }
  return true;
}

QString RecipeManager::loadAiClassificationActiveModelPath(const QString& cameraId) const
{
  QJsonObject root;
  loadJsonObject(cameraRecipePath(cameraId), root);

  return root.value("tools").toObject()
    .value("aiClassification").toObject()
    .value("activeModelPath").toString().trimmed();
}

bool RecipeManager::saveAiClassificationActiveModelPath(
  const QString& cameraId,
  const QString& modelPath,
  QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);
  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject ai = tools.value("aiClassification").toObject();
  ai["enabled"] = true;
  ai["activeModelPath"] = modelPath;
  tools["aiClassification"] = ai;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}
