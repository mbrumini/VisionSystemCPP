#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"

#include "processing/geometry/EdgeLineDetector.h"
#include "util/AsyncExecutor.h"

#include <QColor>

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <memory>

using AsyncExecutor::runAsyncTask;

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

