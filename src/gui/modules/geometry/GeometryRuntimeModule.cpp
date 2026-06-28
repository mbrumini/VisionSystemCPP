#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowMeasurementModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryDisplayNames.h"
#include "gui/geometry/GeometryGuideRuntime.h"
#include "gui/geometry/GeometryOverlayPrimitives.h"

#include <QColor>

#include <algorithm>
#include <cmath>

namespace
{
double pointDistance(const QPointF& a, const QPointF& b)
{
  return std::hypot(a.x() - b.x(), a.y() - b.y());
}

double pointSegmentDistance(const QPointF& point, const QPointF& start, const QPointF& end)
{
  const QPointF segment = end - start;
  const double lengthSquared = segment.x() * segment.x() + segment.y() * segment.y();
  if (lengthSquared <= 1e-9)
  {
    return pointDistance(point, start);
  }

  const QPointF relative = point - start;
  const double t = std::clamp(
    (relative.x() * segment.x() + relative.y() * segment.y()) / lengthSquared,
    0.0,
    1.0);
  return pointDistance(point, start + segment * t);
}

double normalizedRadians(double angle)
{
  constexpr double twoPi = 6.28318530717958647692;
  while (angle < 0.0)
  {
    angle += twoPi;
  }
  while (angle >= twoPi)
  {
    angle -= twoPi;
  }
  return angle;
}

bool angleOnArc(double angle, double start, double end)
{
  constexpr double twoPi = 6.28318530717958647692;
  angle = normalizedRadians(angle);
  start = normalizedRadians(start);
  end = normalizedRadians(end);
  if (end < start)
  {
    end += twoPi;
  }
  if (angle < start)
  {
    angle += twoPi;
  }
  return angle >= start && angle <= end;
}
}

QHash<QString, QString> MainWindowGeometryModule::geometryAliasMap(const QString& cameraId) const
{
  QHash<QString, QString> aliases;

  const auto storeAlias = [&aliases](const QString& id, const QString& alias) {
    const QString trimmed = alias.trimmed();
    if (!trimmed.isEmpty())
    {
      aliases.insert(id, trimmed);
    }
  };

  for (const GeometryLineRuntimeConfig& line : m_lineConfigs.value(cameraId))
  {
    storeAlias(line.id, line.alias);
  }
  for (const GeometryPointRuntimeConfig& point : m_pointConfigs.value(cameraId))
  {
    storeAlias(point.id, point.alias);
  }
  for (const GeometryCircleRuntimeConfig& circle : m_circleConfigs.value(cameraId))
  {
    storeAlias(circle.id, circle.alias);
  }
  for (const GeometryArcRuntimeConfig& arc : m_arcConfigs.value(cameraId))
  {
    storeAlias(arc.id, arc.alias);
  }

  return aliases;
}

QString MainWindowGeometryModule::geometryDisplayLabel(const QString& cameraId, const QString& geometryId) const
{
  return GeometryDisplayNames::resolvedLabel(geometryId, {}, geometryAliasMap(cameraId));
}

void MainWindowGeometryModule::syncRuntimeGeometryLabels(const CameraConfig& camera)
{
  const QHash<QString, QString> aliases = geometryAliasMap(camera.id);
  if (aliases.isEmpty())
  {
    return;
  }

  GeometrySet& set = cameraRuntime()[camera.id].geometries();

  const auto applyLabel = [&aliases](auto& geometry) {
    const auto aliasIt = aliases.constFind(geometry.meta.id);
    if (aliasIt != aliases.constEnd())
    {
      geometry.meta.label = aliasIt.value();
    }
  };

  for (LineGeometry& line : set.lines)
  {
    applyLabel(line);
  }
  for (PointGeometry& point : set.points)
  {
    applyLabel(point);
  }
  for (CircleGeometry& circle : set.circles)
  {
    applyLabel(circle);
  }
  for (ArcGeometry& arc : set.arcs)
  {
    applyLabel(arc);
  }

  for (ConstructedPointGeometry& constructed : set.constructedPoints)
  {
    PointGeometry& point = constructed.point;
    const QString id = point.meta.id;
    if (id.endsWith("_center"))
    {
      const QString parentId = id.left(id.size() - QString("_center").size());
      const QString parentAlias = aliases.value(parentId).trimmed();
      if (!parentAlias.isEmpty())
      {
        point.meta.label = QString("%1 centro").arg(parentAlias);
        continue;
      }
    }

    applyLabel(point);
  }

  for (ConstructedLineGeometry& constructed : set.constructedLines)
  {
    applyLabel(constructed.line);
  }
}

void MainWindowGeometryModule::restoreCleanGeometryImage(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  context().imaging->restoreSampleWorkspace(camera);
  if (context().surface)
  {
    context().surface->restoreSurfaceModelPoseFromSample(camera);
  }
  selectedPreview() = context().imaging->loadCameraSamplePreview(camera);

  if (selectedPreview().isNull())
  {
    context().imaging->ensureReferenceImageVisible(camera);
  }
  else
  {
    largeImage()->setImage(selectedPreview());
  }
  largeImage()->clearRoi();
  largeImage()->clearExclusionRects();
  largeImage()->clearCircles();
  largeImage()->clearGeometryArea();
  largeImage()->clearGeometryPoints();
  largeImage()->clearGeometryLines();
  largeImage()->clearGeometryOverlay();
}

void MainWindowGeometryModule::showRuntimeGeometryOverlay(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  const GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  const bool compact = context().machineRunning != nullptr && *context().machineRunning;
  const int lineWidth = compact ? 2 : 6;
  const int circleWidth = compact ? 2 : 7;
  const int arcWidth = compact ? 2 : 7;

  GeometryOverlay overlay;
  for (const LineGeometry& line : geometries.lines)
  {
    overlay.lines.append({
      QPointF(line.start.x, line.start.y),
      QPointF(line.end.x, line.end.y),
      QColor("#35c46a"),
      lineWidth
    });
  }
  for (const ConstructedLineGeometry& constructed : geometries.constructedLines)
  {
    overlay.lines.append({
      QPointF(constructed.line.start.x, constructed.line.start.y),
      QPointF(constructed.line.end.x, constructed.line.end.y),
      QColor("#7fd9ff"),
      lineWidth
    });
  }
  for (const PointGeometry& point : geometries.points)
  {
    GeometryDiagnosticDrawing::appendOrangePointCross(overlay, point.point, compact);
  }
  for (const ConstructedPointGeometry& constructed : geometries.constructedPoints)
  {
    GeometryDiagnosticDrawing::appendOrangePointCross(overlay, constructed.point.point, compact);
  }
  for (const CircleGeometry& circle : geometries.circles)
  {
    appendGeometryCirclePolyline(overlay, circle.center, circle.radius, QColor("#00d2ff"), circleWidth);
  }
  for (const ArcGeometry& arc : geometries.arcs)
  {
    appendGeometryArcPolyline(
      overlay,
      arc.center,
      arc.radius,
      arc.startAngleRadians,
      arc.endAngleRadians,
      QColor("#ff4fd8"),
      arcWidth);
  }
  appendCurrentPartPoseOverlay(camera, overlay);
  if (context().constructedGeometry)
  {
    context().constructedGeometry->rebuildConstructedGeometryRecipe(camera);
  }
  if (context().measurement)
  {
    context().measurement->rebuildMeasurementRecipe(camera);
    context().measurement->appendMeasurementOverlay(camera, overlay, compact);
  }
  largeImage()->setGeometryOverlay(overlay);
}

void MainWindowGeometryModule::refreshMeasurementOverlay(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  const bool editingArc =
    context().activeDrawingRecipe &&
    *context().activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry &&
    m_drawingTarget == DrawingTarget::Arc;
  if (!editingArc)
  {
    showRuntimeGeometryOverlay(camera);
  }

  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
}

bool MainWindowGeometryModule::handleImagePick(const CameraConfig& camera, const QPointF& imagePoint)
{
  if (camera.id != selectedCameraId())
  {
    return false;
  }

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  const cv::Mat& frame = cameraRuntime()[camera.id].currentFrame();
  const cv::Size imageSize = frame.empty() ? cv::Size() : frame.size();
  const QSize referenceSize = guideReferenceSize(camera.id);
  constexpr double kPickTolerance = 18.0;
  int bestIndex = -1;
  double bestDistance = kPickTolerance;

  if (m_drawingTarget == DrawingTarget::Line)
  {
    const QVector<GeometryLineRuntimeConfig>& lines = m_lineConfigs[camera.id];
    for (int i = 0; i < lines.size(); ++i)
    {
      const GeometryLineRuntimeConfig& line = lines[i];
      const bool usePart = GeometryGuideRuntime::shouldUsePartGuide(
        pose.valid, line.hasLine, line.hasImageLine, line.anchorInImageSpace, referenceSize, imageSize);
      QPointF start;
      QPointF end;
      if (usePart || line.hasImageLine)
      {
        const cv::Point2d imageStart = GeometryGuideRuntime::resolveImagePoint(
          pose, usePart, line.partStart, line.imageStart, referenceSize, imageSize);
        const cv::Point2d imageEnd = GeometryGuideRuntime::resolveImagePoint(
          pose, usePart, line.partEnd, line.imageEnd, referenceSize, imageSize);
        start = QPointF(imageStart.x, imageStart.y);
        end = QPointF(imageEnd.x, imageEnd.y);
      }
      else
      {
        bool foundRuntimeLine = false;
        const GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
        for (const LineGeometry& runtimeLine : geometries.lines)
        {
          if (runtimeLine.meta.id == line.id)
          {
            start = QPointF(runtimeLine.start.x, runtimeLine.start.y);
            end = QPointF(runtimeLine.end.x, runtimeLine.end.y);
            foundRuntimeLine = true;
            break;
          }
        }
        if (!foundRuntimeLine)
        {
          continue;
        }
      }

      const double distance = pointSegmentDistance(imagePoint, start, end);
      if (distance < bestDistance)
      {
        bestDistance = distance;
        bestIndex = i;
      }
    }

    if (bestIndex >= 0)
    {
      m_activeLineIndexes[camera.id] = bestIndex;
      GeometryLineRuntimeConfig& line = activeGeometryLineConfig(camera.id);
      const bool usePart = GeometryGuideRuntime::shouldUsePartGuide(
        pose.valid, line.hasLine, line.hasImageLine, line.anchorInImageSpace, referenceSize, imageSize);
      QPointF start;
      QPointF end;
      bool resolved = false;
      if (usePart || line.hasImageLine)
      {
        const cv::Point2d imageStart = GeometryGuideRuntime::resolveImagePoint(
          pose, usePart, line.partStart, line.imageStart, referenceSize, imageSize);
        const cv::Point2d imageEnd = GeometryGuideRuntime::resolveImagePoint(
          pose, usePart, line.partEnd, line.imageEnd, referenceSize, imageSize);
        start = QPointF(imageStart.x, imageStart.y);
        end = QPointF(imageEnd.x, imageEnd.y);
        resolved = true;
      }
      else
      {
        const GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
        for (const LineGeometry& runtimeLine : geometries.lines)
        {
          if (runtimeLine.meta.id == line.id)
          {
            start = QPointF(runtimeLine.start.x, runtimeLine.start.y);
            end = QPointF(runtimeLine.end.x, runtimeLine.end.y);
            resolved = true;
            break;
          }
        }
      }

      if (resolved)
      {
        m_lineMouseControllers[camera.id].setLine(start, end, line.bandHalfWidth);
      }
      *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
      m_drawingTarget = DrawingTarget::Line;
      largeImage()->setGeometryOverlayPointEditingEnabled(true);
      updateGeometryLineOverlay(camera);
      return true;
    }
  }
  else if (m_drawingTarget == DrawingTarget::Point)
  {
    const QVector<GeometryPointRuntimeConfig>& points = m_pointConfigs[camera.id];
    for (int i = 0; i < points.size(); ++i)
    {
      const GeometryPointRuntimeConfig& point = points[i];
      const bool usePart = GeometryGuideRuntime::shouldUsePartGuide(
        pose.valid, point.hasGuide, point.hasImageGuide, point.anchorInImageSpace, referenceSize, imageSize);
      if (!usePart && !point.hasImageGuide)
      {
        continue;
      }

      const cv::Point2d start = GeometryGuideRuntime::resolveImagePoint(
        pose, usePart, point.partStart, point.imageStart, referenceSize, imageSize);
      const cv::Point2d end = GeometryGuideRuntime::resolveImagePoint(
        pose, usePart, point.partEnd, point.imageEnd, referenceSize, imageSize);
      const double distance = pointSegmentDistance(imagePoint, QPointF(start.x, start.y), QPointF(end.x, end.y));
      if (distance < bestDistance)
      {
        bestDistance = distance;
        bestIndex = i;
      }
    }

    if (bestIndex >= 0)
    {
      m_activePointIndexes[camera.id] = bestIndex;
      GeometryPointRuntimeConfig& point = activeGeometryPointConfig(camera.id);
      const bool usePart = GeometryGuideRuntime::shouldUsePartGuide(
        pose.valid, point.hasGuide, point.hasImageGuide, point.anchorInImageSpace, referenceSize, imageSize);
      if (usePart || point.hasImageGuide)
      {
        const cv::Point2d imageStart = GeometryGuideRuntime::resolveImagePoint(
          pose, usePart, point.partStart, point.imageStart, referenceSize, imageSize);
        const cv::Point2d imageEnd = GeometryGuideRuntime::resolveImagePoint(
          pose, usePart, point.partEnd, point.imageEnd, referenceSize, imageSize);
        m_pointMouseControllers[camera.id].setLine(
          QPointF(imageStart.x, imageStart.y),
          QPointF(imageEnd.x, imageEnd.y),
          3.0);
      }
      *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
      m_drawingTarget = DrawingTarget::Point;
      largeImage()->setGeometryOverlayPointEditingEnabled(true);
      updateGeometryPointOverlay(camera);
      return true;
    }
  }
  else if (m_drawingTarget == DrawingTarget::Circle)
  {
    const QVector<GeometryCircleRuntimeConfig>& circles = m_circleConfigs[camera.id];
    for (int i = 0; i < circles.size(); ++i)
    {
      const GeometryCircleRuntimeConfig& circle = circles[i];
      const bool usePart = GeometryGuideRuntime::shouldUsePartGuide(
        pose.valid, circle.hasCircle, circle.hasImageCircle, circle.anchorInImageSpace, referenceSize, imageSize);
      if (!usePart && !circle.hasImageCircle)
      {
        continue;
      }

      const cv::Point2d center = GeometryGuideRuntime::resolveImagePoint(
        pose, usePart, circle.partCenter, circle.imageCenter, referenceSize, imageSize);
      const double radius = usePart
        ? circle.radius
        : GeometryGuideRuntime::mapImageGuideRadius(circle.radius, referenceSize, imageSize);
      const double distance = std::abs(pointDistance(imagePoint, QPointF(center.x, center.y)) - radius);
      if (distance < bestDistance)
      {
        bestDistance = distance;
        bestIndex = i;
      }
    }

    if (bestIndex >= 0)
    {
      m_activeCircleIndexes[camera.id] = bestIndex;
      *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
      m_drawingTarget = DrawingTarget::Circle;
      showConfiguredGeometryCircles(camera, true);
      return true;
    }
  }
  else if (m_drawingTarget == DrawingTarget::Arc)
  {
    const QVector<GeometryArcRuntimeConfig>& arcs = m_arcConfigs[camera.id];
    for (int i = 0; i < arcs.size(); ++i)
    {
      const GeometryArcRuntimeConfig& arc = arcs[i];
      const bool usePart = GeometryGuideRuntime::shouldUsePartGuide(
        pose.valid, arc.hasArc, arc.hasImageArc, arc.anchorInImageSpace, referenceSize, imageSize);
      if (!usePart && !arc.hasImageArc)
      {
        continue;
      }

      const cv::Point2d center = GeometryGuideRuntime::resolveImagePoint(
        pose, usePart, arc.partCenter, arc.imageCenter, referenceSize, imageSize);
      const cv::Point2d start = GeometryGuideRuntime::resolveImagePoint(
        pose, usePart, arc.partStart, arc.imageStart, referenceSize, imageSize);
      const cv::Point2d end = GeometryGuideRuntime::resolveImagePoint(
        pose, usePart, arc.partEnd, arc.imageEnd, referenceSize, imageSize);
      const double radius = usePart
        ? std::hypot(start.x - center.x, start.y - center.y)
        : GeometryGuideRuntime::mapImageGuideRadius(arc.radius, referenceSize, imageSize);
      const double angle = std::atan2(imagePoint.y() - center.y, imagePoint.x() - center.x);
      const double startAngle = std::atan2(start.y - center.y, start.x - center.x);
      const double endAngle = std::atan2(end.y - center.y, end.x - center.x);
      if (!angleOnArc(angle, startAngle, endAngle))
      {
        continue;
      }

      const double distance = std::abs(pointDistance(imagePoint, QPointF(center.x, center.y)) - radius);
      if (distance < bestDistance)
      {
        bestDistance = distance;
        bestIndex = i;
      }
    }

    if (bestIndex >= 0)
    {
      m_activeArcIndexes[camera.id] = bestIndex;
      *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
      m_drawingTarget = DrawingTarget::Arc;
      largeImage()->setGeometryOverlayPointEditingEnabled(true);
      showConfiguredGeometryArcs(camera);
      return true;
    }
  }

  return false;
}

