#include "gui/modules/MainWindowSetupModule.h"

#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/setup/SetupResultsDialog.h"
#include "gui/modules/setup/SetupResultsFormatter.h"

#include <opencv2/core.hpp>

void MainWindowSetupModule::updateCameraSetupDetails(const CameraConfig& camera)
{
  if (!context().setupPanel || !*context().setupPanel || *context().setupCameraId != camera.id)
  {
    return;
  }

  (*context().setupPanel)->setDetailsText(cameraSetupDetailsText(camera));
  updateSetupResultsPopup(camera);
}


QString MainWindowSetupModule::cameraSetupDetailsText(const CameraConfig& camera) const
{
  const auto runtimeIt = cameraRuntime().find(camera.id);
  const CameraRuntime* runtime = runtimeIt == cameraRuntime().end() ? nullptr : &runtimeIt->second;
  const PartPose pose = runtime ? runtime->currentPose() : makeInvalidPartPose(camera.id);
  const QString samplePath = recipes().firstCameraSampleImagePath(camera.id);
  const QString testPath = recipes().firstCameraTestImagePath(camera.id);
  const qint64 scanElapsedMs = context().lastSetupScanElapsedMs->value(camera.id, -1);
  QString sourceLabel = tr("labels.folder");
  QString sourceDetails = context().imaging->resolvedCameraFolder(camera);
  if (camera.type == "vimba")
  {
    sourceLabel = "VimbaX";
    sourceDetails = QString("serial=%1 id=%2 interface=%3 trigger=%4/%5")
      .arg(camera.serial.isEmpty() ? tr("status.invalid") : camera.serial)
      .arg(camera.deviceId.isEmpty() ? tr("status.invalid") : camera.deviceId)
      .arg(camera.interfaceId.isEmpty() ? tr("status.invalid") : camera.interfaceId)
      .arg(camera.trigger.mode.isEmpty() ? tr("status.invalid") : camera.trigger.mode)
      .arg(camera.trigger.source.isEmpty() ? tr("status.invalid") : camera.trigger.source);
  }
  else if (camera.type == "usb")
  {
    sourceLabel = "USB";
    sourceDetails = QString("index=%1 id=%2 trigger=%3/%4")
      .arg(camera.usbIndex)
      .arg(camera.deviceId.isEmpty() ? tr("status.invalid") : camera.deviceId)
      .arg(camera.trigger.mode.isEmpty() ? tr("status.invalid") : camera.trigger.mode)
      .arg(camera.trigger.source.isEmpty() ? tr("status.invalid") : camera.trigger.source);
  }

  const QString calibrationDetails = camera.calibration.enabled
    ? QString("%1 %2 mm/px file=%3")
        .arg(camera.calibration.type)
        .arg((camera.calibration.pixelSizeXMm + camera.calibration.pixelSizeYMm) * 0.5, 0, 'f', 8)
        .arg(camera.calibration.file.isEmpty() ? tr("status.notAvailable") : camera.calibration.file)
    : tr("status.notAvailable");

  return QString("%1: %2\n%3: %4\n%5: %6\n%7: %8\n%9: %10\n%11: %12\n%13: %14\n%15: %16\n%17: %18")
    .arg(tr("labels.source"), camera.type)
    .arg(sourceLabel, sourceDetails)
    .arg(tr("labels.sampleImage"), samplePath.isEmpty() ? tr("status.invalid") : samplePath)
    .arg(tr("labels.testImages"), testPath.isEmpty() ? tr("status.invalid") : recipes().cameraTestImagesPath(camera.id))
    .arg(tr("labels.status"), runtime ? runtimeStatusText(context(), runtime->status()) : tr("status.stopped"))
    .arg(tr("labels.frame"), runtime ? QString::number(runtime->frameIndex()) : QStringLiteral("0"))
    .arg(
      tr("labels.partPose"),
      pose.valid
        ? QString("X=%1 Y=%2 A=%3 S=%4")
            .arg(pose.origin.x, 0, 'f', 1)
            .arg(pose.origin.y, 0, 'f', 1)
            .arg(pose.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
            .arg(pose.score, 0, 'f', 2)
        : tr("status.invalid"))
    .arg(tr("labels.scanTime"), scanElapsedMs >= 0 ? QString("%1 ms").arg(scanElapsedMs) : tr("status.invalid"))
    .arg("Calibrazione", calibrationDetails);
}

void MainWindowSetupModule::showSetupResultsPopup(const CameraConfig& camera)
{
  if (!m_resultsDialog)
  {
    m_resultsDialog = new SetupResultsDialog(window());
    m_resultsDialog->setWindowTitle(tr("actions.frameResults"));
  }

  updateSetupResultsPopup(camera);
  m_resultsDialog->show();
  m_resultsDialog->raise();
  m_resultsDialog->activateWindow();
}

void MainWindowSetupModule::updateSetupResultsPopup(const CameraConfig& camera)
{
  if (!m_resultsDialog)
  {
    return;
  }

  const auto runtimeIt = cameraRuntime().find(camera.id);
  const CameraRuntime* runtime = runtimeIt == cameraRuntime().end() ? nullptr : &runtimeIt->second;
  m_resultsDialog->setResultsText(setupResultsText(camera, runtime, recipes(), context()));
}

