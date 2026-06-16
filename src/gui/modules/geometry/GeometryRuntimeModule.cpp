#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowMeasurementModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryOverlayPrimitives.h"

#include <QColor>

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
    largeImage()->clearImage();
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
  GeometryOverlay overlay;
  for (const LineGeometry& line : geometries.lines)
  {
    overlay.lines.append({
      QPointF(line.start.x, line.start.y),
      QPointF(line.end.x, line.end.y),
      QColor("#35c46a"),
      6
    });
  }
  for (const PointGeometry& point : geometries.points)
  {
    GeometryDiagnosticDrawing::appendOrangePointCross(overlay, point.point);
  }
  for (const CircleGeometry& circle : geometries.circles)
  {
    appendGeometryCirclePolyline(overlay, circle.center, circle.radius, QColor("#00d2ff"), 7);
  }
  for (const ArcGeometry& arc : geometries.arcs)
  {
    appendGeometryArcPolyline(overlay, arc.center, arc.radius, arc.startAngleRadians, arc.endAngleRadians, QColor("#ff4fd8"), 7);
  }
  appendCurrentPartPoseOverlay(camera, overlay);
  largeImage()->setGeometryOverlay(overlay);
}

void MainWindowGeometryModule::refreshMeasurementOverlay(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  if (context().constructedGeometry)
  {
    context().constructedGeometry->rebuildConstructedGeometryRecipe(camera);
  }

  if (context().measurement)
  {
    context().measurement->rebuildMeasurementRecipe(camera);
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    context().measurement->appendMeasurementOverlay(camera, overlay);
    largeImage()->setGeometryOverlay(overlay);
  }

  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
}

