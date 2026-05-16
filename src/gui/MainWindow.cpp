#include "MainWindow.h"

#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSplitter>
#include <QVBoxLayout>

namespace
{
QString projectPath(const QString& relativePath)
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(relativePath);
}

}

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
{
  QString error;
  if (!m_translations.loadLanguage("it", &error))
  {
    m_translations.loadLanguage("en");
  }

  buildUi();
  loadConfiguration();
}

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

  auto* root = new QWidget(this);
  auto* rootLayout = new QHBoxLayout(root);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  auto* splitter = new QSplitter(Qt::Horizontal, root);
  splitter->setChildrenCollapsible(false);

  m_imageStack = new QStackedWidget(splitter);
  m_gridPage = new QWidget(m_imageStack);

  auto* gridScrollArea = new QScrollArea(m_gridPage);
  gridScrollArea->setWidgetResizable(true);
  gridScrollArea->setFrameShape(QFrame::NoFrame);

  m_gridContent = new QWidget(gridScrollArea);
  m_gridLayout = new QGridLayout(m_gridContent);
  m_gridLayout->setContentsMargins(12, 12, 12, 12);
  m_gridLayout->setSpacing(10);
  m_gridLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

  for (int i = 0; i < 4; ++i)
  {
    m_gridLayout->setColumnStretch(i, 1);
    m_gridLayout->setRowStretch(i, 1);
  }

  gridScrollArea->setWidget(m_gridContent);
  auto* gridPageLayout = new QVBoxLayout(m_gridPage);
  gridPageLayout->setContentsMargins(0, 0, 0, 0);
  gridPageLayout->addWidget(gridScrollArea);
  m_imageStack->addWidget(m_gridPage);

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
  m_largeImage->setRoiChangedHandler([this](const QRect& roi) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    QString error;
    if (!m_recipeManager.saveLocalizationRoi(m_selectedCameraId, roi, &error))
    {
      appendLog(error);
      return;
    }

    appendLog(QString("%1: %2 x=%3 y=%4 w=%5 h=%6")
                .arg(trText("log.localizationRoiSaved"))
                .arg(m_selectedCameraId)
                .arg(roi.x())
                .arg(roi.y())
                .arg(roi.width())
                .arg(roi.height()));
  });

  largeLayout->addWidget(m_largeTitle);
  largeLayout->addWidget(m_largeImage, 1);
  m_imageStack->addWidget(largePage);

  auto* panelScrollArea = new QScrollArea(splitter);
  panelScrollArea->setWidgetResizable(true);
  panelScrollArea->setFrameShape(QFrame::NoFrame);
  panelScrollArea->setMinimumWidth(340);
  panelScrollArea->setMaximumWidth(460);

  auto* panel = new QWidget(panelScrollArea);
  panel->setMinimumWidth(340);
  auto* panelLayout = new QVBoxLayout(panel);
  panelLayout->setContentsMargins(14, 14, 14, 14);
  panelLayout->setSpacing(12);

  m_systemStatus = new QLabel(trText("status.systemReady"), panel);
  m_systemStatus->setObjectName("panelStatus");
  panelLayout->addWidget(m_systemStatus);

  auto* commands = new QGroupBox(trText("groups.generalCommands"), panel);
  auto* commandsLayout = new QGridLayout(commands);
  const QVector<QPair<QString, QString>> commandsList = {
    {"start", "commands.start"},
    {"stop", "commands.stop"},
    {"resetErrors", "commands.resetErrors"},
    {"reloadConfig", "commands.reloadConfig"},
    {"gridView", "commands.gridView"},
    {"exit", "commands.exit"}
  };

  for (int i = 0; i < commandsList.size(); ++i)
  {
    const QString commandId = commandsList[i].first;
    const QString commandLabel = trText(commandsList[i].second);
    auto* button = new QPushButton(commandLabel, commands);
    commandsLayout->addWidget(button, i / 2, i % 2);

    if (commandId == "gridView")
    {
      connect(button, &QPushButton::clicked, this, [this]() { showGridView(); });
    }
    else if (commandId == "reloadConfig")
    {
      connect(button, &QPushButton::clicked, this, [this]() { loadConfiguration(); });
    }
    else if (commandId == "exit")
    {
      connect(button, &QPushButton::clicked, qApp, &QApplication::quit);
    }
    else
    {
      connect(button, &QPushButton::clicked, this, [this, commandLabel]() {
        appendLog(trText("log.command") + ": " + commandLabel);
      });
    }
  }

  panelLayout->addWidget(commands);

  auto* cameraBox = new QGroupBox(trText("groups.selectedCamera"), panel);
  auto* cameraLayout = new QVBoxLayout(cameraBox);
  m_cameraDetails = new QLabel(trText("labels.selectThumbnail"), cameraBox);
  m_cameraDetails->setWordWrap(true);
  cameraLayout->addWidget(m_cameraDetails);
  panelLayout->addWidget(cameraBox);

  auto* toolsBox = new QGroupBox(trText("groups.cameraTools"), panel);
  m_toolsLayout = new QVBoxLayout(toolsBox);
  m_toolsContainer = toolsBox;
  panelLayout->addWidget(toolsBox);

  auto* logBox = new QGroupBox(trText("groups.eventLog"), panel);
  auto* logLayout = new QVBoxLayout(logBox);
  m_log = new QTextEdit(logBox);
  m_log->setReadOnly(true);
  m_log->setMinimumHeight(160);
  logLayout->addWidget(m_log);
  panelLayout->addWidget(logBox, 1);
  panelScrollArea->setWidget(panel);

  splitter->addWidget(m_imageStack);
  splitter->addWidget(panelScrollArea);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);

  rootLayout->addWidget(splitter);
  setCentralWidget(root);

  setStyleSheet(
    "QMainWindow,QWidget{background:#0f1419;color:#eef2f6;font-family:'Segoe UI';font-size:13px;}"
    "QGroupBox{border:1px solid #313b46;border-radius:6px;margin-top:10px;padding:10px 8px 8px 8px;font-weight:600;}"
    "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;color:#cdd6df;}"
    "QPushButton{background:#24313d;border:1px solid #3b4652;border-radius:5px;padding:8px;color:#f5f7fa;}"
    "QPushButton:hover{background:#2e3d4b;}"
    "QTextEdit{background:#111820;border:1px solid #313b46;border-radius:5px;color:#d7dee6;}"
    "#largeTitle{font-size:20px;font-weight:700;color:#f4f7fb;}"
    "#panelStatus{font-size:17px;font-weight:700;color:#f4f7fb;}"
    "#toolPanelTitle{font-size:15px;font-weight:700;color:#f4f7fb;}"
    "#toolPanelNote{color:#9aa4ad;}"
  );
}

void MainWindow::buildMenu()
{
  menuBar()->clear();

  QMenu* recipesMenu = menuBar()->addMenu(trText("menu.recipes"));
  recipesMenu->addAction(trText("menu.selectRecipe"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.selectRecipe"));
  });
  recipesMenu->addAction(trText("menu.newRecipe"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.newRecipe"));
  });
  recipesMenu->addAction(trText("menu.importRecipe"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.importRecipe"));
  });
  recipesMenu->addAction(trText("menu.exportRecipe"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.exportRecipe"));
  });

  QMenu* configMenu = menuBar()->addMenu(trText("menu.configurations"));
  configMenu->addAction(trText("menu.cameras"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.cameras"));
  });
  configMenu->addAction(trText("menu.paths"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.paths"));
  });
  configMenu->addAction(trText("menu.diagnostics"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.diagnostics"));
  });

  QMenu* languageMenu = menuBar()->addMenu(trText("menu.language"));
  languageMenu->addAction("Italiano", this, [this]() { changeLanguage("it"); });
  languageMenu->addAction("English", this, [this]() { changeLanguage("en"); });

  QMenu* systemMenu = menuBar()->addMenu(trText("menu.system"));
  systemMenu->addAction(trText("commands.start"), this, [this]() {
    appendLog(trText("log.command") + ": " + trText("commands.start"));
  });
  systemMenu->addAction(trText("commands.stop"), this, [this]() {
    appendLog(trText("log.command") + ": " + trText("commands.stop"));
  });
  systemMenu->addAction(trText("commands.reloadConfig"), this, [this]() { loadConfiguration(); });
  systemMenu->addAction(trText("commands.toggleFullScreen"), this, [this]() { toggleFullScreen(); });
  systemMenu->addSeparator();
  systemMenu->addAction(trText("commands.exit"), qApp, &QApplication::quit);
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

void MainWindow::showGridView()
{
  m_imageStack->setCurrentIndex(0);
  appendLog(trText("log.gridView"));
}

void MainWindow::selectCamera(const CameraConfig& camera)
{
  m_selectedCameraId = camera.id;
  m_selectedCamera = camera;
  m_largeImage->setRoiDrawingEnabled(false);
  m_largeImage->clearRoi();

  for (CameraTileWidget* tile : m_tiles)
  {
    tile->setSelected(tile->camera().id == camera.id);
  }

  m_selectedPreview = loadCameraPreview(camera);
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
  if (m_recipeManager.loadLocalizationRoi(camera.id, savedRoi))
  {
    m_largeImage->setRoi(savedRoi);
  }

  m_imageStack->setCurrentIndex(1);
  QApplication::processEvents();
  updateLargePreview();
  updateControlPanel(&camera);
  appendLog(trText("log.cameraSelected") + ": " + camera.id);
}

void MainWindow::updateControlPanel(const CameraConfig* camera)
{
  clearToolPanel();

  if (!camera)
  {
    m_cameraDetails->setText(trText("labels.selectThumbnail"));
    m_toolsLayout->addWidget(new QLabel(trText("labels.noToolSelected"), m_toolsContainer));
    m_toolsLayout->addStretch(1);
    return;
  }

  m_cameraDetails->setText(
    QString("%1 %2 | %3\n%4: %5\n%6: %7\n%8: %9\n%10: %11")
      .arg(trText("labels.camera"))
      .arg(camera->slot)
      .arg(camera->displayName)
      .arg(trText("labels.profile"))
      .arg(camera->processingProfileId)
      .arg(trText("labels.mode"))
      .arg(camera->profile.imageMode)
      .arg(trText("labels.inspections"))
      .arg(camera->profile.inspectionTypes.join(", "))
      .arg(trText("labels.source"))
      .arg(camera->folder));

  showCameraToolList(*camera);
}

void MainWindow::showCameraToolList(const CameraConfig& camera)
{
  clearToolPanel();

  for (const QString& tool : camera.profile.guiTools)
  {
    auto* button = new QPushButton(ToolCatalog::label(tool, m_translations), m_toolsContainer);
    connect(button, &QPushButton::clicked, this, [this, camera, tool]() {
      showToolPanel(camera, tool);
    });
    m_toolsLayout->addWidget(button);
  }

  m_toolsLayout->addStretch(1);
}

void MainWindow::showToolPanel(const CameraConfig& camera, const QString& toolId)
{
  clearToolPanel();

  const ToolDefinition tool = ToolCatalog::tool(toolId, m_translations);
  auto* panel = new ToolPanelWidget(
    tool,
    QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot),
    trText("labels.placeholderNote"),
    trText("commands.backToCameraTools"),
    [this, camera]() {
      showCameraToolList(camera);
      appendLog(trText("log.backToCameraTools") + ": " + camera.id);
    },
    [this, camera, tool](const ToolActionDefinition& action) {
      if (tool.id == "localization" && action.id == "searchRoi")
      {
        activateLocalizationRoiDrawing(camera);
        return;
      }

      appendLog(trText("log.placeholder") + ": " + tool.label + " -> " + action.label);
    },
    m_toolsContainer);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + tool.label);
}

void MainWindow::activateLocalizationRoiDrawing(const CameraConfig& camera)
{
  if (!isBwDimensionalCamera(camera))
  {
    appendLog(trText("log.localizationNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_largeImage->setRoiDrawingEnabled(true);
  appendLog(trText("log.localizationRoiDrawing") + ": " + camera.id);
}

bool MainWindow::isBwDimensionalCamera(const CameraConfig& camera) const
{
  return camera.profile.imageMode == "bw" &&
    camera.profile.inspectionTypes.contains("dimensional");
}

void MainWindow::clearToolPanel()
{
  while (QLayoutItem* item = m_toolsLayout->takeAt(0))
  {
    if (QWidget* widget = item->widget())
    {
      widget->deleteLater();
    }

    delete item;
  }
}

void MainWindow::appendLog(const QString& message)
{
  const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
  m_log->append(QString("[%1] %2").arg(timestamp, message));
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

QPixmap MainWindow::loadCameraPreview(const CameraConfig& camera) const
{
  const QString imagePath = firstImageInFolder(camera.folder);

  if (imagePath.isEmpty())
  {
    return {};
  }

  return QPixmap(imagePath);
}

QString MainWindow::firstImageInFolder(const QString& folder) const
{
  QDir directory(projectPath(folder));
  const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tif", "*.tiff"};
  const QFileInfoList files = directory.entryInfoList(filters, QDir::Files, QDir::Name);

  if (files.isEmpty())
  {
    return {};
  }

  return files.first().absoluteFilePath();
}
