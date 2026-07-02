#include "gui/modules/MainWindowSetupModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/setup/AiLocalizationPaths.h"
#include "gui/modules/setup/AiPythonRuntime.h"
#include "gui/modules/setup/SetupCameraResolver.h"
#include "processing/SurfaceProcessingUtils.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QEventLoop>
#include <QProcess>
#include <QUuid>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>

namespace
{
bool maskCentroid(const cv::Mat& mask, cv::Point2d* center)
{
  if (mask.empty())
  {
    return false;
  }
  const cv::Moments moments = cv::moments(mask, true);
  if (std::abs(moments.m00) <= 0.000001)
  {
    return false;
  }
  *center = {moments.m10 / moments.m00, moments.m01 / moments.m00};
  return true;
}
}

void MainWindowSetupModule::runAiLocalizationInference(const CameraConfig& camera)
{
  const CameraConfig effectiveCamera = currentConfiguredCamera(config(), camera);
  QString error;
  cv::Mat frame = context().imaging->currentInputImage(effectiveCamera, &error);
  if (frame.empty())
  {
    log(error.isEmpty() ? "Localizzazione AI: acquisizione fallita." : error);
    if (m_aiLocalizationInferenceResultLabel)
    {
      m_aiLocalizationInferenceResultLabel->setText("Inferenza: immagine non disponibile");
    }
    return;
  }
  if (m_aiLocalizationInferenceResultLabel)
  {
    m_aiLocalizationInferenceResultLabel->setText("Inferenza: in corso...");
  }
  runAiLocalizationInferenceFrame(
    effectiveCamera,
    frame,
    [this, effectiveCamera](const AiLocalizationFrameResult& result) {
      applyAiLocalizationInferenceResult(effectiveCamera, result);
    });
}

bool MainWindowSetupModule::runAiLocalizationInferenceSync(const CameraConfig& camera, const cv::Mat& frame)
{
  if (frame.empty())
  {
    return false;
  }

  const CameraConfig effectiveCamera = currentConfiguredCamera(config(), camera);
  bool success = false;
  QEventLoop loop;

  runAiLocalizationInferenceFrame(
    effectiveCamera,
    frame,
    [&success, &loop, this, effectiveCamera](const AiLocalizationFrameResult& result) {
      applyAiLocalizationInferenceResult(effectiveCamera, result);
      success = result.found && result.error.isEmpty();
      loop.quit();
    });

  loop.exec();
  return success;
}

void MainWindowSetupModule::runAiLocalizationInferenceFrame(
  const CameraConfig& camera,
  const cv::Mat& frame,
  std::function<void(const AiLocalizationFrameResult&)> callback)
{
  AiLocalizationFrameResult immediate;
  if (frame.empty())
  {
    immediate.error = "Immagine non disponibile";
    callback(immediate);
    return;
  }
  const QString modelPath = aiNewestLocalizationModelPath(recipes(), camera.id);
  if (modelPath.isEmpty() || !QFileInfo::exists(modelPath))
  {
    immediate.error = "Modello localizzazione AI non disponibile";
    callback(immediate);
    return;
  }
  if (!ensureAiLocalizationInferenceWorker(camera.id, modelPath))
  {
    immediate.error = "Worker localizzazione AI non avviato";
    callback(immediate);
    return;
  }

  std::vector<uchar> buffer;
  if (!cv::imencode(".png", frame, buffer))
  {
    immediate.error = "Codifica in memoria fallita";
    callback(immediate);
    return;
  }

  QByteArray b64 = QByteArray(reinterpret_cast<const char*>(buffer.data()), static_cast<int>(buffer.size())).toBase64();
  const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

  m_aiLocalizationInferenceCallbacks.insert(requestId, std::move(callback));
  m_aiLocalizationInferenceFrames.insert(requestId, frame.clone());
  m_aiLocalizationInferenceRequestIdToCameraId.insert(requestId, camera.id);

  QJsonObject request;
  request["request_id"] = requestId;
  request["image_data"] = QString::fromLatin1(b64);
  request["camera_id"] = camera.id;
  QProcess* process = m_aiLocalizationInferenceProcesses.value(camera.id, nullptr);
  if (process)
  {
    process->write(
      QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n");
  }
  log(QString("Localizzazione AI richiesta: camera=%1 request=%2")
    .arg(camera.id, requestId));
}

bool MainWindowSetupModule::ensureAiLocalizationInferenceWorker(const QString& cameraId, const QString& modelPath)
{
  QProcess* process = m_aiLocalizationInferenceProcesses.value(cameraId, nullptr);
  QString currentModelPath = m_aiLocalizationInferenceModelPaths.value(cameraId, QString());

  if (process &&
      process->state() != QProcess::NotRunning &&
      currentModelPath == modelPath)
  {
    return true;
  }
  stopAiLocalizationInferenceWorker(cameraId);
  const QString script = aiProjectPath(
    "tools/ai/localization_segmentation_inference_worker.py");
  if (!QFileInfo::exists(script))
  {
    return false;
  }

  process = new QProcess(window());
  configureHiddenProcess(process);
  process->setWorkingDirectory(RecipeJsonUtils::appRootPath());
  process->setProcessChannelMode(QProcess::SeparateChannels);
  QObject::connect(
    process,
    &QProcess::readyReadStandardOutput,
    window(),
    [this, cameraId]() { handleAiLocalizationInferenceOutput(cameraId); });
  QObject::connect(
    process,
    &QProcess::readyReadStandardError,
    window(),
    [this, cameraId]() {
      QProcess* p = m_aiLocalizationInferenceProcesses.value(cameraId, nullptr);
      if (!p) return;
      const QString text = QString::fromLocal8Bit(p->readAllStandardError()).trimmed();
      for (const QString& line : text.split('\n', Qt::SkipEmptyParts))
      {
        log(QString("Localizzazione AI (%1): %2").arg(cameraId, line.trimmed()));
      }
    });
  QObject::connect(
    process,
    qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
    window(),
    [this, cameraId](int code, QProcess::ExitStatus status) {
      log(QString("Worker localizzazione AI per %1 terminato: code=%2 status=%3")
        .arg(cameraId).arg(code)
        .arg(status == QProcess::NormalExit ? "normal" : "crash"));
      AiLocalizationFrameResult failed;
      failed.error = "Worker localizzazione AI terminato";

      QList<QString> requestsToFail;
      for (auto it = m_aiLocalizationInferenceCallbacks.cbegin(); it != m_aiLocalizationInferenceCallbacks.cend(); ++it)
      {
        if (m_aiLocalizationInferenceRequestIdToCameraId.value(it.key()) == cameraId)
        {
          requestsToFail.append(it.key());
        }
      }
      for (const QString& reqId : requestsToFail)
      {
        auto callback = m_aiLocalizationInferenceCallbacks.take(reqId);
        m_aiLocalizationInferenceFrames.remove(reqId);
        m_aiLocalizationInferenceRequestIdToCameraId.remove(reqId);
        if (callback)
        {
          callback(failed);
        }
      }
      m_aiLocalizationInferenceModelPaths.remove(cameraId);
    });

  m_aiLocalizationInferenceProcesses[cameraId] = process;
  m_aiLocalizationInferenceModelPaths[cameraId] = modelPath;
  process->start(
    aiPythonProgram(),
    aiPythonUnbufferedArguments({
      script,
      "--model", modelPath,
      "--device", "0"
    }));
  return process->waitForStarted(5000);
}

void MainWindowSetupModule::stopAiLocalizationInferenceWorker(const QString& cameraId)
{
  QProcess* process = m_aiLocalizationInferenceProcesses.take(cameraId);
  if (!process)
  {
    return;
  }
  if (process->state() != QProcess::NotRunning)
  {
    process->closeWriteChannel();
    process->terminate();
    if (!process->waitForFinished(1500))
    {
      process->kill();
    }
  }
  process->deleteLater();
  m_aiLocalizationInferenceModelPaths.remove(cameraId);
}

void MainWindowSetupModule::stopAiLocalizationInferenceWorker()
{
  const QList<QString> cameraIds = m_aiLocalizationInferenceProcesses.keys();
  for (const QString& cameraId : cameraIds)
  {
    stopAiLocalizationInferenceWorker(cameraId);
  }
}

void MainWindowSetupModule::handleAiLocalizationInferenceOutput(const QString& cameraId)
{
  QProcess* process = m_aiLocalizationInferenceProcesses.value(cameraId, nullptr);
  if (!process)
  {
    return;
  }
  while (process->canReadLine())
  {
    const QByteArray lineBytes = process->readLine().trimmed();
    if (lineBytes.isEmpty())
    {
      continue;
    }
    const QJsonDocument document = QJsonDocument::fromJson(lineBytes);
    if (!document.isObject())
    {
      log("Localizzazione AI: " + QString::fromUtf8(lineBytes));
      continue;
    }
    const QJsonObject object = document.object();
    const QString type = object.value("type").toString();
    if (type == "ready")
    {
      log(QString("Modello localizzazione AI per %1 pronto: load=%2 ms")
        .arg(cameraId).arg(object.value("load_ms").toDouble(), 0, 'f', 1));
      continue;
    }

    const QString requestId = object.value("request_id").toString();
    auto callbackIt = m_aiLocalizationInferenceCallbacks.find(requestId);
    if (callbackIt == m_aiLocalizationInferenceCallbacks.end())
    {
      log("Localizzazione AI: risposta senza richiesta " + requestId);
      continue;
    }
    const auto callback = callbackIt.value();
    m_aiLocalizationInferenceCallbacks.erase(callbackIt);
    m_aiLocalizationInferenceRequestIdToCameraId.remove(requestId);

    AiLocalizationFrameResult result;
    result.processed = type == "result";
    result.error = type == "error" ? object.value("message").toString() : QString();
    result.found = object.value("found").toBool();
    result.confidence = object.value("confidence").toDouble();
    result.referenceConfidence = object.value("reference_confidence").toDouble();
    result.elapsedMs = object.value("elapsed_ms").toDouble();

    cv::Mat originalFrame = m_aiLocalizationInferenceFrames.take(requestId);

    if (result.found)
    {
      cv::Mat mask;
      const QString maskData = object.value("mask_data").toString();
      if (!maskData.isEmpty())
      {
        QByteArray bytes = QByteArray::fromBase64(maskData.toUtf8());
        std::vector<uchar> buf(bytes.begin(), bytes.end());
        mask = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);
      }

      if (mask.empty())
      {
        result.found = false;
        result.error = "Maschera AI non valida o vuota";
      }
      else
      {
        result.pose = MaskPoseEstimator().estimate(mask);
        result.found = result.pose.found;
        if (!result.found)
        {
          result.error = "Localizzazione contorno fallita";
        }
        else
        {
          result.diagnosticImage = originalFrame.clone();
          if (!result.diagnosticImage.empty())
          {
            const bool drawContours = !context().machineRunning || !*context().machineRunning;
            if (drawContours)
            {
              drawStyledContour(result.diagnosticImage, result.pose.contour);
            }

            if (object.value("reference_found").toBool())
            {
              cv::Mat refMask;
              const QString refData = object.value("reference_mask_data").toString();
              if (!refData.isEmpty())
              {
                QByteArray bytes = QByteArray::fromBase64(refData.toUtf8());
                std::vector<uchar> buf(bytes.begin(), bytes.end());
                refMask = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);
              }
              result.hasOrientationReference = maskCentroid(refMask, &result.orientationReferenceCenter);
            }

            double finalAngle = result.pose.angleRadians;
            if (result.hasOrientationReference)
            {
              finalAngle = std::atan2(
                result.orientationReferenceCenter.y - result.pose.center.y,
                result.orientationReferenceCenter.x - result.pose.center.x);
            }

            const cv::Point2d xDir(std::cos(finalAngle), std::sin(finalAngle));
            const cv::Point2d yDir(-xDir.y, xDir.x);
            const double axisLength = 60.0;
            const cv::Point2d xStart = result.pose.center - xDir * axisLength;
            const cv::Point2d xEnd = result.pose.center + xDir * axisLength;
            const cv::Point2d yStart = result.pose.center - yDir * axisLength;
            const cv::Point2d yEnd = result.pose.center + yDir * axisLength;

            drawStyledAxes(result.diagnosticImage, result.pose.center, xStart, xEnd, yStart, yEnd);
            drawStyledCenterOfMass(result.diagnosticImage, result.pose.center);

            if (result.hasOrientationReference)
            {
              const cv::Point pieceCenter(
                qRound(result.pose.center.x), qRound(result.pose.center.y));
              const cv::Point referenceCenter(
                qRound(result.orientationReferenceCenter.x),
                qRound(result.orientationReferenceCenter.y));
              cv::drawMarker(
                result.diagnosticImage,
                referenceCenter,
                cv::Scalar(0, 165, 255),
                cv::MARKER_DIAMOND,
                20,
                2);
              cv::line(
                result.diagnosticImage,
                pieceCenter,
                referenceCenter,
                cv::Scalar(0, 165, 255),
                2);
            }
          }
        }
      }
    }
    callback(result);
  }
}

void MainWindowSetupModule::applyAiLocalizationInferenceResult(
  const CameraConfig& camera,
  const AiLocalizationFrameResult& result)
{
  if (!result.error.isEmpty() || !result.found)
  {
    cameraRuntime()[camera.id].clearCurrentPose(camera.id);
    const QString message = result.error.isEmpty()
      ? QString("Pezzo non trovato\nTempo: %1 ms").arg(result.elapsedMs, 0, 'f', 1)
      : "Inferenza: errore - " + result.error;
    if (m_aiLocalizationInferenceResultLabel)
    {
      m_aiLocalizationInferenceResultLabel->setText(message);
    }
    log("Localizzazione AI: " + message);
    refreshSetupGeometryResults(camera);
    return;
  }

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
  pose.xAxis = {std::cos(pose.angleRadians), std::sin(pose.angleRadians)};
  pose.yAxis = {-pose.xAxis.y, pose.xAxis.x};
  cameraRuntime()[camera.id].setCurrentPose(pose);

  if (camera.id == selectedCameraId() && !result.diagnosticImage.empty())
  {
    selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
    largeImage()->setImage(selectedPreview());
    GeometryOverlay overlay;
    if (context().geometry)
    {
      context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
    }
    largeImage()->setGeometryOverlay(overlay);
  }
  if (m_aiLocalizationInferenceResultLabel)
  {
    m_aiLocalizationInferenceResultLabel->setText(
      QString("Centro: %1, %2 px\nAngolo: %3°\nArea: %4 px²\n"
              "Confidenza: %5\nTempo: %6 ms")
        .arg(result.pose.center.x, 0, 'f', 1)
        .arg(result.pose.center.y, 0, 'f', 1)
        .arg(pose.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
        .arg(result.pose.area, 0, 'f', 0)
        .arg(result.confidence, 0, 'f', 4)
        .arg(result.elapsedMs, 0, 'f', 1));
  }
  log(QString("Localizzazione AI trovata: camera=%1 cx=%2 cy=%3 angle=%4 "
              "area=%5 confidence=%6 elapsedMs=%7 orientation=%8")
    .arg(camera.id)
    .arg(result.pose.center.x, 0, 'f', 2)
    .arg(result.pose.center.y, 0, 'f', 2)
    .arg(pose.angleRadians * 180.0 / CV_PI, 0, 'f', 2)
    .arg(result.pose.area, 0, 'f', 0)
    .arg(result.confidence, 0, 'f', 4)
    .arg(result.elapsedMs, 0, 'f', 1)
    .arg(result.hasOrientationReference ? "reference" : "pca"));
  refreshSetupGeometryResults(camera);
}
