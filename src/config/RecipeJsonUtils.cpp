#include "RecipeJsonUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QCoreApplication>

namespace RecipeJsonUtils
{
QString appRootPath()
{
  const QString runtimeRoot = QCoreApplication::applicationDirPath();
  if (QFile::exists(QDir(runtimeRoot).filePath("config/cameras.json")) ||
      QFileInfo::exists(QDir(runtimeRoot).filePath("recipes")))
  {
    return runtimeRoot;
  }

  return QString::fromUtf8(PROJECT_SOURCE_DIR);
}

QString appPath(const QString& relativePath)
{
  return QDir(appRootPath()).filePath(relativePath);
}

QString recipesRoot()
{
  return appPath("recipes");
}

QString appSettingsPath()
{
  return appPath("config/app_settings.json");
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

QString normalizedSurfaceLocalizationMethod(const QString& method)
{
  if (method == "edge" || method == "edgePca" || method == "massPca" || method == "model" || method == "aiYolo" || method == "two_circles_axis")
  {
    return method;
  }

  return "threshold";
}

void pruneSurfaceLocalizationForMethod(QJsonObject& surfaceLocalization, const QString& method)
{
  const QString normalizedMethod = normalizedSurfaceLocalizationMethod(method);
  surfaceLocalization["method"] = normalizedMethod;

  if (normalizedMethod != "threshold" && normalizedMethod != "massPca")
  {
    surfaceLocalization.remove("annulus");
    surfaceLocalization.remove("threshold");
  }

  if (normalizedMethod != "edge" && normalizedMethod != "edgePca")
  {
    surfaceLocalization.remove("edge");
  }

  if (normalizedMethod != "model")
  {
    surfaceLocalization.remove("model");
  }

  if (normalizedMethod != "aiYolo")
  {
    surfaceLocalization.remove("aiYolo");
  }

  if (normalizedMethod != "two_circles_axis")
  {
    surfaceLocalization.remove("strategy");
  }
}

void setGeometryArray(QJsonObject& geometries, const QString& key, const QJsonArray& values)
{
  if (values.isEmpty())
  {
    geometries.remove(key);
    return;
  }

  geometries[key] = values;
}

void writeGeometriesTool(QJsonObject& tools, QJsonObject geometries)
{
  static const QStringList geometryKeys = {"lines", "points", "circles", "arcs", "constructed"};

  for (const QString& key : geometryKeys)
  {
    if (geometries.value(key).toArray().isEmpty())
    {
      geometries.remove(key);
    }
  }

  bool hasGeometry = false;
  for (const QString& key : geometryKeys)
  {
    if (geometries.contains(key))
    {
      hasGeometry = true;
      break;
    }
  }

  if (!hasGeometry)
  {
    tools.remove("geometries");
    return;
  }

  geometries["enabled"] = true;
  tools["geometries"] = geometries;
}

QString nextPrefixedId(const QString& prefix, const QStringList& existingIds)
{
  const QString pattern = prefix + "_";
  int maxIndex = 0;
  for (const QString& id : existingIds)
  {
    if (!id.startsWith(pattern))
    {
      continue;
    }

    bool ok = false;
    const int index = id.mid(pattern.size()).toInt(&ok);
    if (ok && index > maxIndex)
    {
      maxIndex = index;
    }
  }

  return QString("%1_%2").arg(prefix).arg(maxIndex + 1);
}

QString ensureUniquePrefixedId(const QString& prefix, const QString& preferredId, QStringList& assignedIds)
{
  QString id = preferredId.trimmed();
  if (id.isEmpty() || assignedIds.contains(id))
  {
    id = nextPrefixedId(prefix, assignedIds);
  }

  assignedIds.append(id);
  return id;
}
}
