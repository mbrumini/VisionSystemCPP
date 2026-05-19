#include "MainWindow.h"

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
}
GeometryLineRuntimeConfig& MainWindow::activeGeometryLineConfig(const QString& cameraId)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[cameraId];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig first;
    first.id = "line_1";
    lines.append(first);
    m_activeGeometryLineIndexes[cameraId] = 0;
    return lines[0];
  }

  int index = qBound(0, m_activeGeometryLineIndexes.value(cameraId, 0), lines.size() - 1);
  m_activeGeometryLineIndexes[cameraId] = index;
  return lines[index];
}

const GeometryLineRuntimeConfig& MainWindow::activeGeometryLineConfig(const QString& cameraId) const
{
  const QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs.value(cameraId);
  static const GeometryLineRuntimeConfig fallback;
  if (lines.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeGeometryLineIndexes.value(cameraId, 0), lines.size() - 1);
  return lines[index];
}

void MainWindow::addGeometryLine(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig first;
    first.id = "line_1";
    lines.append(first);
    m_activeGeometryLineIndexes[camera.id] = 0;
    return;
  }

  GeometryLineRuntimeConfig& activeLine = activeGeometryLineConfig(camera.id);
  if (!activeLine.hasImageLine && !activeLine.hasLine)
  {
    return;
  }

  GeometryLineRuntimeConfig line = activeGeometryLineConfig(camera.id);
  line.id = QString("line_%1").arg(lines.size() + 1);
  line.hasImageLine = false;
  line.hasLine = false;
  lines.append(line);
  m_activeGeometryLineIndexes[camera.id] = lines.size() - 1;
}

void MainWindow::removeActiveGeometryLine(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
    m_activeGeometryLineIndexes[camera.id] = 0;
    showGeometryLinePanel(camera);
    return;
  }

  const int index = qBound(0, m_activeGeometryLineIndexes.value(camera.id, 0), lines.size() - 1);
  lines.removeAt(index);
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
  }

  m_activeGeometryLineIndexes[camera.id] = qBound(0, index, lines.size() - 1);
  m_lineGeometryMouseControllers[camera.id].begin(activeGeometryLineConfig(camera.id).bandHalfWidth);
  saveGeometryLinesRecipe(camera);
  updateGeometryLineOverlay(camera);
  showGeometryLinePanel(camera);
}

void MainWindow::loadGeometryPointRecipe(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  if (!points.isEmpty())
  {
    return;
  }

  const QVector<GeometryPointRecipeConfig> recipes = m_recipeManager.loadGeometryPoints(camera.id);
  for (const GeometryPointRecipeConfig& recipe : recipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryPointRuntimeConfig point;
    point.enabled = recipe.enabled;
    point.id = recipe.id;
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
  m_activeGeometryPointIndexes[camera.id] = qBound(0, m_activeGeometryPointIndexes.value(camera.id, 0), points.size() - 1);
}

void MainWindow::loadGeometryCirclesRecipe(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[camera.id];
  if (!circles.isEmpty())
  {
    return;
  }

  const QVector<GeometryCircleRecipeConfig> recipes = m_recipeManager.loadGeometryCircles(camera.id);
  for (const GeometryCircleRecipeConfig& recipe : recipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryCircleRuntimeConfig circle;
    circle.enabled = recipe.enabled;
    circle.id = recipe.id;
    circle.partCenter = cv::Point2d(recipe.partCenter.x(), recipe.partCenter.y());
    circle.radius = recipe.radius;
    circle.innerBand = recipe.innerBand;
    circle.outerBand = recipe.outerBand;
    circle.edgeSensitivity = recipe.edgeSensitivity;
    circle.edgeCleanupDerivative = recipe.edgeCleanupDerivative;
    circle.edgeStatisticalFilter = recipe.edgeStatisticalFilter;
    circle.useSubpixel = recipe.useSubpixel;
    circle.transition = transitionFromRecipe(recipe.transition);
    circle.pickMode = pickModeFromRecipe(recipe.pickMode);
    circle.hasCircle = true;
    circles.append(circle);
  }

  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }
  m_activeGeometryCircleIndexes[camera.id] = qBound(0, m_activeGeometryCircleIndexes.value(camera.id, 0), circles.size() - 1);
}

void MainWindow::saveGeometryPointRecipe(const CameraConfig& camera)
{
  QVector<GeometryPointRecipeConfig> recipes;
  for (const GeometryPointRuntimeConfig& point : m_geometryPointConfigs[camera.id])
  {
    if (!point.hasGuide)
    {
      continue;
    }

    GeometryPointRecipeConfig recipe;
    recipe.enabled = point.enabled;
    recipe.id = point.id;
    recipe.partStart = QPointF(point.partStart.x, point.partStart.y);
    recipe.partEnd = QPointF(point.partEnd.x, point.partEnd.y);
    recipe.edgeSensitivity = point.edgeSensitivity;
    recipe.useSubpixel = point.useSubpixel;
    recipe.transition = transitionToRecipe(point.transition);
    recipe.pickMode = pickModeToRecipe(point.pickMode);
    recipes.append(recipe);
  }

  QString error;
  if (!m_recipeManager.saveGeometryPoints(camera.id, recipes, &error))
  {
    appendLog(error);
  }
}

void MainWindow::saveGeometryCirclesRecipe(const CameraConfig& camera)
{
  QVector<GeometryCircleRecipeConfig> recipes;
  for (const GeometryCircleRuntimeConfig& circle : m_geometryCircleConfigs[camera.id])
  {
    if (!circle.hasCircle)
    {
      continue;
    }

    GeometryCircleRecipeConfig recipe;
    recipe.enabled = circle.enabled;
    recipe.id = circle.id;
    recipe.partCenter = QPointF(circle.partCenter.x, circle.partCenter.y);
    recipe.radius = circle.radius;
    recipe.innerBand = circle.innerBand;
    recipe.outerBand = circle.outerBand;
    recipe.edgeSensitivity = circle.edgeSensitivity;
    recipe.edgeCleanupDerivative = circle.edgeCleanupDerivative;
    recipe.edgeStatisticalFilter = circle.edgeStatisticalFilter;
    recipe.useSubpixel = circle.useSubpixel;
    recipe.transition = transitionToRecipe(circle.transition);
    recipe.pickMode = pickModeToRecipe(circle.pickMode);
    recipes.append(recipe);
  }

  QString error;
  if (!m_recipeManager.saveGeometryCircles(camera.id, recipes, &error))
  {
    appendLog(error);
  }
}

void MainWindow::addGeometryPoint(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
    m_activeGeometryPointIndexes[camera.id] = 0;
    return;
  }

  GeometryPointRuntimeConfig& activePoint = activeGeometryPointConfig(camera.id);
  if (!activePoint.hasImageGuide && !activePoint.hasGuide)
  {
    return;
  }

  GeometryPointRuntimeConfig point = activeGeometryPointConfig(camera.id);
  point.id = QString("point_%1").arg(points.size() + 1);
  point.hasImageGuide = false;
  point.hasGuide = false;
  points.append(point);
  m_activeGeometryPointIndexes[camera.id] = points.size() - 1;
}

void MainWindow::addGeometryCircle(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[camera.id];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
    m_activeGeometryCircleIndexes[camera.id] = 0;
    return;
  }

  GeometryCircleRuntimeConfig& activeCircle = activeGeometryCircleConfig(camera.id);
  if (!activeCircle.hasImageCircle && !activeCircle.hasCircle)
  {
    return;
  }

  GeometryCircleRuntimeConfig circle = activeCircle;
  circle.id = QString("circle_%1").arg(circles.size() + 1);
  circle.hasImageCircle = false;
  circle.hasCircle = false;
  circles.append(circle);
  m_activeGeometryCircleIndexes[camera.id] = circles.size() - 1;
}

void MainWindow::removeActiveGeometryPoint(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
    m_activeGeometryPointIndexes[camera.id] = 0;
    showGeometryPointPanel(camera);
    return;
  }

  const int index = qBound(0, m_activeGeometryPointIndexes.value(camera.id, 0), points.size() - 1);
  points.removeAt(index);
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }

  m_activeGeometryPointIndexes[camera.id] = qBound(0, index, points.size() - 1);
  m_pointGeometryMouseControllers[camera.id].begin(3.0);
  saveGeometryPointRecipe(camera);
  updateGeometryPointOverlay(camera);
  showGeometryPointPanel(camera);
}

void MainWindow::removeActiveGeometryCircle(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[camera.id];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
    m_activeGeometryCircleIndexes[camera.id] = 0;
    showGeometryCirclePanel(camera);
    return;
  }

  const int index = qBound(0, m_activeGeometryCircleIndexes.value(camera.id, 0), circles.size() - 1);
  circles.removeAt(index);
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }

  m_activeGeometryCircleIndexes[camera.id] = qBound(0, index, circles.size() - 1);
  saveGeometryCirclesRecipe(camera);
  showGeometryCirclePanel(camera);
}

GeometryPointRuntimeConfig& MainWindow::activeGeometryPointConfig(const QString& cameraId)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[cameraId];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }

  int index = qBound(0, m_activeGeometryPointIndexes.value(cameraId, 0), points.size() - 1);
  m_activeGeometryPointIndexes[cameraId] = index;
  return points[index];
}

GeometryCircleRuntimeConfig& MainWindow::activeGeometryCircleConfig(const QString& cameraId)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[cameraId];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }

  int index = qBound(0, m_activeGeometryCircleIndexes.value(cameraId, 0), circles.size() - 1);
  m_activeGeometryCircleIndexes[cameraId] = index;
  return circles[index];
}

const GeometryPointRuntimeConfig& MainWindow::activeGeometryPointConfig(const QString& cameraId) const
{
  const QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs.value(cameraId);
  static const GeometryPointRuntimeConfig fallback;
  if (points.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeGeometryPointIndexes.value(cameraId, 0), points.size() - 1);
  return points[index];
}

const GeometryCircleRuntimeConfig& MainWindow::activeGeometryCircleConfig(const QString& cameraId) const
{
  const QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs.value(cameraId);
  static const GeometryCircleRuntimeConfig fallback;
  if (circles.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeGeometryCircleIndexes.value(cameraId, 0), circles.size() - 1);
  return circles[index];
}

void MainWindow::loadGeometryLinesRecipe(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  if (!lines.isEmpty())
  {
    return;
  }

  const QVector<GeometryLineRecipeConfig> recipes = m_recipeManager.loadGeometryLines(camera.id);
  for (const GeometryLineRecipeConfig& recipe : recipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryLineRuntimeConfig line;
    line.id = recipe.id;
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
  m_activeGeometryLineIndexes[camera.id] = qBound(0, m_activeGeometryLineIndexes.value(camera.id, 0), lines.size() - 1);
}

void MainWindow::saveGeometryLinesRecipe(const CameraConfig& camera)
{
  QVector<GeometryLineRecipeConfig> recipes;
  for (const GeometryLineRuntimeConfig& line : m_geometryLineConfigs[camera.id])
  {
    if (!line.hasLine)
    {
      continue;
    }

    GeometryLineRecipeConfig recipe;
    recipe.enabled = line.enabled;
    recipe.id = line.id;
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
    recipes.append(recipe);
  }

  QString error;
  if (!m_recipeManager.saveGeometryLines(camera.id, recipes, &error))
  {
    appendLog(error);
  }
}
