#include "RecipeManager.h"

#include "RecipeJsonUtils.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtGlobal>

using namespace RecipeJsonUtils;

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

  setGeometryArray(geometries, "lines", lines);
  writeGeometriesTool(tools, geometries);
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

QVector<GeometryPointRecipeConfig> RecipeManager::loadGeometryPoints(const QString& cameraId) const
{
  QVector<GeometryPointRecipeConfig> configs;

  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return configs;
  }

  const QJsonArray points = root.value("tools").toObject()
    .value("geometries").toObject()
    .value("points").toArray();

  for (const QJsonValue& value : points)
  {
    const QJsonObject point = value.toObject();
    GeometryPointRecipeConfig config;
    config.id = point.value("id").toString(QString("point_%1").arg(configs.size() + 1));
    if (config.id.isEmpty())
    {
      continue;
    }

    config.enabled = point.value("enabled").toBool(false);
    config.partStart = pointFFromJson(point.value("partStart").toObject());
    config.partEnd = pointFFromJson(point.value("partEnd").toObject());
    config.edgeSensitivity = point.value("edgeSensitivity").toInt(config.edgeSensitivity);
    config.useSubpixel = point.value("useSubpixel").toBool(config.useSubpixel);
    config.transition = point.value("transition").toString(config.transition);
    config.pickMode = point.value("pickMode").toString(config.pickMode);
    configs.append(config);
  }

  return configs;
}

GeometryPointRecipeConfig RecipeManager::loadGeometryPoint(const QString& cameraId, const QString& pointId) const
{
  const QVector<GeometryPointRecipeConfig> configs = loadGeometryPoints(cameraId);
  for (const GeometryPointRecipeConfig& config : configs)
  {
    if (config.id == pointId)
    {
      return config;
    }
  }

  GeometryPointRecipeConfig config;
  config.id = pointId;
  return config;
}

bool RecipeManager::saveGeometryPoints(const QString& cameraId, const QVector<GeometryPointRecipeConfig>& configs, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject geometries = tools.value("geometries").toObject();

  QJsonArray points;
  for (const GeometryPointRecipeConfig& config : configs)
  {
    QJsonObject point;
    point["enabled"] = config.enabled;
    point["id"] = config.id.isEmpty() ? QString("point_%1").arg(points.size() + 1) : config.id;
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

  setGeometryArray(geometries, "points", points);
  writeGeometriesTool(tools, geometries);
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveGeometryPoint(const QString& cameraId, const GeometryPointRecipeConfig& config, QString* errorMessage) const
{
  QVector<GeometryPointRecipeConfig> configs = loadGeometryPoints(cameraId);
  bool replaced = false;
  for (GeometryPointRecipeConfig& existing : configs)
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

  return saveGeometryPoints(cameraId, configs, errorMessage);
}

QVector<GeometryCircleRecipeConfig> RecipeManager::loadGeometryCircles(const QString& cameraId) const
{
  QVector<GeometryCircleRecipeConfig> configs;

  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return configs;
  }

  const QJsonArray circles = root.value("tools").toObject()
    .value("geometries").toObject()
    .value("circles").toArray();

  for (const QJsonValue& value : circles)
  {
    const QJsonObject circle = value.toObject();
    GeometryCircleRecipeConfig config;
    config.id = circle.value("id").toString(QString("circle_%1").arg(configs.size() + 1));
    if (config.id.isEmpty())
    {
      continue;
    }

    config.enabled = circle.value("enabled").toBool(false);
    config.partCenter = pointFFromJson(circle.value("partCenter").toObject());
    config.radius = circle.value("radius").toDouble(config.radius);
    config.innerBand = circle.value("innerBand").toInt(config.innerBand);
    config.outerBand = circle.value("outerBand").toInt(config.outerBand);
    config.edgeSensitivity = circle.value("edgeSensitivity").toInt(config.edgeSensitivity);
    config.edgeCleanupDerivative = circle.value("edgeCleanupDerivative").toInt(config.edgeCleanupDerivative);
    config.edgeStatisticalFilter = circle.value("edgeStatisticalFilter").toInt(config.edgeStatisticalFilter);
    config.useSubpixel = circle.value("useSubpixel").toBool(config.useSubpixel);
    config.scanDirection = circle.value("scanDirection").toString(config.scanDirection);
    config.transition = circle.value("transition").toString(config.transition);
    config.pickMode = circle.value("pickMode").toString(config.pickMode);
    configs.append(config);
  }

  return configs;
}

bool RecipeManager::saveGeometryCircles(const QString& cameraId, const QVector<GeometryCircleRecipeConfig>& configs, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject geometries = tools.value("geometries").toObject();

  QJsonArray circles;
  for (const GeometryCircleRecipeConfig& config : configs)
  {
    QJsonObject circle;
    circle["enabled"] = config.enabled;
    circle["id"] = config.id.isEmpty() ? QString("circle_%1").arg(circles.size() + 1) : config.id;
    circle["type"] = "edge_circle";
    circle["coordinateSpace"] = "part";
    circle["partCenter"] = pointFToJson(config.partCenter);
    circle["radius"] = qMax(1.0, config.radius);
    circle["innerBand"] = qBound(1, config.innerBand, 500);
    circle["outerBand"] = qBound(1, config.outerBand, 500);
    circle["edgeSensitivity"] = qBound(1, config.edgeSensitivity, 255);
    circle["edgeCleanupDerivative"] = qBound(0, config.edgeCleanupDerivative, 100);
    circle["edgeStatisticalFilter"] = qBound(0, config.edgeStatisticalFilter, 100);
    circle["useSubpixel"] = config.useSubpixel;
    circle["scanDirection"] = config.scanDirection == "normal_negative" ? "normal_negative" : "normal_positive";
    circle["transition"] = config.transition == "dark_to_light" ? "dark_to_light" : "light_to_dark";
    circle["pickMode"] = config.pickMode == "last" || config.pickMode == "best" ? config.pickMode : "first";
    circles.append(circle);
  }

  setGeometryArray(geometries, "circles", circles);
  writeGeometriesTool(tools, geometries);
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

QVector<GeometryArcRecipeConfig> RecipeManager::loadGeometryArcs(const QString& cameraId) const
{
  QVector<GeometryArcRecipeConfig> configs;

  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return configs;
  }

  const QJsonArray arcs = root.value("tools").toObject()
    .value("geometries").toObject()
    .value("arcs").toArray();

  for (const QJsonValue& value : arcs)
  {
    const QJsonObject arc = value.toObject();
    GeometryArcRecipeConfig config;
    config.id = arc.value("id").toString(QString("arc_%1").arg(configs.size() + 1));
    if (config.id.isEmpty())
    {
      continue;
    }

    config.enabled = arc.value("enabled").toBool(false);
    config.partCenter = pointFFromJson(arc.value("partCenter").toObject());
    config.partStart = pointFFromJson(arc.value("partStart").toObject());
    config.partEnd = pointFFromJson(arc.value("partEnd").toObject());
    config.radius = arc.value("radius").toDouble(config.radius);
    config.startAngleRadians = arc.value("startAngleRadians").toDouble(config.startAngleRadians);
    config.endAngleRadians = arc.value("endAngleRadians").toDouble(config.endAngleRadians);
    config.innerBand = arc.value("innerBand").toInt(config.innerBand);
    config.outerBand = arc.value("outerBand").toInt(config.outerBand);
    config.edgeSensitivity = arc.value("edgeSensitivity").toInt(config.edgeSensitivity);
    config.edgeCleanupDerivative = arc.value("edgeCleanupDerivative").toInt(config.edgeCleanupDerivative);
    config.edgeStatisticalFilter = arc.value("edgeStatisticalFilter").toInt(config.edgeStatisticalFilter);
    config.useSubpixel = arc.value("useSubpixel").toBool(config.useSubpixel);
    config.scanDirection = arc.value("scanDirection").toString(config.scanDirection);
    config.transition = arc.value("transition").toString(config.transition);
    config.pickMode = arc.value("pickMode").toString(config.pickMode);
    configs.append(config);
  }

  return configs;
}

bool RecipeManager::saveGeometryArcs(const QString& cameraId, const QVector<GeometryArcRecipeConfig>& configs, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject geometries = tools.value("geometries").toObject();

  QJsonArray arcs;
  for (const GeometryArcRecipeConfig& config : configs)
  {
    QJsonObject arc;
    arc["enabled"] = config.enabled;
    arc["id"] = config.id.isEmpty() ? QString("arc_%1").arg(arcs.size() + 1) : config.id;
    arc["type"] = "edge_arc";
    arc["coordinateSpace"] = "part";
    arc["partCenter"] = pointFToJson(config.partCenter);
    arc["partStart"] = pointFToJson(config.partStart);
    arc["partEnd"] = pointFToJson(config.partEnd);
    arc["radius"] = qMax(1.0, config.radius);
    arc["startAngleRadians"] = config.startAngleRadians;
    arc["endAngleRadians"] = config.endAngleRadians;
    arc["innerBand"] = qBound(1, config.innerBand, 500);
    arc["outerBand"] = qBound(1, config.outerBand, 500);
    arc["edgeSensitivity"] = qBound(1, config.edgeSensitivity, 255);
    arc["edgeCleanupDerivative"] = qBound(0, config.edgeCleanupDerivative, 100);
    arc["edgeStatisticalFilter"] = qBound(0, config.edgeStatisticalFilter, 100);
    arc["useSubpixel"] = config.useSubpixel;
    arc["scanDirection"] = config.scanDirection == "normal_negative" ? "normal_negative" : "normal_positive";
    arc["transition"] = config.transition == "dark_to_light" ? "dark_to_light" : "light_to_dark";
    arc["pickMode"] = config.pickMode == "last" || config.pickMode == "best" ? config.pickMode : "first";
    arcs.append(arc);
  }

  setGeometryArray(geometries, "arcs", arcs);
  writeGeometriesTool(tools, geometries);
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

QVector<ConstructedGeometryRecipeConfig> RecipeManager::loadConstructedGeometries(const QString& cameraId) const
{
  QVector<ConstructedGeometryRecipeConfig> configs;

  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return configs;
  }

  const QJsonArray constructed = root.value("tools").toObject()
    .value("geometries").toObject()
    .value("constructed").toArray();

  for (const QJsonValue& value : constructed)
  {
    const QJsonObject item = value.toObject();
    ConstructedGeometryRecipeConfig config;
    config.id = item.value("id").toString(QString("constructed_%1").arg(configs.size() + 1));
    config.enabled = item.value("enabled").toBool(config.enabled);
    config.type = item.value("type").toString();
    config.sourceAId = item.value("sourceAId").toString();
    config.sourceBId = item.value("sourceBId").toString();
    config.offset = item.value("offset").toDouble(config.offset);
    if (config.id.isEmpty() || config.type.isEmpty() || config.sourceAId.isEmpty())
    {
      continue;
    }

    configs.append(config);
  }

  return configs;
}

bool RecipeManager::saveConstructedGeometries(const QString& cameraId,
                                              const QVector<ConstructedGeometryRecipeConfig>& configs,
                                              QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject geometries = tools.value("geometries").toObject();

  QJsonArray constructed;
  for (const ConstructedGeometryRecipeConfig& config : configs)
  {
    if (config.type.isEmpty() || config.sourceAId.isEmpty())
    {
      continue;
    }

    QJsonObject item;
    item["enabled"] = config.enabled;
    item["id"] = config.id.isEmpty() ? QString("constructed_%1").arg(constructed.size() + 1) : config.id;
    item["type"] = config.type;
    item["sourceAId"] = config.sourceAId;
    if (!config.sourceBId.isEmpty())
    {
      item["sourceBId"] = config.sourceBId;
    }
    if (config.type == "offset_line")
    {
      item["offset"] = config.offset;
    }
    constructed.append(item);
  }

  setGeometryArray(geometries, "constructed", constructed);
  writeGeometriesTool(tools, geometries);
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::appendConstructedGeometry(const QString& cameraId,
                                              const ConstructedGeometryRecipeConfig& config,
                                              QString* errorMessage) const
{
  QVector<ConstructedGeometryRecipeConfig> configs = loadConstructedGeometries(cameraId);
  ConstructedGeometryRecipeConfig saved = config;
  if (saved.id.isEmpty())
  {
    saved.id = QString("constructed_%1").arg(configs.size() + 1);
  }
  configs.append(saved);
  return saveConstructedGeometries(cameraId, configs, errorMessage);
}
