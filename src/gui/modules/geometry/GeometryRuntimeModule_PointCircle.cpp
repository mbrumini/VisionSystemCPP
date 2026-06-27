#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryGuideRuntime.h"
#include "gui/geometry/GeometryMath.h"
#include "gui/geometry/GeometryOverlayPrimitives.h"
#include "processing/geometry/EdgeCircleDetector.h"
#include "processing/geometry/EdgeCircleDetectorExperimental.h"
#include "processing/geometry/EdgePointDetector.h"

#include <QColor>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

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
  showConfiguredGeometryCircles(camera);
  largeImage()->setThreePointCircleDrawingEnabled(true);
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

  PartPose pose = cameraRuntime()[camera.id].currentPose();
  if (!pose.valid && context().surface && context().surface->restoreSurfaceModelPoseFromSample(camera))
  {
    pose = cameraRuntime()[camera.id].currentPose();
  }
  if (pose.valid)
  {
    config.partCenter = imageToPart(pose, config.imageCenter);
    config.hasCircle = true;
    config.anchorInImageSpace = false;
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
}

void MainWindowGeometryModule::handleGeometryCircleBandChanged(const CameraConfig& camera,
                                                               const QVector<ImageCircle>& circles,
                                                               int changedRadiusIndex)
{
  if (camera.id != selectedCameraId() || circles.size() < 3)
  {
    return;
  }

  GeometryCircleRuntimeConfig& config = activeGeometryCircleConfig(camera.id);
  const QPoint center = circles[0].center;
  const int guideRadius = qMax(2, static_cast<int>(std::round(config.radius)));
  const int currentInnerRadius = qMax(1, guideRadius - config.innerBand);
  const int currentOuterRadius = guideRadius + config.outerBand;
  const int outerRadius = changedRadiusIndex == 0
    ? qMax(guideRadius + 1, circles[0].radius)
    : currentOuterRadius;
  const int innerRadius = changedRadiusIndex == 2
    ? qBound(1, circles[2].radius, guideRadius - 1)
    : currentInnerRadius;

  config.imageCenter = cv::Point2d(center.x(), center.y());
  config.radius = guideRadius;
  config.innerBand = qMax(1, guideRadius - innerRadius);
  config.outerBand = qMax(1, outerRadius - guideRadius);
  config.hasImageCircle = true;

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid)
  {
    config.partCenter = imageToPart(pose, config.imageCenter);
    config.hasCircle = true;
    config.anchorInImageSpace = false;
  }
  else
  {
    config.hasCircle = false;
  }

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
  cv::Size imageSize;
  const cv::Mat& frame = cameraRuntime()[camera.id].currentFrame();
  if (!frame.empty())
  {
    imageSize = frame.size();
  }
  QVector<GeometryCircleRuntimeConfig> circles = {circle};
  QVector<GeometryLineRuntimeConfig> lines;
  QVector<GeometryPointRuntimeConfig> points;
  QVector<GeometryArcRuntimeConfig> arcs;
  GeometryGuideRuntime::syncPartGuidesFromImage(pose, lines, points, circles, arcs);
  circle = circles.first();

  const QSize referenceSize = guideReferenceSize(camera.id);
  const bool usePartCircle = GeometryGuideRuntime::shouldUsePartGuide(
    pose.valid,
    circle.hasCircle,
    circle.hasImageCircle,
    circle.anchorInImageSpace,
    referenceSize,
    imageSize);
  if (usePartCircle || circle.hasImageCircle)
  {
    const cv::Point2d center = GeometryGuideRuntime::resolveImagePoint(
      pose, usePartCircle, circle.partCenter, circle.imageCenter, referenceSize, imageSize);
    const double guideRadius = usePartCircle
      ? circle.radius
      : GeometryGuideRuntime::mapImageGuideRadius(circle.radius, referenceSize, imageSize);
    const double innerRadius = std::max(1.0, guideRadius - circle.innerBand);
    const double outerRadius = guideRadius + circle.outerBand;
    appendGeometryCirclePolyline(overlay, center, innerRadius, QColor(0, 210, 255, 90), 1);
    appendGeometryCirclePolyline(overlay, center, guideRadius, QColor("#00d2ff"), 5);
    appendGeometryCirclePolyline(overlay, center, outerRadius, QColor(0, 210, 255, 90), 1);
    appendGeometryCircleSearchBandGuides(
      overlay,
      center,
      innerRadius,
      outerRadius,
      circle.scanDirection != EdgeLineScanDirection::NormalNegative,
      QColor(0, 210, 255, 150),
      2);
    largeImage()->setCircles({
      {QPoint(qRound(center.x), qRound(center.y)), qRound(outerRadius)},
      {QPoint(qRound(center.x), qRound(center.y)), qRound(guideRadius)},
      {QPoint(qRound(center.x), qRound(center.y)), qRound(innerRadius)}
    });
    largeImage()->setCircleBandEditingEnabled(true);
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
  if (!pose.valid && context().surface)
  {
    context().surface->localizePoseOnSample(camera);
  }
  const PartPose& resolvedPose = cameraRuntime()[camera.id].currentPose();
  QVector<GeometryCircleRuntimeConfig> circles = {circleConfig};
  QVector<GeometryLineRuntimeConfig> lines;
  QVector<GeometryPointRuntimeConfig> points;
  QVector<GeometryArcRuntimeConfig> arcs;
  GeometryGuideRuntime::syncPartGuidesFromImage(resolvedPose, lines, points, circles, arcs);
  circleConfig = circles.first();

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  const QSize referenceSize = guideReferenceSize(camera.id);
  const bool usePartCircle = GeometryGuideRuntime::shouldUsePartGuide(
    resolvedPose.valid,
    circleConfig.hasCircle,
    circleConfig.hasImageCircle,
    circleConfig.anchorInImageSpace,
    referenceSize,
    input.size());
  if (!usePartCircle && !circleConfig.hasImageCircle)
  {
    showConfiguredGeometryCircles(camera);
    log(tr("log.geometryCircleMissing") + ": " + camera.id);
    return;
  }

  const cv::Point2d guideCenter = GeometryGuideRuntime::resolveImagePoint(
    resolvedPose, usePartCircle, circleConfig.partCenter, circleConfig.imageCenter, referenceSize, input.size());
  const double guideRadius = usePartCircle
    ? circleConfig.radius
    : GeometryGuideRuntime::mapImageGuideRadius(circleConfig.radius, referenceSize, input.size());
  EdgeCircleDetectorConfig config;
  config.id = circleConfig.id;
  config.label = circleConfig.alias.trimmed().isEmpty() ? circleConfig.id : circleConfig.alias.trimmed();
  config.guideCenter = guideCenter;
  config.guideRadius = guideRadius;
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
        .arg(guideRadius, 0, 'f', 1)
        .arg(circleConfig.innerBand)
        .arg(circleConfig.outerBand)
        .arg(circleConfig.edgeSensitivity)
        .arg(circleConfig.edgeCleanupDerivative)
        .arg(circleConfig.edgeStatisticalFilter)
        .arg(circleConfig.scanDirection == EdgeLineScanDirection::NormalNegative ? "normal_negative" : "normal_positive")
        .arg(circleConfig.transition == EdgeLineTransition::DarkToLight ? "dark_to_light" : "light_to_dark")
        .arg(circleConfig.pickMode == EdgeLinePickMode::Last ? "last" : (circleConfig.pickMode == EdgeLinePickMode::Best ? "best" : "first")));

  const EdgeCircleDetectorResult result = detectEdgeCircleWithSelectedDetector(input, config);
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
  appendGeometryCirclePolyline(circleOverlay, result.circle.center, result.circle.radius, QColor("#00d2ff"), 7);
  appendCurrentPartPoseOverlay(camera, circleOverlay);
  largeImage()->setGeometryOverlay(circleOverlay);
  refreshMeasurementOverlay(camera);
  log(QString("%1: %2 cx=%3 cy=%4 r=%5")
              .arg(tr("log.geometryCircleFound"))
              .arg(camera.id)
              .arg(result.circle.center.x, 0, 'f', 1)
              .arg(result.circle.center.y, 0, 'f', 1)
              .arg(result.circle.radius, 0, 'f', 1));

  if (resolvedPose.valid)
  {
    circleConfig.partCenter = imageToPart(resolvedPose, result.circle.center);
    circleConfig.hasCircle = true;
  }
  else
  {
    circleConfig.imageCenter = result.circle.center;
    circleConfig.hasImageCircle = true;
  }
  circleConfig.radius = result.circle.radius;
  saveGeometryCirclesRecipe(camera);
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
  cv::Size imageSize;
  const cv::Mat& frame = cameraRuntime().at(camera.id).currentFrame();
  if (!frame.empty())
  {
    imageSize = frame.size();
  }
  const QSize referenceSize = guideReferenceSize(camera.id);

  for (int i = 0; i < points.size(); ++i)
  {
    if (!includeActive && i == activeIndex)
    {
      continue;
    }

    const GeometryPointRuntimeConfig& point = points[i];
    const bool usePartGuide = GeometryGuideRuntime::shouldUsePartGuide(
      pose.valid,
      point.hasGuide,
      point.hasImageGuide,
      point.anchorInImageSpace,
      referenceSize,
      imageSize);
    if (!usePartGuide && !point.hasImageGuide)
    {
      continue;
    }

    const cv::Point2d imageStart = GeometryGuideRuntime::resolveImagePoint(
      pose, usePartGuide, point.partStart, point.imageStart, referenceSize, imageSize);
    const cv::Point2d imageEnd = GeometryGuideRuntime::resolveImagePoint(
      pose, usePartGuide, point.partEnd, point.imageEnd, referenceSize, imageSize);
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
    appendGeometrySegmentDirectionArrow(
      overlay,
      start,
      end,
      highlightActive ? QColor("#ff4fd8") : QColor(170, 205, 220, 130),
      highlightActive ? 2 : 1);
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

  const bool compact = context().machineRunning != nullptr && *context().machineRunning;
  GeometryDiagnosticDrawing::appendCyanPointCross(overlay, pose.origin, compact);

  const int imageWidth = selectedPreview().width();
  const int imageHeight = selectedPreview().height();
  const double imageDiagonal = imageWidth > 0 && imageHeight > 0
    ? std::hypot(static_cast<double>(imageWidth), static_cast<double>(imageHeight))
    : 1000.0;
  const double armLength = std::max(220.0, imageDiagonal * (compact ? 0.34 : 0.42));
  const int axisWidth = compact ? 3 : 5;
  const QColor xColor(255, 35, 35);
  const QColor yColor(35, 220, 90);
  const cv::Point2d xStart = pose.origin - pose.xAxis * armLength;
  const cv::Point2d xEnd = pose.origin + pose.xAxis * armLength;
  const cv::Point2d yStart = pose.origin - pose.yAxis * armLength;
  const cv::Point2d yEnd = pose.origin + pose.yAxis * armLength;
  overlay.lines.append({
    QPointF(xStart.x, xStart.y),
    QPointF(xEnd.x, xEnd.y),
    xColor,
    axisWidth
  });
  overlay.lines.append({
    QPointF(yStart.x, yStart.y),
    QPointF(yEnd.x, yEnd.y),
    yColor,
    axisWidth
  });
  overlay.points.append({QPointF(xEnd.x, xEnd.y), "X", xColor, compact ? 4.0 : 6.0});
  overlay.points.append({QPointF(yEnd.x, yEnd.y), "Y", yColor, compact ? 4.0 : 6.0});

  syncThreadExtractionRoiOverlay(camera);
}

void MainWindowGeometryModule::testGeometryPoint(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  GeometryPointRuntimeConfig& pointConfig = activeGeometryPointConfig(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (!pose.valid && context().surface)
  {
    context().surface->localizePoseOnSample(camera);
  }
  const PartPose& resolvedPose = cameraRuntime()[camera.id].currentPose();
  QVector<GeometryPointRuntimeConfig> points = {pointConfig};
  QVector<GeometryLineRuntimeConfig> lines;
  QVector<GeometryCircleRuntimeConfig> circles;
  QVector<GeometryArcRuntimeConfig> arcs;
  GeometryGuideRuntime::syncPartGuidesFromImage(resolvedPose, lines, points, circles, arcs);
  pointConfig = points.first();

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  const QSize referenceSize = guideReferenceSize(camera.id);
  const bool usePartGuide = GeometryGuideRuntime::shouldUsePartGuide(
    resolvedPose.valid,
    pointConfig.hasGuide,
    pointConfig.hasImageGuide,
    pointConfig.anchorInImageSpace,
    referenceSize,
    input.size());
  if (!usePartGuide && !pointConfig.hasImageGuide)
  {
    updateGeometryPointOverlay(camera);
    return;
  }

  const cv::Point2d imageStart = GeometryGuideRuntime::resolveImagePoint(
    resolvedPose, usePartGuide, pointConfig.partStart, pointConfig.imageStart, referenceSize, input.size());
  const cv::Point2d imageEnd = GeometryGuideRuntime::resolveImagePoint(
    resolvedPose, usePartGuide, pointConfig.partEnd, pointConfig.imageEnd, referenceSize, input.size());

  EdgePointDetectorConfig config;
  config.id = pointConfig.id;
  config.label = pointConfig.alias.trimmed().isEmpty() ? pointConfig.id : pointConfig.alias.trimmed();
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
  refreshMeasurementOverlay(camera);

  log(QString("%1: %2 x=%3 y=%4")
              .arg(tr("log.geometryPointFound"))
              .arg(camera.id)
              .arg(result.point.point.x, 0, 'f', 1)
              .arg(result.point.point.y, 0, 'f', 1));
}

