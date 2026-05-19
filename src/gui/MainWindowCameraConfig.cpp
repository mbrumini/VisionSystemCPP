#include "MainWindow.h"

#include "gui/SurfaceLocalizationPanelWidget.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"
#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "processing/SurfaceModelTrainer.h"
#include "processing/geometry/EdgeLineDetector.h"
#include "processing/geometry/EdgePointDetector.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QSettings>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>
#include <type_traits>
#include "util/AsyncExecutor.h"
#include <thread>
#include <memory>

using AsyncExecutor::runAsyncTask;

namespace
{
QString projectPath(const QString& relativePath)
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(relativePath);
}



}


void MainWindow::configureCameraSource(const CameraConfig& camera)
{
  const QString currentFolder = camera.folder.isEmpty()
    ? projectPath("data/images")
    : resolvedCameraFolder(camera);
  const QString selectedFolder = QFileDialog::getExistingDirectory(
    this,
    QString("%1 | %2").arg(trText("menu.cameras"), camera.id),
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
  if (!m_config.saveCameraSource(configPath, camera.id, "file", storedFolder, &error))
  {
    QMessageBox::warning(this, trText("menu.cameras"), error);
    return;
  }

  m_cameraRuntime.erase(camera.id);
  loadConfiguration();
  appendLog(QString("%1: %2 -> %3").arg(trText("log.cameraSourceSaved"), camera.id, storedFolder));
}

void MainWindow::configureCameraSampleImage(const CameraConfig& camera)
{
  const QString currentFolder = camera.folder.isEmpty()
    ? RecipeManager::recipesRootPath()
    : resolvedCameraFolder(camera);
  const QString selectedFile = QFileDialog::getOpenFileName(
    this,
    QString("%1 | %2").arg(trText("actions.assignSampleImage"), camera.id),
    currentFolder,
    "Images (*.bmp *.jpg *.jpeg *.png *.tif *.tiff)");

  if (selectedFile.isEmpty())
  {
    return;
  }

  QString error;
  if (!m_recipeManager.importCameraSampleImage(camera.id, selectedFile, &error))
  {
    QMessageBox::warning(this, trText("actions.assignSampleImage"), error);
    return;
  }

  if (camera.id == m_selectedCameraId)
  {
    m_selectedImagePath = cameraSampleImagePath(camera);
    m_cameraRuntime[camera.id].stop();
    m_selectedPreview = QPixmap(m_selectedImagePath);
    m_largeImage->setImage(m_selectedPreview);
    showCameraSetupPanel(camera);
  }

  appendLog(QString("%1: %2").arg(trText("log.cameraSampleSaved"), camera.id));
}

void MainWindow::configureCameraTestImages(const CameraConfig& camera)
{
  const QString currentFolder = camera.folder.isEmpty()
    ? RecipeManager::recipesRootPath()
    : resolvedCameraFolder(camera);
  const QString selectedFolder = QFileDialog::getExistingDirectory(
    this,
    QString("%1 | %2").arg(trText("actions.assignTestImages"), camera.id),
    currentFolder);

  if (selectedFolder.isEmpty())
  {
    return;
  }

  QString error;
  if (!m_recipeManager.importCameraTestImages(camera.id, selectedFolder, &error))
  {
    QMessageBox::warning(this, trText("actions.assignTestImages"), error);
    return;
  }

  if (camera.id == m_selectedCameraId)
  {
    m_cameraRuntime[camera.id].stop();
    showCameraSetupPanel(camera);
  }

  appendLog(QString("%1: %2").arg(trText("log.cameraTestImagesSaved"), camera.id));
}

void MainWindow::acquireCameraSampleImage(const CameraConfig& camera)
{
  cv::Mat sample;
  const auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt != m_cameraRuntime.end() && !runtimeIt->second.currentFrame().empty())
  {
    sample = runtimeIt->second.currentFrame().clone();
  }
  else
  {
    QString imageError;
    sample = currentInputImage(camera, &imageError);
    if (sample.empty())
    {
      appendLog(imageError);
      return;
    }
  }

  QString error;
  if (!m_recipeManager.ensureCameraImageFolders(camera.id, &error))
  {
    QMessageBox::warning(this, trText("actions.acquireSampleImage"), error);
    return;
  }

  const QString samplePath = QDir(m_recipeManager.cameraSampleImagesPath(camera.id)).filePath("sample.png");
  if (!cv::imwrite(samplePath.toStdString(), sample))
  {
    QMessageBox::warning(
      this,
      trText("actions.acquireSampleImage"),
      trText("log.cameraSampleAcquireFailed") + ": " + samplePath);
    return;
  }

  if (camera.id == m_selectedCameraId)
  {
    m_selectedImagePath = samplePath;
    m_selectedPreview = matToPixmap(sample);
    m_largeImage->setImage(m_selectedPreview);
    showCameraSetupPanel(camera);
  }

  appendLog(QString("%1: %2").arg(trText("log.cameraSampleAcquired"), camera.id));
}

void MainWindow::loadConfiguration()
{
  QString error;
  const QString configPath = projectPath("config/cameras.json");

  if (!m_config.load(configPath, &error))
  {
    m_systemStatus->setText(trText("status.configError"));
    appendLog(error);
    return;
  }

  buildMenu();

  while (QLayoutItem* item = m_gridLayout->takeAt(0))
  {
    if (QWidget* widget = item->widget())
    {
      widget->deleteLater();
    }

    delete item;
  }

  m_tiles.clear();
  const QVector<CameraConfig> cameras = m_config.activeCameras();
  ensureRecipeCameraFolders();

  for (int i = 0; i < cameras.size(); ++i)
  {
    auto* tile = new CameraTileWidget(cameras[i], m_gridContent);
    tile->setPreview(loadCameraPreview(cameras[i]));
    tile->setClickHandler([this](const CameraConfig& camera) { selectCamera(camera); });
    m_gridLayout->addWidget(tile, i / 4, i % 4);
    m_tiles.append(tile);
  }

  m_systemStatus->setText(QString("%1 | %2 %3")
                            .arg(trText("status.systemReady"))
                            .arg(cameras.size())
                            .arg(trText("status.activeCameras")));
  appendLog(QString("%1: %2 %3 %4 %5")
              .arg(trText("log.configLoaded"))
              .arg(cameras.size())
              .arg(trText("status.activeCameras"))
              .arg(trText("labels.max"))
              .arg(m_config.maxCameras()));

  if (!cameras.isEmpty())
  {
    updateControlPanel(nullptr);
  }

  showGridView();
}


