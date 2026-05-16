#include "RecipeManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QString recipesRoot()
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("recipes");
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

RecipeManager::RecipeManager(QString recipeId)
  : m_recipeId(std::move(recipeId))
{
}

QString RecipeManager::recipeId() const
{
  return m_recipeId;
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
