#include "gui/modules/MainWindowThreadModule.h"

#include "config/GeometryRecipeJson.h"
#include "gui/ImagePrimitives.h"
#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/TouchIconButton.h"
#include "processing/thread/ThreadInspectionUtils.h"
#include "processing/thread/ThreadProfileExtractor.h"
#include "processing/thread/ThreadProfileMeasurer.h"
#include "processing/thread/ThreadProfileRootAnalyzer.h"
#include "geometry/GeometrySet.h"
#include "processing/GeometryMeasurementPipeline.h"
#include "runtime/PartPose.h"

#include <algorithm>
#include <memory>

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

MainWindowThreadModule::MainWindowThreadModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

namespace
{
constexpr double kThreadOverlaySmoothingAlpha = 0.28;

double blendValue(double previous, double current, bool hasPrevious)
{
  if (!hasPrevious)
  {
    return current;
  }

  return previous * (1.0 - kThreadOverlaySmoothingAlpha) + current * kThreadOverlaySmoothingAlpha;
}

template <typename PointType>
const PointType* findNearestPoint(const std::vector<PointType>& points, double alongPx, double maxDistancePx)
{
  const PointType* best = nullptr;
  double bestDistance = maxDistancePx;
  for (const PointType& point : points)
  {
    const double distance = std::abs(point.alongPx - alongPx);
    if (distance <= bestDistance)
    {
      bestDistance = distance;
      best = &point;
    }
  }
  return best;
}

template <typename PointType>
PointType blendPoints(const PointType* previous, const PointType& incoming)
{
  if (!previous)
  {
    return incoming;
  }

  PointType blended = incoming;
  blended.alongPx = blendValue(previous->alongPx, incoming.alongPx, true);
  blended.topY = blendValue(previous->topY, incoming.topY, true);
  blended.bottomY = blendValue(previous->bottomY, incoming.bottomY, true);
  blended.diameterPx = blended.bottomY - blended.topY;
  blended.imageTopX = blendValue(previous->imageTopX, incoming.imageTopX, true);
  blended.imageTopY = blendValue(previous->imageTopY, incoming.imageTopY, true);
  blended.imageBottomX = blendValue(previous->imageBottomX, incoming.imageBottomX, true);
  blended.imageBottomY = blendValue(previous->imageBottomY, incoming.imageBottomY, true);
  return blended;
}
}

ThreadDiameterValues MainWindowThreadModule::smoothThreadOverlay(
  const QString& cameraId,
  const ThreadDiameterValues& incoming)
{
  if (!incoming.valid && incoming.grooves.empty() && incoming.crests.empty())
  {
    m_stableThreadOverlay.remove(cameraId);
    return incoming;
  }

  ThreadDiameterValues& previous = m_stableThreadOverlay[cameraId];
  if (!previous.valid)
  {
    previous = incoming;
    return incoming;
  }

  const double matchDistance = incoming.pitchPx > 0.5 ? incoming.pitchPx * 0.45 : 12.0;

  ThreadDiameterValues smoothed = incoming;
  smoothed.grooves.clear();
  smoothed.grooves.reserve(incoming.grooves.size());
  for (const ThreadGroovePoint& groove : incoming.grooves)
  {
    const ThreadGroovePoint* previousGroove =
      findNearestPoint(previous.grooves, groove.alongPx, matchDistance);
    smoothed.grooves.push_back(blendPoints(previousGroove, groove));
  }

  smoothed.crests.clear();
  smoothed.crests.reserve(incoming.crests.size());
  for (const ThreadCrestPoint& crest : incoming.crests)
  {
    const ThreadCrestPoint* previousCrest = findNearestPoint(previous.crests, crest.alongPx, matchDistance);
    smoothed.crests.push_back(blendPoints(previousCrest, crest));
  }

  previous = smoothed;
  return smoothed;
}

void MainWindowThreadModule::ensureThreadInspectionPose(const CameraConfig& camera)
{
  if (context().setup)
  {
    context().setup->refreshPoseForCurrentFrame(camera);
  }

  if (cameraRuntime()[camera.id].currentPose().valid)
  {
    return;
  }

  if (context().surface)
  {
    context().surface->localizePoseOnSample(camera);
  }
}

namespace
{
ImageRotatedRect threadExtractionRotatedArea(
  const ThreadInspectionSettings& settings,
  const PartPose& pose)
{
  if (!settings.hasExtractionRoi)
  {
    return {};
  }

  if (!pose.valid && usePartSpaceThreadRoi(settings, pose))
  {
    return {};
  }

  const std::array<cv::Point2f, 4> quad = effectiveThreadExtractionImageQuad(settings, pose);
  const cv::Point2f center = (quad[0] + quad[1] + quad[2] + quad[3]) * 0.25f;
  const double width = cv::norm(quad[1] - quad[0]);
  const double height = cv::norm(quad[3] - quad[0]);
  if (width <= 2.0 || height <= 2.0)
  {
    return {};
  }

  const double angleDegrees = std::atan2(quad[1].y - quad[0].y, quad[1].x - quad[0].x) * 180.0 / CV_PI;
  return {
    QPointF(center.x, center.y),
    QSizeF(width, height),
    angleDegrees
  };
}

void appendPartPoseAxesOverlay(GeometryOverlay& overlay, const PartPose& pose, const QSize& imageSize)
{
  if (!pose.valid)
  {
    return;
  }

  const double imageDiagonal = imageSize.width() > 0 && imageSize.height() > 0
    ? std::hypot(static_cast<double>(imageSize.width()), static_cast<double>(imageSize.height()))
    : 1000.0;
  const double armLength = std::max(220.0, imageDiagonal * 0.38);
  const QColor xColor(255, 35, 35);
  const QColor yColor(35, 220, 90);
  const cv::Point2d xStart = pose.origin - pose.xAxis * armLength;
  const cv::Point2d xEnd = pose.origin + pose.xAxis * armLength;
  const cv::Point2d yStart = pose.origin - pose.yAxis * armLength;
  const cv::Point2d yEnd = pose.origin + pose.yAxis * armLength;

  overlay.lines.append({QPointF(xStart.x, xStart.y), QPointF(xEnd.x, xEnd.y), xColor, 4});
  overlay.lines.append({QPointF(yStart.x, yStart.y), QPointF(yEnd.x, yEnd.y), yColor, 4});
  overlay.points.append({QPointF(xEnd.x, xEnd.y), "X", xColor, 5.0});
  overlay.points.append({QPointF(yEnd.x, yEnd.y), "Y", yColor, 5.0});
}

void appendThreadProfileOverlay(GeometryOverlay& overlay,
                              const ThreadProfileResult& result,
                              const ThreadDiameterValues& diameters,
                              bool includeProfileLines)
{
  if (result.columns.size() < 2)
  {
    return;
  }

  const QColor crestColor(80, 220, 80);
  const QColor rootColor(60, 200, 255);
  const QColor crestPointColor(0, 170, 255);
  const QColor groovePointColor(255, 70, 70);
  constexpr double crestPointRadius = 4.0;
  constexpr double groovePointRadius = 3.0;
  if (!includeProfileLines)
  {
    for (const ThreadCrestPoint& crest : diameters.crests)
    {
      overlay.points.append({QPointF(crest.imageTopX, crest.imageTopY), QString(), crestPointColor, crestPointRadius});
      overlay.points.append({QPointF(crest.imageBottomX, crest.imageBottomY), QString(), crestPointColor, crestPointRadius});
    }
    for (const ThreadGroovePoint& groove : diameters.grooves)
    {
      overlay.points.append({QPointF(groove.imageTopX, groove.imageTopY), QString(), groovePointColor, groovePointRadius});
      overlay.points.append({QPointF(groove.imageBottomX, groove.imageBottomY), QString(), groovePointColor, groovePointRadius});
    }
    return;
  }

  for (size_t index = 1; index < result.columns.size(); ++index)
  {
    const ThreadProfileColumn& previous = result.columns[index - 1];
    const ThreadProfileColumn& current = result.columns[index];
    if (!previous.valid || !current.valid)
    {
      continue;
    }

    overlay.lines.append({
      QPointF(previous.crestX, previous.crestY),
      QPointF(current.crestX, current.crestY),
      crestColor,
      2
    });
    overlay.lines.append({
      QPointF(previous.rootX, previous.rootY),
      QPointF(current.rootX, current.rootY),
      rootColor,
      2
    });
  }

  for (const ThreadCrestPoint& crest : diameters.crests)
  {
    overlay.points.append({QPointF(crest.imageTopX, crest.imageTopY), QString(), crestPointColor, crestPointRadius});
    overlay.points.append({QPointF(crest.imageBottomX, crest.imageBottomY), QString(), crestPointColor, crestPointRadius});
  }
  for (const ThreadGroovePoint& groove : diameters.grooves)
  {
    overlay.points.append({QPointF(groove.imageTopX, groove.imageTopY), QString(), groovePointColor, groovePointRadius});
    overlay.points.append({QPointF(groove.imageBottomX, groove.imageBottomY), QString(), groovePointColor, groovePointRadius});
  }
}
}

void MainWindowThreadModule::syncExtractionRoiOverlay(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  const bool machineRunning = context().machineRunning != nullptr && *context().machineRunning;
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid && !machineRunning)
  {
    QString migrateError;
    if (recipes().migrateThreadInspectionRoiToPartSpace(camera.id, pose, &migrateError))
    {
      log(tr("log.threadRoiMigratedToPart"));
    }
    else if (!migrateError.isEmpty())
    {
      log(migrateError);
    }
  }

  ThreadInspectionSettings settings = recipes().loadThreadInspectionSettings(camera.id);
  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  const QSize imageSize = input.empty()
    ? QSize(selectedPreview().width(), selectedPreview().height())
    : QSize(input.cols, input.rows);
  const ImageRotatedRect rotatedArea = threadExtractionRotatedArea(settings, pose);

  largeImage()->clearRoi();
  if (rotatedArea.size.width() > 2.0 && rotatedArea.size.height() > 2.0)
  {
    largeImage()->setGeometryArea(rotatedArea);
  }
  else
  {
    largeImage()->clearGeometryArea();
  }

  Q_UNUSED(imageSize);
}

void MainWindowThreadModule::activateThreadExtractionRoiDrawing(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (!pose.valid && context().surface)
  {
    context().surface->localizePoseOnSample(camera);
  }
  if (!cameraRuntime()[camera.id].currentPose().valid)
  {
    log(tr("log.threadRoiNeedsPose") + ": " + camera.id);
    return;
  }

  context().imaging->ensureReferenceImageVisible(camera);
  syncExtractionRoiOverlay(camera);
  largeImage()->setRoiDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::ThreadInspection;
  log(tr("log.threadRoiDrawing") + ": " + camera.id);
}

void MainWindowThreadModule::clearThreadExtractionRoi(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  QString error;
  if (!recipes().clearThreadInspectionExtractionRoi(camera.id, &error))
  {
    log(error);
    return;
  }

  context().deactivateImageDrawingTools();
  largeImage()->clearRoi();
  largeImage()->clearGeometryArea();
  context().imaging->ensureReferenceImageVisible(camera);
  syncExtractionRoiOverlay(camera);
  log(tr("log.threadRoiCleared") + ": " + camera.id);
  activateThreadExtractionRoiDrawing(camera);
}

void MainWindowThreadModule::applyThreadMeasurements(const CameraConfig& camera, const ThreadProfileResult& result)
{
  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  geometries.measurements.erase(
    std::remove_if(
      geometries.measurements.begin(),
      geometries.measurements.end(),
      [](const MeasurementResult& measurement) {
        return measurement.type == QStringLiteral("thread_major_diameter") ||
               measurement.type == QStringLiteral("thread_minor_diameter") ||
               measurement.type == QStringLiteral("thread_pitch") ||
               measurement.type == QStringLiteral("thread_phase") ||
               measurement.type == QStringLiteral("thread_pitch_diameter");
      }),
    geometries.measurements.end());

  ThreadInspectionSettings settings = recipes().loadThreadInspectionSettings(camera.id);
  if (!settings.enabled || !settings.hasExtractionRoi)
  {
    return;
  }

  if (settings.majorDiameter.alias.isEmpty())
  {
    settings.majorDiameter.alias = tr("labels.threadMajorDiameter");
  }
  if (settings.pitchLength.alias.isEmpty())
  {
    settings.pitchLength.alias = tr("labels.threadPitch");
  }
  if (settings.phaseOffset.alias.isEmpty())
  {
    settings.phaseOffset.alias = tr("labels.threadPhase");
  }
  if (settings.minorDiameter.alias.isEmpty())
  {
    settings.minorDiameter.alias = tr("labels.threadMinorDiameter");
  }

  CameraMeasurementCalibration calibration;
  calibration.enabled = camera.calibration.enabled;
  calibration.pixelSizeXMm = camera.calibration.pixelSizeXMm;
  calibration.pixelSizeYMm = camera.calibration.pixelSizeYMm;

  const ThreadDiameterValues diameters = ThreadProfileMeasurer().measureDiameters(result);
  if (!diameters.valid &&
      context().isDetailedLogEnabled &&
      context().isDetailedLogEnabled() &&
      !diameters.diagnostic.isEmpty())
  {
    log(QStringLiteral("thread diagnostic: %1 %2").arg(camera.id, diameters.diagnostic));
  }
  const QVector<MeasurementResult> measurements =
    ThreadProfileMeasurer().buildMeasurementResults(settings, diameters, calibration);
  for (const MeasurementResult& measurement : measurements)
  {
    geometries.measurements.append(measurement);
  }

  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
}

void MainWindowThreadModule::refreshThreadProfileOverlay(const CameraConfig& camera)
{
  if (m_refreshingThreadOverlay)
  {
    return;
  }

  struct ReentrancyGuard
  {
    bool& flag;
    explicit ReentrancyGuard(bool& inFlag) : flag(inFlag) { flag = true; }
    ~ReentrancyGuard() { flag = false; }
  };
  const ReentrancyGuard guard(m_refreshingThreadOverlay);

  const ThreadInspectionSettings settings = recipes().loadThreadInspectionSettings(camera.id);
  const bool updateView = camera.id == selectedCameraId();
  if (!settings.hasExtractionRoi)
  {
    if (updateView)
    {
      syncExtractionRoiOverlay(camera);
    }
    applyThreadMeasurements(camera, {});
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  const bool machineRunning = context().machineRunning != nullptr && *context().machineRunning;

  if (updateView)
  {
    syncExtractionRoiOverlay(camera);
  }

  ThreadProfileResult result;
  if (!input.empty() && canExtractThreadProfile(settings, pose))
  {
    result = ThreadProfileExtractor().extractOriented(input, pose, settings);
  }

  applyThreadMeasurements(camera, result);

  if (!updateView)
  {
    return;
  }

  if (input.empty() || !canExtractThreadProfile(settings, pose))
  {
    largeImage()->setGeometryOverlay({});
    return;
  }

  const bool showDiagnosticPreview = !machineRunning && !result.diagnosticImage.empty();
  if (showDiagnosticPreview)
  {
    selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
    largeImage()->setImage(selectedPreview());
  }

  if (result.validColumns < 2)
  {
    largeImage()->setGeometryOverlay({});
    return;
  }

  GeometryOverlay overlay;
  const QSize overlayImageSize = input.empty()
    ? QSize(selectedPreview().width(), selectedPreview().height())
    : QSize(input.cols, input.rows);
  appendPartPoseAxesOverlay(overlay, pose, overlayImageSize);
  const ThreadDiameterValues measured = ThreadProfileMeasurer().measureDiameters(result);
  if (machineRunning)
  {
    m_stableThreadOverlay.remove(camera.id);
  }
  const ThreadDiameterValues diameters = machineRunning ? measured : smoothThreadOverlay(camera.id, measured);
  appendThreadProfileOverlay(overlay, result, diameters, !showDiagnosticPreview);
  largeImage()->setGeometryOverlay(overlay);
}

void MainWindowThreadModule::testThreadExtraction(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  const ThreadInspectionSettings settings = recipes().loadThreadInspectionSettings(camera.id);
  if (!settings.hasExtractionRoi)
  {
    log(tr("log.threadRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError.isEmpty() ? tr("log.threadExtractionFailed") : imageError);
    return;
  }

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (!canExtractThreadProfile(settings, pose))
  {
    ensureThreadInspectionPose(camera);
  }

  const PartPose& resolvedPose = cameraRuntime()[camera.id].currentPose();
  if (!canExtractThreadProfile(settings, resolvedPose))
  {
    log(tr("log.partPoseMissing") + ": " + camera.id);
    refreshThreadProfileOverlay(camera);
    return;
  }

  const ThreadProfileResult result = ThreadProfileExtractor().extractOriented(input, resolvedPose, settings);
  if (!result.valid)
  {
    log(tr("log.threadExtractionFailed") + ": " + camera.id + " (" + result.error + ")");
    refreshThreadProfileOverlay(camera);
    return;
  }

  refreshThreadProfileOverlay(camera);
  if (result.validColumns < static_cast<int>(result.columns.size()) / 6)
  {
    log(tr("log.threadThresholdHint"));
  }
  log(QString("%1: %2 cols=%3/%4")
        .arg(tr("log.threadExtractionOk"))
        .arg(camera.id)
        .arg(result.validColumns)
        .arg(result.columns.size()));
}

void MainWindowThreadModule::showThreadInspectionPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  const ThreadInspectionSettings settings = recipes().loadThreadInspectionSettings(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  context().imaging->ensureReferenceImageVisible(camera);

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.threadInspection"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(tr("labels.threadInspectionNote"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* poseLabel = new QLabel(
    pose.valid ? tr("labels.partPose")
               : (settings.imageRoi.isValid() ? tr("labels.threadImageRoiPreview") : tr("log.partPoseMissing")),
    panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setSpacing(6);

  auto* roiButton = createTouchIconButton("threadExtractionRoi", tr("actions.threadExtractionRoi"), buttons);
  auto* clearRoiButton = createTouchIconButton("clear", tr("actions.clearThreadRoi"), buttons);
  auto* testButton = createTouchIconButton("testThreadExtraction", tr("actions.testThreadExtraction"), buttons);
  QObject::connect(roiButton, &QPushButton::clicked, window(), [this, camera]() {
    activateThreadExtractionRoiDrawing(camera);
  });
  QObject::connect(clearRoiButton, &QPushButton::clicked, window(), [this, camera]() {
    clearThreadExtractionRoi(camera);
  });
  QObject::connect(testButton, &QPushButton::clicked, window(), [this, camera]() {
    testThreadExtraction(camera);
  });
  buttonsLayout->addWidget(roiButton, 0, 0, 1, 2);
  buttonsLayout->addWidget(clearRoiButton, 1, 0);
  buttonsLayout->addWidget(testButton, 1, 1);
  layout->addWidget(buttons);

  auto* filterBox = new QGroupBox(tr("labels.threadDirtFilter"), panel);
  auto* filterLayout = new QGridLayout(filterBox);
  filterLayout->setContentsMargins(8, 10, 8, 8);

  auto working = std::make_shared<ThreadInspectionSettings>(settings);
  auto filterTimer = new QTimer(panel);
  filterTimer->setSingleShot(true);
  filterTimer->setInterval(180);

  auto persistFilter = [this, camera, working]() {
    if (working->thresholdMin > working->thresholdMax)
    {
      std::swap(working->thresholdMin, working->thresholdMax);
    }

    QString error;
    if (!recipes().saveThreadInspectionSettings(camera.id, *working, &error))
    {
      log(error);
      return;
    }

    if (recipes().loadThreadInspectionSettings(camera.id).hasExtractionRoi)
    {
      testThreadExtraction(camera);
    }
  };

  QObject::connect(filterTimer, &QTimer::timeout, window(), [persistFilter]() {
    persistFilter();
  });

  auto queueFilterSave = [filterTimer]() {
    filterTimer->start();
  };

  auto addSlider = [&](int row,
                       const QString& labelText,
                       int minValue,
                       int maxValue,
                       int value,
                       const std::function<void(int)>& applyValue) {
    auto* label = new QLabel(labelText, filterBox);
    auto* valueLabel = new QLabel(QString::number(value), filterBox);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* slider = new QSlider(Qt::Horizontal, filterBox);
    slider->setRange(minValue, maxValue);
    slider->setValue(value);
    slider->setTracking(true);
    QObject::connect(slider, &QSlider::valueChanged, panel, [valueLabel, applyValue, queueFilterSave](int newValue) {
      valueLabel->setText(QString::number(newValue));
      applyValue(newValue);
      queueFilterSave();
    });
    filterLayout->addWidget(label, row, 0);
    filterLayout->addWidget(valueLabel, row, 1);
    filterLayout->addWidget(slider, row + 1, 0, 1, 2);
  };

  addSlider(0, tr("labels.thresholdMin"), 0, 255, working->thresholdMin, [working](int value) {
    working->thresholdMin = value;
  });
  addSlider(2, tr("labels.thresholdMax"), 0, 255, working->thresholdMax, [working](int value) {
    working->thresholdMax = value;
  });
  addSlider(4, tr("labels.threadMorphOpenRadius"), 0, 15, working->morphOpenRadius, [working](int value) {
    working->morphOpenRadius = value;
  });
  addSlider(6, tr("labels.threadMinSpeckArea"), 0, 200, working->minSpeckAreaPx, [working](int value) {
    working->minSpeckAreaPx = value;
  });
  addSlider(8, tr("labels.threadProfileSmoothRadius"), 0, 15, working->profileSmoothRadius, [working](int value) {
    working->profileSmoothRadius = value;
  });

  auto* outlierLabel = new QLabel(tr("labels.threadOutlierSigma"), filterBox);
  auto* outlierSpin = new QDoubleSpinBox(filterBox);
  outlierSpin->setRange(0.0, 8.0);
  outlierSpin->setSingleStep(0.1);
  outlierSpin->setDecimals(1);
  outlierSpin->setValue(working->outlierRejectSigma);
  QObject::connect(outlierSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), panel, [working, queueFilterSave](double value) {
    working->outlierRejectSigma = value;
    queueFilterSave();
  });
  filterLayout->addWidget(outlierLabel, 10, 0);
  filterLayout->addWidget(outlierSpin, 10, 1);

  layout->addWidget(filterBox);

  auto* backButton = createTouchIconButton("back", tr("commands.backToCameraTools"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    context().showCameraToolList(camera);
    log(tr("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tr("tools.threadInspection"));
  syncExtractionRoiOverlay(camera);
  refreshThreadProfileOverlay(camera);
}
