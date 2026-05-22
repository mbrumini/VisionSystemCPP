#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryMath.h"
#include "processing/geometry/EdgeCircleDetector.h"
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

void appendCirclePolyline(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double radius,
  const QColor& color,
  int width)
{
  if (radius <= 1.0)
  {
    return;
  }

  constexpr int segments = 96;
  QPointF previous(center.x + radius, center.y);
  for (int i = 1; i <= segments; ++i)
  {
    const double angle = 2.0 * CV_PI * static_cast<double>(i) / static_cast<double>(segments);
    const QPointF current(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
    overlay.lines.append({previous, current, color, width});
    previous = current;
  }
}

void appendArcPolyline(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double radius,
  double startAngle,
  double endAngle,
  const QColor& color,
  int width)
{
  if (radius <= 1.0)
  {
    return;
  }

  double span = endAngle - startAngle;
  while (span < 0.0)
  {
    span += 2.0 * CV_PI;
  }
  if (span < 0.001)
  {
    span = 2.0 * CV_PI;
  }

  const int segments = std::max(8, static_cast<int>(std::round(64.0 * span / (2.0 * CV_PI))));
  QPointF previous;
  for (int i = 0; i <= segments; ++i)
  {
    const double angle = startAngle + span * static_cast<double>(i) / static_cast<double>(segments);
    const QPointF current(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
    if (i > 0)
    {
      overlay.lines.append({previous, current, color, width});
    }
    previous = current;
  }
}
}

void MainWindowGeometryModule::activateGeometryLineDrawing(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  context().deactivateImageDrawingTools();
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Line;
  m_lineMouseControllers[camera.id].begin(activeGeometryLineConfig(camera.id).bandHalfWidth);
  largeImage()->setGeometryOverlayPointEditingEnabled(false);
  largeImage()->setGeometryPointPickingEnabled(true);
  updateGeometryLineOverlay(camera);
  log(tr("log.geometryLineDrawing") + ": " + camera.id);
}

void MainWindowGeometryModule::handleGeometryLinePoint(const CameraConfig& camera, const QPointF& imagePoint)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  LineGeometryMouseController& controller = m_lineMouseControllers[camera.id];
  controller.setBandHalfWidth(activeGeometryLineConfig(camera.id).bandHalfWidth);
  const bool completed = controller.handleClick(imagePoint);
  updateGeometryLineOverlay(camera);

  if (!completed)
  {
    return;
  }

  const LineGeometryEditorState& state = controller.state();
  GeometryLineRuntimeConfig& config = activeGeometryLineConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageLine = true;

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasLine = true;
  }

  largeImage()->setGeometryPointPickingEnabled(false);
  largeImage()->setGeometryOverlayPointEditingEnabled(true);
  updateGeometryLineOverlay(camera);
  log(tr("log.geometryLineGuideSaved") + ": " + camera.id);
  saveGeometryLinesRecipe(camera);
  testGeometryLine(camera);
}

void MainWindowGeometryModule::handleGeometryLineHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint)
{
  if (camera.id != selectedCameraId() || pointIndex < 0 || pointIndex > 1)
  {
    return;
  }

  LineGeometryMouseController& controller = m_lineMouseControllers[camera.id];
  controller.movePoint(pointIndex, imagePoint);

  const LineGeometryEditorState& state = controller.state();
  if (!state.hasStart || !state.hasEnd)
  {
    updateGeometryLineOverlay(camera);
    return;
  }

  GeometryLineRuntimeConfig& config = activeGeometryLineConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageLine = true;

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasLine = true;
  }
  else
  {
    config.hasLine = false;
  }

  updateGeometryLineOverlay(camera);
  saveGeometryLinesRecipe(camera);
  testGeometryLine(camera);
}

GeometryOverlay MainWindowGeometryModule::configuredGeometryLinesOverlay(const CameraConfig& camera, bool includeActive) const
{
  GeometryOverlay overlay;
  const PartPose& pose = cameraRuntime().at(camera.id).currentPose();
  const QVector<GeometryLineRuntimeConfig> lines = m_lineConfigs.value(camera.id);
  const int activeIndex = m_activeLineIndexes.value(camera.id, 0);

  for (int i = 0; i < lines.size(); ++i)
  {
    if (!includeActive && i == activeIndex)
    {
      continue;
    }

    const GeometryLineRuntimeConfig& line = lines[i];
    const bool usePartLine = pose.valid && line.hasLine;
    if (!usePartLine && !line.hasImageLine)
    {
      continue;
    }

    const cv::Point2d imageStart = usePartLine ? partToImage(pose, line.partStart) : line.imageStart;
    const cv::Point2d imageEnd = usePartLine ? partToImage(pose, line.partEnd) : line.imageEnd;
    const QPointF start(imageStart.x, imageStart.y);
    const QPointF end(imageEnd.x, imageEnd.y);
    const QPointF center = (start + end) * 0.5;
    const QPointF delta = end - start;
    const double length = std::hypot(delta.x(), delta.y());
    const double angleDegrees = std::atan2(delta.y(), delta.x()) * 180.0 / CV_PI;
    const bool highlightActive =
      *context().activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry &&
      m_drawingTarget == DrawingTarget::Line &&
      i == activeIndex;
    const QColor lineColor = highlightActive ? QColor("#ff4fd8") : QColor(170, 205, 220, 170);
    const QColor bandColor = highlightActive ? QColor(0, 210, 255, 35) : QColor(120, 170, 190, 22);
    overlay.bands.append({center, QSizeF(length, line.bandHalfWidth * 2.0), angleDegrees, lineColor, bandColor});
    overlay.lines.append({start, end, lineColor, highlightActive ? 3 : 1});
    if (includeActive && highlightActive)
    {
      overlay.points.append({start, "1"});
      overlay.points.append({end, "2"});
    }
  }

  return overlay;
}

void MainWindowGeometryModule::updateGeometryLineOverlay(const CameraConfig& camera, const GeometryOverlay& extraOverlay)
{
  GeometryOverlay overlay = configuredGeometryLinesOverlay(camera, false);
  GeometryOverlay activeOverlay = m_lineMouseControllers[camera.id].overlay();
  for (const GeometryOverlayPoint& point : activeOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : activeOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : activeOverlay.bands)
  {
    overlay.bands.append(band);
  }

  for (const GeometryOverlayPoint& point : extraOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : extraOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : extraOverlay.bands)
  {
    overlay.bands.append(band);
  }
  appendCurrentPartPoseOverlay(camera, overlay);
  largeImage()->setGeometryOverlay(overlay);
}

void MainWindowGeometryModule::restoreCleanGeometryImage(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  const auto runtimeIt = cameraRuntime().find(camera.id);
  if (runtimeIt != cameraRuntime().end() && !runtimeIt->second.currentFrame().empty())
  {
    selectedPreview() = context().imaging->matToPixmap(runtimeIt->second.currentFrame());
  }
  else
  {
    selectedPreview() = context().imaging->loadCameraPreview(camera);
  }

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

void MainWindowGeometryModule::testGeometryLine(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  GeometryLineRuntimeConfig& lineConfig = activeGeometryLineConfig(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid && !lineConfig.hasLine && lineConfig.hasImageLine)
  {
    lineConfig.partStart = imageToPart(pose, lineConfig.imageStart);
    lineConfig.partEnd = imageToPart(pose, lineConfig.imageEnd);
    lineConfig.hasLine = true;
  }

  const bool usePartLine = pose.valid && lineConfig.hasLine;
  if (!usePartLine && !lineConfig.hasImageLine)
  {
    log(tr("log.geometryLineRoiMissing") + ": " + camera.id);
    return;
  }

  const cv::Point2d imageStart = usePartLine ? partToImage(pose, lineConfig.partStart) : lineConfig.imageStart;
  const cv::Point2d imageEnd = usePartLine ? partToImage(pose, lineConfig.partEnd) : lineConfig.imageEnd;
  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  EdgeLineDetectorConfig config;
  config.id = lineConfig.id;
  config.label = lineConfig.id;
  config.guideStart = imageStart;
  config.guideEnd = imageEnd;
  config.bandHalfWidth = lineConfig.bandHalfWidth;
  config.edgeSensitivity = lineConfig.edgeSensitivity;
  config.edgeCleanupDerivative = lineConfig.edgeCleanupDerivative;
  config.edgeStatisticalFilter = lineConfig.edgeStatisticalFilter;
  config.useSubpixel = MainWindowCameraProfile::isBwDimensional(camera, MainWindowModuleBase::config()) && lineConfig.useSubpixel;
  config.scanDirection = lineConfig.scanDirection;
  config.transition = lineConfig.transition;
  config.pickMode = lineConfig.pickMode;

  // Run detection in background to avoid blocking UI using helper
  auto job = [input, config]() -> EdgeLineDetectorResult {
    EdgeLineDetector detector;
    return detector.detect(input, config);
  };

  const QString __pendingCameraId_testGeometryLine = camera.id;
  context().incPendingJobs(__pendingCameraId_testGeometryLine);
  runAsyncTask(decltype(job)(job), window(), [this, camera, imageStart, imageEnd, __pendingCameraId_testGeometryLine](const EdgeLineDetectorResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testGeometryLine](void*) { context().decPendingJobs(__pendingCameraId_testGeometryLine); });
    if (!result.processed || result.diagnosticImage.empty())
    {
      log(result.message.isEmpty() ? tr("log.geometryLineFailed") + ": " + camera.id : result.message);
      return;
    }

    selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
    largeImage()->setImage(selectedPreview());
    largeImage()->setRoi(QRect(result.searchRoi.x, result.searchRoi.y, result.searchRoi.width, result.searchRoi.height));

    LineGeometryMouseController& controller = m_lineMouseControllers[camera.id];
    controller.setLine(QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y), activeGeometryLineConfig(camera.id).bandHalfWidth);
    m_drawingTarget = DrawingTarget::Line;
    updateGeometryLineOverlay(camera);

    if (!result.found)
    {
      log(result.message.isEmpty() ? tr("log.geometryLineNotFound") + ": " + camera.id : result.message);
      return;
    }

    GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
    for (int i = geometries.lines.size() - 1; i >= 0; --i)
    {
      if (geometries.lines[i].meta.id == result.line.meta.id)
      {
        geometries.lines.removeAt(i);
      }
    }
    geometries.lines.append(result.line);
    GeometryOverlay detectedOverlay;
    detectedOverlay.lines.append({
      QPointF(result.line.start.x, result.line.start.y),
      QPointF(result.line.end.x, result.line.end.y),
      QColor("#35c46a"),
      6
    });
    updateGeometryLineOverlay(camera, detectedOverlay);

    log(QString("%1: %2 x1=%3 y1=%4 x2=%5 y2=%6")
                .arg(tr("log.geometryLineFound"))
                .arg(camera.id)
                .arg(result.line.start.x, 0, 'f', 1)
                .arg(result.line.start.y, 0, 'f', 1)
                .arg(result.line.end.x, 0, 'f', 1)
                .arg(result.line.end.y, 0, 'f', 1));
  });
}

void MainWindowGeometryModule::testConfiguredGeometryLines(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

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
    config.label = line.id;
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
      continue;
    }

    if (!result.found)
    {
      continue;
    }

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
    config.label = point.id;
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

    appendCirclePolyline(detectedOverlay, guideCenter, std::max(1.0, circle.radius - circle.innerBand), QColor(0, 210, 255, 90), 1);
    appendCirclePolyline(detectedOverlay, guideCenter, circle.radius, QColor(0, 210, 255, 170), 2);
    appendCirclePolyline(detectedOverlay, guideCenter, circle.radius + circle.outerBand, QColor(0, 210, 255, 90), 1);

    EdgeCircleDetectorConfig config;
    config.id = circle.id;
    config.label = circle.id;
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
    appendCirclePolyline(detectedOverlay, result.circle.center, result.circle.radius, QColor("#00d2ff"), 7);
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
    appendArcPolyline(detectedOverlay, guideCenter, std::max(1.0, guideRadius - arc.innerBand), guideStartAngle, guideEndAngle, QColor(255, 79, 216, 90), 1);
    appendArcPolyline(detectedOverlay, guideCenter, guideRadius, guideStartAngle, guideEndAngle, QColor(255, 79, 216, 170), 2);
    appendArcPolyline(detectedOverlay, guideCenter, guideRadius + arc.outerBand, guideStartAngle, guideEndAngle, QColor(255, 79, 216, 90), 1);

    EdgeCircleDetectorConfig config;
    config.id = arc.id;
    config.label = arc.id;
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
    appendArcPolyline(detectedOverlay, result.arc.center, result.arc.radius, result.arc.startAngleRadians, result.arc.endAngleRadians, QColor("#ff4fd8"), 7);
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

  selectedPreview() = context().imaging->matToPixmap(diagnostic);
  largeImage()->setImage(selectedPreview());
  largeImage()->clearRoi();
  largeImage()->clearCircles();
  GeometryOverlay setupOverlay = detectedOverlay;
  appendCurrentPartPoseOverlay(camera, setupOverlay);
  largeImage()->setGeometryOverlay(setupOverlay);
}

void MainWindowGeometryModule::activateGeometryPointDrawing(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  context().deactivateImageDrawingTools();
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Point;
  m_pointMouseControllers[camera.id].begin(3.0);
  largeImage()->setGeometryOverlayPointEditingEnabled(false);
  largeImage()->setGeometryPointPickingEnabled(true);
  updateGeometryPointOverlay(camera);
  log(tr("log.geometryPointDrawing") + ": " + camera.id);
}

void MainWindowGeometryModule::activateGeometryCircleDrawing(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  context().deactivateImageDrawingTools();
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Circle;
  largeImage()->setThreePointCircleDrawingEnabled(true);
  showConfiguredGeometryCircles(camera);
  log(tr("log.geometryCircleDrawing") + ": " + camera.id);
}

void MainWindowGeometryModule::handleGeometryCirclePoints(const CameraConfig& camera, const QVector<QPoint>& points)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  ImageCircle imageCircle;
  if (!GeometryMath::circleFromThreePoints(points, imageCircle))
  {
    log(tr("log.surfaceEdgeCircleInvalid") + ": " + camera.id);
    return;
  }

  GeometryCircleRuntimeConfig& config = activeGeometryCircleConfig(camera.id);
  config.imageCenter = cv::Point2d(imageCircle.center.x(), imageCircle.center.y());
  config.radius = imageCircle.radius;
  config.hasImageCircle = true;

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid)
  {
    config.partCenter = imageToPart(pose, config.imageCenter);
    config.hasCircle = true;
  }
  else
  {
    config.hasCircle = false;
  }

  largeImage()->setThreePointCircleDrawingEnabled(false);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Circle;
  saveGeometryCirclesRecipe(camera);
  showConfiguredGeometryCircles(camera);
  testGeometryCircle(camera);
}

void MainWindowGeometryModule::showConfiguredGeometryCircles(const CameraConfig& camera)
{
  GeometryCircleRuntimeConfig& circle = activeGeometryCircleConfig(camera.id);
  GeometryOverlay overlay;
  largeImage()->clearCircles();
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  const bool usePartCircle = pose.valid && circle.hasCircle;
  if (usePartCircle || circle.hasImageCircle)
  {
    const cv::Point2d center = usePartCircle ? partToImage(pose, circle.partCenter) : circle.imageCenter;
    appendCirclePolyline(overlay, center, std::max(1.0, circle.radius - circle.innerBand), QColor(0, 210, 255, 90), 1);
    appendCirclePolyline(overlay, center, circle.radius, QColor("#00d2ff"), 5);
    appendCirclePolyline(overlay, center, circle.radius + circle.outerBand, QColor(0, 210, 255, 90), 1);
  }
  appendCurrentPartPoseOverlay(camera, overlay);
  largeImage()->setGeometryOverlay(overlay);
}

void MainWindowGeometryModule::testGeometryCircle(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  GeometryCircleRuntimeConfig& circleConfig = activeGeometryCircleConfig(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid && !circleConfig.hasCircle && circleConfig.hasImageCircle)
  {
    circleConfig.partCenter = imageToPart(pose, circleConfig.imageCenter);
    circleConfig.hasCircle = true;
    saveGeometryCirclesRecipe(camera);
  }

  const bool usePartCircle = pose.valid && circleConfig.hasCircle;
  if (!usePartCircle && !circleConfig.hasImageCircle)
  {
    showConfiguredGeometryCircles(camera);
    log(tr("log.geometryCircleMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  const cv::Point2d guideCenter = usePartCircle ? partToImage(pose, circleConfig.partCenter) : circleConfig.imageCenter;
  EdgeCircleDetectorConfig config;
  config.id = circleConfig.id;
  config.label = circleConfig.id;
  config.guideCenter = guideCenter;
  config.guideRadius = circleConfig.radius;
  config.innerBand = circleConfig.innerBand;
  config.outerBand = circleConfig.outerBand;
  config.edgeSensitivity = circleConfig.edgeSensitivity;
  config.edgeCleanupDerivative = circleConfig.edgeCleanupDerivative;
  config.edgeStatisticalFilter = circleConfig.edgeStatisticalFilter;
  config.useSubpixel = MainWindowCameraProfile::isBwDimensional(camera, MainWindowModuleBase::config()) && circleConfig.useSubpixel;
  config.scanDirection = circleConfig.scanDirection;
  config.transition = circleConfig.transition;
  config.pickMode = circleConfig.pickMode;

  log(QString("geometry circle detect: %1 id=%2 center=%3,%4 r=%5 band=%6/%7 sensitivity=%8 cleanup=%9 stat=%10 scan=%11 transition=%12 pick=%13")
        .arg(camera.id)
        .arg(circleConfig.id)
        .arg(guideCenter.x, 0, 'f', 1)
        .arg(guideCenter.y, 0, 'f', 1)
        .arg(circleConfig.radius, 0, 'f', 1)
        .arg(circleConfig.innerBand)
        .arg(circleConfig.outerBand)
        .arg(circleConfig.edgeSensitivity)
        .arg(circleConfig.edgeCleanupDerivative)
        .arg(circleConfig.edgeStatisticalFilter)
        .arg(circleConfig.scanDirection == EdgeLineScanDirection::NormalNegative ? "normal_negative" : "normal_positive")
        .arg(circleConfig.transition == EdgeLineTransition::DarkToLight ? "dark_to_light" : "light_to_dark")
        .arg(circleConfig.pickMode == EdgeLinePickMode::Last ? "last" : (circleConfig.pickMode == EdgeLinePickMode::Best ? "best" : "first")));

  EdgeCircleDetector detector;
  const EdgeCircleDetectorResult result = detector.detect(input, config);
  if (!result.processed || result.diagnosticImage.empty())
  {
    log(result.message.isEmpty() ? tr("log.geometryCircleFailed") + ": " + camera.id : result.message);
    return;
  }

  selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
  largeImage()->setImage(selectedPreview());
  showConfiguredGeometryCircles(camera);

  if (!result.found)
  {
    log(result.message.isEmpty() ? tr("log.geometryCircleNotFound") + ": " + camera.id : result.message);
    return;
  }

  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  for (int i = geometries.circles.size() - 1; i >= 0; --i)
  {
    if (geometries.circles[i].meta.id == circleConfig.id)
    {
      geometries.circles.removeAt(i);
    }
  }
  geometries.circles.append(result.circle);
  GeometryOverlay circleOverlay;
  appendCirclePolyline(circleOverlay, result.circle.center, result.circle.radius, QColor("#00d2ff"), 7);
  appendCurrentPartPoseOverlay(camera, circleOverlay);
  largeImage()->setGeometryOverlay(circleOverlay);
  log(QString("%1: %2 cx=%3 cy=%4 r=%5")
              .arg(tr("log.geometryCircleFound"))
              .arg(camera.id)
              .arg(result.circle.center.x, 0, 'f', 1)
              .arg(result.circle.center.y, 0, 'f', 1)
              .arg(result.circle.radius, 0, 'f', 1));
}

void MainWindowGeometryModule::handleGeometryPointGuidePoint(const CameraConfig& camera, const QPointF& imagePoint)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  LineGeometryMouseController& controller = m_pointMouseControllers[camera.id];
  const bool completed = controller.handleClick(imagePoint);
  updateGeometryPointOverlay(camera);

  if (!completed)
  {
    return;
  }

  const LineGeometryEditorState& state = controller.state();
  GeometryPointRuntimeConfig& config = activeGeometryPointConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageGuide = true;

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasGuide = true;
  }

  largeImage()->setGeometryPointPickingEnabled(false);
  largeImage()->setGeometryOverlayPointEditingEnabled(true);
  updateGeometryPointOverlay(camera);
  log(tr("log.geometryPointGuideSaved") + ": " + camera.id);
  saveGeometryPointRecipe(camera);
  testGeometryPoint(camera);
}

void MainWindowGeometryModule::handleGeometryPointHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint)
{
  if (camera.id != selectedCameraId() || pointIndex < 0 || pointIndex > 1)
  {
    return;
  }

  LineGeometryMouseController& controller = m_pointMouseControllers[camera.id];
  controller.movePoint(pointIndex, imagePoint);

  const LineGeometryEditorState& state = controller.state();
  if (!state.hasStart || !state.hasEnd)
  {
    updateGeometryPointOverlay(camera);
    return;
  }

  GeometryPointRuntimeConfig& config = activeGeometryPointConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageGuide = true;

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasGuide = true;
  }
  else
  {
    config.hasGuide = false;
  }

  updateGeometryPointOverlay(camera);
  saveGeometryPointRecipe(camera);
  testGeometryPoint(camera);
}

GeometryOverlay MainWindowGeometryModule::configuredGeometryPointsOverlay(const CameraConfig& camera, bool includeActive) const
{
  GeometryOverlay overlay;
  const PartPose& pose = cameraRuntime().at(camera.id).currentPose();
  const QVector<GeometryPointRuntimeConfig> points = m_pointConfigs.value(camera.id);
  const int activeIndex = m_activePointIndexes.value(camera.id, 0);

  for (int i = 0; i < points.size(); ++i)
  {
    if (!includeActive && i == activeIndex)
    {
      continue;
    }

    const GeometryPointRuntimeConfig& point = points[i];
    const bool usePartGuide = pose.valid && point.hasGuide;
    if (!usePartGuide && !point.hasImageGuide)
    {
      continue;
    }

    const cv::Point2d imageStart = usePartGuide ? partToImage(pose, point.partStart) : point.imageStart;
    const cv::Point2d imageEnd = usePartGuide ? partToImage(pose, point.partEnd) : point.imageEnd;
    const QPointF start(imageStart.x, imageStart.y);
    const QPointF end(imageEnd.x, imageEnd.y);
    const QPointF center = (start + end) * 0.5;
    const QPointF delta = end - start;
    const double length = std::hypot(delta.x(), delta.y());
    const double angleDegrees = std::atan2(delta.y(), delta.x()) * 180.0 / CV_PI;
    const bool highlightActive =
      *context().activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry &&
      m_drawingTarget == DrawingTarget::Point &&
      i == activeIndex;
    const QColor lineColor = highlightActive ? QColor("#ff4fd8") : QColor(170, 205, 220, 170);
    const QColor bandColor = highlightActive ? QColor(255, 79, 216, 24) : QColor(120, 170, 190, 18);
    overlay.bands.append({center, QSizeF(length, 6.0), angleDegrees, lineColor, bandColor});
    overlay.lines.append({start, end, lineColor, highlightActive ? 3 : 1});
    if (includeActive && highlightActive)
    {
      overlay.points.append({start, "1"});
      overlay.points.append({end, "2"});
    }
  }

  return overlay;
}

void MainWindowGeometryModule::updateGeometryPointOverlay(const CameraConfig& camera, const GeometryOverlay& extraOverlay)
{
  GeometryOverlay overlay = configuredGeometryPointsOverlay(camera, false);
  GeometryOverlay activeOverlay = m_pointMouseControllers[camera.id].overlay();
  for (const GeometryOverlayPoint& point : activeOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : activeOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : activeOverlay.bands)
  {
    overlay.bands.append(band);
  }
  for (const GeometryOverlayPoint& point : extraOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : extraOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : extraOverlay.bands)
  {
    overlay.bands.append(band);
  }
  appendCurrentPartPoseOverlay(camera, overlay);
  largeImage()->setGeometryOverlay(overlay);
}

void MainWindowGeometryModule::appendCurrentPartPoseOverlay(const CameraConfig& camera, GeometryOverlay& overlay) const
{
  const auto runtimeIt = cameraRuntime().find(camera.id);
  if (runtimeIt == cameraRuntime().end())
  {
    return;
  }

  const PartPose& pose = runtimeIt->second.currentPose();
  if (!pose.valid)
  {
    return;
  }

  GeometryDiagnosticDrawing::appendCyanPointCross(overlay, pose.origin);
}

void MainWindowGeometryModule::testGeometryPoint(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  GeometryPointRuntimeConfig& pointConfig = activeGeometryPointConfig(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid && !pointConfig.hasGuide && pointConfig.hasImageGuide)
  {
    pointConfig.partStart = imageToPart(pose, pointConfig.imageStart);
    pointConfig.partEnd = imageToPart(pose, pointConfig.imageEnd);
    pointConfig.hasGuide = true;
  }

  const bool usePartGuide = pose.valid && pointConfig.hasGuide;
  if (!usePartGuide && !pointConfig.hasImageGuide)
  {
    updateGeometryPointOverlay(camera);
    return;
  }

  const cv::Point2d imageStart = usePartGuide ? partToImage(pose, pointConfig.partStart) : pointConfig.imageStart;
  const cv::Point2d imageEnd = usePartGuide ? partToImage(pose, pointConfig.partEnd) : pointConfig.imageEnd;
  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  EdgePointDetectorConfig config;
  config.id = pointConfig.id;
  config.label = pointConfig.id;
  config.scanStart = imageStart;
  config.scanEnd = imageEnd;
  config.edgeSensitivity = pointConfig.edgeSensitivity;
  config.useSubpixel = MainWindowCameraProfile::isBwDimensional(camera, MainWindowModuleBase::config()) && pointConfig.useSubpixel;
  config.transition = pointConfig.transition;
  config.pickMode = pointConfig.pickMode;

  EdgePointDetector detector;
  const EdgePointDetectorResult result = detector.detect(input, config);
  if (!result.processed || result.diagnosticImage.empty())
  {
    log(result.message.isEmpty() ? tr("log.geometryPointFailed") + ": " + camera.id : result.message);
    return;
  }

  selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
  largeImage()->setImage(selectedPreview());
  largeImage()->clearRoi();

  LineGeometryMouseController& controller = m_pointMouseControllers[camera.id];
  controller.setLine(QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y), 3.0);
  m_drawingTarget = DrawingTarget::Point;

  GeometryOverlay detectedOverlay;
  if (result.found)
  {
    GeometryDiagnosticDrawing::appendOrangePointCross(detectedOverlay, result.point.point);
  }
  updateGeometryPointOverlay(camera, detectedOverlay);

  if (!result.found)
  {
    log(result.message.isEmpty() ? tr("log.geometryPointNotFound") + ": " + camera.id : result.message);
    return;
  }

  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  for (int i = geometries.points.size() - 1; i >= 0; --i)
  {
    if (geometries.points[i].meta.id == pointConfig.id)
    {
      geometries.points.removeAt(i);
    }
  }
  geometries.points.append(result.point);

  log(QString("%1: %2 x=%3 y=%4")
              .arg(tr("log.geometryPointFound"))
              .arg(camera.id)
              .arg(result.point.point.x, 0, 'f', 1)
              .arg(result.point.point.y, 0, 'f', 1));
}
