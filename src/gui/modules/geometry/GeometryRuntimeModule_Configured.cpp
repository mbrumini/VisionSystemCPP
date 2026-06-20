#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryOverlayPrimitives.h"
#include "processing/geometry/EdgeCircleDetector.h"
#include "processing/geometry/EdgePointDetector.h"
#include "processing/geometry/EdgeLineDetector.h"

#include <QColor>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace
{
double normalizedSetupArcAngle(double angle)
{
  while (angle < 0.0)
  {
    angle += 2.0 * CV_PI;
  }
  while (angle >= 2.0 * CV_PI)
  {
    angle -= 2.0 * CV_PI;
  }
  return angle;
}
}

void MainWindowGeometryModule::testConfiguredGeometryLines(const CameraConfig& camera)
{
  loadGeometryLinesRecipe(camera);
  QVector<GeometryLineRuntimeConfig>& lines = m_lineConfigs[camera.id];
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  cv::Mat diagnostic;
  if (input.channels() == 1)
  {
    cv::cvtColor(input, diagnostic, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(diagnostic);
  }

  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  geometries.lines.clear();
  GeometryOverlay detectedOverlay;

  for (int i = 0; i < lines.size(); ++i)
  {
    GeometryLineRuntimeConfig& line = lines[i];
    if (!line.enabled)
    {
      continue;
    }

    if (pose.valid && !line.hasLine && line.hasImageLine)
    {
      line.partStart = imageToPart(pose, line.imageStart);
      line.partEnd = imageToPart(pose, line.imageEnd);
      line.hasLine = true;
    }

    const bool usePartLine = pose.valid && line.hasLine;
    if (!usePartLine && !line.hasImageLine)
    {
      continue;
    }

    const cv::Point2d imageStart = usePartLine ? partToImage(pose, line.partStart) : line.imageStart;
    const cv::Point2d imageEnd = usePartLine ? partToImage(pose, line.partEnd) : line.imageEnd;

    EdgeLineDetectorConfig config;
    config.id = line.id;
    config.label = line.alias.trimmed().isEmpty() ? line.id : line.alias.trimmed();
    config.guideStart = imageStart;
    config.guideEnd = imageEnd;
    config.bandHalfWidth = line.bandHalfWidth;
    config.edgeSensitivity = line.edgeSensitivity;
    config.edgeCleanupDerivative = line.edgeCleanupDerivative;
    config.edgeStatisticalFilter = line.edgeStatisticalFilter;
    config.useSubpixel = MainWindowCameraProfile::isBwDimensional(camera, MainWindowModuleBase::config()) && line.useSubpixel;
    config.scanDirection = line.scanDirection;
    config.transition = line.transition;
    config.pickMode = line.pickMode;

    EdgeLineDetector detector;
    const EdgeLineDetectorResult result = detector.detect(input, config);
    if (!result.processed)
    {
      log(QString("geometry line skipped: %1 id=%2 guide=(%3,%4)-(%5,%6) band=%7 reason=%8")
            .arg(camera.id)
            .arg(line.id)
            .arg(imageStart.x, 0, 'f', 1)
            .arg(imageStart.y, 0, 'f', 1)
            .arg(imageEnd.x, 0, 'f', 1)
            .arg(imageEnd.y, 0, 'f', 1)
            .arg(line.bandHalfWidth)
            .arg(result.message));
      continue;
    }

    if (!result.found)
    {
      log(QString("geometry line not found: %1 id=%2 guide=(%3,%4)-(%5,%6) band=%7 raw=%8 filtered=%9 reason=%10")
            .arg(camera.id)
            .arg(line.id)
            .arg(imageStart.x, 0, 'f', 1)
            .arg(imageStart.y, 0, 'f', 1)
            .arg(imageEnd.x, 0, 'f', 1)
            .arg(imageEnd.y, 0, 'f', 1)
            .arg(line.bandHalfWidth)
            .arg(static_cast<int>(result.rawEdgePoints.size()))
            .arg(static_cast<int>(result.edgePoints.size()))
            .arg(result.message));
      continue;
    }

    log(QString("geometry line found: %1 id=%2 guide=(%3,%4)-(%5,%6) band=%7 raw=%8 filtered=%9 line=(%10,%11)-(%12,%13)")
          .arg(camera.id)
          .arg(line.id)
          .arg(imageStart.x, 0, 'f', 1)
          .arg(imageStart.y, 0, 'f', 1)
          .arg(imageEnd.x, 0, 'f', 1)
          .arg(imageEnd.y, 0, 'f', 1)
          .arg(line.bandHalfWidth)
          .arg(static_cast<int>(result.rawEdgePoints.size()))
          .arg(static_cast<int>(result.edgePoints.size()))
          .arg(result.line.start.x, 0, 'f', 1)
          .arg(result.line.start.y, 0, 'f', 1)
          .arg(result.line.end.x, 0, 'f', 1)
          .arg(result.line.end.y, 0, 'f', 1));
    geometries.lines.append(result.line);
    cv::line(
      diagnostic,
      cv::Point(static_cast<int>(std::round(result.line.start.x)), static_cast<int>(std::round(result.line.start.y))),
      cv::Point(static_cast<int>(std::round(result.line.end.x)), static_cast<int>(std::round(result.line.end.y))),
      cv::Scalar(0, 255, 0),
      4,
      cv::LINE_AA);
    detectedOverlay.lines.append({
      QPointF(result.line.start.x, result.line.start.y),
      QPointF(result.line.end.x, result.line.end.y),
      QColor("#35c46a"),
      6
    });
  }

  loadGeometryPointRecipe(camera);
  QVector<GeometryPointRuntimeConfig>& points = m_pointConfigs[camera.id];
  geometries.points.clear();
  for (GeometryPointRuntimeConfig& point : points)
  {
    if (!point.enabled || !point.hasGuide || !pose.valid)
    {
      continue;
    }

    const cv::Point2d imageStart = partToImage(pose, point.partStart);
    const cv::Point2d imageEnd = partToImage(pose, point.partEnd);

    EdgePointDetectorConfig config;
    config.id = point.id;
    config.label = point.alias.trimmed().isEmpty() ? point.id : point.alias.trimmed();
    config.scanStart = imageStart;
    config.scanEnd = imageEnd;
    config.edgeSensitivity = point.edgeSensitivity;
    config.useSubpixel = MainWindowCameraProfile::isBwDimensional(camera, MainWindowModuleBase::config()) && point.useSubpixel;
    config.transition = point.transition;
    config.pickMode = point.pickMode;

    EdgePointDetector detector;
    const EdgePointDetectorResult result = detector.detect(input, config);
    if (result.processed && result.found)
    {
      geometries.points.append(result.point);
      GeometryDiagnosticDrawing::drawOrangePointCross(diagnostic, result.point.point);
      GeometryDiagnosticDrawing::appendOrangePointCross(detectedOverlay, result.point.point);
    }
  }

  loadGeometryCirclesRecipe(camera);
  QVector<GeometryCircleRuntimeConfig>& circles = m_circleConfigs[camera.id];
  geometries.circles.clear();
  for (GeometryCircleRuntimeConfig& circle : circles)
  {
    if (!circle.enabled)
    {
      continue;
    }

    if (pose.valid && !circle.hasCircle && circle.hasImageCircle)
    {
      circle.partCenter = imageToPart(pose, circle.imageCenter);
      circle.hasCircle = true;
    }

    const bool usePartCircle = pose.valid && circle.hasCircle;
    if (!usePartCircle && !circle.hasImageCircle)
    {
      continue;
    }

    const cv::Point2d guideCenter = usePartCircle ? partToImage(pose, circle.partCenter) : circle.imageCenter;
    const int guideRadius = static_cast<int>(std::round(circle.radius));
    if (guideRadius <= 0)
    {
      continue;
    }

    appendGeometryCirclePolyline(detectedOverlay, guideCenter, std::max(1.0, circle.radius - circle.innerBand), QColor(0, 210, 255, 90), 1);
    appendGeometryCirclePolyline(detectedOverlay, guideCenter, circle.radius, QColor(0, 210, 255, 170), 2);
    appendGeometryCirclePolyline(detectedOverlay, guideCenter, circle.radius + circle.outerBand, QColor(0, 210, 255, 90), 1);

    EdgeCircleDetectorConfig config;
    config.id = circle.id;
    config.label = circle.alias.trimmed().isEmpty() ? circle.id : circle.alias.trimmed();
    config.guideCenter = guideCenter;
    config.guideRadius = circle.radius;
    config.innerBand = circle.innerBand;
    config.outerBand = circle.outerBand;
    config.edgeSensitivity = circle.edgeSensitivity;
    config.edgeCleanupDerivative = circle.edgeCleanupDerivative;
    config.edgeStatisticalFilter = circle.edgeStatisticalFilter;
    config.useSubpixel = MainWindowCameraProfile::isBwDimensional(camera, MainWindowModuleBase::config()) && circle.useSubpixel;
    config.scanDirection = circle.scanDirection;
    config.transition = circle.transition;
    config.pickMode = circle.pickMode;

    EdgeCircleDetector detector;
    const EdgeCircleDetectorResult result = detector.detect(input, config);
    if (!result.processed)
    {
      continue;
    }

    if (!result.found)
    {
      continue;
    }

    geometries.circles.append(result.circle);
    appendGeometryCirclePolyline(detectedOverlay, result.circle.center, result.circle.radius, QColor("#00d2ff"), 7);
    cv::circle(
      diagnostic,
      cv::Point(static_cast<int>(std::round(result.circle.center.x)), static_cast<int>(std::round(result.circle.center.y))),
      static_cast<int>(std::round(result.circle.radius)),
      cv::Scalar(255, 255, 0),
      4,
      cv::LINE_AA);
  }

  loadGeometryArcsRecipe(camera);
  QVector<GeometryArcRuntimeConfig>& arcs = m_arcConfigs[camera.id];
  geometries.arcs.clear();
  for (GeometryArcRuntimeConfig& arc : arcs)
  {
    if (!arc.enabled)
    {
      continue;
    }

    if (pose.valid && !arc.hasArc && arc.hasImageArc)
    {
      arc.partCenter = imageToPart(pose, arc.imageCenter);
      arc.partStart = imageToPart(pose, arc.imageStart);
      arc.partEnd = imageToPart(pose, arc.imageEnd);
      arc.hasArc = true;
    }

    const bool usePartArc = pose.valid && arc.hasArc;
    if (!usePartArc && !arc.hasImageArc)
    {
      continue;
    }

    const cv::Point2d guideCenter = usePartArc ? partToImage(pose, arc.partCenter) : arc.imageCenter;
    const cv::Point2d guideStart = usePartArc ? partToImage(pose, arc.partStart) : arc.imageStart;
    const cv::Point2d guideEnd = usePartArc ? partToImage(pose, arc.partEnd) : arc.imageEnd;
    const double guideRadius = std::hypot(guideStart.x - guideCenter.x, guideStart.y - guideCenter.y);
    if (guideRadius <= 1.0)
    {
      continue;
    }

    const double guideStartAngle = normalizedSetupArcAngle(std::atan2(guideStart.y - guideCenter.y, guideStart.x - guideCenter.x));
    const double guideEndAngle = normalizedSetupArcAngle(std::atan2(guideEnd.y - guideCenter.y, guideEnd.x - guideCenter.x));
    appendGeometryArcPolyline(detectedOverlay, guideCenter, std::max(1.0, guideRadius - arc.innerBand), guideStartAngle, guideEndAngle, QColor(255, 79, 216, 90), 1);
    appendGeometryArcPolyline(detectedOverlay, guideCenter, guideRadius, guideStartAngle, guideEndAngle, QColor(255, 79, 216, 170), 2);
    appendGeometryArcPolyline(detectedOverlay, guideCenter, guideRadius + arc.outerBand, guideStartAngle, guideEndAngle, QColor(255, 79, 216, 90), 1);

    EdgeCircleDetectorConfig config;
    config.id = arc.id;
    config.label = arc.alias.trimmed().isEmpty() ? arc.id : arc.alias.trimmed();
    config.guideCenter = guideCenter;
    config.guideRadius = guideRadius;
    config.innerBand = arc.innerBand;
    config.outerBand = arc.outerBand;
    config.edgeSensitivity = arc.edgeSensitivity;
    config.edgeCleanupDerivative = arc.edgeCleanupDerivative;
    config.edgeStatisticalFilter = arc.edgeStatisticalFilter;
    config.useSubpixel = MainWindowCameraProfile::isBwDimensional(camera, MainWindowModuleBase::config()) && arc.useSubpixel;
    config.scanDirection = arc.scanDirection;
    config.transition = arc.transition;
    config.pickMode = arc.pickMode;
    config.useArc = true;
    config.startAngleRadians = guideStartAngle;
    config.endAngleRadians = guideEndAngle;

    EdgeCircleDetector detector;
    const EdgeCircleDetectorResult result = detector.detect(input, config);
    if (!result.processed || !result.found)
    {
      continue;
    }

    geometries.arcs.append(result.arc);
    appendGeometryArcPolyline(detectedOverlay, result.arc.center, result.arc.radius, result.arc.startAngleRadians, result.arc.endAngleRadians, QColor("#ff4fd8"), 7);
    double startDegrees = config.startAngleRadians * 180.0 / CV_PI;
    double endDegrees = config.endAngleRadians * 180.0 / CV_PI;
    if (endDegrees < startDegrees)
    {
      endDegrees += 360.0;
    }
    cv::ellipse(
      diagnostic,
      cv::Point(static_cast<int>(std::round(result.arc.center.x)), static_cast<int>(std::round(result.arc.center.y))),
      cv::Size(static_cast<int>(std::round(result.arc.radius)), static_cast<int>(std::round(result.arc.radius))),
      0.0,
      startDegrees,
      endDegrees,
      cv::Scalar(216, 79, 255),
      4,
      cv::LINE_AA);
  }

  const bool updateView = camera.id == selectedCameraId();
  if (updateView)
  {
    selectedPreview() = context().imaging->matToPixmap(diagnostic);
    largeImage()->setImage(selectedPreview());
    largeImage()->clearRoi();
    largeImage()->clearCircles();
  }

  if (updateView &&
      *context().activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry &&
      m_drawingTarget == DrawingTarget::Line)
  {
    GeometryLineRuntimeConfig& activeLine = activeGeometryLineConfig(camera.id);
    if (pose.valid && !activeLine.hasLine && activeLine.hasImageLine)
    {
      activeLine.partStart = imageToPart(pose, activeLine.imageStart);
      activeLine.partEnd = imageToPart(pose, activeLine.imageEnd);
      activeLine.hasLine = true;
    }

    const bool usePartLine = pose.valid && activeLine.hasLine;
    if (usePartLine || activeLine.hasImageLine)
    {
      const cv::Point2d imageStart = usePartLine ? partToImage(pose, activeLine.partStart) : activeLine.imageStart;
      const cv::Point2d imageEnd = usePartLine ? partToImage(pose, activeLine.partEnd) : activeLine.imageEnd;
      m_lineMouseControllers[camera.id].setLine(
        QPointF(imageStart.x, imageStart.y),
        QPointF(imageEnd.x, imageEnd.y),
        activeLine.bandHalfWidth);
    }

    updateGeometryLineOverlay(camera, detectedOverlay);
    return;
  }

  if (updateView &&
      *context().activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry &&
      m_drawingTarget == DrawingTarget::Point)
  {
    updateGeometryPointOverlay(camera, detectedOverlay);
    return;
  }

  if (updateView)
  {
    GeometryOverlay setupOverlay = detectedOverlay;
    appendCurrentPartPoseOverlay(camera, setupOverlay);
    largeImage()->setGeometryOverlay(setupOverlay);
  }
}

