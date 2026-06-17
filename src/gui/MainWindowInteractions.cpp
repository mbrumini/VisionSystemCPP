#include "gui/MainWindow.h"

#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "gui/TouchIconButton.h"

#include <QApplication>
#include <QDateTime>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

void MainWindow::incPendingJobs(const QString& cameraId)
{
  const int v = m_cameraPendingJobs.value(cameraId, 0) + 1;
  m_cameraPendingJobs[cameraId] = v;
  m_cameraProcessingBusy[cameraId] = true;
  updateCameraStripStatus(cameraId);
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
  if (cameraId == m_selectedCameraId)
  {
    updateMeasurementResults();
  }
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

void MainWindow::startMachine()
{
  m_machineRunning = true;
  const bool keepCurrentTool =
    !m_selectedCameraId.isEmpty() &&
    m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::None;
  if (!keepCurrentTool)
  {
    deactivateImageDrawingTools();
  }
  if (!m_selectedCameraId.isEmpty() &&
      (m_selectedCamera.type == "file" || m_selectedCamera.type == "usb" || m_selectedCamera.type == "vimba"))
  {
    m_setup.startCameraSimulation(m_selectedCamera, false);
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
  appendLog(trText("log.command") + ": " + trText("commands.start"));
}

void MainWindow::stopMachine()
{
  const bool keepCurrentTool =
    !m_selectedCameraId.isEmpty() &&
    m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::None;
  if (!m_selectedCameraId.isEmpty() &&
      (m_selectedCamera.type == "file" || m_selectedCamera.type == "usb"))
  {
    m_setup.stopCameraSimulation(m_selectedCamera, false);
  }
  m_machineRunning = false;
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

  auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt != m_cameraRuntime.end() && !runtimeIt->second.currentFrame().empty())
  {
    m_selectedPreview = m_imaging.matToPixmap(runtimeIt->second.currentFrame());
  }
  else
  {
    m_selectedPreview = m_selectedImagePath.isEmpty() ? QPixmap() : QPixmap(m_selectedImagePath);
  }
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
    m_geometry.appendCurrentPartPoseOverlay(camera, overlay);
    m_largeImage->setGeometryOverlay(overlay);
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

void MainWindow::appendLog(const QString& message)
{
  const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
  const QString line = QString("[%1] %2").arg(timestamp, message);
  m_detailedLogger.logLine(message);

  if (!m_log)
  {
    return;
  }

  m_log->append(line);
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

QString MainWindow::trText(const QString& key) const
{
  return m_translations.text(key);
}

