#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/geometry/ConfiguredGeometryDetector.h"
#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryOverlayPrimitives.h"
#include "processing/GeometryMeasurementPipeline.h"
#include "util/CameraAsyncExecutor.h"

#include <QColor>

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <utility>

namespace
{
bool isMachineRunMode(const MainWindowContext& context)
{
  return context.machineRunning != nullptr && *context.machineRunning;
}

void applyGeometryPromotions(const PartPose& pose,
                             QVector<GeometryLineRuntimeConfig>& lines,
                             QVector<GeometryPointRuntimeConfig>& points,
                             QVector<GeometryCircleRuntimeConfig>& circles,
                             QVector<GeometryArcRuntimeConfig>& arcs)
{
  if (!pose.valid)
  {
    return;
  }

  for (GeometryLineRuntimeConfig& line : lines)
  {
    if (line.anchorInImageSpace || !line.hasImageLine || line.hasLine)
    {
      continue;
    }
    line.partStart = imageToPart(pose, line.imageStart);
    line.partEnd = imageToPart(pose, line.imageEnd);
    line.hasLine = true;
  }

  for (GeometryPointRuntimeConfig& point : points)
  {
    if (point.anchorInImageSpace || !point.hasImageGuide || point.hasGuide)
    {
      continue;
    }
    point.partStart = imageToPart(pose, point.imageStart);
    point.partEnd = imageToPart(pose, point.imageEnd);
    point.hasGuide = true;
  }

  for (GeometryCircleRuntimeConfig& circle : circles)
  {
    if (circle.anchorInImageSpace || !circle.hasImageCircle || circle.hasCircle)
    {
      continue;
    }
    circle.partCenter = imageToPart(pose, circle.imageCenter);
    circle.hasCircle = true;
    circle.anchorInImageSpace = false;
  }

  for (GeometryArcRuntimeConfig& arc : arcs)
  {
    if (arc.anchorInImageSpace || !arc.hasImageArc || arc.hasArc)
    {
      continue;
    }
    arc.partCenter = imageToPart(pose, arc.imageCenter);
    arc.partStart = imageToPart(pose, arc.imageStart);
    arc.partEnd = imageToPart(pose, arc.imageEnd);
    arc.hasArc = true;
  }
}

ConfiguredGeometryDetectInput buildConfiguredGeometryInput(const CameraConfig& camera,
                                                           const MainWindowContext& context,
                                                           const PartPose& pose,
                                                           const cv::Mat& input,
                                                           const QVector<GeometryLineRuntimeConfig>& lines,
                                                           const QVector<GeometryPointRuntimeConfig>& points,
                                                           const QVector<GeometryCircleRuntimeConfig>& circles,
                                                           const QVector<GeometryArcRuntimeConfig>& arcs,
                                                           bool buildDiagnostic,
                                                           bool buildGuideOverlay)
{
  ConfiguredGeometryDetectInput detectInput;
  detectInput.image = input;
  detectInput.pose = pose;
  detectInput.useSubpixelDimensional =
    MainWindowCameraProfile::isBwDimensional(camera, *context.config);
  detectInput.buildDiagnostic = buildDiagnostic;
  detectInput.buildGuideOverlay = buildGuideOverlay;
  detectInput.lines = lines;
  detectInput.points = points;
  detectInput.circles = circles;
  detectInput.arcs = arcs;
  return detectInput;
}
}

void MainWindowGeometryModule::applyConfiguredGeometryDetectionResult(const CameraConfig& camera,
                                                                    const ConfiguredGeometryDetectOutput& result)
{
  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  geometries.lines = result.lines;
  geometries.points = result.points;
  geometries.circles = result.circles;
  geometries.arcs = result.arcs;
}

void MainWindowGeometryModule::applyGeometryPipelineResult(const CameraConfig& camera,
                                                         const GeometryPipelineOutput& result)
{
  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  geometries.lines = result.lines;
  geometries.points = result.points;
  geometries.circles = result.circles;
  geometries.arcs = result.arcs;
  geometries.constructedPoints = result.constructedPoints;
  geometries.constructedLines = result.constructedLines;
  geometries.measurements = result.measurements;
}

void MainWindowGeometryModule::testConfiguredGeometryLinesAsync(const CameraConfig& camera,
                                                              std::function<void()> onComplete)
{
  loadGeometryLinesRecipe(camera);
  loadGeometryPointRecipe(camera);
  loadGeometryCirclesRecipe(camera);
  loadGeometryArcsRecipe(camera);

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    if (onComplete)
    {
      onComplete();
    }
    return;
  }

  QVector<GeometryLineRuntimeConfig>& lines = m_lineConfigs[camera.id];
  QVector<GeometryPointRuntimeConfig>& points = m_pointConfigs[camera.id];
  QVector<GeometryCircleRuntimeConfig>& circles = m_circleConfigs[camera.id];
  QVector<GeometryArcRuntimeConfig>& arcs = m_arcConfigs[camera.id];
  applyGeometryPromotions(pose, lines, points, circles, arcs);

  const quint64 generation = ++m_geometryJobGeneration[camera.id];
  GeometryPipelineInput pipelineInput;
  pipelineInput.geometry = buildConfiguredGeometryInput(
    camera,
    context(),
    pose,
    input.clone(),
    lines,
    points,
    circles,
    arcs,
    false,
    false);
  pipelineInput.constructedConfigs = recipes().loadConstructedGeometries(camera.id);
  pipelineInput.measurementConfigs = recipes().loadMeasurements(camera.id);
  pipelineInput.calibration.enabled = camera.calibration.enabled;
  pipelineInput.calibration.pixelSizeXMm = camera.calibration.pixelSizeXMm;
  pipelineInput.calibration.pixelSizeYMm = camera.calibration.pixelSizeYMm;

  CameraAsyncExecutor::runAsyncTask(
    camera.id,
    [pipelineInput]() { return runGeometryMeasurementPipeline(pipelineInput); },
    window(),
    [this, camera, generation, onComplete = std::move(onComplete)](const GeometryPipelineOutput& result) {
      if (m_geometryJobGeneration.value(camera.id) != generation)
      {
        return;
      }

      applyGeometryPipelineResult(camera, result);
      if (camera.id == selectedCameraId())
      {
        showRuntimeGeometryOverlay(camera);
      }
      if (onComplete)
      {
        onComplete();
      }
    },
    QString("pipeline.%1").arg(camera.id));
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

  loadGeometryPointRecipe(camera);
  loadGeometryCirclesRecipe(camera);
  loadGeometryArcsRecipe(camera);
  QVector<GeometryPointRuntimeConfig>& points = m_pointConfigs[camera.id];
  QVector<GeometryCircleRuntimeConfig>& circles = m_circleConfigs[camera.id];
  QVector<GeometryArcRuntimeConfig>& arcs = m_arcConfigs[camera.id];
  applyGeometryPromotions(pose, lines, points, circles, arcs);

  const bool updateView = camera.id == selectedCameraId();
  const bool runMode = isMachineRunMode(context());
  const ConfiguredGeometryDetectInput detectInput = buildConfiguredGeometryInput(
    camera,
    context(),
    pose,
    input,
    lines,
    points,
    circles,
    arcs,
    !runMode,
    !runMode && updateView);
  const ConfiguredGeometryDetectOutput result = detectConfiguredGeometries(detectInput);
  applyConfiguredGeometryDetectionResult(camera, result);

  GeometryOverlay detectedOverlay = result.guideOverlay;

  if (updateView && runMode)
  {
    showRuntimeGeometryOverlay(camera);
    return;
  }

  if (updateView && !result.diagnostic.empty())
  {
    selectedPreview() = context().imaging->matToPixmap(result.diagnostic);
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

    const bool usePartLine = pose.valid && activeLine.hasLine && !activeLine.anchorInImageSpace;
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
