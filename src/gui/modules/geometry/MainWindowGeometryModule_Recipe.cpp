#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"

#include "config/RecipeJsonUtils.h"

#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "processing/geometry/EdgePointDetector.h"
#include "util/AsyncExecutor.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

using AsyncExecutor::runAsyncTask;

namespace
{
QString scanDirectionToRecipe(EdgeLineScanDirection direction)
{
  return direction == EdgeLineScanDirection::NormalNegative ? "normal_negative" : "normal_positive";
}

EdgeLineScanDirection scanDirectionFromRecipe(const QString& value)
{
  return value == "normal_negative" ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
}

QString transitionToRecipe(EdgeLineTransition transition)
{
  return transition == EdgeLineTransition::DarkToLight ? "dark_to_light" : "light_to_dark";
}

EdgeLineTransition transitionFromRecipe(const QString& value)
{
  return value == "dark_to_light" ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
}

QString pickModeToRecipe(EdgeLinePickMode mode)
{
  if (mode == EdgeLinePickMode::Last)
  {
    return "last";
  }

  if (mode == EdgeLinePickMode::Best)
  {
    return "best";
  }

  return "first";
}

EdgeLinePickMode pickModeFromRecipe(const QString& value)
{
  if (value == "last")
  {
    return EdgeLinePickMode::Last;
  }

  if (value == "best")
  {
    return EdgeLinePickMode::Best;
  }

  return EdgeLinePickMode::First;
}

template<typename Config>
QString nextGeometryId(const QString& prefix, const QVector<Config>& configs)
{
  QStringList existingIds;
  existingIds.reserve(configs.size());
  for (const Config& config : configs)
  {
    existingIds.append(config.id);
  }
  return RecipeJsonUtils::nextPrefixedId(prefix, existingIds);
}

}

MainWindowGeometryModule::MainWindowGeometryModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

LineGeometryMouseController& MainWindowGeometryModule::lineMouseController(const QString& cameraId)
{
  return m_lineMouseControllers[cameraId];
}

LineGeometryMouseController& MainWindowGeometryModule::pointMouseController(const QString& cameraId)
{
  return m_pointMouseControllers[cameraId];
}

void MainWindowGeometryModule::reloadRecipeState()
{
  m_lineConfigs.clear();
  m_activeLineIndexes.clear();
  m_pointConfigs.clear();
  m_activePointIndexes.clear();
  m_circleConfigs.clear();
  m_activeCircleIndexes.clear();
  m_arcConfigs.clear();
  m_activeArcIndexes.clear();
  m_lineMouseControllers.clear();
  m_pointMouseControllers.clear();
  m_drawingTarget = DrawingTarget::None;

  for (const CameraConfig& camera : config().activeCameras())
  {
    loadGeometryLinesRecipe(camera);
    loadGeometryPointRecipe(camera);
    loadGeometryCirclesRecipe(camera);
    loadGeometryArcsRecipe(camera);
  }
}

GeometryLineRuntimeConfig& MainWindowGeometryModule::activeGeometryLineConfig(const QString& cameraId)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_lineConfigs[cameraId];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig first;
    first.id = "line_1";
    lines.append(first);
    m_activeLineIndexes[cameraId] = 0;
    return lines[0];
  }

  int index = qBound(0, m_activeLineIndexes.value(cameraId, 0), lines.size() - 1);
  m_activeLineIndexes[cameraId] = index;
  return lines[index];
}

const GeometryLineRuntimeConfig& MainWindowGeometryModule::activeGeometryLineConfig(const QString& cameraId) const
{
  const QVector<GeometryLineRuntimeConfig>& lines = m_lineConfigs.value(cameraId);
  static const GeometryLineRuntimeConfig fallback;
  if (lines.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeLineIndexes.value(cameraId, 0), lines.size() - 1);
  return lines[index];
}

void MainWindowGeometryModule::addGeometryLine(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_lineConfigs[camera.id];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig first;
    first.id = "line_1";
    lines.append(first);
    m_activeLineIndexes[camera.id] = 0;
    return;
  }

  GeometryLineRuntimeConfig& activeLine = activeGeometryLineConfig(camera.id);
  if (!activeLine.hasImageLine && !activeLine.hasLine)
  {
    return;
  }

  GeometryLineRuntimeConfig line = activeGeometryLineConfig(camera.id);
  line.id = nextGeometryId("line", lines);
  line.hasImageLine = false;
  line.hasLine = false;
  lines.append(line);
  m_activeLineIndexes[camera.id] = lines.size() - 1;
}

void MainWindowGeometryModule::loadGeometryPointRecipe(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_pointConfigs[camera.id];
  if (!points.isEmpty())
  {
    return;
  }

  const QVector<GeometryPointRecipeConfig> pointRecipes = recipes().loadGeometryPoints(camera.id);
  QStringList usedIds;
  for (const GeometryPointRecipeConfig& recipe : pointRecipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryPointRuntimeConfig point;
    point.enabled = recipe.enabled;
    point.id = RecipeJsonUtils::ensureUniquePrefixedId("point", recipe.id, usedIds);
    point.alias = recipe.alias;
    point.partStart = cv::Point2d(recipe.partStart.x(), recipe.partStart.y());
    point.partEnd = cv::Point2d(recipe.partEnd.x(), recipe.partEnd.y());
    point.edgeSensitivity = recipe.edgeSensitivity;
    point.useSubpixel = recipe.useSubpixel;
    point.transition = transitionFromRecipe(recipe.transition);
    point.pickMode = pickModeFromRecipe(recipe.pickMode);
    point.hasGuide = true;
    points.append(point);
  }

  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }
  m_activePointIndexes[camera.id] = qBound(0, m_activePointIndexes.value(camera.id, 0), points.size() - 1);
}

void MainWindowGeometryModule::loadGeometryCirclesRecipe(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_circleConfigs[camera.id];
  if (!circles.isEmpty())
  {
    return;
  }

  const QVector<GeometryCircleRecipeConfig> circleRecipes = recipes().loadGeometryCircles(camera.id);
  QStringList usedIds;
  for (const GeometryCircleRecipeConfig& recipe : circleRecipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryCircleRuntimeConfig circle;
    circle.enabled = recipe.enabled;
    circle.id = RecipeJsonUtils::ensureUniquePrefixedId("circle", recipe.id, usedIds);
    circle.alias = recipe.alias;
    circle.radius = recipe.radius;
    circle.innerBand = recipe.innerBand;
    circle.outerBand = recipe.outerBand;
    circle.edgeSensitivity = recipe.edgeSensitivity;
    circle.edgeCleanupDerivative = recipe.edgeCleanupDerivative;
    circle.edgeStatisticalFilter = recipe.edgeStatisticalFilter;
    circle.useSubpixel = recipe.useSubpixel;
    circle.scanDirection = scanDirectionFromRecipe(recipe.scanDirection);
    circle.transition = transitionFromRecipe(recipe.transition);
    circle.pickMode = pickModeFromRecipe(recipe.pickMode);
    if (recipe.coordinateSpace == "image")
    {
      circle.imageCenter = cv::Point2d(recipe.imageCenter.x(), recipe.imageCenter.y());
      circle.hasImageCircle = true;
    }
    else
    {
      circle.partCenter = cv::Point2d(recipe.partCenter.x(), recipe.partCenter.y());
      circle.hasCircle = true;
    }
    circles.append(circle);
  }

  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }
  m_activeCircleIndexes[camera.id] = qBound(0, m_activeCircleIndexes.value(camera.id, 0), circles.size() - 1);
}

void MainWindowGeometryModule::saveGeometryPointRecipe(const CameraConfig& camera)
{
  QVector<GeometryPointRecipeConfig> recipeList;
  for (const GeometryPointRuntimeConfig& point : m_pointConfigs[camera.id])
  {
    if (!point.hasGuide)
    {
      continue;
    }

    GeometryPointRecipeConfig recipe;
    recipe.enabled = point.enabled;
    recipe.id = point.id;
    recipe.alias = point.alias;
    recipe.partStart = QPointF(point.partStart.x, point.partStart.y);
    recipe.partEnd = QPointF(point.partEnd.x, point.partEnd.y);
    recipe.edgeSensitivity = point.edgeSensitivity;
    recipe.useSubpixel = point.useSubpixel;
    recipe.transition = transitionToRecipe(point.transition);
    recipe.pickMode = pickModeToRecipe(point.pickMode);
    recipeList.append(recipe);
  }

  QString error;
  if (!recipes().saveGeometryPoints(camera.id, recipeList, &error))
  {
    log(error);
  }
}

void MainWindowGeometryModule::saveGeometryCirclesRecipe(const CameraConfig& camera)
{
  QVector<GeometryCircleRecipeConfig> recipeList;
  for (const GeometryCircleRuntimeConfig& circle : m_circleConfigs[camera.id])
  {
    if (!circle.hasCircle && !circle.hasImageCircle)
    {
      continue;
    }

    GeometryCircleRecipeConfig recipe;
    recipe.enabled = circle.enabled;
    recipe.id = circle.id;
    recipe.alias = circle.alias;
    if (circle.hasCircle)
    {
      recipe.coordinateSpace = "part";
      recipe.partCenter = QPointF(circle.partCenter.x, circle.partCenter.y);
    }
    else
    {
      recipe.coordinateSpace = "image";
      recipe.imageCenter = QPointF(circle.imageCenter.x, circle.imageCenter.y);
    }
    recipe.radius = circle.radius;
    recipe.innerBand = circle.innerBand;
    recipe.outerBand = circle.outerBand;
    recipe.edgeSensitivity = circle.edgeSensitivity;
    recipe.edgeCleanupDerivative = circle.edgeCleanupDerivative;
    recipe.edgeStatisticalFilter = circle.edgeStatisticalFilter;
    recipe.useSubpixel = circle.useSubpixel;
    recipe.scanDirection = scanDirectionToRecipe(circle.scanDirection);
    recipe.transition = transitionToRecipe(circle.transition);
    recipe.pickMode = pickModeToRecipe(circle.pickMode);
    recipeList.append(recipe);
  }

  if (recipeList.isEmpty())
  {
    return;
  }

  QString error;
  if (!recipes().saveGeometryCircles(camera.id, recipeList, &error))
  {
    log(error);
  }
}

void MainWindowGeometryModule::addGeometryPoint(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_pointConfigs[camera.id];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
    m_activePointIndexes[camera.id] = 0;
    return;
  }

  GeometryPointRuntimeConfig& activePoint = activeGeometryPointConfig(camera.id);
  if (!activePoint.hasImageGuide && !activePoint.hasGuide)
  {
    return;
  }

  GeometryPointRuntimeConfig point = activeGeometryPointConfig(camera.id);
  point.id = nextGeometryId("point", points);
  point.alias.clear();
  point.hasImageGuide = false;
  point.hasGuide = false;
  points.append(point);
  m_activePointIndexes[camera.id] = points.size() - 1;
}

void MainWindowGeometryModule::addGeometryCircle(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_circleConfigs[camera.id];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
    m_activeCircleIndexes[camera.id] = 0;
    return;
  }

  GeometryCircleRuntimeConfig& activeCircle = activeGeometryCircleConfig(camera.id);
  if (!activeCircle.hasImageCircle && !activeCircle.hasCircle)
  {
    return;
  }

  GeometryCircleRuntimeConfig circle = activeCircle;
  circle.id = nextGeometryId("circle", circles);
  circle.alias.clear();
  circle.hasImageCircle = false;
  circle.hasCircle = false;
  circles.append(circle);
  m_activeCircleIndexes[camera.id] = circles.size() - 1;
}

GeometryPointRuntimeConfig& MainWindowGeometryModule::activeGeometryPointConfig(const QString& cameraId)
{
  QVector<GeometryPointRuntimeConfig>& points = m_pointConfigs[cameraId];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }

  int index = qBound(0, m_activePointIndexes.value(cameraId, 0), points.size() - 1);
  m_activePointIndexes[cameraId] = index;
  return points[index];
}

GeometryCircleRuntimeConfig& MainWindowGeometryModule::activeGeometryCircleConfig(const QString& cameraId)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_circleConfigs[cameraId];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }

  int index = qBound(0, m_activeCircleIndexes.value(cameraId, 0), circles.size() - 1);
  m_activeCircleIndexes[cameraId] = index;
  return circles[index];
}

const GeometryPointRuntimeConfig& MainWindowGeometryModule::activeGeometryPointConfig(const QString& cameraId) const
{
  const QVector<GeometryPointRuntimeConfig>& points = m_pointConfigs.value(cameraId);
  static const GeometryPointRuntimeConfig fallback;
  if (points.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activePointIndexes.value(cameraId, 0), points.size() - 1);
  return points[index];
}

const GeometryCircleRuntimeConfig& MainWindowGeometryModule::activeGeometryCircleConfig(const QString& cameraId) const
{
  const QVector<GeometryCircleRuntimeConfig>& circles = m_circleConfigs.value(cameraId);
  static const GeometryCircleRuntimeConfig fallback;
  if (circles.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeCircleIndexes.value(cameraId, 0), circles.size() - 1);
  return circles[index];
}

void MainWindowGeometryModule::loadGeometryLinesRecipe(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_lineConfigs[camera.id];
  if (!lines.isEmpty())
  {
    return;
  }

  const QVector<GeometryLineRecipeConfig> lineRecipes = recipes().loadGeometryLines(camera.id);
  QStringList usedIds;
  for (const GeometryLineRecipeConfig& recipe : lineRecipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryLineRuntimeConfig line;
    line.id = RecipeJsonUtils::ensureUniquePrefixedId("line", recipe.id, usedIds);
    line.alias = recipe.alias;
    line.enabled = recipe.enabled;
    line.partStart = cv::Point2d(recipe.partStart.x(), recipe.partStart.y());
    line.partEnd = cv::Point2d(recipe.partEnd.x(), recipe.partEnd.y());
    line.bandHalfWidth = recipe.bandHalfWidth;
    line.edgeSensitivity = recipe.edgeSensitivity;
    line.edgeCleanupDerivative = recipe.edgeCleanupDerivative;
    line.edgeStatisticalFilter = recipe.edgeStatisticalFilter;
    line.useSubpixel = recipe.useSubpixel;
    line.scanDirection = scanDirectionFromRecipe(recipe.scanDirection);
    line.transition = transitionFromRecipe(recipe.transition);
    line.pickMode = pickModeFromRecipe(recipe.pickMode);
    line.hasLine = true;
    lines.append(line);
  }

  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
  }
  m_activeLineIndexes[camera.id] = qBound(0, m_activeLineIndexes.value(camera.id, 0), lines.size() - 1);
}

void MainWindowGeometryModule::saveGeometryLinesRecipe(const CameraConfig& camera)
{
  QVector<GeometryLineRecipeConfig> recipeList;
  for (const GeometryLineRuntimeConfig& line : m_lineConfigs[camera.id])
  {
    if (!line.hasLine)
    {
      continue;
    }

    GeometryLineRecipeConfig recipe;
    recipe.enabled = line.enabled;
    recipe.id = line.id;
    recipe.alias = line.alias;
    recipe.partStart = QPointF(line.partStart.x, line.partStart.y);
    recipe.partEnd = QPointF(line.partEnd.x, line.partEnd.y);
    recipe.bandHalfWidth = line.bandHalfWidth;
    recipe.edgeSensitivity = line.edgeSensitivity;
    recipe.edgeCleanupDerivative = line.edgeCleanupDerivative;
    recipe.edgeStatisticalFilter = line.edgeStatisticalFilter;
    recipe.useSubpixel = line.useSubpixel;
    recipe.scanDirection = scanDirectionToRecipe(line.scanDirection);
    recipe.transition = transitionToRecipe(line.transition);
    recipe.pickMode = pickModeToRecipe(line.pickMode);
    recipeList.append(recipe);
  }

  QString error;
  if (!recipes().saveGeometryLines(camera.id, recipeList, &error))
  {
    log(error);
  }
}
