#include "gui/modules/MainWindowSetupModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/setup/AiLocalizationPaths.h"
#include "gui/modules/setup/AiPythonRuntime.h"
#include "gui/modules/setup/SetupCameraResolver.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>
#include <QUuid>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>

namespace
{
cv::Mat diagnosticImage(const QString& imagePath, const MaskPoseResult& pose)
{
  cv::Mat diagnostic = cv::imread(imagePath.toStdString(), cv::IMREAD_COLOR);
  if (diagnostic.empty() || !pose.found)
  {
    return diagnostic;
  }
  cv::drawContours(
    diagnostic,
    std::vector<std::vector<cv::Point>>{pose.contour},
    0,
    cv::Scalar(0, 255, 0),
    2);
  const cv::Point center(qRound(pose.center.x), qRound(pose.center.y));
  cv::drawMarker(
    diagnostic, center, cv::Scalar(0, 255, 255),
    cv::MARKER_CROSS, 24, 2);
  const double axisLength = 60.0;
  cv::line(
    diagnostic,
    center,
    center + cv::Point(
      qRound(std::cos(pose.angleRadians) * axisLength),
      qRound(std::sin(pose.angleRadians) * axisLength)),
    cv::Scalar(255, 255, 0),
    2);
  return diagnostic;
}
}

void MainWindowSetupModule::runAiLocalizationInference(const CameraConfig& camera)
{
  const CameraConfig effectiveCamera = currentConfiguredCamera(config(), camera);
  CameraRuntime& runtime = cameraRuntime()[effectiveCamera.id];
  cv::Mat frame = runtime.currentFrame().clone();
  if (frame.empty())
  {
    QString error;
    if (!runtime.step(
          effectiveCamera,
          context().imaging->cameraTestImagesFolder(effectiveCamera),
          &error))
    {
      log(error.isEmpty() ? "Localizzazione AI: acquisizione fallita." : error);
      return;
    }
    frame = runtime.currentFrame().clone();
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
  if (!ensureAiLocalizationInferenceWorker(modelPath))
  {
    immediate.error = "Worker localizzazione AI non avviato";
    callback(immediate);
    return;
  }

  const QString folder = QDir(RecipeManager::recipesRootPath()).filePath(
    QString("%1/images/%2/ai/localization_segmentation/inference")
      .arg(recipes().recipeId(), camera.id));
  QDir().mkpath(folder);
  const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
  const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QString imagePath = QDir(folder).filePath(
    QString("%1_input_%2.png").arg(camera.id, stamp));
  const QString maskPath = QDir(folder).filePath(
    QString("%1_mask_%2.png").arg(camera.id, stamp));
  if (!cv::imwrite(imagePath.toStdString(), frame))
  {
    immediate.error = "Salvataggio frame inferenza fallito";
    callback(immediate);
    return;
  }

  m_aiLocalizationInferenceCallbacks.insert(requestId, std::move(callback));
  QJsonObject request;
  request["request_id"] = requestId;
  request["image"] = imagePath;
  request["mask"] = maskPath;
  request["camera_id"] = camera.id;
  m_aiLocalizationInferenceProcess->write(
    QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n");
  log(QString("Localizzazione AI richiesta: camera=%1 request=%2")
    .arg(camera.id, requestId));
}

bool MainWindowSetupModule::ensureAiLocalizationInferenceWorker(const QString& modelPath)
{
  if (m_aiLocalizationInferenceProcess &&
      m_aiLocalizationInferenceProcess->state() != QProcess::NotRunning &&
      m_aiLocalizationInferenceModelPath == modelPath)
  {
    return true;
  }
  stopAiLocalizationInferenceWorker();
  const QString script = aiProjectPath(
    "tools/ai/localization_segmentation_inference_worker.py");
  if (!QFileInfo::exists(script))
  {
    return false;
  }

  m_aiLocalizationInferenceProcess = new QProcess(window());
  m_aiLocalizationInferenceProcess->setWorkingDirectory(RecipeJsonUtils::appRootPath());
  m_aiLocalizationInferenceProcess->setProcessChannelMode(QProcess::SeparateChannels);
  QObject::connect(
    m_aiLocalizationInferenceProcess,
    &QProcess::readyReadStandardOutput,
    window(),
    [this]() { handleAiLocalizationInferenceOutput(); });
  QObject::connect(
    m_aiLocalizationInferenceProcess,
    &QProcess::readyReadStandardError,
    window(),
    [this]() {
      const QString text = QString::fromLocal8Bit(
        m_aiLocalizationInferenceProcess->readAllStandardError()).trimmed();
      for (const QString& line : text.split('\n', Qt::SkipEmptyParts))
      {
        log("Localizzazione AI: " + line.trimmed());
      }
    });
  QObject::connect(
    m_aiLocalizationInferenceProcess,
    qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
    window(),
    [this](int code, QProcess::ExitStatus status) {
      log(QString("Worker localizzazione AI terminato: code=%1 status=%2")
        .arg(code)
        .arg(status == QProcess::NormalExit ? "normal" : "crash"));
      AiLocalizationFrameResult failed;
      failed.error = "Worker localizzazione AI terminato";
      const auto callbacks = m_aiLocalizationInferenceCallbacks;
      m_aiLocalizationInferenceCallbacks.clear();
      for (const auto& callback : callbacks)
      {
        callback(failed);
      }
      m_aiLocalizationInferenceModelPath.clear();
    });

  m_aiLocalizationInferenceModelPath = modelPath;
  m_aiLocalizationInferenceProcess->start(
    aiPythonProgram(),
    aiPythonUnbufferedArguments({
      script,
      "--model", modelPath,
      "--device", "0"
    }));
  return m_aiLocalizationInferenceProcess->waitForStarted(5000);
}

void MainWindowSetupModule::stopAiLocalizationInferenceWorker()
{
  if (!m_aiLocalizationInferenceProcess)
  {
    return;
  }
  if (m_aiLocalizationInferenceProcess->state() != QProcess::NotRunning)
  {
    m_aiLocalizationInferenceProcess->closeWriteChannel();
    m_aiLocalizationInferenceProcess->terminate();
    if (!m_aiLocalizationInferenceProcess->waitForFinished(1500))
    {
      m_aiLocalizationInferenceProcess->kill();
    }
  }
  m_aiLocalizationInferenceProcess->deleteLater();
  m_aiLocalizationInferenceProcess = nullptr;
  m_aiLocalizationInferenceModelPath.clear();
}

void MainWindowSetupModule::handleAiLocalizationInferenceOutput()
{
  const QString text = QString::fromUtf8(
    m_aiLocalizationInferenceProcess->readAllStandardOutput()).trimmed();
  for (const QString& line : text.split('\n', Qt::SkipEmptyParts))
  {
    const QJsonDocument document = QJsonDocument::fromJson(line.trimmed().toUtf8());
    if (!document.isObject())
    {
      log("Localizzazione AI: " + line);
      continue;
    }
    const QJsonObject object = document.object();
    const QString type = object.value("type").toString();
    if (type == "ready")
    {
      log(QString("Modello localizzazione AI pronto: load=%1 ms")
        .arg(object.value("load_ms").toDouble(), 0, 'f', 1));
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

    AiLocalizationFrameResult result;
    result.processed = type == "result";
    result.error = type == "error" ? object.value("message").toString() : QString();
    result.found = object.value("found").toBool();
    result.imagePath = object.value("image").toString();
    result.maskPath = object.value("mask").toString();
    result.confidence = object.value("confidence").toDouble();
    result.elapsedMs = object.value("elapsed_ms").toDouble();
    if (result.found)
    {
      const cv::Mat mask = cv::imread(
        result.maskPath.toStdString(), cv::IMREAD_GRAYSCALE);
      result.pose = MaskPoseEstimator().estimate(mask);
      result.found = result.pose.found;
      if (!result.found)
      {
        result.error = "Maschera AI non valida";
      }
      result.diagnosticImage = diagnosticImage(result.imagePath, result.pose);
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
    return;
  }

  PartPose pose;
  pose.valid = true;
  pose.cameraId = camera.id;
  pose.method = "ai_segmentation";
  pose.origin = result.pose.center;
  pose.angleRadians = result.pose.angleRadians;
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
        .arg(result.pose.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
        .arg(result.pose.area, 0, 'f', 0)
        .arg(result.confidence, 0, 'f', 4)
        .arg(result.elapsedMs, 0, 'f', 1));
  }
  log(QString("Localizzazione AI trovata: camera=%1 cx=%2 cy=%3 angle=%4 "
              "area=%5 confidence=%6 elapsedMs=%7")
    .arg(camera.id)
    .arg(result.pose.center.x, 0, 'f', 2)
    .arg(result.pose.center.y, 0, 'f', 2)
    .arg(result.pose.angleRadians * 180.0 / CV_PI, 0, 'f', 2)
    .arg(result.pose.area, 0, 'f', 0)
    .arg(result.confidence, 0, 'f', 4)
    .arg(result.elapsedMs, 0, 'f', 1));
}
