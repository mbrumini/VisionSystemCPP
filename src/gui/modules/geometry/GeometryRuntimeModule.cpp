#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
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

