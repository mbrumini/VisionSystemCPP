#include "MainWindow.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

namespace
{
QString projectPath(const QString& relativePath)
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(relativePath);
}

QString toolLabel(const QString& toolId)
{
  static const QHash<QString, QString> labels = {
    {"measurements", "Misure"},
    {"threshold", "Soglia"},
    {"calibration", "Calibrazione"},
    {"roi", "ROI"},
    {"saveSample", "Salva campione"},
    {"dimensions", "Dimensioni"},
    {"tolerances", "Tolleranze"},
    {"surfaceDefects", "Difetti superficie"},
    {"lighting", "Illuminazione"},
    {"contrast", "Contrasto"},
    {"defectMap", "Mappa difetti"},
    {"aiModel", "Modello AI"},
    {"confidence", "Confidenza"},
    {"classes", "Classi"},
    {"datasetCapture", "Cattura dataset"}
  };

  return labels.value(toolId, toolId);
}
}

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
{
  buildUi();
  loadConfiguration();
}

void MainWindow::buildUi()
{
  setWindowTitle("VisionSystemCPP");
  resize(1600, 900);

  auto* root = new QWidget(this);
  auto* rootLayout = new QHBoxLayout(root);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  auto* splitter = new QSplitter(Qt::Horizontal, root);
  splitter->setChildrenCollapsible(false);

  m_imageStack = new QStackedWidget(splitter);
  m_gridPage = new QWidget(m_imageStack);

  auto* gridWrapper = new QWidget(m_gridPage);
  auto* gridLayout = new QGridLayout(gridWrapper);
  gridLayout->setContentsMargins(12, 12, 12, 12);
  gridLayout->setSpacing(10);

  auto* gridPageLayout = new QVBoxLayout(m_gridPage);
  gridPageLayout->setContentsMargins(0, 0, 0, 0);
  gridPageLayout->addWidget(gridWrapper);
  m_imageStack->addWidget(m_gridPage);

  auto* largePage = new QWidget(m_imageStack);
  auto* largeLayout = new QVBoxLayout(largePage);
  largeLayout->setContentsMargins(16, 16, 16, 16);
  largeLayout->setSpacing(10);

  m_largeTitle = new QLabel("Nessuna camera selezionata", largePage);
  m_largeTitle->setObjectName("largeTitle");
  m_largeImage = new QLabel(largePage);
  m_largeImage->setAlignment(Qt::AlignCenter);
  m_largeImage->setStyleSheet("background:#101418;color:#9aa4ad;");
  m_largeImage->setMinimumSize(800, 600);

  largeLayout->addWidget(m_largeTitle);
  largeLayout->addWidget(m_largeImage, 1);
  m_imageStack->addWidget(largePage);

  auto* panel = new QWidget(splitter);
  panel->setMinimumWidth(340);
  panel->setMaximumWidth(460);
  auto* panelLayout = new QVBoxLayout(panel);
  panelLayout->setContentsMargins(14, 14, 14, 14);
  panelLayout->setSpacing(12);

  m_systemStatus = new QLabel("Sistema pronto", panel);
  m_systemStatus->setObjectName("panelStatus");
  panelLayout->addWidget(m_systemStatus);

  auto* commands = new QGroupBox("Comandi generali", panel);
  auto* commandsLayout = new QGridLayout(commands);
  const QStringList commandNames = {"Start", "Stop", "Reset errori", "Reload config", "Vista griglia", "Esci"};

  for (int i = 0; i < commandNames.size(); ++i)
  {
    auto* button = new QPushButton(commandNames[i], commands);
    commandsLayout->addWidget(button, i / 2, i % 2);

    if (commandNames[i] == "Vista griglia")
    {
      connect(button, &QPushButton::clicked, this, [this]() { showGridView(); });
    }
    else if (commandNames[i] == "Reload config")
    {
      connect(button, &QPushButton::clicked, this, [this]() { loadConfiguration(); });
    }
    else if (commandNames[i] == "Esci")
    {
      connect(button, &QPushButton::clicked, qApp, &QApplication::quit);
    }
    else
    {
      connect(button, &QPushButton::clicked, this, [this, name = commandNames[i]]() {
        appendLog("Comando: " + name);
      });
    }
  }

  panelLayout->addWidget(commands);

  auto* cameraBox = new QGroupBox("Camera selezionata", panel);
  auto* cameraLayout = new QVBoxLayout(cameraBox);
  m_cameraDetails = new QLabel("Seleziona una miniatura", cameraBox);
  m_cameraDetails->setWordWrap(true);
  cameraLayout->addWidget(m_cameraDetails);
  panelLayout->addWidget(cameraBox);

  auto* toolsBox = new QGroupBox("Tool camera", panel);
  m_toolsLayout = new QVBoxLayout(toolsBox);
  m_toolsContainer = toolsBox;
  panelLayout->addWidget(toolsBox);

  auto* logBox = new QGroupBox("Log eventi", panel);
  auto* logLayout = new QVBoxLayout(logBox);
  m_log = new QTextEdit(logBox);
  m_log->setReadOnly(true);
  m_log->setMinimumHeight(160);
  logLayout->addWidget(m_log);
  panelLayout->addWidget(logBox, 1);

  splitter->addWidget(m_imageStack);
  splitter->addWidget(panel);
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
  );
}

void MainWindow::loadConfiguration()
{
  QString error;
  const QString configPath = projectPath("config/cameras.json");

  if (!m_config.load(configPath, &error))
  {
    m_systemStatus->setText("Errore configurazione");
    appendLog(error);
    return;
  }

  auto* gridWrapper = m_gridPage->layout()->itemAt(0)->widget();
  auto* gridLayout = qobject_cast<QGridLayout*>(gridWrapper->layout());

  while (QLayoutItem* item = gridLayout->takeAt(0))
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
    auto* tile = new CameraTileWidget(cameras[i], gridWrapper);
    tile->setPreview(loadCameraPreview(cameras[i]));
    tile->setClickHandler([this](const CameraConfig& camera) { selectCamera(camera); });
    gridLayout->addWidget(tile, i / 4, i % 4);
    m_tiles.append(tile);
  }

  m_systemStatus->setText(QString("Sistema pronto | %1 camere attive").arg(cameras.size()));
  appendLog(QString("Configurazione caricata: %1 camere attive su massimo %2")
              .arg(cameras.size())
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
  appendLog("Vista griglia");
}

void MainWindow::selectCamera(const CameraConfig& camera)
{
  m_selectedCameraId = camera.id;

  for (CameraTileWidget* tile : m_tiles)
  {
    tile->setSelected(tile->camera().id == camera.id);
  }

  const QPixmap preview = loadCameraPreview(camera);
  m_largeTitle->setText(camera.displayName + " | " + camera.id);

  if (preview.isNull())
  {
    m_largeImage->setText("NO IMAGE");
  }
  else
  {
    m_largeImage->setPixmap(preview.scaled(m_largeImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }

  m_imageStack->setCurrentIndex(1);
  updateControlPanel(&camera);
  appendLog("Camera selezionata: " + camera.id);
}

void MainWindow::updateControlPanel(const CameraConfig* camera)
{
  while (QLayoutItem* item = m_toolsLayout->takeAt(0))
  {
    if (QWidget* widget = item->widget())
    {
      widget->deleteLater();
    }

    delete item;
  }

  if (!camera)
  {
    m_cameraDetails->setText("Seleziona una miniatura");
    m_toolsLayout->addWidget(new QLabel("Nessun tool selezionato", m_toolsContainer));
    m_toolsLayout->addStretch(1);
    return;
  }

  m_cameraDetails->setText(
    QString("Camera %1 | %2\nProfilo: %3\nModalita': %4\nControlli: %5\nSorgente: %6")
      .arg(camera->slot)
      .arg(camera->displayName)
      .arg(camera->processingProfileId)
      .arg(camera->profile.imageMode)
      .arg(camera->profile.inspectionTypes.join(", "))
      .arg(camera->folder));

  for (const QString& tool : camera->profile.guiTools)
  {
    auto* button = new QPushButton(toolLabel(tool), m_toolsContainer);
    connect(button, &QPushButton::clicked, this, [this, tool]() {
      appendLog("Tool selezionato: " + toolLabel(tool));
    });
    m_toolsLayout->addWidget(button);
  }

  m_toolsLayout->addStretch(1);
}

void MainWindow::appendLog(const QString& message)
{
  const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
  m_log->append(QString("[%1] %2").arg(timestamp, message));
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
