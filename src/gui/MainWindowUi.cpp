#include "gui/MainWindow.h"

#include "gui/modules/MainWindowCameraProfile.h"

#include "gui/SurfaceLocalizationPanelWidget.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"
#include "gui/TouchIconButton.h"
#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryMath.h"
#include "processing/SurfaceModelTrainer.h"
#include "processing/geometry/EdgeLineDetector.h"
#include "processing/geometry/EdgePointDetector.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <functional>
#include <vector>
#include <type_traits>
#include <memory>

void MainWindow::resizeEvent(QResizeEvent* event)
{
  QMainWindow::resizeEvent(event);
  updateLargePreview();
}

void MainWindow::buildUi()
{
  setWindowTitle("VisionSystemCPP");
  resize(1600, 900);
  buildMenu();
  if (!m_simulationTimer)
  {
    m_simulationTimer = new QTimer(this);
    connect(m_simulationTimer, &QTimer::timeout, this, [this]() {
      for (const CameraConfig& camera : m_config.activeCameras())
      {
        if (camera.type != "simulator" && m_cameraRuntime[camera.id].running())
        {
          m_setup.advanceCameraFrame(camera);
        }
      }
    });
  }

  auto* root = new QWidget(this);
  auto* rootLayout = new QGridLayout(root);
  rootLayout->setContentsMargins(8, 8, 8, 8);
  rootLayout->setHorizontalSpacing(8);
  rootLayout->setVerticalSpacing(8);

  m_commandToolbar = new CommandToolbarWidget(root);
  m_commandToolbar->setLabels(trText("commands.start"),
                              trText("commands.stop"),
                              trText("commands.gridView"),
                              trText("commands.reloadConfig"),
                              trText("commands.toggleFullScreen"),
                              trText("commands.help"));
  m_commandToolbar->setStartHandler([this]() { startMachine(); });
  m_commandToolbar->setStopHandler([this]() { stopMachine(); });
  m_commandToolbar->setGridHandler([this]() { showGridView(); });
  m_commandToolbar->setReloadHandler([this]() { loadConfiguration(); });
  m_commandToolbar->setFullscreenHandler([this]() { toggleFullScreen(); });
  m_commandToolbar->setHelpHandler([this]() { showHelp(); });
  rootLayout->addWidget(m_commandToolbar, 0, 0, 1, 2);

  m_gridPage = new QWidget(root);
  m_gridPage->hide();
  m_gridContent = new QWidget(m_gridPage);
  m_gridContent->hide();
  m_gridLayout = new QGridLayout(m_gridContent);
  m_gridLayout->setContentsMargins(0, 0, 0, 0);
  m_gridLayout->setSpacing(0);

  m_imageStack = new QStackedWidget(root);

  auto* overviewPage = new QWidget(m_imageStack);
  auto* overviewLayout = new QVBoxLayout(overviewPage);
  overviewLayout->setContentsMargins(16, 16, 16, 16);
  overviewLayout->setSpacing(10);
  m_measurementResults = new MeasurementResultsWidget(overviewPage);
  m_measurementResults->setTitle(trText("tools.measurements"));
  overviewLayout->addWidget(m_measurementResults, 1);
  m_imageStack->addWidget(overviewPage);

  auto* largePage = new QWidget(m_imageStack);
  auto* largeLayout = new QVBoxLayout(largePage);
  largeLayout->setContentsMargins(16, 16, 16, 16);
  largeLayout->setSpacing(10);

  m_largeTitle = new QLabel(trText("labels.noCameraSelected"), largePage);
  m_largeTitle->setObjectName("largeTitle");
  m_largeImage = new ImageViewWidget(largePage);
  m_largeImage->setStyleSheet("background:#101418;color:#9aa4ad;");
  m_largeImage->setMinimumSize(320, 240);
  m_largeImage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setupLargeImageHandlers();
  largeLayout->addWidget(m_largeTitle);
  largeLayout->addWidget(m_largeImage, 1);
  m_imageStack->addWidget(largePage);

  m_cameraStrip = new CameraStripWidget(root);
  m_cameraStrip->setCameraClickHandler([this](const CameraConfig& camera) {
    if (camera.id == m_selectedCameraId)
    {
      showGridView();
      return;
    }
    selectCamera(camera);
  });

  auto* rightPanel = new QWidget(root);
  rightPanel->setMinimumWidth(480);
  rightPanel->setMaximumWidth(560);
  auto* rightLayout = new QHBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(8);

  m_toolIconBar = new ToolIconBarWidget(rightPanel);
  m_toolIconBar->setToolClickHandler([this](const QString& toolId) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }
    if (m_machineRunning)
    {
      appendLog(trText("log.startReadOnly"));
      updateControlPanel(&m_selectedCamera);
      return;
    }
    if (toolId != "setup" && m_setupCameraId == m_selectedCameraId)
    {
      m_setup.stopCameraSimulation(m_selectedCamera, false);
    }
    if (toolId != "ai")
    {
      m_setup.stopAiClassificationCapture();
    }
    m_toolIconBar->setActiveTool(toolId);
    if (toolId == "setup")
    {
      m_setup.showCameraSetupPanel(m_selectedCamera);
    }
    else if (toolId == "constructedGeometries")
    {
      m_constructedGeometry.showConstructedGeometryPanel(m_selectedCamera);
    }
    else if (toolId == "measurements")
    {
      m_measurement.showMeasurementPanel(m_selectedCamera);
    }
    else if (toolId == "localization")
    {
      showLocalizationStrategyList(m_selectedCamera);
    }
    else
    {
      m_setup.showToolPanel(m_selectedCamera, toolId);
    }
  });
  rightLayout->addWidget(m_toolIconBar);

  auto* panelScrollArea = new QScrollArea(rightPanel);
  panelScrollArea->setWidgetResizable(true);
  panelScrollArea->setFrameShape(QFrame::NoFrame);
  panelScrollArea->setMinimumWidth(370);
  panelScrollArea->setMaximumWidth(470);

  auto* panel = new QWidget(panelScrollArea);
  panel->setMinimumWidth(370);
  auto* panelLayout = new QVBoxLayout(panel);
  panelLayout->setContentsMargins(10, 10, 10, 10);
  panelLayout->setSpacing(8);

  m_systemStatus = new QLabel(trText("status.systemReady"), panel);
  m_systemStatus->setObjectName("panelStatus");
  panelLayout->addWidget(m_systemStatus);

  auto* cameraBox = new QGroupBox(trText("groups.selectedCamera"), panel);
  auto* cameraLayout = new QVBoxLayout(cameraBox);
  m_cameraDetails = new QLabel(trText("labels.selectThumbnail"), cameraBox);
  m_cameraDetails->setWordWrap(true);
  cameraLayout->addWidget(m_cameraDetails);
  panelLayout->addWidget(cameraBox);

  auto* toolsBox = new QGroupBox(trText("groups.cameraTools"), panel);
  m_toolsLayout = new QVBoxLayout(toolsBox);
  m_toolsContainer = toolsBox;
  panelLayout->addWidget(toolsBox, 1);

  auto* logBox = new QGroupBox(trText("groups.eventLog"), panel);
  m_logBox = logBox;
  auto* logLayout = new QVBoxLayout(logBox);
  m_log = new QTextEdit(logBox);
  m_log->setReadOnly(true);
  m_log->setMaximumHeight(130);
  logLayout->addWidget(m_log);
  logBox->setVisible(m_accessSession.isAuthenticated());
  panelLayout->addWidget(logBox);
  panelScrollArea->setWidget(panel);

  rightLayout->addWidget(panelScrollArea, 1);

  rootLayout->addWidget(m_imageStack, 1, 0);
  rootLayout->addWidget(rightPanel, 1, 1, 3, 1);
  rootLayout->addWidget(m_cameraStrip, 2, 0);
  rootLayout->setColumnStretch(0, 1);
  rootLayout->setColumnStretch(1, 0);
  rootLayout->setRowStretch(1, 1);
  setCentralWidget(root);

  applyApplicationStyle();
}

