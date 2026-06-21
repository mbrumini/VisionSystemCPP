#include "gui/modules/MainWindowSetupModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/setup/AiClassificationPaths.h"
#include "gui/modules/setup/AiPythonRuntime.h"
#include "gui/modules/setup/SetupCameraResolver.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>

#include <opencv2/imgcodecs.hpp>

void MainWindowSetupModule::runAiClassificationInference(const CameraConfig& camera)
{
  const CameraConfig effectiveCamera = currentConfiguredCamera(config(), camera);
  const QString modelPath = QDir::cleanPath(aiClassificationModelFilePath(recipes(), effectiveCamera.id));
  if (!QFileInfo::exists(modelPath))
  {
    log(QString("AI inference model missing: %1").arg(modelPath));
    return;
  }

  CameraRuntime& runtime = cameraRuntime()[effectiveCamera.id];
  QString error;
  const QString testFolder = context().imaging->cameraTestImagesFolder(effectiveCamera);
  const int framesToAcquire = effectiveCamera.type == "usb" ? 3 : 1;
  for (int i = 0; i < framesToAcquire; ++i)
  {
    if (!runtime.step(effectiveCamera, testFolder, &error))
    {
      log(error.isEmpty() ? QString("AI inference acquisition failed: %1").arg(effectiveCamera.id) : error);
      if (m_aiInferenceResultLabel)
      {
        m_aiInferenceResultLabel->setText("Inferenza: acquisizione fallita");
      }
      return;
    }
  }

  const cv::Mat frame = runtime.currentFrame().clone();
  if (frame.empty())
  {
    log(QString("AI inference image missing: %1").arg(effectiveCamera.id));
    if (m_aiInferenceResultLabel)
    {
      m_aiInferenceResultLabel->setText("Inferenza: immagine non disponibile");
    }
    return;
  }

  if (effectiveCamera.id == selectedCameraId())
  {
    selectedPreview() = context().imaging->matToPixmap(frame);
    largeImage()->setImage(selectedPreview());
  }

  if (!ensureAiInferenceWorker(modelPath))
  {
    if (m_aiInferenceResultLabel)
    {
      m_aiInferenceResultLabel->setText("Inferenza: worker non avviato");
    }
    return;
  }

  const QString folder = QDir(RecipeManager::recipesRootPath()).filePath(
    QString("%1/images/%2/ai/inference").arg(recipes().recipeId(), effectiveCamera.id));
  QDir().mkpath(folder);
  const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
  const QString imagePath = QDir(folder).filePath(QString("%1_inference_%2.png").arg(effectiveCamera.id, stamp));
  if (!cv::imwrite(imagePath.toStdString(), frame))
  {
    log(QString("AI inference image save failed: %1").arg(imagePath));
    if (m_aiInferenceResultLabel)
    {
      m_aiInferenceResultLabel->setText("Inferenza: salvataggio immagine fallito");
    }
    return;
  }

  if (m_aiInferenceResultLabel)
  {
    m_aiInferenceResultLabel->setText("Inferenza: in corso...");
  }
  sendAiInferenceRequest(imagePath);
}

bool MainWindowSetupModule::ensureAiInferenceWorker(const QString& modelPath)
{
  if (m_aiInferenceProcess &&
      m_aiInferenceProcess->state() != QProcess::NotRunning &&
      m_aiInferenceModelPath == modelPath)
  {
    return true;
  }

  stopAiInferenceWorker();

  const QString script = aiProjectPath("tools/ai/classification_inference_worker.py");
  if (!QFileInfo::exists(script))
  {
    log(QString("AI inference worker missing: %1").arg(script));
    return false;
  }

  m_aiInferenceProcess = new QProcess(window());
  m_aiInferenceProcess->setWorkingDirectory(RecipeJsonUtils::appRootPath());
  m_aiInferenceProcess->setProcessChannelMode(QProcess::SeparateChannels);
  QObject::connect(m_aiInferenceProcess, &QProcess::readyReadStandardOutput, window(), [this]() {
    handleAiInferenceOutput();
  });
  QObject::connect(m_aiInferenceProcess, &QProcess::readyReadStandardError, window(), [this]() {
    const QString text = QString::fromLocal8Bit(m_aiInferenceProcess->readAllStandardError()).trimmed();
    for (const QString& line : text.split('\n', Qt::SkipEmptyParts))
    {
      log("AI inference: " + line.trimmed());
    }
  });
  QObject::connect(m_aiInferenceProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), window(), [this](int code, QProcess::ExitStatus status) {
    log(QString("AI inference worker finished: code=%1 status=%2").arg(code).arg(status == QProcess::NormalExit ? "normal" : "crash"));
    m_aiInferenceModelPath.clear();
  });

  m_aiInferenceModelPath = modelPath;
  const QStringList arguments = aiPythonUnbufferedArguments({
    script,
    "--model", modelPath,
    "--device", "0"
  });
  log(QString("AI inference worker start: %1 %2").arg(aiPythonProgram(), arguments.join(' ')));
  m_aiInferenceProcess->start(aiPythonProgram(), arguments);
  return m_aiInferenceProcess->waitForStarted(3000);
}

void MainWindowSetupModule::stopAiInferenceWorker()
{
  if (!m_aiInferenceProcess)
  {
    return;
  }

  if (m_aiInferenceProcess->state() != QProcess::NotRunning)
  {
    m_aiInferenceProcess->closeWriteChannel();
    m_aiInferenceProcess->terminate();
    if (!m_aiInferenceProcess->waitForFinished(1500))
    {
      m_aiInferenceProcess->kill();
    }
  }

  m_aiInferenceProcess->deleteLater();
  m_aiInferenceProcess = nullptr;
  m_aiInferenceModelPath.clear();
}

void MainWindowSetupModule::handleAiInferenceOutput()
{
  if (!m_aiInferenceProcess)
  {
    return;
  }
  while (m_aiInferenceProcess->canReadLine())
  {
    const QByteArray lineBytes = m_aiInferenceProcess->readLine().trimmed();
    if (lineBytes.isEmpty())
    {
      continue;
    }
    const QJsonDocument document = QJsonDocument::fromJson(lineBytes);
    if (!document.isObject())
    {
      log("AI inference: " + QString::fromUtf8(lineBytes));
      continue;
    }

    const QJsonObject object = document.object();
    const QString type = object.value("type").toString();
    if (type == "ready")
    {
      log(QString("AI inference model ready: %1 loadMs=%2")
            .arg(object.value("model").toString())
            .arg(object.value("load_ms").toDouble(), 0, 'f', 1));
    }
    else if (type == "result")
    {
      const QString className = object.value("class").toString();
      const double confidence = object.value("confidence").toDouble();
      const double elapsedMs = object.value("elapsed_ms").toDouble();
      if (m_aiInferenceResultLabel)
      {
        m_aiInferenceResultLabel->setText(
          QString("Classe: %1\nConfidenza: %2\nTempo inferenza: %3 ms")
            .arg(className)
            .arg(confidence, 0, 'f', 4)
            .arg(elapsedMs, 0, 'f', 1));
      }
      log(QString("AI inference result: class=%1 confidence=%2 index=%3 elapsedMs=%4 image=%5")
            .arg(className)
            .arg(confidence, 0, 'f', 4)
            .arg(object.value("index").toInt())
            .arg(elapsedMs, 0, 'f', 1)
            .arg(object.value("image").toString()));
    }
    else if (type == "error")
    {
      if (m_aiInferenceResultLabel)
      {
        m_aiInferenceResultLabel->setText("Inferenza: errore - " + object.value("message").toString());
      }
      log("AI inference error: " + object.value("message").toString());
    }
  }
}

void MainWindowSetupModule::sendAiInferenceRequest(const QString& imagePath)
{
  if (!m_aiInferenceProcess || m_aiInferenceProcess->state() == QProcess::NotRunning)
  {
    log("AI inference worker not running");
    return;
  }

  QJsonObject request;
  request["image"] = imagePath;
  const QByteArray line = QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n";
  m_aiInferenceProcess->write(line);
  log(QString("AI inference request: %1").arg(imagePath));
}
