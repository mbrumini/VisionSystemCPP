#include "gui/modules/MainWindowCameraConfigModule.h"

#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowSetupModule.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

#include <opencv2/imgcodecs.hpp>

namespace
{
QString projectPath(const QString& relativePath)
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(relativePath);
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

  const QDir projectDirectory(QString::fromUtf8(PROJECT_SOURCE_DIR));
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
