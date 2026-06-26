#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowMeasurementModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryDisplayNames.h"
#include "gui/geometry/GeometryOverlayPrimitives.h"

#include <QColor>

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

  showRuntimeGeometryOverlay(camera);

  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
}

