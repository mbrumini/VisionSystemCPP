#include "gui/modules/MainWindowSetupModule.h"

#include "gui/modules/MainWindowCameraConfigModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/setup/SetupCameraResolver.h"

#include <QTimer>

void MainWindowSetupModule::startAiClassificationCapture(
  const CameraConfig& camera,
  bool toClass,
  const AiClassificationClassConfig& classConfig)
{
  if (!m_aiClassificationCaptureTimer)
  {
    m_aiClassificationCaptureTimer = new QTimer(window());
    QObject::connect(m_aiClassificationCaptureTimer, &QTimer::timeout, window(), [this]() {
      captureAiClassificationFrame();
    });
  }

  const CameraConfig effectiveCamera = currentConfiguredCamera(config(), camera);

  m_aiClassificationCaptureCameraId = effectiveCamera.id;
  m_aiClassificationCaptureCamera = effectiveCamera;
  m_aiClassificationCaptureToClass = toClass;
  m_aiClassificationCaptureClass = classConfig;

  startCameraSimulation(effectiveCamera, false);
  const int intervalMs = qMax(50, cameraRuntime()[effectiveCamera.id].intervalMs());
  m_aiClassificationCaptureTimer->start(intervalMs);
  captureAiClassificationFrame();
  log(QString("%1: %2 target=%3 classId=%4 className=%5")
        .arg(tr("log.aiCaptureStarted"))
        .arg(effectiveCamera.id)
        .arg(toClass ? QStringLiteral("class") : QStringLiteral("raw"))
        .arg(toClass ? classConfig.id : 0)
        .arg(toClass ? classConfig.name : QStringLiteral("raw")));
}

void MainWindowSetupModule::stopAiClassificationCapture()
{
  const QString cameraId = m_aiClassificationCaptureCameraId;
  const bool wasActive = !cameraId.isEmpty();

  if (m_aiClassificationCaptureTimer)
  {
    m_aiClassificationCaptureTimer->stop();
  }

  m_aiClassificationCaptureCameraId.clear();
  m_aiClassificationCaptureCamera = {};
  m_aiClassificationCaptureToClass = false;
  m_aiClassificationCaptureClass = {};

  if (wasActive)
  {
    const auto runtimeIt = cameraRuntime().find(cameraId);
    if (runtimeIt != cameraRuntime().end())
    {
      runtimeIt->second.stop();
    }

    if (cameraId == selectedCameraId() && context().simulationTimer)
    {
      context().simulationTimer->stop();
    }
  }

  if (wasActive)
  {
    log(tr("log.aiCaptureStopped"));
  }
}

void MainWindowSetupModule::captureAiClassificationFrame()
{
  if (m_aiClassificationCaptureCameraId.isEmpty())
  {
    stopAiClassificationCapture();
    return;
  }

  if (m_aiClassificationCaptureToClass)
  {
    log(QString("AI capture tick: camera=%1 classId=%2 className=%3")
          .arg(m_aiClassificationCaptureCamera.id)
          .arg(m_aiClassificationCaptureClass.id)
          .arg(m_aiClassificationCaptureClass.name));
    context().cameraConfig->acquireCameraAiClassificationClassImage(m_aiClassificationCaptureCamera, m_aiClassificationCaptureClass);
    return;
  }

  log(QString("AI capture tick: camera=%1 raw").arg(m_aiClassificationCaptureCamera.id));
  context().cameraConfig->acquireCameraAiClassificationRawImage(m_aiClassificationCaptureCamera);
}

