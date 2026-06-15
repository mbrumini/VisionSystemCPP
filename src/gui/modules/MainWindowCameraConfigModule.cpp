#include "gui/modules/MainWindowCameraConfigModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/CameraAssignmentDialog.h"
#include "gui/UsbCameraAssignmentDialog.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowSetupModule.h"

#include <QDir>
#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>

#include <opencv2/imgcodecs.hpp>

namespace
{
QString projectPath(const QString& relativePath)
{
  return RecipeJsonUtils::appPath(relativePath);
}

CameraConfig cameraById(const AppConfig& config, const QString& cameraId)
{
  for (const CameraConfig& camera : config.cameras())
  {
    if (camera.id == cameraId)
    {
      return camera;
    }
  }

  return {};
}

CameraConfig cameraByIdOrSlot(const AppConfig& config, const QString& cameraId, int slot)
{
  const CameraConfig byId = cameraById(config, cameraId);
  if (!byId.id.isEmpty())
  {
    return byId;
  }

  for (const CameraConfig& camera : config.cameras())
  {
    if (camera.slot == slot)
    {
      return camera;
    }
  }

  return {};
}

cv::Mat currentCameraFrameForSave(MainWindowContext& context, const CameraConfig& camera, QString* errorMessage)
{
  const auto runtimeIt = context.cameraRuntime->find(camera.id);
  if (runtimeIt != context.cameraRuntime->end() && !runtimeIt->second.currentFrame().empty())
  {
    return runtimeIt->second.currentFrame().clone();
  }

  return context.imaging->currentInputImage(camera, errorMessage);
}
}

MainWindowCameraConfigModule::MainWindowCameraConfigModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

void MainWindowCameraConfigModule::configureCameraSource(const CameraConfig& camera)
{
  MainWindowImagingModule& imaging = *context().imaging;
  const QString currentFolder = camera.folder.isEmpty()
    ? projectPath("data/images")
    : imaging.resolvedCameraFolder(camera);
  const QString selectedFolder = QFileDialog::getExistingDirectory(
    window(),
    QString("%1 | %2").arg(tr("menu.cameras"), camera.id),
    currentFolder);

  if (selectedFolder.isEmpty())
  {
    return;
  }

  const QDir projectDirectory(RecipeJsonUtils::appRootPath());
  QString storedFolder = projectDirectory.relativeFilePath(selectedFolder);
  if (storedFolder.startsWith(".."))
  {
    storedFolder = QDir::toNativeSeparators(selectedFolder);
  }

  QString error;
  const QString configPath = projectPath("config/cameras.json");
  if (!config().saveCameraSource(configPath, camera.id, "file", storedFolder, &error))
  {
    QMessageBox::warning(window(), tr("menu.cameras"), error);
    return;
  }

  cameraRuntime().erase(camera.id);
  if (context().loadConfiguration)
  {
    context().loadConfiguration();
  }
  log(QString("%1: %2 -> %3").arg(tr("log.cameraSourceSaved"), camera.id, storedFolder));
}

void MainWindowCameraConfigModule::configureVimbaCamera(const CameraConfig& camera)
{
  configureVimbaCameraSlot(camera.slot, camera.id);
}

void MainWindowCameraConfigModule::configureVimbaCameraSlot(int currentSlot, const QString& currentCameraId)
{
  const int defaultSlot = currentSlot <= 0 ? 1 : currentSlot;
  log(QString("VimbaX assignment open: currentCamera=%1 currentSlot=%2 maxSlot=%3")
        .arg(currentCameraId.isEmpty() ? "<menu>" : currentCameraId)
        .arg(defaultSlot)
        .arg(config().maxCameras()));

  CameraAssignmentDialog dialog(
    defaultSlot,
    config().maxCameras(),
    [this](const QString& message) { log(message); },
    window());
  if (dialog.exec() != QDialog::Accepted)
  {
    log(QString("VimbaX assignment canceled: currentCamera=%1").arg(currentCameraId.isEmpty() ? "<menu>" : currentCameraId));
    return;
  }

  const CameraDeviceInfo device = dialog.selectedDevice();
  if (device.deviceId.isEmpty() && device.serial.isEmpty())
  {
    log("VimbaX assignment rejected: no camera selected");
    QMessageBox::warning(window(), tr("menu.cameras"), tr("log.vimbaNoCameraSelected"));
    return;
  }

  QString error;
  const QString configPath = projectPath("config/cameras.json");
  log(QString("VimbaX assignment save begin: path='%1' slot=%2 enabled=%3 name='%4' model='%5' serial='%6' id='%7' interface='%8'")
        .arg(configPath)
        .arg(dialog.selectedSlot())
        .arg(dialog.cameraEnabled() ? "true" : "false")
        .arg(dialog.displayName())
        .arg(device.modelName)
        .arg(device.serial)
        .arg(device.deviceId)
        .arg(device.interfaceId));

  if (!config().saveVimbaCameraAssignment(
        configPath,
        dialog.selectedSlot(),
        device.deviceId,
        device.serial,
        dialog.displayName(),
        device.modelName,
        device.interfaceId,
        dialog.cameraEnabled(),
        &error))
  {
    log("VimbaX assignment save failed: " + error);
    QMessageBox::warning(window(), tr("menu.cameras"), error);
    return;
  }

  const QString assignedCameraId = QString("CAM%1").arg(dialog.selectedSlot(), 2, 10, QChar('0'));
  cameraRuntime().erase(assignedCameraId);
  log(QString("VimbaX assignment runtime reset: %1").arg(assignedCameraId));
  if (context().loadConfiguration)
  {
    context().loadConfiguration();
    log("VimbaX assignment configuration reloaded");
  }

  const CameraConfig assignedCamera = cameraById(config(), assignedCameraId);
  if (!assignedCamera.id.isEmpty() && context().selectCamera)
  {
    context().selectCamera(assignedCamera);
    log(QString("VimbaX assignment selected slot: %1").arg(assignedCameraId));
  }

  log(QString("%1: %2 -> %3 trigger=external/ioBoard")
        .arg(tr("log.vimbaCameraAssigned"))
        .arg(assignedCameraId)
        .arg(device.serial.isEmpty() ? device.deviceId : device.serial));
}

void MainWindowCameraConfigModule::configureUsbCamera(const CameraConfig& camera)
{
  configureUsbCameraSlot(camera.slot, camera.id);
}

void MainWindowCameraConfigModule::configureUsbCameraSlot(int currentSlot, const QString& currentCameraId)
{
  const int defaultSlot = currentSlot <= 0 ? 1 : currentSlot;
  log(QString("USB camera assignment open: currentCamera=%1 currentSlot=%2 maxSlot=%3")
        .arg(currentCameraId.isEmpty() ? "<menu>" : currentCameraId)
        .arg(defaultSlot)
        .arg(config().maxCameras()));

  UsbCameraAssignmentDialog dialog(
    defaultSlot,
    cameraByIdOrSlot(config(), currentCameraId, defaultSlot).usbIndex,
    config().maxCameras(),
    [this](const QString& message) { log(message); },
    window());
  if (dialog.exec() != QDialog::Accepted)
  {
    log(QString("USB camera assignment canceled: currentCamera=%1").arg(currentCameraId.isEmpty() ? "<menu>" : currentCameraId));
    return;
  }

  const UsbCameraDeviceInfo device = dialog.selectedDevice();
  if (device.index < 0)
  {
    log("USB camera assignment rejected: no camera selected");
    QMessageBox::warning(window(), tr("menu.cameras"), tr("log.usbNoCameraSelected"));
    return;
  }

  QString error;
  const QString configPath = projectPath("config/cameras.json");
  log(QString("USB camera assignment save begin: path='%1' slot=%2 enabled=%3 name='%4' index=%5 size=%6x%7 fps=%8")
        .arg(configPath)
        .arg(dialog.selectedSlot())
        .arg(dialog.cameraEnabled() ? "true" : "false")
        .arg(dialog.displayName())
        .arg(device.index)
        .arg(device.width)
        .arg(device.height)
        .arg(device.fps, 0, 'f', 2));

  if (!config().saveUsbCameraAssignment(
        configPath,
        dialog.selectedSlot(),
        device.index,
        dialog.displayName(),
        dialog.cameraEnabled(),
        &error))
  {
    log("USB camera assignment save failed: " + error);
    QMessageBox::warning(window(), tr("menu.cameras"), error);
    return;
  }

  const QString assignedCameraId = QString("CAM%1").arg(dialog.selectedSlot(), 2, 10, QChar('0'));
  cameraRuntime().erase(assignedCameraId);
  log(QString("USB camera assignment runtime reset: %1").arg(assignedCameraId));
  if (context().loadConfiguration)
  {
    context().loadConfiguration();
    log("USB camera assignment configuration reloaded");
  }

  const CameraConfig assignedCamera = cameraById(config(), assignedCameraId);
  if (!assignedCamera.id.isEmpty() && context().setup)
  {
    context().setup->startCameraSimulation(assignedCamera);
    log(QString("USB camera assignment live start requested: %1").arg(assignedCameraId));
  }

  log(QString("%1: %2 -> index %3")
        .arg(tr("log.usbCameraAssigned"))
        .arg(assignedCameraId)
        .arg(device.index));
}

void MainWindowCameraConfigModule::configureCameraSampleImage(const CameraConfig& camera)
{
  MainWindowImagingModule& imaging = *context().imaging;
  const QString currentFolder = camera.folder.isEmpty()
    ? RecipeManager::recipesRootPath()
    : imaging.resolvedCameraFolder(camera);
  const QString selectedFile = QFileDialog::getOpenFileName(
    window(),
    QString("%1 | %2").arg(tr("actions.assignSampleImage"), camera.id),
    currentFolder,
    "Images (*.bmp *.jpg *.jpeg *.png *.tif *.tiff)");

  if (selectedFile.isEmpty())
  {
    return;
  }

  QString error;
  if (!recipes().importCameraSampleImage(camera.id, selectedFile, &error))
  {
    QMessageBox::warning(window(), tr("actions.assignSampleImage"), error);
    return;
  }

  if (camera.id == selectedCameraId())
  {
    selectedImagePath() = imaging.cameraSampleImagePath(camera);
    cameraRuntime()[camera.id].stop();
    selectedPreview() = QPixmap(selectedImagePath());
    largeImage()->setImage(selectedPreview());
    if (context().setup)
    {
      context().setup->showCameraSetupPanel(camera);
    }
  }

  log(QString("%1: %2").arg(tr("log.cameraSampleSaved"), camera.id));
}

void MainWindowCameraConfigModule::configureCameraTestImages(const CameraConfig& camera)
{
  MainWindowImagingModule& imaging = *context().imaging;
  const QString currentFolder = camera.folder.isEmpty()
    ? RecipeManager::recipesRootPath()
    : imaging.resolvedCameraFolder(camera);
  const QString selectedFolder = QFileDialog::getExistingDirectory(
    window(),
    QString("%1 | %2").arg(tr("actions.assignTestImages"), camera.id),
    currentFolder);

  if (selectedFolder.isEmpty())
  {
    return;
  }

  QString error;
  if (!recipes().importCameraTestImages(camera.id, selectedFolder, &error))
  {
    QMessageBox::warning(window(), tr("actions.assignTestImages"), error);
    return;
  }

  if (camera.id == selectedCameraId())
  {
    cameraRuntime()[camera.id].stop();
    if (context().setup)
    {
      context().setup->showCameraSetupPanel(camera);
    }
  }

  log(QString("%1: %2").arg(tr("log.cameraTestImagesSaved"), camera.id));
}

void MainWindowCameraConfigModule::acquireCameraSampleImage(const CameraConfig& camera)
{
  MainWindowImagingModule& imaging = *context().imaging;
  cv::Mat sample;
  const auto runtimeIt = cameraRuntime().find(camera.id);
  if (runtimeIt != cameraRuntime().end() && !runtimeIt->second.currentFrame().empty())
  {
    sample = runtimeIt->second.currentFrame().clone();
  }
  else
  {
    QString imageError;
    sample = imaging.currentInputImage(camera, &imageError);
    if (sample.empty())
    {
      log(imageError);
      return;
    }
  }

  QString error;
  if (!recipes().ensureCameraImageFolders(camera.id, &error))
  {
    QMessageBox::warning(window(), tr("actions.acquireSampleImage"), error);
    return;
  }

  const QString samplePath = QDir(recipes().cameraSampleImagesPath(camera.id)).filePath("sample.png");
  if (!cv::imwrite(samplePath.toStdString(), sample))
  {
    QMessageBox::warning(
      window(),
      tr("actions.acquireSampleImage"),
      tr("log.cameraSampleAcquireFailed") + ": " + samplePath);
    return;
  }

  if (camera.id == selectedCameraId())
  {
    selectedImagePath() = samplePath;
    selectedPreview() = imaging.matToPixmap(sample);
    largeImage()->setImage(selectedPreview());
    if (context().setup)
    {
      context().setup->showCameraSetupPanel(camera);
    }
  }

  log(QString("%1: %2").arg(tr("log.cameraSampleAcquired"), camera.id));
}

void MainWindowCameraConfigModule::acquireCameraAiSampleImage(const CameraConfig& camera)
{
  QString imageError;
  const cv::Mat sample = currentCameraFrameForSave(context(), camera, &imageError);
  if (sample.empty())
  {
    log(imageError);
    return;
  }

  QString error;
  if (!recipes().ensureCameraImageFolders(camera.id, &error))
  {
    QMessageBox::warning(window(), tr("actions.acquireAiSampleImage"), error);
    return;
  }

  const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
  const QString samplePath = QDir(recipes().cameraAiRawImagesPath(camera.id)).filePath(QString("%1_%2.png").arg(camera.id, stamp));
  if (!cv::imwrite(samplePath.toStdString(), sample))
  {
    QMessageBox::warning(
      window(),
      tr("actions.acquireAiSampleImage"),
      tr("log.cameraAiSampleAcquireFailed") + ": " + samplePath);
    return;
  }

  log(QString("%1: %2 -> %3").arg(tr("log.cameraAiSampleAcquired"), camera.id, samplePath));
}

void MainWindowCameraConfigModule::acquireCameraAiClassificationRawImage(const CameraConfig& camera)
{
  QString imageError;
  const cv::Mat sample = currentCameraFrameForSave(context(), camera, &imageError);
  if (sample.empty())
  {
    log(imageError);
    return;
  }

  QString error;
  if (!recipes().ensureCameraImageFolders(camera.id, &error))
  {
    QMessageBox::warning(window(), tr("actions.acquireAiRawImage"), error);
    return;
  }

  const QString folder = recipes().cameraAiClassificationRawImagesPath(camera.id);
  QDir().mkpath(folder);
  const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
  const QString imagePath = QDir(folder).filePath(QString("%1_raw_%2.png").arg(camera.id, stamp));
  if (!cv::imwrite(imagePath.toStdString(), sample))
  {
    QMessageBox::warning(window(), tr("actions.acquireAiRawImage"), tr("log.cameraAiSampleAcquireFailed") + ": " + imagePath);
    return;
  }

  log(QString("%1: %2 -> %3").arg(tr("log.cameraAiRawAcquired"), camera.id, imagePath));
}

void MainWindowCameraConfigModule::acquireCameraAiClassificationClassImage(
  const CameraConfig& camera,
  const AiClassificationClassConfig& classConfig)
{
  QString imageError;
  const cv::Mat sample = currentCameraFrameForSave(context(), camera, &imageError);
  if (sample.empty())
  {
    log(imageError);
    return;
  }

  const QString folder = recipes().cameraAiClassificationClassImagesPath(camera.id, classConfig);
  QDir().mkpath(folder);
  const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
  const QString imagePath = QDir(folder).filePath(QString("%1_%2_%3.png").arg(camera.id).arg(classConfig.id, 3, 10, QChar('0')).arg(stamp));
  if (!cv::imwrite(imagePath.toStdString(), sample))
  {
    QMessageBox::warning(window(), tr("actions.acquireAiClassImage"), tr("log.cameraAiSampleAcquireFailed") + ": " + imagePath);
    return;
  }

  log(QString("%1: %2 class=%3 -> %4").arg(tr("log.cameraAiClassAcquired"), camera.id, classConfig.name, imagePath));
}
