#include "RecipeManager.h"

#include "RecipeJsonUtils.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

using namespace RecipeJsonUtils;

namespace
{
QString classFolderName(const AiClassificationClassConfig& classConfig)
{
  const QString name = RecipeManager::normalizeRecipeId(classConfig.name);
  return QString("%1_%2")
    .arg(qMax(1, classConfig.id), 3, 10, QChar('0'))
    .arg(name.isEmpty() ? QString("class") : name);
}
}

QString RecipeManager::cameraAiClassificationRawImagesPath(const QString& cameraId) const
{
  return cameraImagesPath(cameraId, "ai/classification/raw");
}

QString RecipeManager::cameraAiClassificationClassImagesPath(
  const QString& cameraId,
  const AiClassificationClassConfig& classConfig) const
{
  return cameraImagesPath(cameraId, "ai/classification/classes/" + classFolderName(classConfig));
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
