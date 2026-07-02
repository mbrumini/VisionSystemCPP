#include "gui/MainWindow.h"

#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowThreadModule.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "gui/TouchIconButton.h"
#include "simulator/SimulatorBridge.h"
#include "gui/SurfaceLocalizationAdapters.h"
#include "processing/SurfaceDefectProcessor.h"
#include "util/AsyncExecutor.h"
#include "util/CameraAsyncExecutor.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QPolygon>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QSettings>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>

#include <cmath>

void MainWindow::incPendingJobs(const QString& cameraId)
{
  const int v = m_cameraPendingJobs.value(cameraId, 0) + 1;
  m_cameraPendingJobs[cameraId] = v;
  m_cameraProcessingBusy[cameraId] = true;
  updateCameraStripStatus(cameraId);
  if (m_machineRunning && m_selectedCameraId.isEmpty())
  {
    refreshProductionOverviewPanel();
  }
  else if (m_machineRunning && cameraId == m_selectedCameraId)
  {
    refreshCameraMonitoringPanel(m_selectedCamera);
  }
}

void MainWindow::decPendingJobs(const QString& cameraId)
{
  int v = m_cameraPendingJobs.value(cameraId, 0) - 1;
  if (v < 0)
  {
    v = 0;
  }
  m_cameraPendingJobs[cameraId] = v;
  m_cameraProcessingBusy[cameraId] = (v > 0);
  updateCameraStripStatus(cameraId);
  if (m_machineRunning && v == 0)
  {
    notifyProductionPieceCompleted(cameraId);
  }
  if (m_machineRunning && m_selectedCameraId.isEmpty())
  {
    refreshProductionOverviewPanel();
  }
  else if (cameraId == m_selectedCameraId)
  {
    scheduleMeasurementResultsUpdate();
    if (m_machineRunning)
    {
      refreshCameraMonitoringPanel(m_selectedCamera);
    }
  }
}

void MainWindow::publishSimulatorResult(const QString& cameraId)
{
  const QString resultMode =
    QSettings().value("simulator/resultMode", "full").toString();
  if (resultMode == "none")
  {
    return;
  }

  const auto runtimeIt = m_cameraRuntime.find(cameraId);
  if (runtimeIt == m_cameraRuntime.end())
  {
    return;
  }

  const CameraRuntime& runtime = runtimeIt->second;
  const SimulatorFrameMetadata& metadata = runtime.currentSimulatorFrame();
  if (!metadata.valid())
  {
    return;
  }

  const PartPose& pose = runtime.currentPose();
  QJsonObject poseObject;
  poseObject["valid"] = pose.valid;
  poseObject["x"] = pose.origin.x;
  poseObject["y"] = pose.origin.y;
  poseObject["angleDeg"] = pose.angleRadians * 180.0 / CV_PI;
  poseObject["score"] = pose.score;
  poseObject["method"] = pose.method;

  const GeometrySet& geometries = runtime.geometries();
  QJsonArray measurements;
  if (resultMode != "summary")
  {
    for (const MeasurementResult& measurement : geometries.measurements)
    {
      QJsonObject item;
      item["id"] = measurement.id;
      item["alias"] = measurement.alias;
      item["type"] = measurement.type;
      item["valid"] = measurement.valid;
      item["valuePixels"] = measurement.valuePixels;
      item["hasRealValue"] = measurement.hasRealValue;
      item["valueReal"] = measurement.valueReal;
      item["unit"] = measurement.unit;
      item["judgement"] = measurement.judgement;
      if (measurement.sampleCount > 0)
      {
        item["sampleCount"] = measurement.sampleCount;
      }
      if (measurement.pointCount > 0)
      {
        item["pointCount"] = measurement.pointCount;
      }
      if (!measurement.diagnostic.isEmpty())
      {
        item["diagnostic"] = measurement.diagnostic;
      }
      measurements.append(item);
    }
  }

  QJsonObject geometryCounts;
  geometryCounts["points"] = geometries.points.size();
  geometryCounts["lines"] = geometries.lines.size();
  geometryCounts["circles"] = geometries.circles.size();
  geometryCounts["arcs"] = geometries.arcs.size();
  geometryCounts["constructedPoints"] = geometries.constructedPoints.size();
  geometryCounts["constructedLines"] = geometries.constructedLines.size();
  geometryCounts["edges"] = geometries.edges.size();
  geometryCounts["contours"] = geometries.contours.size();

  QJsonObject result;
  result["status"] = "processed";
  result["resultMode"] = resultMode;
  result["visionFrameIndex"] = runtime.frameIndex();
  result["processingMs"] = m_lastSetupScanElapsedMs.value(cameraId, -1);
  result["pose"] = poseObject;
  result["geometryCounts"] = geometryCounts;
  result["measurements"] = measurements;
  result["warnings"] = QJsonArray();
  result["errors"] = QJsonArray();

  SimulatorBridge::instance().publishResult(metadata, result);
}

void MainWindow::handleSimulatorFrameAvailable(const QString& channel)
{
  CameraConfig simulatorCamera;
  if (channel != "SELECTED")
  {
    for (const CameraConfig& camera : m_config.activeCameras())
    {
      const QString configuredChannel =
        camera.simulatorChannel.isEmpty() ? camera.id : camera.simulatorChannel;
      if (camera.type == "simulator" && configuredChannel == channel)
      {
        simulatorCamera = camera;
        break;
      }
    }
  }

  if (simulatorCamera.id.isEmpty())
  {
    for (const CameraConfig& camera : m_config.activeCameras())
    {
      if (camera.id == m_selectedCameraId)
      {
        simulatorCamera = camera;
        break;
      }
    }
  }

  if (simulatorCamera.id.isEmpty())
  {
    appendLog("Frame simulatore senza slot configurato: " + channel);
    return;
  }

  if (!m_machineRunning)
  {
    const int discardedFrames = SimulatorBridge::instance().discardAllQueuedFrames(
      QStringLiteral("Vision in STOP: premere Start generale"));
    if (discardedFrames > 0)
    {
      appendLog(QString("Simulatore: scartati %1 frame in coda (STOP attivo)").arg(discardedFrames));
    }
    else
    {
      appendLog(QString("Frame simulatore in attesa di START: %1 channel=%2")
        .arg(simulatorCamera.id, channel));
    }
    SimulatorBridge::instance().publishChannelEvent(
      channel,
      "waitingStart",
      "Vision e' in STOP: premere Start generale");
    return;
  }

  SimulatorBridge::instance().publishChannelEvent(
    channel,
    "frameScheduled",
    "Frame assegnato alla pipeline camera");
  processNextSimulatorFrame(simulatorCamera);
}

void MainWindow::handleSimulatorSampleAvailable(const SimulatorFrame& frame)
{
  CameraConfig camera;
  if (frame.metadata.channel != "SELECTED" && frame.metadata.cameraId != "SELECTED")
  {
    for (const CameraConfig& configured : m_config.activeCameras())
    {
      const QString channel =
        configured.simulatorChannel.isEmpty() ? configured.id : configured.simulatorChannel;
      if (configured.type == "simulator" && channel == frame.metadata.channel)
      {
        camera = configured;
        break;
      }
    }
  }

  if (camera.id.isEmpty())
  {
    for (const CameraConfig& configured : m_config.activeCameras())
    {
      if (configured.id == m_selectedCameraId)
      {
        camera = configured;
        break;
      }
    }
  }

  if (camera.id.isEmpty() || frame.image.empty())
  {
    appendLog("Campione simulatore senza camera valida: " + frame.metadata.channel);
    return;
  }

  const QString sampleDirectory = m_recipeManager.cameraSampleImagesPath(camera.id);
  if (!QDir().mkpath(sampleDirectory))
  {
    appendLog("Impossibile creare cartella campione simulatore: " + sampleDirectory);
    return;
  }
  const QString samplePath = QDir(sampleDirectory).filePath("sample.png");
  if (!cv::imwrite(samplePath.toStdString(), frame.image))
  {
    appendLog("Impossibile salvare campione simulatore: " + samplePath);
    return;
  }

  if (m_selectedCameraId != camera.id)
  {
    selectCamera(camera);
  }
  m_cameraRuntime[camera.id].stop();
  m_cameraRuntime[camera.id].setCurrentFrame(frame.image);
  m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
  m_cameraRuntime[camera.id].clearGeometries();
  m_thread.clearThreadOverlays(camera.id);
  deactivateImageDrawingTools();
  m_selectedPreview = m_imaging.matToPixmap(frame.image);
  m_selectedImagePath = samplePath;
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->clearRoi();
  m_largeImage->clearExclusionRects();
  m_largeImage->clearCircles();
  m_largeImage->clearGeometryArea();
  m_largeImage->clearGeometryPoints();
  m_largeImage->clearGeometryLines();
  m_largeImage->clearGeometryOverlay();
  m_largeImage->clearDetectedCircle();
  updateLargePreview();
  appendLog(QString("Campione simulatore ricevuto: %1 channel=%2")
    .arg(camera.id, frame.metadata.channel));
  appendLog("Campione simulatore salvato: " + samplePath);
}

void MainWindow::processNextSimulatorFrame(const CameraConfig& camera)
{
  if (!m_machineRunning || camera.type != "simulator" ||
      m_cameraPendingJobs.value(camera.id, 0) > 0)
  {
    return;
  }

  appendLogLazy([cameraId = camera.id] {
    return QString("Pipeline simulatore attende frame: %1").arg(cameraId);
  });
  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  QString error;
  runtime.clearProductionTracking(camera.id);
  if (!runtime.running() && !runtime.start(camera, {}, &error))
  {
    appendLog(error);
    return;
  }
  if (!runtime.step(camera, {}, &error))
  {
    return;
  }
  appendLogLazy([cameraId = camera.id, frameId = runtime.currentSimulatorFrame().frameId] {
    return QString("Pipeline simulatore acquisito: %1 frame=%2").arg(cameraId).arg(frameId);
  });
  if (camera.id == m_selectedCameraId && !runtime.currentFrame().empty())
  {
    m_selectedPreview = m_imaging.matToPixmap(runtime.currentFrame());
    m_largeImage->setImage(m_selectedPreview);
    updateLargePreview();
  }
  SimulatorBridge::instance().publishFrameEvent(
    runtime.currentSimulatorFrame(),
    "processingStarted",
    "Elaborazione asincrona avviata");

  const SurfaceAnnulusLocalizationConfig localization =
    m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  const QString frameStrategy = runtime.currentSimulatorFrame().strategyId.trimmed();
  const QString recipeMethod = localization.method;
  const QString requestedStrategy =
    recipeMethod.isEmpty()
      ? (frameStrategy.isEmpty() ? QStringLiteral("massPca") : frameStrategy)
      : recipeMethod;
  if (!frameStrategy.isEmpty() && frameStrategy != requestedStrategy)
  {
    appendLog(QString("Pipeline simulatore strategia frame '%1' ignorata, uso ricetta '%2'")
      .arg(frameStrategy, requestedStrategy));
  }
  const QString requestedRecipe = runtime.currentSimulatorFrame().recipeId;
  if (!requestedRecipe.isEmpty() && requestedRecipe != m_recipeManager.recipeId())
  {
    const QString message = QString("Ricetta attiva '%1', richiesta '%2'")
      .arg(m_recipeManager.recipeId(), requestedRecipe);
    appendLog("Pipeline simulatore ricetta errata: " + message);
    SimulatorBridge::instance().publishFrameEvent(
      runtime.currentSimulatorFrame(), "processingError", message);
    runtime.clearCurrentPose(camera.id);
    if (camera.id == m_selectedCameraId)
    {
      m_largeImage->clearGeometryOverlay();
      updateLargePreview();
    }
    publishSimulatorResult(camera.id);
    return;
  }
  if (requestedStrategy == "aiYolo")
  {
    const cv::Mat input = runtime.currentFrame().clone();
    const qint64 startedAt = QDateTime::currentMSecsSinceEpoch();
    incPendingJobs(camera.id);
    appendLog(QString("Pipeline simulatore AI avviata: %1 frame=%2 recipe=%3")
      .arg(camera.id)
      .arg(runtime.currentSimulatorFrame().frameId)
      .arg(m_recipeManager.recipeId()));
    m_setup.runAiLocalizationInferenceFrame(
      camera,
      input,
      [this, camera, startedAt](const AiLocalizationFrameResult& result) {
        CameraRuntime& completedRuntime = m_cameraRuntime[camera.id];
        m_lastSetupScanElapsedMs[camera.id] =
          QDateTime::currentMSecsSinceEpoch() - startedAt;

        if (!result.error.isEmpty())
        {
          completedRuntime.clearCurrentPose(camera.id);
          m_lastSurfaceLocalizationResults.remove(camera.id);
          SimulatorBridge::instance().publishFrameEvent(
            completedRuntime.currentSimulatorFrame(),
            "processingError",
            result.error);
          appendLog(QString("Pipeline simulatore AI errore: %1 frame=%2 error=%3")
            .arg(camera.id)
            .arg(completedRuntime.currentSimulatorFrame().frameId)
            .arg(result.error));
        }
        else
        {
          SimulatorBridge::instance().publishFrameEvent(
            completedRuntime.currentSimulatorFrame(),
            "processingCompleted",
            result.found
              ? "Localizzazione AI completata"
              : "Localizzazione AI completata senza posa valida");
          if (result.found)
          {
            PartPose pose;
            pose.valid = true;
            pose.cameraId = camera.id;
            pose.method = "ai_segmentation";
            pose.origin = result.pose.center;
            pose.angleRadians = result.hasOrientationReference
              ? std::atan2(
                  result.orientationReferenceCenter.y - result.pose.center.y,
                  result.orientationReferenceCenter.x - result.pose.center.x)
              : result.pose.angleRadians;
            pose.score = result.confidence;
            pose.xAxis = {
              std::cos(pose.angleRadians),
              std::sin(pose.angleRadians)
            };
            pose.yAxis = {-pose.xAxis.y, pose.xAxis.x};
            completedRuntime.setCurrentPose(pose);

            SurfaceLocalizationReference reference;
            reference.found = true;
            reference.method = "ai_segmentation";
            reference.center = result.pose.center;
            reference.angleRadians = pose.angleRadians;
            reference.score = result.confidence;
            reference.inputPoints = static_cast<int>(result.pose.contour.size());
            reference.usedPoints = reference.inputPoints;
            reference.xAxisStart = result.pose.center;
            reference.xAxisEnd = result.pose.center + pose.xAxis * 60.0;
            reference.yAxisStart = result.pose.center;
            reference.yAxisEnd = result.pose.center + pose.yAxis * 60.0;
            m_lastSurfaceLocalizationResults.insert(camera.id, reference);
            appendLog(QString(
              "Pipeline simulatore AI coordinate: %1 frame=%2 X=%3 Y=%4 A=%5 "
              "confidence=%6 inferenceMs=%7")
              .arg(camera.id)
              .arg(completedRuntime.currentSimulatorFrame().frameId)
              .arg(result.pose.center.x, 0, 'f', 3)
              .arg(result.pose.center.y, 0, 'f', 3)
              .arg(pose.angleRadians * 180.0 / CV_PI, 0, 'f', 3)
              .arg(result.confidence, 0, 'f', 4)
              .arg(result.elapsedMs, 0, 'f', 1));
          }
          else
          {
            completedRuntime.clearCurrentPose(camera.id);
            m_lastSurfaceLocalizationResults.remove(camera.id);
            appendLog(QString("Pipeline simulatore AI: %1 frame=%2 found=false")
              .arg(camera.id)
              .arg(completedRuntime.currentSimulatorFrame().frameId));
          }
        }

        if (camera.id == m_selectedCameraId)
        {
          if (!result.diagnosticImage.empty())
          {
            m_selectedPreview = m_imaging.matToPixmap(result.diagnosticImage);
            m_largeImage->setImage(m_selectedPreview);
          }
          updateLargePreview();
        }
        m_setup.refreshSetupGeometryResults(camera, [this, camera, frameId = completedRuntime.currentSimulatorFrame().frameId]() {
          appendLog(QString("Pipeline simulatore invio risultato: %1 frame=%2")
            .arg(camera.id)
            .arg(frameId));
          publishSimulatorResult(camera.id);
          decPendingJobs(camera.id);
          updateMeasurementResults();
          QTimer::singleShot(0, this, [this, camera]() {
            processNextSimulatorFrame(camera);
          });
        });
      });
    return;
  }
  QRect roi;
  const QVector<QPoint> polygon = m_recipeManager.loadSurfaceDefectPolygon(camera.id);
  const bool hasArea =
    m_recipeManager.loadSurfaceDefectRoi(camera.id, roi) || polygon.size() >= 3;
  const SurfaceModelConfig model = m_recipeManager.loadSurfaceModel(camera.id);
  const SurfaceLocalizationStrategyConfig surfaceStrategy =
    m_recipeManager.loadSurfaceLocalizationStrategy(camera.id);
  QString setupError;
  if ((requestedStrategy == "massPca" || requestedStrategy == "edgePca") && !hasArea)
  {
    setupError = "ROI o poligono di ricerca mancante";
  }
  else if (requestedStrategy == "threshold" &&
           (!localization.hasOuterCircle || !localization.hasInnerCircle))
  {
    setupError = "Cerchi interno/esterno della corona non configurati";
  }
  else if (requestedStrategy == "edge" &&
           (!localization.hasEdgeCircle || localization.edgeRadius <= localization.edgeBandInner))
  {
    setupError = "Cerchio e fascia edge non configurati";
  }
  else if ((requestedStrategy == "shapeModel" || requestedStrategy == "templateModel") &&
           !model.hasModel)
  {
    setupError = "Modello non acquisito nella ricetta";
  }
  else if (requestedStrategy == "two_circles_axis" &&
           (surfaceStrategy.name != "two_circles_axis" ||
            surfaceStrategy.features.size() < 2))
  {
    setupError = "Strategia due cerchi non configurata";
  }
  else if (requestedStrategy != "massPca" && requestedStrategy != "edgePca" &&
           requestedStrategy != "threshold" && requestedStrategy != "edge" &&
           requestedStrategy != "shapeModel" && requestedStrategy != "templateModel" &&
           requestedStrategy != "two_circles_axis")
  {
    setupError = "Strategia non supportata: " + requestedStrategy;
  }

  if (!setupError.isEmpty())
  {
    runtime.clearCurrentPose(camera.id);
    appendLog(QString("Pipeline simulatore setup mancante: %1 strategy=%2 error=%3")
      .arg(camera.id, requestedStrategy, setupError));
    SimulatorBridge::instance().publishFrameEvent(
      runtime.currentSimulatorFrame(),
      "processingError",
      setupError);
    if (camera.id == m_selectedCameraId)
    {
      m_largeImage->clearGeometryOverlay();
      updateLargePreview();
    }
    publishSimulatorResult(camera.id);
    return;
  }

  if (!roi.isValid() && polygon.size() >= 3)
  {
    roi = QPolygon(polygon).boundingRect().normalized();
  }

  const cv::Mat input = runtime.currentFrame().clone();
  const QVector<QRect> exclusions = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);
  if (requestedStrategy == "massPca")
  {
    m_surface.ensureMassPcaReferenceFromSample(camera);
  }
  const SurfaceDefectSettings thresholdSettings =
    m_recipeManager.loadSurfaceDefectSettings(camera.id);
  const SurfaceTwoCirclesStrategyConfig twoCirclesConfig =
    SurfaceLocalizationAdapters::toProcessorStrategy(surfaceStrategy);
  cv::Mat templateImage;
  if (requestedStrategy == "templateModel")
  {
    templateImage = cv::imread(model.templateImagePath.toStdString(), cv::IMREAD_COLOR);
    if (templateImage.empty())
    {
      const QString message = "Immagine template non disponibile: " + model.templateImagePath;
      SimulatorBridge::instance().publishFrameEvent(
        runtime.currentSimulatorFrame(), "processingError", message);
      publishSimulatorResult(camera.id);
      return;
    }
  }
  // I frame del simulatore ruotano/traslano nel canvas: il ROI ricetta e' fisso in immagine.
  const cv::Rect searchRect(0, 0, input.cols, input.rows);

  const qint64 startedAt = QDateTime::currentMSecsSinceEpoch();
  const bool createDiagnosticImage = (camera.id == m_selectedCameraId);
  const bool drawContours = !m_machineRunning;
  auto job = [
      this, input, searchRect, exclusions, localization, thresholdSettings,
      cameraId = camera.id,
      requestedStrategy, model, templateImage, twoCirclesConfig, createDiagnosticImage, drawContours]() {
    try
    {
      SurfaceDefectProcessor processor;
      if (requestedStrategy == "massPca")
      {
        const SurfaceThresholdSettings settings =
          SurfaceLocalizationAdapters::thresholdSettingsFromRecipe(thresholdSettings);
        return processor.detectByGrayscaleThreshold(
          input,
          searchRect,
          SurfaceLocalizationAdapters::toCvRects(exclusions),
          settings,
          createDiagnosticImage,
          drawContours);
      }

      if (requestedStrategy == "edgePca")
      {
        const bool resolveAmbiguity =
          localization.pcaResolveAmbiguity || thresholdSettings.pcaResolveAmbiguity;
        return processor.locateByEdgePca(
          input,
          searchRect,
          SurfaceLocalizationAdapters::toCvRects(exclusions),
          localization.edgeSensitivity,
          resolveAmbiguity,
          createDiagnosticImage,
          drawContours);
      }

      if (requestedStrategy == "threshold" || requestedStrategy == "edge")
      {
        SurfaceAnnulusThresholdConfig config;
        if (requestedStrategy == "threshold")
        {
          config.center = cv::Point(localization.center.x(), localization.center.y());
          config.outerRadius = localization.outerRadius;
          config.innerRadius = localization.innerRadius;
          config.threshold.minValue = localization.thresholdMin;
          config.threshold.maxValue = localization.thresholdMax;
          return processor.locateAnnulusByGrayscaleThreshold(
            input, config, SurfaceLocalizationAdapters::toCvRects(exclusions), createDiagnosticImage, drawContours);
        }
        config.center = cv::Point(localization.edgeCenter.x(), localization.edgeCenter.y());
        config.outerRadius = localization.edgeRadius + localization.edgeBandOuter;
        config.innerRadius = std::max(0, localization.edgeRadius - localization.edgeBandInner);
        config.edgeSensitivity = localization.edgeSensitivity;
        config.edgeFitMaxError = localization.edgeFitMaxError;
        return processor.locateAnnulusByEdge(
          input, config, SurfaceLocalizationAdapters::toCvRects(exclusions), createDiagnosticImage, drawContours);
      }

      if (requestedStrategy == "two_circles_axis")
      {
        const SurfaceStrategyResult strategyResult =
          processor.locateTwoCirclesAxis(
            input,
            twoCirclesConfig,
            SurfaceLocalizationAdapters::toCvRects(exclusions),
            createDiagnosticImage,
            drawContours);
        SurfaceDefectResult converted;
        converted.processed = strategyResult.processed;
        converted.diagnosticImage = strategyResult.diagnosticImage;
        converted.localization.found = strategyResult.found;
        converted.localization.method = strategyResult.strategyName;
        converted.localization.center = strategyResult.origin;
        converted.localization.angleRadians = std::atan2(
          strategyResult.xAxisEnd.y - strategyResult.xAxisStart.y,
          strategyResult.xAxisEnd.x - strategyResult.xAxisStart.x);
        converted.localization.score = strategyResult.found ? 1.0 : 0.0;
        converted.localization.inputPoints =
          static_cast<int>(strategyResult.features.size());
        converted.localization.usedPoints =
          static_cast<int>(strategyResult.features.size());
        converted.localization.xAxisStart = strategyResult.xAxisStart;
        converted.localization.xAxisEnd = strategyResult.xAxisEnd;
        converted.localization.yAxisStart = strategyResult.yAxisStart;
        converted.localization.yAxisEnd = strategyResult.yAxisEnd;
        return converted;
      }

      if (requestedStrategy == "shapeModel" || (requestedStrategy == "model" && model.modelType != "template"))
      {
        SurfaceShapeMatchConfig config;
        config.searchRoi = cv::Rect(0, 0, input.cols, input.rows);
        config.edgeSensitivity = model.edgeSensitivity;
        config.maxShapeDistance = model.maxShapeDistance;
        for (const QPoint& point : model.contour)
        {
          config.modelContour.emplace_back(point.x(), point.y());
        }
        return processor.locateByShapeMatching(
          input, config, SurfaceLocalizationAdapters::toCvRects(exclusions), createDiagnosticImage, drawContours);
      }

      SurfaceTemplateMatchConfig config;
      config.searchRoi = cv::Rect(0, 0, input.cols, input.rows);
      config.modelImage = templateImage;
      config.edgeSensitivity = model.edgeSensitivity;
      config.minScore = model.minTemplateScore;
      config.angleStartDegrees = model.angleStartDegrees;
      config.angleEndDegrees = model.angleEndDegrees;
      config.angleStepDegrees = model.angleStepDegrees;
      config.useEdges = model.modelUseEdges;
      if (model.hasReferenceAngle)
      {
        config.hasReferenceAngle = true;
        config.referenceAngleDegrees = model.referenceAngleDegrees;
      }
      else
      {
        RecipeRotatedRoi roi;
        if (m_recipeManager.loadSurfaceDefectRotatedRoi(cameraId, roi) && roi.valid)
        {
          config.hasReferenceAngle = true;
          config.referenceAngleDegrees = roi.angleDegrees;
        }
      }
      return processor.locateByTemplateMatching(
        input, config, SurfaceLocalizationAdapters::toCvRects(exclusions), createDiagnosticImage, drawContours);
    }
    catch (const cv::Exception&)
    {
      return SurfaceDefectResult{};
    }
  };

  incPendingJobs(camera.id);
  appendLogLazy([cameraId = camera.id, requestedStrategy, recipeId = m_recipeManager.recipeId()] {
    return QString("Pipeline simulatore job avviato: %1 method=%2 recipe=%3")
      .arg(cameraId, requestedStrategy, recipeId);
  });
  CameraAsyncExecutor::runAsyncTask(
    camera.id,
    std::move(job),
    this,
    [this, camera, startedAt](const SurfaceDefectResult& result) {
      appendLogLazy([cameraId = camera.id, found = result.localization.found] {
        return QString("Pipeline simulatore job completato: %1 found=%2")
          .arg(cameraId)
          .arg(found ? "true" : "false");
      });
      CameraRuntime& completedRuntime = m_cameraRuntime[camera.id];
      SimulatorBridge::instance().publishFrameEvent(
        completedRuntime.currentSimulatorFrame(),
        "processingCompleted",
        result.localization.found
          ? "Localizzazione completata"
          : "Elaborazione completata senza posa valida");
      m_lastSetupScanElapsedMs[camera.id] =
        QDateTime::currentMSecsSinceEpoch() - startedAt;
      if (result.localization.found)
      {
        m_lastSurfaceLocalizationResults.insert(camera.id, result.localization);
        completedRuntime.setCurrentPose(
          m_imaging.partPoseFromSurfaceReference(camera, result.localization));
        m_thread.syncExtractionRoiOverlay(camera);
        appendLog(
          QString("Pipeline simulatore coordinate: %1 frame=%2 X=%3 Y=%4 A=%5 method=%6 score=%7")
            .arg(camera.id)
            .arg(completedRuntime.currentSimulatorFrame().frameId)
            .arg(result.localization.center.x, 0, 'f', 3)
            .arg(result.localization.center.y, 0, 'f', 3)
            .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 3)
            .arg(QString::fromStdString(result.localization.method))
            .arg(result.localization.score, 0, 'f', 4));
      }
      else
      {
        m_lastSurfaceLocalizationResults.remove(camera.id);
        completedRuntime.clearCurrentPose(camera.id);
        appendLog(QString(
          "Pipeline simulatore coordinate: %1 frame=%2 found=false method=%3 "
          "candidates=%4 accepted=%5 bestDistance=%6")
            .arg(camera.id)
            .arg(completedRuntime.currentSimulatorFrame().frameId)
            .arg(QString::fromStdString(result.localization.method))
            .arg(result.localization.inputPoints)
            .arg(result.localization.usedPoints)
            .arg(result.localization.meanError, 0, 'f', 4));
      }

      if (camera.id == m_selectedCameraId)
      {
        const cv::Mat& displayImage = m_machineRunning
          ? completedRuntime.currentFrame()
          : (result.diagnosticImage.empty() ? completedRuntime.currentFrame() : result.diagnosticImage);
        if (!displayImage.empty())
        {
          m_selectedPreview = m_imaging.matToPixmap(displayImage);
          m_largeImage->setImage(m_selectedPreview);
        }
        if (result.localization.found)
        {
          m_geometry.showRuntimeGeometryOverlay(camera);
        }
        else
        {
          m_largeImage->clearGeometryOverlay();
        }

        QRect displayRoi;
        if (completedRuntime.currentSimulatorFrame().strategyId == "massPca" ||
            completedRuntime.currentSimulatorFrame().strategyId == "edgePca" ||
            completedRuntime.currentSimulatorFrame().strategyId == "shapeModel" ||
            completedRuntime.currentSimulatorFrame().strategyId == "templateModel")
        {
          m_largeImage->clearRoi();
          m_largeImage->setSearchPolygon({});
        }
        else
        {
          if (m_recipeManager.loadSurfaceDefectRoi(camera.id, displayRoi))
          {
            m_largeImage->setRoi(displayRoi);
          }
          else
          {
            m_largeImage->clearRoi();
          }
          m_largeImage->setSearchPolygon(
            m_recipeManager.loadSurfaceDefectPolygon(camera.id));
        }
        m_thread.syncExtractionRoiOverlay(camera);
        m_largeImage->setExclusionRects(
          m_recipeManager.loadSurfaceDefectExclusionRects(camera.id));
        updateLargePreview();
      }

      m_setup.refreshSetupGeometryResults(camera, [this, camera, frameId = completedRuntime.currentSimulatorFrame().frameId]() {
        appendLog(QString("Pipeline simulatore invio risultato: %1 frame=%2")
          .arg(camera.id)
          .arg(frameId));
        publishSimulatorResult(camera.id);
        decPendingJobs(camera.id);
        updateMeasurementResults();
        QTimer::singleShot(0, this, [this, camera]() {
          processNextSimulatorFrame(camera);
        });
      });

      appendLog(QString("Pipeline simulatore geometry avviata: %1 frame=%2")
        .arg(camera.id)
        .arg(completedRuntime.currentSimulatorFrame().frameId));
    },
    QString("simulator.%1.%2").arg(camera.id, requestedStrategy));
}

void MainWindow::rebuildUi()
{
  QWidget* oldCentralWidget = centralWidget();
  setCentralWidget(nullptr);

  if (oldCentralWidget)
  {
    oldCentralWidget->deleteLater();
  }

  m_tiles.clear();
  m_selectedCameraId.clear();
  m_selectedCamera = {};
  m_selectedPreview = {};
  m_selectedImagePath.clear();
  buildUi();
  loadConfiguration();
}

void MainWindow::changeLanguage(const QString& languageCode)
{
  QString error;
  if (!m_translations.loadLanguage(languageCode, &error))
  {
    appendLog(error);
    return;
  }

  rebuildUi();
  appendLog(trText("log.languageChanged") + ": " + languageCode);
}

void MainWindow::toggleFullScreen()
{
  if (isFullScreen())
  {
    showMaximized();
    appendLog(trText("log.windowMode") + ": " + trText("commands.maximized"));
  }
  else
  {
    showFullScreen();
    appendLog(trText("log.windowMode") + ": " + trText("commands.fullScreen"));
  }

  updateLargePreview();
}

void MainWindow::showGridView()
{
  deactivateImageDrawingTools();
  m_imageStack->setCurrentIndex(0);
  m_selectedCameraId.clear();
  m_selectedCamera = {};
  m_selectedPreview = {};
  m_selectedImagePath.clear();
  m_largeTitle->setText(trText("labels.productionOverview"));
  m_largeImage->clearImage();
  m_largeImage->clearRoi();
  m_largeImage->clearExclusionRects();
  m_largeImage->clearCircles();
  m_largeImage->clearGeometryOverlay();
  m_largeImage->hide();

  if (m_measurementResults)
  {
    m_measurementResults->show();
    m_measurementResults->setExpanded(true);
  }

  for (CameraTileWidget* tile : m_tiles)
  {
    tile->setSelected(false);
  }
  if (m_cameraStrip)
  {
    m_cameraStrip->setSelectedCamera({});
  }
  updateMeasurementResults();

  updateControlPanel(nullptr);
  appendLog(trText("log.gridView"));
}

void MainWindow::resetProductionCameraState(const CameraConfig& camera)
{
  auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt != m_cameraRuntime.end())
  {
    runtimeIt->second.clearProductionTracking(camera.id);
  }

  m_lastSurfaceLocalizationResults.remove(camera.id);
  m_lastSetupScanElapsedMs.remove(camera.id);

  m_geometry.resetRuntimeGeometryForProduction(camera);
}

void MainWindow::startMachine()
{
  m_machineRunning = true;
  resetMeasurementStatistics();
  QStringList cameraIds;
  for (const CameraConfig& camera : m_config.activeCameras())
  {
    cameraIds.append(camera.id);
  }
  CameraAsyncExecutor::ensurePools(cameraIds);
  m_lastSurfaceLocalizationResults.clear();
  for (const CameraConfig& camera : m_config.activeCameras())
  {
    resetProductionCameraState(camera);
  }
  const bool keepCurrentTool =
    !m_selectedCameraId.isEmpty() &&
    m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::None;
  if (!keepCurrentTool)
  {
    deactivateImageDrawingTools();
  }
  for (const CameraConfig& camera : m_config.activeCameras())
  {
    if (camera.type == "simulator")
    {
      CameraRuntime& runtime = m_cameraRuntime[camera.id];
      QString error;
      if (!runtime.start(camera, {}, &error))
      {
        appendLog(error);
        continue;
      }
      appendLog(QString("START arma simulatore: %1 channel=%2")
        .arg(camera.id, camera.simulatorChannel.isEmpty() ? camera.id : camera.simulatorChannel));
      processNextSimulatorFrame(camera);
    }
    else
    {
      m_setup.startCameraSimulation(camera, false, CameraRuntime::RunMode::ProductionExternal);
    }
  }
  if (m_systemStatus)
  {
    m_systemStatus->setText(QString("START | %1 %2")
                              .arg(m_config.activeCameras().size())
                              .arg(trText("status.activeCameras")));
  }
  if (m_commandToolbar)
  {
    m_commandToolbar->setStatusText(m_systemStatus ? m_systemStatus->text() : "START");
  }
  if (!keepCurrentTool)
  {
    updateControlPanel(m_selectedCameraId.isEmpty() ? nullptr : &m_selectedCamera);
  }
  m_productionThroughput.clear();
  if (!m_throughputRefreshTimer)
  {
    m_throughputRefreshTimer = new QTimer(this);
    connect(m_throughputRefreshTimer, &QTimer::timeout, this, [this]() {
      if (!m_machineRunning)
      {
        return;
      }
      if (m_selectedCameraId.isEmpty())
      {
        refreshProductionOverviewPanel();
      }
      else
      {
        refreshCameraMonitoringPanel(m_selectedCamera);
      }
    });
  }
  m_throughputRefreshTimer->start(1000);
  appendLog(trText("log.command") + ": " + trText("commands.start"));
}

void MainWindow::stopMachine()
{
  const bool keepCurrentTool =
    !m_selectedCameraId.isEmpty() &&
    m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::None;

  m_machineRunning = false;

  CameraAsyncExecutor::waitForAll();

  const int discardedFrames = SimulatorBridge::instance().discardAllQueuedFrames(
    QStringLiteral("Vision in STOP: frame scartato"));
  if (discardedFrames > 0)
  {
    appendLog(QString("Simulatore: scartati %1 frame in coda").arg(discardedFrames));
  }

  for (const CameraConfig& camera : m_config.activeCameras())
  {
    auto runtimeIt = m_cameraRuntime.find(camera.id);
    if (runtimeIt != m_cameraRuntime.end())
    {
      runtimeIt->second.stop();
    }
    resetProductionCameraState(camera);
  }

  m_cameraPendingJobs.clear();
  m_cameraProcessingBusy.clear();
  m_lastPublishedMeasurements.clear();
  m_productionThroughput.clear();
  m_measurementResultsUpdatePending = false;

  if (m_largeImage)
  {
    m_largeImage->clearGeometryOverlay();
  }
  if (m_cameraStrip)
  {
    for (const CameraConfig& camera : m_config.activeCameras())
    {
      m_cameraStrip->setCameraBusy(camera.id, false);
      m_cameraStrip->setCameraResult(camera.id, "READY");
    }
  }
  if (m_throughputRefreshTimer)
  {
    m_throughputRefreshTimer->stop();
  }
  if (m_systemStatus)
  {
    m_systemStatus->setText(QString("%1 | %2 %3")
                              .arg(trText("status.systemReady"))
                              .arg(m_config.activeCameras().size())
                              .arg(trText("status.activeCameras")));
  }
  if (m_commandToolbar)
  {
    m_commandToolbar->setStatusText(m_systemStatus ? m_systemStatus->text() : trText("status.systemReady"));
  }
  if (!keepCurrentTool)
  {
    updateControlPanel(m_selectedCameraId.isEmpty() ? nullptr : &m_selectedCamera);
  }
  appendLog(trText("log.command") + ": " + trText("commands.stop"));
}

void MainWindow::selectCamera(const CameraConfig& camera)
{
  m_selectedCameraId = camera.id;
  m_selectedCamera = camera;
  m_selectedImagePath = m_imaging.cameraSampleImagePath(camera);
  deactivateImageDrawingTools();
  m_largeImage->show();
  m_largeImage->clearRoi();
  m_largeImage->clearExclusionRects();
  m_largeImage->clearCircles();
  m_largeImage->clearGeometryOverlay();

  for (CameraTileWidget* tile : m_tiles)
  {
    tile->setSelected(tile->camera().id == camera.id);
  }
  if (m_cameraStrip)
  {
    m_cameraStrip->setSelectedCamera(camera.id);
  }

  m_selectedPreview = m_imaging.loadCameraSamplePreview(camera);
  m_largeTitle->setText(camera.displayName + " | " + camera.id);

  if (m_selectedPreview.isNull())
  {
    m_largeImage->clearImage();
  }
  else
  {
    m_largeImage->setImage(m_selectedPreview);
    updateLargePreview();
  }

  QRect savedRoi;
  if (MainWindowCameraProfile::isGrayscaleLocalization(camera, m_config))
  {
    if (m_recipeManager.loadSurfaceDefectRoi(camera.id, savedRoi))
    {
      m_largeImage->setRoi(savedRoi);
    }

    m_largeImage->setExclusionRects(m_recipeManager.loadSurfaceDefectExclusionRects(camera.id));
    m_largeImage->clearCircles();
    GeometryOverlay overlay;
    if (m_recipeManager.hasSurfaceLocalizationSetup(camera.id))
    {
      m_geometry.appendCurrentPartPoseOverlay(camera, overlay);
    }
    m_largeImage->setGeometryOverlay(overlay);
    m_thread.syncExtractionRoiOverlay(camera);
  }
  else if (m_recipeManager.loadLocalizationRoi(camera.id, savedRoi))
  {
    m_largeImage->setRoi(savedRoi);
    m_largeImage->setExclusionRects(m_recipeManager.loadLocalizationExclusionRects(camera.id));
  }

  m_imageStack->setCurrentIndex(1);
  QApplication::processEvents();
  updateLargePreview();
  updateControlPanel(&camera);
  updateMeasurementResults();
  appendLog(trText("log.cameraSelected") + ": " + camera.id);
}

void MainWindow::appendLog(const QString& message, bool alwaysShow)
{
  if (!alwaysShow && !m_detailedLogger.enabled())
  {
    return;
  }

  m_detailedLogger.logLine(message);

  if (!m_log)
  {
    return;
  }

  const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
  const QString line = QString("[%1] %2").arg(timestamp, message);
  m_log->append(line);
}

bool MainWindow::isDetailedLogEnabled() const
{
  return m_detailedLogger.enabled();
}

void MainWindow::syncAsyncMetricsLogging()
{
  if (!m_detailedLogger.enabled())
  {
    AsyncExecutor::setMetricsHandler(nullptr);
    return;
  }

  AsyncExecutor::setMetricsHandler([this](const QString& name, qint64 ms) {
    appendLog(name.isEmpty()
      ? QString("metric: %1 ms").arg(ms)
      : QString("metric %1: %2 ms").arg(name).arg(ms));
  });
}

void MainWindow::updateLargePreview()
{
  if (!m_largeImage || m_selectedPreview.isNull())
  {
    return;
  }

  const QSize targetSize = m_largeImage->contentsRect().size();

  if (targetSize.isEmpty())
  {
    return;
  }

  m_largeImage->setImage(m_selectedPreview);
}

QString MainWindow::formatPiecesPerMinute(double piecesPerMinute) const
{
  if (piecesPerMinute <= 0.0)
  {
    return "-";
  }

  return QString("%1 %2").arg(piecesPerMinute, 0, 'f', 1).arg(trText("labels.piecesPerMinuteUnit"));
}

QString MainWindow::throughputInstantText(const QString& cameraId) const
{
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  return formatPiecesPerMinute(m_productionThroughput.value(cameraId).instantaneousPiecesPerMinute(nowMs));
}

QString MainWindow::throughputAverageText(const QString& cameraId) const
{
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  return formatPiecesPerMinute(
    m_productionThroughput.value(cameraId).averagePiecesPerMinute(nowMs));
}

QString MainWindow::throughputOverviewInstantText() const
{
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  double sum = 0.0;
  int count = 0;
  for (const CameraConfig& camera : m_config.activeCameras())
  {
    const double instant = m_productionThroughput.value(camera.id).instantaneousPiecesPerMinute(nowMs);
    if (instant > 0.0)
    {
      sum += instant;
      ++count;
    }
  }

  return formatPiecesPerMinute(count > 0 ? sum / static_cast<double>(count) : 0.0);
}

QString MainWindow::throughputOverviewAverageText() const
{
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  double sum = 0.0;
  int count = 0;
  for (const CameraConfig& camera : m_config.activeCameras())
  {
    const ProductionThroughputTracker& tracker = m_productionThroughput.value(camera.id);
    if (!tracker.hasData())
    {
      continue;
    }

    sum += tracker.averagePiecesPerMinute(nowMs);
    ++count;
  }

  return formatPiecesPerMinute(count > 0 ? sum / static_cast<double>(count) : 0.0);
}

void MainWindow::notifyProductionPieceCompleted(const QString& cameraId)
{
  if (!m_machineRunning)
  {
    return;
  }

  m_productionThroughput[cameraId].recordPiece(QDateTime::currentMSecsSinceEpoch());
  updateMeasurementResults();
  updateControlPanel(m_selectedCameraId.isEmpty() ? nullptr : &m_selectedCamera);
}

QString MainWindow::trText(const QString& key) const
{
  return m_translations.text(key);
}

