#include "TestVisionWindow.h"

#include "CampaignEditorDialog.h"
#include "TestVisionArtifacts.h"
#include "TestVisionJson.h"
#include "TestVisionMath.h"
#include "TestVisionSyntheticImages.h"
#include "TestVisionStyle.h"
#include "simulator/SimulatorProtocol.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTime>
#include <QVBoxLayout>

#define NOMINMAX
#include <Windows.h>

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <vector>

namespace
{
const wchar_t* kPipePath = L"\\\\.\\pipe\\VisionSystemSimulator";
}


TestVisionWindow::TestVisionWindow(const QString& scenarioPath, QWidget* parent)
  : QMainWindow(parent)
  , m_scenarioPath(scenarioPath)
{
  m_analysisRefreshTimer.setSingleShot(true);
  m_analysisRefreshTimer.setInterval(200);
  connect(&m_analysisRefreshTimer, &QTimer::timeout, this, [this]() {
    refreshAnalysis(false);
  });

  setWindowTitle("VisionSystem TestVision");
  resize(1280, 900);
  TestVisionStyle::apply(this);
  buildUi();

  QString error;
  if (!loadScenario(&error))
  {
    QMessageBox::critical(this, "TestVision", error);
    m_startButton->setEnabled(false);
  }
}

TestVisionWindow::~TestVisionWindow()
{
  closePipe();
}

void TestVisionWindow::setSendOnlyMode(bool enabled)
{
  m_sendOnlyMode = enabled;
  if (m_sendOnlyCheck)
  {
    m_sendOnlyCheck->setChecked(enabled);
  }
}

bool TestVisionWindow::sendOnlyMode() const
{
  return m_sendOnlyMode;
}

void TestVisionWindow::startCampaignWorker(
  const QJsonObject& item,
  int cycle,
  std::function<void(const QJsonObject&)> completion,
  std::function<void(const QString&, const TestResult&)> resultCallback)
{
  const QString cameraId = item.value("cameraId").toString("CAM01");
  const QString baseScenarioName = m_scenario.value("name").toString(
    QFileInfo(m_scenarioPath).completeBaseName());
  m_scenario["name"] = baseScenarioName + "_" + cameraId;
  m_scenario["output"] = QString("../reports/%1_%2.json")
    .arg(QFileInfo(m_scenarioPath).completeBaseName(), cameraId);
  m_workerItem = item;
  m_workerCycle = cycle;
  m_workerCompletion = std::move(completion);
  m_workerResultCallback = std::move(resultCallback);
  m_isCampaignWorker = true;
  applyCampaignItem(item);
  QTimer::singleShot(0, this, [this]() { startTest(); });
}

bool TestVisionWindow::loadCampaignFromFile(
  const QString& path,
  QString* error,
  bool autoStart)
{
  const QJsonObject campaign = testVisionLoadJson(path, error);
  const QJsonArray items = campaign.value("items").toArray();
  if (campaign.isEmpty() || items.isEmpty())
  {
    if (error && error->isEmpty())
    {
      *error = "La campagna non contiene prove.";
    }
    return false;
  }
  m_campaignPath = path;
  m_campaign = campaign;
  m_campaignItems = items;
  m_campaignCycles = qMax(1, campaign.value("cycles").toInt(1));
  if (campaign.contains("executionProfile"))
  {
    applyExecutionProfile(testVisionExecutionProfileFromJson(
      campaign.value("executionProfile").toObject()));
  }
  m_startCampaignButton->setEnabled(true);
  appendLog(QString("Campagna caricata: %1 | prove=%2 | cicli=%3 | multicamera=%4")
    .arg(campaign.value("name").toString(QFileInfo(path).completeBaseName()))
    .arg(items.size())
    .arg(m_campaignCycles)
    .arg(campaign.value("parallel").toBool(true) ? "si" : "no"));
  if (autoStart)
  {
    QTimer::singleShot(0, this, [this]() { startCampaign(); });
  }
  return true;
}

void TestVisionWindow::startBroadcastFromCameras(const QStringList& cameras)
{
  if (!m_cameraTargets)
  {
    return;
  }
  QMap<QString, QString> requested;
  for (const QString& configuredTarget : cameras)
  {
    const QStringList parts = configuredTarget.split(':');
    const QString cameraId = parts.value(0).trimmed().toUpper();
    const QString strategyId = parts.value(1).trimmed();
    requested.insert(cameraId, strategyId);
  }
  for (int row = 0; row < m_cameraTargets->rowCount(); ++row)
  {
    QTableWidgetItem* enabled = m_cameraTargets->item(row, 0);
    QTableWidgetItem* camera = m_cameraTargets->item(row, 1);
    const QString cameraId = camera->data(Qt::UserRole).toString();
    enabled->setCheckState(
      requested.contains(cameraId)
        ? Qt::Checked
        : Qt::Unchecked);
    if (requested.contains(cameraId) && !requested.value(cameraId).isEmpty())
    {
      auto* strategy = qobject_cast<QComboBox*>(
        m_cameraTargets->cellWidget(row, 2));
      const int strategyIndex = strategy
        ? strategy->findData(requested.value(cameraId))
        : -1;
      if (strategyIndex >= 0)
      {
        strategy->setCurrentIndex(strategyIndex);
      }
    }
  }
  QTimer::singleShot(0, this, [this]() { startTest(); });
}

QDoubleSpinBox* TestVisionWindow::doubleSpin(double value, double minimum, double maximum, double step, int decimals)
  {
    auto* spin = new QDoubleSpinBox(this);
    spin->setRange(minimum, maximum);
    spin->setDecimals(decimals);
    spin->setSingleStep(step);
    spin->setValue(value);
    return spin;
  }

void TestVisionWindow::buildUi()
  {
    auto* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    setCentralWidget(tabs);

    auto* execution = new QWidget(tabs);
    auto* executionLayout = new QVBoxLayout(execution);
    executionLayout->setContentsMargins(10, 10, 10, 10);
    executionLayout->setSpacing(10);

    auto* header = new QWidget(execution);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    auto* titleBlock = new QVBoxLayout();
    auto* title = new QLabel("TestVision", header);
    title->setObjectName("appTitle");
    auto* subtitle = new QLabel("Validazione simulatore · pose · misure · campagne", header);
    subtitle->setObjectName("appSubtitle");
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);
    headerLayout->addLayout(titleBlock, 1);
    m_stateLabel = new QLabel("Pronto", header);
    m_stateLabel->setObjectName("statusBadge");
    m_stateLabel->setProperty("state", "idle");
    m_stateLabel->setMinimumWidth(280);
    m_stateLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(m_stateLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    executionLayout->addWidget(header);

    auto* splitter = new QSplitter(Qt::Horizontal, execution);

    auto* previewPanel = new QWidget(splitter);
    auto* previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    auto* previewTitle = new QLabel("Anteprima frame", previewPanel);
    previewTitle->setObjectName("sectionHint");
    previewLayout->addWidget(previewTitle);
    m_preview = new QLabel(previewPanel);
    m_preview->setObjectName("imagePreview");
    m_preview->setMinimumSize(320, 240);
    m_preview->setMaximumSize(520, 390);
    m_preview->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_preview, 0, Qt::AlignHCenter);

    auto* statusGroup = new QGroupBox("Stato esecuzione", previewPanel);
    auto* statusForm = new QFormLayout(statusGroup);
    statusForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_scenarioLabel = new QLabel("-", statusGroup);
    m_scenarioLabel->setWordWrap(true);
    m_frameLabel = new QLabel("-", statusGroup);
    m_expectedLabel = new QLabel("-", statusGroup);
    m_expectedLabel->setWordWrap(true);
    m_actualLabel = new QLabel("-", statusGroup);
    m_actualLabel->setWordWrap(true);
    statusForm->addRow("Scenario", m_scenarioLabel);
    statusForm->addRow("Frame", m_frameLabel);
    statusForm->addRow("Atteso", m_expectedLabel);
    statusForm->addRow("Rilevato", m_actualLabel);
    previewLayout->addWidget(statusGroup);

    auto* configScroll = new QScrollArea(splitter);
    configScroll->setWidgetResizable(true);
    configScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* configPanel = new QWidget(configScroll);
    configScroll->setWidget(configPanel);
    auto* configLayout = new QVBoxLayout(configPanel);
    configLayout->setContentsMargins(4, 4, 8, 4);
    configLayout->setSpacing(8);

    auto* broadcast = new QWidget(tabs);
    auto* broadcastLayout = new QVBoxLayout(broadcast);
    broadcastLayout->setContentsMargins(10, 10, 10, 10);
    m_xMin = doubleSpin(0.0, -1000.0, 1000.0, 0.5);
    m_xMax = doubleSpin(0.0, -1000.0, 1000.0, 0.5);
    m_xStep = doubleSpin(1.0, 0.001, 1000.0, 0.5);
    m_yMin = doubleSpin(0.0, -1000.0, 1000.0, 0.5);
    m_yMax = doubleSpin(0.0, -1000.0, 1000.0, 0.5);
    m_yStep = doubleSpin(1.0, 0.001, 1000.0, 0.5);
    m_angleMin = doubleSpin(0.0, -360.0, 360.0, 5.0);
    m_angleMax = doubleSpin(45.0, -360.0, 360.0, 5.0);
    m_angleStep = doubleSpin(15.0, 0.001, 360.0, 5.0);
    m_pixelSize = doubleSpin(0.0654, 0.000001, 1000.0, 0.001, 6);
    m_resolutionCombo = new QComboBox(configPanel);
    m_resolutionCombo->addItem("640 × 480", QVariant::fromValue(QSize(640, 480)));
    m_resolutionCombo->addItem("1280 × 720", QVariant::fromValue(QSize(1280, 720)));
    m_resolutionCombo->addItem("1920 × 1080", QVariant::fromValue(QSize(1920, 1080)));
    m_resolutionCombo->addItem("2048 × 1536", QVariant::fromValue(QSize(2048, 1536)));
    m_resolutionCombo->addItem("Personalizzata", QVariant::fromValue(QSize(-1, -1)));
    m_canvasWidthSpin = new QSpinBox(configPanel);
    m_canvasWidthSpin->setRange(64, 8192);
    m_canvasWidthSpin->setSingleStep(16);
    m_canvasWidthSpin->setValue(640);
    m_canvasHeightSpin = new QSpinBox(configPanel);
    m_canvasHeightSpin->setRange(64, 8192);
    m_canvasHeightSpin->setSingleStep(16);
    m_canvasHeightSpin->setValue(480);
    m_canvasSizeLabel = new QLabel(configPanel);
    m_canvasSizeLabel->setWordWrap(true);
    m_shapeCombo = new QComboBox(configPanel);
    m_shapeCombo->addItem("Croce asimmetrica", "cross");
    m_shapeCombo->addItem("Rettangolo asimmetrico", "rectangle");
    m_shapeCombo->addItem("Cerchi concentrici", "circles");
    m_shapeCombo->addItem("Piastra con fori", "plate");
    m_shapeCombo->addItem("Profilo a L", "l_profile");
    m_shapeCombo->addItem("Gambo vite", "screw_shank");
    m_shapeCombo->addItem("Stress tutti i tool", "stress_all_tools");
    m_shapeCombo->addItem("Ruota dentata", "gear");
    m_strategyCombo = new QComboBox(configPanel);
    m_strategyCombo->addItem("Massa / PCA", "massPca");
    m_strategyCombo->addItem("Edge / PCA", "edgePca");
    m_strategyCombo->addItem("Corona a soglia", "threshold");
    m_strategyCombo->addItem("Corona edge", "edge");
    m_strategyCombo->addItem("Due cerchi / asse", "two_circles_axis");
    m_strategyCombo->addItem("Shape model", "shapeModel");
    m_strategyCombo->addItem("Template model", "templateModel");
    m_strategyCombo->addItem("Localizzazione AI YOLO", "aiYolo");
    m_recipeCombo = new QComboBox(configPanel);
    m_cameraCombo = new QComboBox(configPanel);
    m_cameraCombo->addItem("Selezionata in Vision", "SELECTED");
    for (int i = 1; i <= 16; ++i)
    {
      const QString camId = QString("CAM%1").arg(i, 2, 10, QChar('0'));
      m_cameraCombo->addItem(camId, camId);
    }
    m_cameraTargets = new QTableWidget(16, 3, broadcast);
    m_cameraTargets->setHorizontalHeaderLabels({"Usa", "Camera", "Strategia"});
    m_cameraTargets->verticalHeader()->hide();
    m_cameraTargets->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_cameraTargets->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_cameraTargets->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_cameraTargets->setMinimumHeight(430);
    for (int i = 1; i <= 16; ++i)
    {
      const QString camId = QString("CAM%1").arg(i, 2, 10, QChar('0'));
      auto* enabled = new QTableWidgetItem();
      enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      enabled->setCheckState(Qt::Unchecked);
      m_cameraTargets->setItem(i - 1, 0, enabled);
      auto* camera = new QTableWidgetItem(camId);
      camera->setData(Qt::UserRole, camId);
      camera->setFlags(Qt::ItemIsEnabled);
      m_cameraTargets->setItem(i - 1, 1, camera);
      auto* strategy = new QComboBox(m_cameraTargets);
      for (int index = 0; index < m_strategyCombo->count(); ++index)
      {
        strategy->addItem(
          m_strategyCombo->itemText(index),
          m_strategyCombo->itemData(index));
      }
      m_cameraTargets->setCellWidget(i - 1, 2, strategy);
    }
    m_passes = new QSpinBox(configPanel);
    m_passes->setRange(1, 1000);
    m_passes->setValue(3);
    m_intervalMs = new QSpinBox(configPanel);
    m_intervalMs->setRange(0, 60000);
    m_intervalMs->setValue(200);

    auto* imageGroup = new QGroupBox("Immagine sintetica", configPanel);
    auto* imageForm = new QFormLayout(imageGroup);
    imageForm->addRow("Shape campione", m_shapeCombo);
    imageForm->addRow("Risoluzione", m_resolutionCombo);
    imageForm->addRow("Larghezza (px)", m_canvasWidthSpin);
    imageForm->addRow("Altezza (px)", m_canvasHeightSpin);
    imageForm->addRow("", m_canvasSizeLabel);

    auto* recipeGroup = new QGroupBox("Ricetta e strategia", configPanel);
    auto* recipeForm = new QFormLayout(recipeGroup);
    recipeForm->addRow("Telecamera", m_cameraCombo);
    recipeForm->addRow("Ricetta Vision", m_recipeCombo);
    recipeForm->addRow("Strategia", m_strategyCombo);
    recipeForm->addRow("Scala (mm/px)", m_pixelSize);

    auto* poseGroup = new QGroupBox("Piano pose", configPanel);
    auto* poseForm = new QFormLayout(poseGroup);
    auto* xRow = new QHBoxLayout();
    xRow->addWidget(m_xMin);
    xRow->addWidget(m_xMax);
    xRow->addWidget(m_xStep);
    auto* yRow = new QHBoxLayout();
    yRow->addWidget(m_yMin);
    yRow->addWidget(m_yMax);
    yRow->addWidget(m_yStep);
    auto* angleRow = new QHBoxLayout();
    angleRow->addWidget(m_angleMin);
    angleRow->addWidget(m_angleMax);
    angleRow->addWidget(m_angleStep);
    poseForm->addRow("X min / max / passo (mm)", xRow);
    poseForm->addRow("Y min / max / passo (mm)", yRow);
    poseForm->addRow("Angolo min / max / passo (°)", angleRow);

    auto* runGroup = new QGroupBox("Esecuzione", configPanel);
    auto* runForm = new QFormLayout(runGroup);
    runForm->addRow("Giri completi", m_passes);
    runForm->addRow("Intervallo frame (ms)", m_intervalMs);
    m_sendOnlyCheck = new QCheckBox("Solo invio (raffica, senza attendere risultati)", configPanel);
    m_sendOnlyCheck->setToolTip(
      "Modalità raffica: invia i frame al ritmo configurato senza attendere risultati "
      "né aggiornare tabelle e report.");
    runForm->addRow(m_sendOnlyCheck);

    configLayout->addWidget(imageGroup);
    configLayout->addWidget(recipeGroup);
    configLayout->addWidget(poseGroup);
    configLayout->addWidget(runGroup);

    auto* toolsGroup = new QGroupBox("Profili e campagne", configPanel);
    auto* toolsLayout = new QVBoxLayout(toolsGroup);
    auto* profileButtons = new QHBoxLayout();
    auto* saveScenarioProfileButton = new QPushButton("Salva nello scenario", configPanel);
    saveScenarioProfileButton->setObjectName("secondaryButton");
    auto* saveProfileButton = new QPushButton("Salva profilo...", configPanel);
    saveProfileButton->setObjectName("secondaryButton");
    auto* loadProfileButton = new QPushButton("Carica profilo...", configPanel);
    loadProfileButton->setObjectName("secondaryButton");
    profileButtons->addWidget(saveScenarioProfileButton);
    profileButtons->addWidget(saveProfileButton);
    profileButtons->addWidget(loadProfileButton);
    toolsLayout->addLayout(profileButtons);

    m_totalLabel = new QLabel(configPanel);
    m_totalLabel->setObjectName("planSummary");
    m_totalLabel->setWordWrap(true);
    toolsLayout->addWidget(m_totalLabel);

    auto* buttons = new QHBoxLayout();
    m_sendSampleButton = new QPushButton("Invia campione", configPanel);
    m_sendSampleButton->setObjectName("secondaryButton");
    m_generateDatasetButton = new QPushButton("Dataset AI", configPanel);
    m_generateDatasetButton->setObjectName("secondaryButton");
    m_loadCampaignButton = new QPushButton("Carica campagna", configPanel);
    m_loadCampaignButton->setObjectName("secondaryButton");
    m_editCampaignButton = new QPushButton("Modifica campagna", configPanel);
    m_editCampaignButton->setObjectName("secondaryButton");
    m_startCampaignButton = new QPushButton("Avvia campagna", configPanel);
    m_startCampaignButton->setObjectName("secondaryButton");
    m_startCampaignButton->setEnabled(false);
    m_startButton = new QPushButton("Avvia test", configPanel);
    m_startButton->setObjectName("primaryButton");
    m_stopButton = new QPushButton("Stop", configPanel);
    m_stopButton->setObjectName("dangerButton");
    m_stopButton->setEnabled(false);
    buttons->addWidget(m_sendSampleButton);
    buttons->addWidget(m_generateDatasetButton);
    buttons->addWidget(m_loadCampaignButton);
    buttons->addWidget(m_editCampaignButton);
    buttons->addWidget(m_startCampaignButton);
    buttons->addStretch(1);
    buttons->addWidget(m_startButton);
    buttons->addWidget(m_stopButton);
    toolsLayout->addLayout(buttons);
    configLayout->addWidget(toolsGroup);
    configLayout->addStretch(1);

    m_broadcastTable = new QTableWidget(broadcast);
    m_broadcastTable->setColumnCount(9);
    m_broadcastTable->setHorizontalHeaderLabels({
      "Camera", "Frame", "Valida", "X", "Y", "Angolo",
      "Err. centro", "Err. angolo", "ms"
    });
    m_broadcastTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_broadcastTable->setMinimumHeight(220);
    m_broadcastTable->hide();

    splitter->addWidget(previewPanel);
    splitter->addWidget(configScroll);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    executionLayout->addWidget(splitter, 1);

    m_log = new QPlainTextEdit(execution);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(140);
    m_log->setPlaceholderText("Log connessione e frame...");
    executionLayout->addWidget(m_log);
    tabs->addTab(execution, "Esecuzione");

    auto* broadcastTitle = new QLabel(
      "Seleziona le telecamere e assegna a ciascuna la propria strategia.",
      broadcast);
    broadcastTitle->setObjectName("sectionHint");
    broadcastTitle->setWordWrap(true);
    broadcastLayout->addWidget(broadcastTitle);
    broadcastLayout->addWidget(m_cameraTargets, 3);
    auto* broadcastButtons = new QHBoxLayout();
    auto* startBroadcastButton = new QPushButton("Avvia broadcast", broadcast);
    startBroadcastButton->setObjectName("primaryButton");
    auto* stopBroadcastButton = new QPushButton("Ferma broadcast", broadcast);
    stopBroadcastButton->setObjectName("dangerButton");
    broadcastButtons->addStretch(1);
    broadcastButtons->addWidget(startBroadcastButton);
    broadcastButtons->addWidget(stopBroadcastButton);
    broadcastLayout->addLayout(broadcastButtons);
    auto* sendOnlyBroadcast = new QCheckBox(
      "Solo invio immagini (non attendere risultati)", broadcast);
    sendOnlyBroadcast->setToolTip(m_sendOnlyCheck->toolTip());
    sendOnlyBroadcast->setChecked(m_sendOnlyCheck->isChecked());
    connect(sendOnlyBroadcast, &QCheckBox::toggled, m_sendOnlyCheck, &QCheckBox::setChecked);
    connect(m_sendOnlyCheck, &QCheckBox::toggled, sendOnlyBroadcast, &QCheckBox::setChecked);
    broadcastLayout->addWidget(sendOnlyBroadcast);
    auto* broadcastResultsTitle = new QLabel("Risultati sincronizzati", broadcast);
    broadcastResultsTitle->setObjectName("sectionHint");
    broadcastLayout->addWidget(broadcastResultsTitle);
    broadcastLayout->addWidget(m_broadcastTable, 2);
    tabs->addTab(broadcast, "Multicamera");

    auto* analysis = new QWidget(tabs);
    auto* analysisLayout = new QVBoxLayout(analysis);
    m_summaryLabel = new QLabel("Nessun dato", analysis);
    m_summaryLabel->setObjectName("planSummary");
    m_summaryLabel->setWordWrap(true);
    analysisLayout->addWidget(m_summaryLabel);
    m_table = new QTableWidget(analysis);
    m_table->setColumnCount(14);
    m_table->setHorizontalHeaderLabels({
      "Posa", "Giro", "X mm", "Y mm", "A °",
      "X att.", "Y att.", "A att.",
      "X ril.", "Y ril.", "A ril.",
      "Err. centro", "Err. angolo", "ms"
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setAlternatingRowColors(true);
    analysisLayout->addWidget(m_table, 1);
    auto* plots = new QSplitter(Qt::Horizontal, analysis);
    m_anglePlot = new PlotWidget("Angolo atteso → rilevato", PlotWidget::Mode::Angle, plots);
    m_centerPlot = new PlotWidget("Errore centro rispetto all'angolo", PlotWidget::Mode::CenterError, plots);
    plots->addWidget(m_anglePlot);
    plots->addWidget(m_centerPlot);
    analysisLayout->addWidget(plots);
    m_repeatabilityPlot = new RepeatabilityPlotWidget(analysis);
    analysisLayout->addWidget(m_repeatabilityPlot);
    tabs->addTab(analysis, "Analisi posa");

    auto* measurementAnalysis = new QWidget(tabs);
    auto* measurementLayout = new QVBoxLayout(measurementAnalysis);
    m_measurementSummaryLabel = new QLabel(
      "Misure rigide: confronto vs frame zero (baseline) e vs nominali ricetta (incl. filetto).",
      measurementAnalysis);
    m_measurementSummaryLabel->setObjectName("sectionHint");
    m_measurementSummaryLabel->setWordWrap(true);
    measurementLayout->addWidget(m_measurementSummaryLabel);
    m_measurementTable = new QTableWidget(measurementAnalysis);
    m_measurementTable->setColumnCount(13);
    m_measurementTable->setHorizontalHeaderLabels({
      "Posa", "Giro", "Angolo°", "Misura", "Tipo",
      "Valore px", "Baseline px", "Δ baseline px",
      "Ricetta px", "Δ ricetta px",
      "Err. centro", "Err. angolo", "ms"
    });
    m_measurementTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_measurementTable->setAlternatingRowColors(true);
    measurementLayout->addWidget(m_measurementTable, 1);
    m_measurementPlot = new MeasurementStabilityPlotWidget(measurementAnalysis);
    measurementLayout->addWidget(m_measurementPlot);
    tabs->addTab(measurementAnalysis, "Analisi misure");

    auto* multiAnalysis = new QWidget(tabs);
    auto* multiAnalysisLayout = new QVBoxLayout(multiAnalysis);
    m_multiSummaryTable = new QTableWidget(multiAnalysis);
    m_multiSummaryTable->setColumnCount(10);
    m_multiSummaryTable->setHorizontalHeaderLabels({
      "Camera", "Frame", "Validi", "Centro medio", "Centro max",
      "Angolo medio", "Angolo max", "Tempo medio", "Tempo max", "Stato"
    });
    m_multiSummaryTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
    m_multiSummaryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_multiSummaryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_multiSummaryTable->setMaximumHeight(260);
    multiAnalysisLayout->addWidget(m_multiSummaryTable);

    auto* multiSelectors = new QHBoxLayout();
    m_multiPrimaryCamera = new QComboBox(multiAnalysis);
    m_multiCompareCamera = new QComboBox(multiAnalysis);
    m_multiCompareCamera->addItem("Nessun confronto", "");
    multiSelectors->addWidget(new QLabel("Telecamera da analizzare", multiAnalysis));
    multiSelectors->addWidget(m_multiPrimaryCamera, 1);
    multiSelectors->addWidget(new QLabel("Confronta con", multiAnalysis));
    multiSelectors->addWidget(m_multiCompareCamera, 1);
    multiAnalysisLayout->addLayout(multiSelectors);

    auto* multiPlots = new QSplitter(Qt::Horizontal, multiAnalysis);
    auto* primaryGroup = new QGroupBox("Dettaglio telecamera", multiPlots);
    auto* primaryLayout = new QVBoxLayout(primaryGroup);
    m_multiPrimarySummary = new QLabel("Nessun dato", primaryGroup);
    m_multiPrimarySummary->setObjectName("planSummary");
    m_multiPrimaryAnglePlot = new PlotWidget(
      "Angolo atteso → rilevato", PlotWidget::Mode::Angle, primaryGroup);
    m_multiPrimaryCenterPlot = new PlotWidget(
      "Errore centro rispetto all'angolo", PlotWidget::Mode::CenterError, primaryGroup);
    primaryLayout->addWidget(m_multiPrimarySummary);
    primaryLayout->addWidget(m_multiPrimaryAnglePlot);
    primaryLayout->addWidget(m_multiPrimaryCenterPlot);

    m_multiCompareGroup = new QGroupBox("Confronto", multiPlots);
    auto* compareLayout = new QVBoxLayout(m_multiCompareGroup);
    m_multiCompareSummary = new QLabel("Nessun confronto", m_multiCompareGroup);
    m_multiCompareAnglePlot = new PlotWidget(
      "Angolo atteso → rilevato", PlotWidget::Mode::Angle, m_multiCompareGroup);
    m_multiCompareCenterPlot = new PlotWidget(
      "Errore centro rispetto all'angolo", PlotWidget::Mode::CenterError, m_multiCompareGroup);
    compareLayout->addWidget(m_multiCompareSummary);
    compareLayout->addWidget(m_multiCompareAnglePlot);
    compareLayout->addWidget(m_multiCompareCenterPlot);
    m_multiCompareGroup->hide();
    multiPlots->addWidget(primaryGroup);
    multiPlots->addWidget(m_multiCompareGroup);
    multiAnalysisLayout->addWidget(multiPlots, 1);
    tabs->addTab(multiAnalysis, "Analisi multicamera");

    connect(m_startButton, &QPushButton::clicked, this, [this]() { startTest(); });
    connect(startBroadcastButton, &QPushButton::clicked, this, [this]() { startTest(); });
    connect(stopBroadcastButton, &QPushButton::clicked, this, [this]() {
      const QList<TestVisionWindow*> workers = m_broadcastWorkers;
      for (TestVisionWindow* worker : workers)
      {
        if (worker)
        {
          worker->stopTest("Broadcast arrestato dall'utente");
        }
      }
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
      if (m_broadcastRunning)
      {
        const QList<TestVisionWindow*> workers = m_broadcastWorkers;
        for (TestVisionWindow* worker : workers)
        {
          if (worker)
          {
            worker->stopTest("Broadcast arrestato dall'utente");
          }
        }
        return;
      }
      stopTest("Arrestato dall'utente");
    });
    connect(m_sendSampleButton, &QPushButton::clicked, this, [this]() { sendSample(); });
    connect(m_generateDatasetButton, &QPushButton::clicked, this, [this]() { generateAiDataset(); });
    connect(m_loadCampaignButton, &QPushButton::clicked, this, [this]() { loadCampaign(); });
    connect(m_editCampaignButton, &QPushButton::clicked, this, [this]() { editCampaign(); });
    connect(m_startCampaignButton, &QPushButton::clicked, this, [this]() { startCampaign(); });
    connect(saveScenarioProfileButton, &QPushButton::clicked, this, [this]() {
      saveExecutionProfileToScenario();
    });
    connect(saveProfileButton, &QPushButton::clicked, this, [this]() {
      saveExecutionProfileToFile();
    });
    connect(loadProfileButton, &QPushButton::clicked, this, [this]() {
      loadExecutionProfileFromFile();
    });
    connect(m_shapeCombo, &QComboBox::currentIndexChanged, this, [this]() { updateMasterShape(false); });
    connect(m_resolutionCombo, &QComboBox::currentIndexChanged, this, [this]() {
      applySelectedResolutionPreset();
    });
    connect(m_canvasWidthSpin, &QSpinBox::valueChanged, this, [this](int) {
      onCanvasSpinboxChanged();
    });
    connect(m_canvasHeightSpin, &QSpinBox::valueChanged, this, [this](int) {
      onCanvasSpinboxChanged();
    });
    connect(m_recipeCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
      m_expectedRecipeId = text.trimmed();
      updateScenarioLabel();
      syncStrategyFromRecipe();
    });
    connect(m_cameraCombo, &QComboBox::currentIndexChanged, this, [this]() {
      if (!m_cameraTargets)
      {
        return;
      }
      const QString cameraId = m_cameraCombo->currentData().toString();
      if (cameraId == "SELECTED")
      {
        return;
      }
      for (int row = 0; row < m_cameraTargets->rowCount(); ++row)
      {
        if (m_cameraTargets->item(row, 1)->data(Qt::UserRole).toString() == cameraId)
        {
          m_cameraTargets->item(row, 0)->setCheckState(Qt::Checked);
          break;
        }
      }
    });
    connect(m_multiSummaryTable, &QTableWidget::cellClicked, this, [this](int row, int) {
      if (row < 0 || row >= m_multiSummaryTable->rowCount())
      {
        return;
      }
      const QString cameraId = m_multiSummaryTable->item(row, 0)->text();
      const int index = m_multiPrimaryCamera->findData(cameraId);
      if (index >= 0)
      {
        m_multiPrimaryCamera->setCurrentIndex(index);
      }
    });
    connect(m_multiPrimaryCamera, &QComboBox::currentIndexChanged, this, [this]() {
      refreshBroadcastAnalysisSelection();
    });
    connect(m_multiCompareCamera, &QComboBox::currentIndexChanged, this, [this]() {
      refreshBroadcastAnalysisSelection();
    });
    for (QDoubleSpinBox* spin : {
           m_xMin, m_xMax, m_xStep, m_yMin, m_yMax, m_yStep,
           m_angleMin, m_angleMax, m_angleStep, m_pixelSize})
    {
      connect(spin, &QDoubleSpinBox::valueChanged, this, [this]() { updatePlanCount(); });
    }
    connect(m_passes, &QSpinBox::valueChanged, this, [this]() { updatePlanCount(); });
    m_pollTimer.setInterval(35);
    connect(&m_pollTimer, &QTimer::timeout, this, [this]() { pollPipe(); });
    m_sendTimer.setSingleShot(true);
    connect(&m_sendTimer, &QTimer::timeout, this, [this]() { sendNextFrame(); });
  }

TestVisionExecutionProfile TestVisionWindow::currentExecutionProfile() const
  {
    TestVisionExecutionProfile profile;
    if (!m_shapeCombo)
    {
      return profile;
    }

    profile.shapeId = m_shapeCombo->currentData().toString();
    profile.strategyId = m_strategyCombo->currentData().toString();
    profile.recipeId = m_recipeCombo->currentText().trimmed();
    profile.cameraId = m_cameraCombo->currentData().toString();
    if (profile.cameraId == "SELECTED")
    {
      profile.cameraId = m_scenario.value("cameraId").toString();
    }
    profile.pixelSizeMm = m_pixelSize->value();
    profile.passes = m_passes->value();
    profile.intervalMs = m_intervalMs->value();
    profile.xMinMm = m_xMin->value();
    profile.xMaxMm = m_xMax->value();
    profile.xStepMm = m_xStep->value();
    profile.yMinMm = m_yMin->value();
    profile.yMaxMm = m_yMax->value();
    profile.yStepMm = m_yStep->value();
    profile.angleMinDeg = m_angleMin->value();
    profile.angleMaxDeg = m_angleMax->value();
    profile.angleStepDeg = m_angleStep->value();
    profile.canvasWidth = m_canvasWidthSpin->value();
    profile.canvasHeight = m_canvasHeightSpin->value();
    profile.canvasBackground = m_canvasBackground;
    profile.frameDeliveryMode = m_sendOnlyCheck && m_sendOnlyCheck->isChecked()
      ? TestVisionFrameDeliveryMode::SendOnly
      : TestVisionFrameDeliveryMode::CollectResults;
    return profile;
  }

void TestVisionWindow::applyExecutionProfile(const TestVisionExecutionProfile& profile)
  {
    if (!m_shapeCombo)
    {
      return;
    }

    const int shapeIndex = m_shapeCombo->findData(profile.shapeId);
    if (shapeIndex >= 0)
    {
      m_shapeCombo->setCurrentIndex(shapeIndex);
    }
    if (!profile.strategyId.isEmpty())
    {
      const int strategyIndex = m_strategyCombo->findData(profile.strategyId);
      if (strategyIndex >= 0)
      {
        m_strategyCombo->setCurrentIndex(strategyIndex);
      }
    }
    if (!profile.recipeId.isEmpty())
    {
      const int recipeIndex = m_recipeCombo->findText(profile.recipeId);
      if (recipeIndex >= 0)
      {
        m_recipeCombo->setCurrentIndex(recipeIndex);
      }
    }
    if (!profile.cameraId.isEmpty())
    {
      const int cameraIndex = m_cameraCombo->findData(profile.cameraId);
      if (cameraIndex >= 0)
      {
        m_cameraCombo->setCurrentIndex(cameraIndex);
      }
      selectBroadcastCamera(profile.cameraId);
    }

    m_pixelSize->setValue(profile.pixelSizeMm);
    m_xMin->setValue(profile.xMinMm);
    m_xMax->setValue(profile.xMaxMm);
    m_xStep->setValue(profile.xStepMm);
    m_yMin->setValue(profile.yMinMm);
    m_yMax->setValue(profile.yMaxMm);
    m_yStep->setValue(profile.yStepMm);
    m_angleMin->setValue(profile.angleMinDeg);
    m_angleMax->setValue(profile.angleMaxDeg);
    m_angleStep->setValue(profile.angleStepDeg);
    m_passes->setValue(qMax(1, profile.passes));
    m_intervalMs->setValue(qMax(0, profile.intervalMs));
    setCanvasSize(profile.canvasWidth, profile.canvasHeight, profile.canvasBackground);
    setSendOnlyMode(profile.frameDeliveryMode == TestVisionFrameDeliveryMode::SendOnly);
    updateMasterShape(false);
    updatePlanCount();
  }

void TestVisionWindow::saveExecutionProfileToScenario()
  {
    QString error;
    if (!testVisionSaveScenarioExecutionProfile(
          m_scenarioPath, currentExecutionProfile(), &error))
    {
      QMessageBox::warning(this, "TestVision", error);
      return;
    }

    m_scenario = testVisionLoadJson(m_scenarioPath, &error);
    appendLog("Impostazioni salvate nello scenario: " + QDir::toNativeSeparators(m_scenarioPath));
    m_stateLabel->setText("Impostazioni salvate nello scenario");
  }

void TestVisionWindow::saveExecutionProfileToFile()
  {
    const QString path = QFileDialog::getSaveFileName(
      this,
      "Salva profilo TestVision",
      QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("tests/profiles/profilo_test.json"),
      "Profili JSON (*.json)");
    if (path.isEmpty())
    {
      return;
    }

    QString error;
    if (!testVisionSaveExecutionProfileFile(path, currentExecutionProfile(), &error))
    {
      QMessageBox::warning(this, "TestVision", error);
      return;
    }

    appendLog("Profilo salvato: " + QDir::toNativeSeparators(path));
    m_stateLabel->setText("Profilo salvato");
  }

void TestVisionWindow::loadExecutionProfileFromFile()
  {
    const QString path = QFileDialog::getOpenFileName(
      this,
      "Carica profilo TestVision",
      QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("tests/profiles"),
      "Profili JSON (*.json)");
    if (path.isEmpty())
    {
      return;
    }

    TestVisionExecutionProfile profile;
    QString error;
    if (!testVisionLoadExecutionProfileFile(path, &profile, &error))
    {
      QMessageBox::warning(this, "TestVision", error);
      return;
    }

    applyExecutionProfile(profile);
    appendLog("Profilo caricato: " + QDir::toNativeSeparators(path));
    m_stateLabel->setText("Profilo caricato");
  }

bool TestVisionWindow::loadScenario(QString* error)
  {
    m_scenario = testVisionLoadJson(m_scenarioPath, error);
    if (m_scenario.isEmpty()) return false;
    const QDir projectRoot = QFileInfo(m_scenarioPath).dir();
    QDir recipesDirectory(projectRoot.absoluteFilePath("../../recipes"));
    const QFileInfoList recipeDirectories =
      recipesDirectory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& recipeDirectory : recipeDirectories)
    {
      if (QFileInfo::exists(QDir(recipeDirectory.absoluteFilePath()).filePath("recipe.json")))
      {
        m_recipeCombo->addItem(recipeDirectory.fileName());
      }
    }
    const QString configuredRecipe = m_scenario.value("recipeId").toString();
    const int recipeIndex = m_recipeCombo->findText(configuredRecipe);
    if (recipeIndex >= 0)
    {
      m_recipeCombo->setCurrentIndex(recipeIndex);
    }
    const QString strategyId = m_scenario.value("strategyId").toString();
    const int strategyIndex = m_strategyCombo->findData(strategyId);
    if (strategyIndex >= 0)
    {
      m_strategyCombo->setCurrentIndex(strategyIndex);
    }
    const QString cameraId = m_scenario.value("cameraId").toString();
    const int camIndex = m_cameraCombo->findData(cameraId);
    if (camIndex >= 0)
    {
      m_cameraCombo->setCurrentIndex(camIndex);
    }
    else
    {
      m_cameraCombo->setCurrentIndex(0);
    }
    selectBroadcastCamera(cameraId);
    applyExecutionProfile(testVisionExecutionProfileFromScenario(m_scenario));
    updateMasterShape(true);
    const QDir scenarioDir = QFileInfo(m_scenarioPath).dir();
    const QString masterPath = scenarioDir.absoluteFilePath(
      m_scenario.value("master").toObject().value("file").toString());
    QDir().mkpath(QFileInfo(masterPath).dir().absolutePath());
    cv::imwrite(masterPath.toStdString(), m_master);
    m_scenarioLabel->setText(QString("%1 | %2 / %3 | ricetta %4")
      .arg(m_scenario.value("name").toString())
      .arg(m_scenario.value("cameraId").toString())
      .arg(m_scenario.value("channel").toString())
      .arg(m_expectedRecipeId));
    m_preview->setPixmap(testVisionMatToPixmap(m_master).scaled(
      m_preview->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    updatePlanCount();
    return true;
  }

void TestVisionWindow::setCanvasSize(int width, int height, int background)
  {
    if (!m_canvasWidthSpin || !m_canvasHeightSpin)
    {
      return;
    }

    m_syncingResolution = true;
    m_canvasWidth = qMax(1, width);
    m_canvasHeight = qMax(1, height);
    if (background >= 0)
    {
      m_canvasBackground = background;
    }
    m_canvasWidthSpin->setValue(m_canvasWidth);
    m_canvasHeightSpin->setValue(m_canvasHeight);
    syncResolutionComboFromCanvas();
    updateCanvasSizeLabel();
    m_syncingResolution = false;
  }

void TestVisionWindow::syncResolutionComboFromCanvas()
  {
    if (!m_resolutionCombo)
    {
      return;
    }

    const QSize current(m_canvasWidth, m_canvasHeight);
    for (int index = 0; index < m_resolutionCombo->count() - 1; ++index)
    {
      if (m_resolutionCombo->itemData(index).toSize() == current)
      {
        m_resolutionCombo->setCurrentIndex(index);
        return;
      }
    }

    m_resolutionCombo->setCurrentIndex(m_resolutionCombo->count() - 1);
  }

void TestVisionWindow::applySelectedResolutionPreset()
  {
    if (!m_resolutionCombo || m_syncingResolution)
    {
      return;
    }

    const QSize preset = m_resolutionCombo->currentData().toSize();
    if (preset.width() > 0 && preset.height() > 0)
    {
      setCanvasSize(preset.width(), preset.height());
      updateMasterShape(false);
    }
  }

void TestVisionWindow::onCanvasSpinboxChanged()
  {
    if (!m_canvasWidthSpin || !m_canvasHeightSpin || m_syncingResolution)
    {
      return;
    }

    m_canvasWidth = m_canvasWidthSpin->value();
    m_canvasHeight = m_canvasHeightSpin->value();
    m_syncingResolution = true;
    syncResolutionComboFromCanvas();
    updateCanvasSizeLabel();
    m_syncingResolution = false;
    updateMasterShape(false);
  }

void TestVisionWindow::updateCanvasSizeLabel()
  {
    if (!m_canvasSizeLabel)
    {
      return;
    }

    const QString warning = testVisionCanvasSizeWarning(m_canvasWidth, m_canvasHeight);
    if (testVisionCanvasFitsProtocol(m_canvasWidth, m_canvasHeight))
    {
      m_canvasSizeLabel->setStyleSheet({});
    }
    else
    {
      m_canvasSizeLabel->setStyleSheet("color:#ff8a65;");
    }
    m_canvasSizeLabel->setText(warning);
  }

void TestVisionWindow::updateMasterShape(bool persistAsset)
  {
    if (!m_shapeCombo)
    {
      return;
    }
    m_master = testVisionCreateShape(
      m_shapeCombo->currentData().toString(),
      m_canvasWidth,
      m_canvasHeight,
      m_canvasBackground);
    const QString suggestedRecipe = m_scenario.value("recipeId").toString(
      "test_" + m_shapeCombo->currentData().toString());
    if (m_recipeCombo && m_recipeCombo->currentText().isEmpty())
    {
      const int suggestedIndex = m_recipeCombo->findText(suggestedRecipe);
      if (suggestedIndex >= 0)
      {
        m_recipeCombo->setCurrentIndex(suggestedIndex);
      }
    }
    m_expectedRecipeId = m_recipeCombo ? m_recipeCombo->currentText().trimmed() : suggestedRecipe;
    updateScenarioLabel();
    if (m_preview && !m_master.empty())
    {
      m_previewPixmap = testVisionMatToPixmap(m_master).scaled(
        m_preview->size(), Qt::KeepAspectRatio, Qt::FastTransformation);
      m_preview->setPixmap(m_previewPixmap);
    }
    if (persistAsset && !m_scenario.isEmpty())
    {
      const QDir scenarioDir = QFileInfo(m_scenarioPath).dir();
      const QString shapeName = m_shapeCombo->currentData().toString();
      const QString path = scenarioDir.absoluteFilePath(
        QString("../assets/%1/master.png").arg(shapeName));
      QDir().mkpath(QFileInfo(path).dir().absolutePath());
      cv::imwrite(path.toStdString(), m_master);
    }
  }

void TestVisionWindow::updateScenarioLabel()
  {
    if (!m_scenarioLabel || m_scenario.isEmpty())
    {
      return;
    }
    m_scenarioLabel->setText(QString("%1 | %2 / %3 | ricetta %4")
      .arg(m_scenario.value("name").toString())
      .arg(m_scenario.value("cameraId").toString())
      .arg(m_scenario.value("channel").toString())
      .arg(m_expectedRecipeId.isEmpty() ? "<nessuna>" : m_expectedRecipeId));
  }

bool TestVisionWindow::connectPipe()
  {
    closePipe();
    m_sentFrames.clear();
    const HANDLE pipe = CreateFileW(
      kPipePath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
    {
      QMessageBox::warning(
        this,
        "TestVision",
        QString("Vision non raggiungibile, errore Windows %1").arg(GetLastError()));
      return false;
    }
    m_pipe = reinterpret_cast<void*>(pipe);
    m_state = State::WaitingHello;
    m_pollTimer.start();
    return true;
  }

void TestVisionWindow::sendSample()
  {
    if (m_state != State::Idle)
    {
      QMessageBox::information(this, "TestVision", "Arrestare il test prima di inviare un campione.");
      return;
    }
    m_pendingSample = true;
    m_stateLabel->setText("Connessione per invio campione...");
    connectPipe();
  }

void TestVisionWindow::writeSample()
  {
    QJsonObject sample;
    sample["type"] = "sample";
    sample["protocolVersion"] = m_scenario.value("protocolVersion").toInt(1);
    sample["scenarioId"] = m_scenario.value("name").toString();
    const QString targetCam = m_cameraCombo->currentData().toString();
    sample["cameraId"] = targetCam;
    sample["channel"] = targetCam;
    int targetSlot = 1;
    if (targetCam != "SELECTED")
    {
      targetSlot = targetCam.mid(3).toInt();
    }
    sample["slot"] = targetSlot;
    sample["frameId"] = 0;
    sample["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    sample["shapeId"] = m_shapeCombo->currentData().toString();
    sample["strategyId"] = m_strategyCombo->currentData().toString();
    sample["recipeId"] = m_expectedRecipeId;
    sample["imageFormat"] = SimulatorProtocol::kPreferredImageFormat;
    sample["imageBase64"] = SimulatorProtocol::encodeImageBase64(m_master);
    QByteArray payload = QJsonDocument(sample).toJson(QJsonDocument::Compact);
    payload.append('\n');
    DWORD written = 0;
    if (!WriteFile(reinterpret_cast<HANDLE>(m_pipe), payload.constData(), static_cast<DWORD>(payload.size()), &written, nullptr))
    {
      stopTest(QString("Invio campione fallito, errore Windows %1").arg(GetLastError()));
      return;
    }
    m_stateLabel->setText("Campione inviato; attendo Vision");
    appendLog("Invio campione: " + m_shapeCombo->currentText());
  }

void TestVisionWindow::updatePlanCount()
  {
    const int xCount = testVisionSteppedValues(m_xMin->value(), m_xMax->value(), m_xStep->value()).size();
    const int yCount = testVisionSteppedValues(m_yMin->value(), m_yMax->value(), m_yStep->value()).size();
    const int aCount = testVisionSteppedValues(m_angleMin->value(), m_angleMax->value(), m_angleStep->value()).size();
    const qint64 total = static_cast<qint64>(xCount) * yCount * aCount * m_passes->value();
    m_totalLabel->setText(QString("Pose: %1 × %2 × %3 = %4\nGiri: %5\nTotale immagini: %6")
      .arg(xCount).arg(yCount).arg(aCount).arg(xCount * yCount * aCount)
      .arg(m_passes->value()).arg(total));
  }

bool TestVisionWindow::buildPlan(QString* error)
  {
    m_plan.clear();
    const QVector<double> xs = testVisionSteppedValues(m_xMin->value(), m_xMax->value(), m_xStep->value());
    const QVector<double> ys = testVisionSteppedValues(m_yMin->value(), m_yMax->value(), m_yStep->value());
    const QVector<double> angles = testVisionSteppedValues(m_angleMin->value(), m_angleMax->value(), m_angleStep->value());
    if (xs.isEmpty() || ys.isEmpty() || angles.isEmpty())
    {
      *error = "Piano prova non valido: controllare minimi, massimi e passi.";
      return false;
    }
    int poseIndex = 0;
    QVector<TestPose> singlePass;
    for (double x : xs)
      for (double y : ys)
        for (double angle : angles)
          singlePass.append({++poseIndex, 1, x, y, angle});
    for (int pass = 1; pass <= m_passes->value(); ++pass)
    {
      for (TestPose pose : singlePass)
      {
        pose.pass = pass;
        m_plan.append(pose);
      }
    }
    return true;
  }

void TestVisionWindow::generateAiDataset()
  {
    QString error;
    if (!buildPlan(&error))
    {
      QMessageBox::warning(this, "TestVision", error);
      return;
    }
    if (m_master.empty())
    {
      QMessageBox::warning(this, "TestVision", "Immagine master non disponibile.");
      return;
    }

    QVector<TestVisionDatasetImage> images;
    for (const TestPose& pose : m_plan)
    {
      if (pose.pass != 1)
      {
        continue;
      }
      const double txPx = pose.xMm / m_pixelSize->value();
      const double tyPx = pose.yMm / m_pixelSize->value();
      TestVisionDatasetImage item;
      item.fileName = QString("image_%1.png").arg(pose.poseIndex, 6, 10, QChar('0'));
      item.image = testVisionTransformImage(m_master, pose.angleDeg, txPx, tyPx);
      item.metadata["poseIndex"] = pose.poseIndex;
      item.metadata["xMm"] = pose.xMm;
      item.metadata["yMm"] = pose.yMm;
      item.metadata["angleDeg"] = pose.angleDeg;
      item.metadata["expectedCenterX"] = (m_canvasWidth - 1) * 0.5 + txPx;
      item.metadata["expectedCenterY"] = (m_canvasHeight - 1) * 0.5 + tyPx;
      item.metadata["shape"] = m_shapeCombo->currentData().toString();
      item.metadata["strategyId"] = m_strategyCombo->currentData().toString();
      images.append(std::move(item));
    }

    const QString recipeId = m_recipeCombo->currentText().trimmed();
    const QString cameraId = m_scenario.value("cameraId").toString("CAM01");
    const QString rawImagesDirectory = QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(
      QString("recipes/%1/images/%2/ai/localization_segmentation/raw")
        .arg(recipeId, cameraId));
    const QString scenarioName = m_scenario.value("name").toString(
      QFileInfo(m_scenarioPath).completeBaseName());
    const QString manifestPath = saveTestVisionLabelingBatch(
      rawImagesDirectory, scenarioName, images, &error);
    if (manifestPath.isEmpty())
    {
      QMessageBox::warning(this, "TestVision", error);
      return;
    }
    appendLog(QString("Immagini AI per labeling: %1 salvate in %2")
      .arg(images.size())
      .arg(QDir::toNativeSeparators(rawImagesDirectory)));
    appendLog("Manifest lotto: " + QDir::toNativeSeparators(manifestPath));
    m_stateLabel->setText(QString("Dataset AI creato: %1 immagini").arg(images.size()));
  }

void TestVisionWindow::loadCampaign()
  {
    const QString path = QFileDialog::getOpenFileName(
      this,
      "Carica campagna TestVision",
      QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("tests/campaigns"),
      "Campagne JSON (*.json)");
    if (path.isEmpty())
    {
      return;
    }
    QString error;
    if (!loadCampaignFromFile(path, &error))
    {
      QMessageBox::warning(
        this, "Campagna TestVision",
        error.isEmpty() ? "La campagna non contiene prove." : error);
      return;
    }
  }

void TestVisionWindow::editCampaign()
  {
    QStringList recipes;
    for (int index = 0; index < m_recipeCombo->count(); ++index)
    {
      recipes.append(m_recipeCombo->itemText(index));
    }
    CampaignEditorDialog dialog(m_campaign, recipes, this);
    if (dialog.exec() != QDialog::Accepted)
    {
      return;
    }
    const QJsonObject campaign = dialog.campaign();
    if (campaign.value("items").toArray().isEmpty())
    {
      QMessageBox::warning(this, "Campagna TestVision", "Aggiungi almeno una prova.");
      return;
    }
    QJsonObject campaignToSave = campaign;
    campaignToSave["executionProfile"] = testVisionExecutionProfileToJson(currentExecutionProfile());
    QString suggestedPath = m_campaignPath;
    if (suggestedPath.isEmpty())
    {
      suggestedPath = QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(
        QString("tests/campaigns/%1.json")
          .arg(campaign.value("name").toString("nuova_campagna")));
    }
    const QString path = QFileDialog::getSaveFileName(
      this,
      "Salva campagna TestVision",
      suggestedPath,
      "Campagne JSON (*.json)");
    if (path.isEmpty())
    {
      return;
    }
    QDir().mkpath(QFileInfo(path).dir().absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      QMessageBox::warning(this, "Campagna TestVision", "Impossibile salvare " + path);
      return;
    }
    file.write(QJsonDocument(campaignToSave).toJson(QJsonDocument::Indented));
    m_campaignPath = path;
    m_campaign = campaignToSave;
    m_campaignItems = campaign.value("items").toArray();
    m_campaignCycles = qMax(1, campaign.value("cycles").toInt(1));
    m_startCampaignButton->setEnabled(true);
    appendLog(QString("Campagna salvata: %1 | prove=%2 | cicli=%3")
      .arg(QDir::toNativeSeparators(path))
      .arg(m_campaignItems.size())
      .arg(m_campaignCycles));
  }

void TestVisionWindow::applyCampaignItem(const QJsonObject& item)
  {
    const QString cameraId = item.value("cameraId").toString(
      m_scenario.value("cameraId").toString("CAM01"));
    const int cameraIndex = m_cameraCombo->findData(cameraId);
    if (cameraIndex >= 0)
    {
      m_cameraCombo->setCurrentIndex(cameraIndex);
    }
    selectBroadcastCamera(cameraId);
    const QString recipeId = item.value("recipeId").toString();
    const int recipeIndex = m_recipeCombo->findText(recipeId);
    if (recipeIndex >= 0)
    {
      m_recipeCombo->setCurrentIndex(recipeIndex);
    }
    const int strategyIndex = m_strategyCombo->findData(
      item.value("strategyId").toString());
    if (strategyIndex >= 0)
    {
      m_strategyCombo->setCurrentIndex(strategyIndex);
    }
    const int shapeIndex = m_shapeCombo->findData(item.value("shapeId").toString());
    if (shapeIndex >= 0)
    {
      m_shapeCombo->setCurrentIndex(shapeIndex);
    }
    m_xMin->setValue(item.value("xMinMm").toDouble(m_xMin->value()));
    m_xMax->setValue(item.value("xMaxMm").toDouble(m_xMax->value()));
    m_xStep->setValue(item.value("xStepMm").toDouble(m_xStep->value()));
    m_yMin->setValue(item.value("yMinMm").toDouble(m_yMin->value()));
    m_yMax->setValue(item.value("yMaxMm").toDouble(m_yMax->value()));
    m_yStep->setValue(item.value("yStepMm").toDouble(m_yStep->value()));
    m_angleMin->setValue(item.value("angleMinDeg").toDouble(m_angleMin->value()));
    m_angleMax->setValue(item.value("angleMaxDeg").toDouble(m_angleMax->value()));
    m_angleStep->setValue(item.value("angleStepDeg").toDouble(m_angleStep->value()));
    m_passes->setValue(qMax(1, item.value("passes").toInt(m_passes->value())));
    m_intervalMs->setValue(qMax(0, item.value("intervalMs").toInt(m_intervalMs->value())));
    if (item.contains("pixelSizeMm"))
    {
      m_pixelSize->setValue(item.value("pixelSizeMm").toDouble(m_pixelSize->value()));
    }
    if (item.contains("canvasWidth") || item.contains("canvasHeight"))
    {
      setCanvasSize(
        item.value("canvasWidth").toInt(m_canvasWidth),
        item.value("canvasHeight").toInt(m_canvasHeight),
        item.value("canvasBackground").toInt(m_canvasBackground));
    }
    if (item.contains("frameDeliveryMode") || item.contains("sendOnly"))
    {
      const TestVisionExecutionProfile profile =
        testVisionExecutionProfileFromJson(item, currentExecutionProfile());
      setSendOnlyMode(profile.frameDeliveryMode == TestVisionFrameDeliveryMode::SendOnly);
    }
    updateMasterShape(false);
  }

void TestVisionWindow::selectBroadcastCamera(const QString& cameraId)
  {
    if (!m_cameraTargets || cameraId.isEmpty() || cameraId == "SELECTED")
    {
      return;
    }
    for (int row = 0; row < m_cameraTargets->rowCount(); ++row)
    {
      const bool match =
        m_cameraTargets->item(row, 1)->data(Qt::UserRole).toString() == cameraId;
      m_cameraTargets->item(row, 0)->setCheckState(
        match ? Qt::Checked : Qt::Unchecked);
      if (match)
      {
        auto* strategy = qobject_cast<QComboBox*>(
          m_cameraTargets->cellWidget(row, 2));
        const int strategyIndex = strategy
          ? strategy->findData(m_strategyCombo->currentData())
          : -1;
        if (strategyIndex >= 0)
        {
          strategy->setCurrentIndex(strategyIndex);
        }
      }
    }
  }

QJsonArray TestVisionWindow::selectedBroadcastItems() const
  {
    QJsonArray items;
    if (m_cameraTargets)
    {
      for (int row = 0; row < m_cameraTargets->rowCount(); ++row)
      {
        if (m_cameraTargets->item(row, 0)->checkState() != Qt::Checked)
        {
          continue;
        }
        auto* strategy = qobject_cast<QComboBox*>(
          m_cameraTargets->cellWidget(row, 2));
        items.append(QJsonObject{
          {"cameraId", m_cameraTargets->item(row, 1)
            ->data(Qt::UserRole).toString()},
          {"strategyId", strategy
            ? strategy->currentData().toString()
            : m_strategyCombo->currentData().toString()}
        });
      }
    }
    return items;
  }

void TestVisionWindow::startCampaign()
  {
    if (m_campaignItems.isEmpty())
    {
      return;
    }
    if (m_campaign.value("parallel").toBool(true))
    {
      startParallelCampaign();
      return;
    }
    m_campaignRunning = true;
    m_campaignCycle = 0;
    m_campaignItemIndex = -1;
    m_campaignSummaries = {};
    m_startCampaignButton->setEnabled(false);
    runNextCampaignItem();
  }

void TestVisionWindow::startParallelCampaign()
  {
    m_campaignRunning = true;
    m_campaignCycle = 0;
    m_campaignSummaries = {};
    m_parallelPendingItems = m_campaignItems;
    m_activeCampaignWorkers = 0;
    m_startCampaignButton->setEnabled(false);
    appendLog(QString("Campagna multicamera avviata: prove=%1 cicli=%2")
      .arg(m_campaignItems.size())
      .arg(m_campaignCycles));
    launchNextParallelBatch();
  }

void TestVisionWindow::launchNextParallelBatch()
  {
    if (!m_campaignRunning || m_activeCampaignWorkers > 0)
    {
      return;
    }
    if (m_parallelPendingItems.isEmpty())
    {
      ++m_campaignCycle;
      if (m_campaignCycle >= m_campaignCycles)
      {
        finishCampaign();
        return;
      }
      m_parallelPendingItems = m_campaignItems;
    }

    const QString recipeId =
      m_parallelPendingItems.first().toObject().value("recipeId").toString();
    QJsonArray batch;
    QJsonArray remaining;
    QSet<QString> usedCameras;
    for (const QJsonValue& value : m_parallelPendingItems)
    {
      QJsonObject item = value.toObject();
      const QString cameraId = item.value("cameraId").toString("CAM01");
      item["cameraId"] = cameraId;
      if (item.value("recipeId").toString() == recipeId &&
          !usedCameras.contains(cameraId))
      {
        usedCameras.insert(cameraId);
        batch.append(item);
      }
      else
      {
        remaining.append(item);
      }
    }
    m_parallelPendingItems = remaining;
    m_activeCampaignWorkers = batch.size();
    appendLog(QString(
      "Batch multicamera ciclo %1/%2: ricetta=%3 telecamere=%4")
      .arg(m_campaignCycle + 1)
      .arg(m_campaignCycles)
      .arg(recipeId)
      .arg(QStringList(usedCameras.values()).join(", ")));

    for (const QJsonValue& value : batch)
    {
      const QJsonObject item = value.toObject();
      const QString cameraId = item.value("cameraId").toString("CAM01");
      auto* worker = new TestVisionWindow(m_scenarioPath, this);
      worker->hide();
      worker->setSendOnlyMode(m_sendOnlyCheck && m_sendOnlyCheck->isChecked());
      worker->startCampaignWorker(
        item,
        m_campaignCycle + 1,
        [this, worker, cameraId](const QJsonObject& summary) {
          m_campaignSummaries.append(summary);
          appendLog(QString("Worker %1 terminato: %2")
            .arg(cameraId, summary.value("status").toString()));
          worker->deleteLater();
          --m_activeCampaignWorkers;
          if (m_activeCampaignWorkers == 0)
          {
            QTimer::singleShot(300, this, [this]() { launchNextParallelBatch(); });
          }
        });
    }
  }

void TestVisionWindow::runNextCampaignItem()
  {
    ++m_campaignItemIndex;
    if (m_campaignItemIndex >= m_campaignItems.size())
    {
      m_campaignItemIndex = 0;
      ++m_campaignCycle;
    }
    if (m_campaignCycle >= m_campaignCycles)
    {
      finishCampaign();
      return;
    }
    const QJsonObject item = m_campaignItems.at(m_campaignItemIndex).toObject();
    m_campaignItemFailure.clear();
    applyCampaignItem(item);
    appendLog(QString("Campagna ciclo %1/%2 prova %3/%4: ricetta=%5 strategia=%6")
      .arg(m_campaignCycle + 1)
      .arg(m_campaignCycles)
      .arg(m_campaignItemIndex + 1)
      .arg(m_campaignItems.size())
      .arg(m_recipeCombo->currentText())
      .arg(m_strategyCombo->currentData().toString()));
    startTest();
  }

QJsonObject TestVisionWindow::buildCampaignSummary(
    const QJsonObject& item,
    int cycle,
    const QString& pipelineFailure) const
  {
    if (m_sendOnlyMode)
    {
      QJsonObject summary;
      summary["cycle"] = cycle;
      summary["cameraId"] = item.value("cameraId").toString(
        m_cameraCombo->currentData().toString());
      summary["recipeId"] = m_recipeCombo->currentText();
      summary["strategyId"] = m_strategyCombo->currentData().toString();
      summary["framesSent"] = m_currentFrameId;
      summary["status"] = "SENT";
      summary["problems"] = QJsonArray{};
      return summary;
    }

    double centerSum = 0.0;
    double angleSum = 0.0;
    double centerMax = 0.0;
    double angleMax = 0.0;
    double timeSum = 0.0;
    double steadyTimeSum = 0.0;
    double timeMax = 0.0;
    int validCount = 0;
    for (int resultIndex = 0; resultIndex < m_results.size(); ++resultIndex)
    {
      const TestResult& result = m_results[resultIndex];
      centerSum += result.centerError;
      angleSum += result.angleError;
      centerMax = std::max(centerMax, result.centerError);
      angleMax = std::max(angleMax, result.angleError);
      timeSum += result.processingMs;
      if (resultIndex > 0) steadyTimeSum += result.processingMs;
      timeMax = std::max(timeMax, result.processingMs);
      if (result.valid) ++validCount;
    }
    const int count = m_results.size();
    const double centerMean = count > 0 ? centerSum / count : 0.0;
    const double angleMean = count > 0 ? angleSum / count : 0.0;
    const double timeMean = count > 0 ? timeSum / count : 0.0;
    const double steadyTimeMean = count > 1
      ? steadyTimeSum / (count - 1)
      : timeMean;
    const double coldStartMs = count > 0 ? m_results.first().processingMs : 0.0;
    const QJsonObject limits = item.value("limits").toObject();
    QJsonArray problems;
    if (!pipelineFailure.isEmpty())
      problems.append("Errore pipeline: " + pipelineFailure);
    if (validCount != count)
      problems.append(QString("Pose non valide: %1/%2").arg(count - validCount).arg(count));
    if (centerMean > limits.value("centerMeanMaxPx").toDouble(5.0))
      problems.append(QString("Errore centro medio alto: %1 px").arg(centerMean, 0, 'f', 2));
    if (centerMax > limits.value("centerMaxPx").toDouble(10.0))
      problems.append(QString("Errore centro massimo alto: %1 px").arg(centerMax, 0, 'f', 2));
    if (angleMean > limits.value("angleMeanMaxDeg").toDouble(5.0))
      problems.append(QString("Errore angolo medio alto: %1°").arg(angleMean, 0, 'f', 2));
    if (angleMax > limits.value("angleMaxDeg").toDouble(15.0))
      problems.append(QString("Errore angolo massimo alto: %1°").arg(angleMax, 0, 'f', 2));
    if (steadyTimeMean > limits.value("processingMeanMaxMs").toDouble(250.0))
      problems.append(QString("Tempo medio a caldo alto: %1 ms").arg(steadyTimeMean, 0, 'f', 1));
    if (timeMax > limits.value("processingMaxMs").toDouble(1000.0))
      problems.append(QString("Picco tempo alto: %1 ms").arg(timeMax, 0, 'f', 1));

    QJsonObject summary;
    summary["cycle"] = cycle;
    summary["cameraId"] = item.value("cameraId").toString(
      m_cameraCombo->currentData().toString());
    summary["recipeId"] = m_recipeCombo->currentText();
    summary["strategyId"] = m_strategyCombo->currentData().toString();
    summary["frames"] = count;
    summary["validFrames"] = validCount;
    summary["centerMeanPx"] = centerMean;
    summary["centerMaxPx"] = centerMax;
    summary["angleMeanDeg"] = angleMean;
    summary["angleMaxDeg"] = angleMax;
    summary["processingMeanMs"] = timeMean;
    summary["processingSteadyMeanMs"] = steadyTimeMean;
    summary["coldStartMs"] = coldStartMs;
    summary["processingMaxMs"] = timeMax;
    summary["status"] = problems.isEmpty() ? "OK" : "PROBLEM";
    summary["problems"] = problems;
    return summary;
  }

void TestVisionWindow::recordCampaignSummary()
  {
    if (!m_campaignRunning)
    {
      return;
    }
    const QJsonObject item = m_campaignItems.at(m_campaignItemIndex).toObject();
    m_campaignSummaries.append(buildCampaignSummary(
      item, m_campaignCycle + 1, m_campaignItemFailure));
  }

void TestVisionWindow::finishCampaign()
  {
    m_campaignRunning = false;
    m_startCampaignButton->setEnabled(true);
    QJsonObject report;
    report["name"] = m_campaign.value("name").toString(
      QFileInfo(m_campaignPath).completeBaseName());
    report["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    report["cycles"] = m_campaignCycles;
    report["runs"] = m_campaignSummaries;
    const QString outputRoot = QDir(QString::fromUtf8(PROJECT_SOURCE_DIR))
      .filePath("tests/reports/campaigns");
    QDir().mkpath(outputRoot);
    const QString outputPath = QDir(outputRoot).filePath(
      QString("%1_%2.json")
        .arg(report.value("name").toString())
        .arg(testVisionTimestamp()));
    QFile file(outputPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    }
    QStringList textLines;
    textLines.append(QString("Campagna: %1").arg(report.value("name").toString()));
    textLines.append(QString("Generata: %1").arg(report.value("generatedAt").toString()));
    textLines.append(QString());
    for (const QJsonValue& value : m_campaignSummaries)
    {
      const QJsonObject run = value.toObject();
      const QString header = QString(
        "Ciclo %1 | %2 | %3 | %4 | %5")
        .arg(run.value("cycle").toInt())
        .arg(run.value("cameraId").toString())
        .arg(run.value("recipeId").toString())
        .arg(run.value("strategyId").toString())
        .arg(run.value("status").toString());
      textLines.append(header);
      const QJsonArray problems = run.value("problems").toArray();
      if (problems.isEmpty())
      {
        textLines.append("  Nessun problema rilevato.");
      }
      else
      {
        for (const QJsonValue& problem : problems)
          textLines.append("  - " + problem.toString());
      }
      textLines.append(QString(
        "  Centro medio/max: %1 / %2 px | Angolo medio/max: %3 / %4° | "
        "Cold start: %5 ms | Media a caldo: %6 ms")
        .arg(run.value("centerMeanPx").toDouble(), 0, 'f', 2)
        .arg(run.value("centerMaxPx").toDouble(), 0, 'f', 2)
        .arg(run.value("angleMeanDeg").toDouble(), 0, 'f', 2)
        .arg(run.value("angleMaxDeg").toDouble(), 0, 'f', 2)
        .arg(run.value("coldStartMs").toDouble(), 0, 'f', 1)
        .arg(run.value("processingSteadyMeanMs").toDouble(), 0, 'f', 1));
      textLines.append(QString());
    }
    const QString textPath = QFileInfo(outputPath).dir().filePath(
      QFileInfo(outputPath).completeBaseName() + ".txt");
    QFile textFile(textPath);
    if (textFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
      textFile.write(textLines.join('\n').toUtf8());
    }
    int problemRuns = 0;
    for (const QJsonValue& value : m_campaignSummaries)
      if (value.toObject().value("status").toString() != "OK") ++problemRuns;
    const QString message = QString(
      "Campagna terminata: %1 prove, %2 OK, %3 con problemi\n%4")
      .arg(m_campaignSummaries.size())
      .arg(m_campaignSummaries.size() - problemRuns)
      .arg(problemRuns)
      .arg(QDir::toNativeSeparators(textPath));
    appendLog(message);
    m_stateLabel->setText(message);
  }

QJsonObject TestVisionWindow::currentBroadcastItem(
    const QString& cameraId,
    const QString& strategyId) const
  {
    return {
      {"cameraId", cameraId},
      {"recipeId", m_recipeCombo->currentText().trimmed()},
      {"strategyId", strategyId},
      {"shapeId", m_shapeCombo->currentData().toString()},
      {"canvasWidth", m_canvasWidthSpin->value()},
      {"canvasHeight", m_canvasHeightSpin->value()},
      {"canvasBackground", m_canvasBackground},
      {"passes", m_passes->value()},
      {"intervalMs", m_intervalMs->value()},
      {"xMinMm", m_xMin->value()},
      {"xMaxMm", m_xMax->value()},
      {"xStepMm", m_xStep->value()},
      {"yMinMm", m_yMin->value()},
      {"yMaxMm", m_yMax->value()},
      {"yStepMm", m_yStep->value()},
      {"angleMinDeg", m_angleMin->value()},
      {"angleMaxDeg", m_angleMax->value()},
      {"angleStepDeg", m_angleStep->value()},
      {"limits", QJsonObject{
        {"centerMeanMaxPx", 1.0e9},
        {"centerMaxPx", 1.0e9},
        {"angleMeanMaxDeg", 1.0e9},
        {"angleMaxDeg", 1.0e9},
        {"processingMeanMaxMs", 1.0e9},
        {"processingMaxMs", 1.0e9}
      }}
    };
  }

void TestVisionWindow::appendBroadcastResult(const QString& cameraId, const TestResult& result)
  {
    if (!m_broadcastTable)
    {
      return;
    }
    const int row = m_broadcastTable->rowCount();
    m_broadcastTable->insertRow(row);
    const QStringList values = {
      cameraId,
      QString::number(result.frameId),
      result.valid ? "SI" : "NO",
      QString::number(result.actualX, 'f', 2),
      QString::number(result.actualY, 'f', 2),
      QString::number(result.actualAngle, 'f', 2),
      QString::number(result.centerError, 'f', 2),
      QString::number(result.angleError, 'f', 2),
      QString::number(result.processingMs, 'f', 1)
    };
    for (int column = 0; column < values.size(); ++column)
    {
      m_broadcastTable->setItem(row, column, new QTableWidgetItem(values[column]));
    }
    m_broadcastResultsByCamera[cameraId].append(result);
    refreshBroadcastAnalysisSummary();
    m_broadcastTable->scrollToBottom();
    m_stateLabel->setText(QString(
      "Broadcast: %1 frame %2 ricevuto | attivi %3")
      .arg(cameraId)
      .arg(result.frameId)
      .arg(m_activeBroadcastWorkers));
  }

void TestVisionWindow::refreshBroadcastAnalysisSummary()
  {
    if (!m_multiSummaryTable || !m_multiPrimaryCamera || !m_multiCompareCamera)
    {
      return;
    }

    const QString primaryCamera = m_multiPrimaryCamera->currentData().toString();
    const QString compareCamera = m_multiCompareCamera->currentData().toString();
    const QSignalBlocker primaryBlocker(m_multiPrimaryCamera);
    const QSignalBlocker compareBlocker(m_multiCompareCamera);
    m_multiPrimaryCamera->clear();
    m_multiCompareCamera->clear();
    m_multiCompareCamera->addItem("Nessun confronto", "");
    m_multiSummaryTable->setRowCount(0);

    QStringList cameraIds = m_broadcastResultsByCamera.keys();
    cameraIds.sort();
    for (const QString& cameraId : cameraIds)
    {
      const QVector<TestResult>& results = m_broadcastResultsByCamera[cameraId];
      double centerSum = 0.0;
      double centerMax = 0.0;
      double angleSum = 0.0;
      double angleMax = 0.0;
      double timeSum = 0.0;
      double timeMax = 0.0;
      int validCount = 0;
      for (const TestResult& result : results)
      {
        if (result.valid)
        {
          ++validCount;
        }
        centerSum += result.centerError;
        centerMax = std::max(centerMax, result.centerError);
        angleSum += result.angleError;
        angleMax = std::max(angleMax, result.angleError);
        timeSum += result.processingMs;
        timeMax = std::max(timeMax, result.processingMs);
      }
      const int count = results.size();
      const double centerMean = count > 0 ? centerSum / count : 0.0;
      const double angleMean = count > 0 ? angleSum / count : 0.0;
      const double timeMean = count > 0 ? timeSum / count : 0.0;
      const bool problem =
        validCount != count || centerMax > 10.0 ||
        angleMax > 15.0 || timeMax > 2000.0;
      const QStringList values = {
        cameraId,
        QString::number(count),
        QString("%1/%2").arg(validCount).arg(count),
        QString::number(centerMean, 'f', 2),
        QString::number(centerMax, 'f', 2),
        QString::number(angleMean, 'f', 2),
        QString::number(angleMax, 'f', 2),
        QString::number(timeMean, 'f', 1),
        QString::number(timeMax, 'f', 1),
        problem ? "PROBLEMA" : "OK"
      };
      const int row = m_multiSummaryTable->rowCount();
      m_multiSummaryTable->insertRow(row);
      for (int column = 0; column < values.size(); ++column)
      {
        auto* item = new QTableWidgetItem(values[column]);
        if (column == values.size() - 1)
        {
          item->setBackground(problem ? QColor("#7a2633") : QColor("#17633a"));
        }
        m_multiSummaryTable->setItem(row, column, item);
      }
      m_multiPrimaryCamera->addItem(cameraId, cameraId);
      m_multiCompareCamera->addItem(cameraId, cameraId);
    }

    int primaryIndex = m_multiPrimaryCamera->findData(primaryCamera);
    if (primaryIndex < 0 && m_multiPrimaryCamera->count() > 0)
    {
      primaryIndex = 0;
    }
    if (primaryIndex >= 0)
    {
      m_multiPrimaryCamera->setCurrentIndex(primaryIndex);
    }
    const int compareIndex = m_multiCompareCamera->findData(compareCamera);
    m_multiCompareCamera->setCurrentIndex(compareIndex >= 0 ? compareIndex : 0);
    refreshBroadcastAnalysisSelection();
  }

void TestVisionWindow::refreshBroadcastAnalysisSelection()
  {
    if (!m_multiPrimaryCamera || !m_multiPrimaryAnglePlot)
    {
      return;
    }
    auto summaryText = [](const QString& cameraId, const QVector<TestResult>& results) {
      if (results.isEmpty())
      {
        return QString("%1: nessun dato").arg(cameraId);
      }
      double centerSum = 0.0;
      double angleSum = 0.0;
      double timeSum = 0.0;
      for (const TestResult& result : results)
      {
        centerSum += result.centerError;
        angleSum += result.angleError;
        timeSum += result.processingMs;
      }
      return QString("%1 | frame %2 | centro medio %3 px | angolo medio %4° | tempo medio %5 ms")
        .arg(cameraId)
        .arg(results.size())
        .arg(centerSum / results.size(), 0, 'f', 2)
        .arg(angleSum / results.size(), 0, 'f', 2)
        .arg(timeSum / results.size(), 0, 'f', 1);
    };

    const QString primaryCamera = m_multiPrimaryCamera->currentData().toString();
    const QVector<TestResult> primaryResults =
      m_broadcastResultsByCamera.value(primaryCamera);
    m_multiPrimarySummary->setText(summaryText(primaryCamera, primaryResults));
    m_multiPrimaryAnglePlot->setResults(primaryResults);
    m_multiPrimaryCenterPlot->setResults(primaryResults);

    const QString compareCamera = m_multiCompareCamera->currentData().toString();
    const QVector<TestResult> compareResults =
      m_broadcastResultsByCamera.value(compareCamera);
    const bool hasComparison = !compareCamera.isEmpty();
    m_multiCompareGroup->setVisible(hasComparison);
    if (hasComparison)
    {
      m_multiCompareSummary->setText(summaryText(compareCamera, compareResults));
      m_multiCompareAnglePlot->setResults(compareResults);
      m_multiCompareCenterPlot->setResults(compareResults);
    }
  }

void TestVisionWindow::startBroadcastTest(const QJsonArray& targets)
  {
    if (targets.size() < 2 || m_broadcastRunning)
    {
      return;
    }
    m_broadcastRunning = true;
    m_activeBroadcastWorkers = targets.size();
    m_broadcastSummaries = {};
    m_broadcastResultsByCamera.clear();
    m_broadcastTable->setRowCount(0);
    refreshBroadcastAnalysisSummary();
    m_broadcastTable->show();
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    QStringList targetDescriptions;
    for (const QJsonValue& value : targets)
    {
      const QJsonObject target = value.toObject();
      targetDescriptions.append(QString("%1=%2")
        .arg(
          target.value("cameraId").toString(),
          target.value("strategyId").toString()));
    }
    appendLog(QString("Broadcast avviato: %1 telecamere | %2")
      .arg(targets.size())
      .arg(targetDescriptions.join(", ")));

    for (const QJsonValue& value : targets)
    {
      const QJsonObject target = value.toObject();
      const QString cameraId = target.value("cameraId").toString();
      const QString strategyId = target.value("strategyId").toString();
      auto* worker = new TestVisionWindow(m_scenarioPath, this);
      worker->hide();
      worker->setSendOnlyMode(m_sendOnlyCheck && m_sendOnlyCheck->isChecked());
      m_broadcastWorkers.append(worker);
      worker->startCampaignWorker(
        currentBroadcastItem(cameraId, strategyId),
        1,
        [this, worker, cameraId](const QJsonObject& summary) {
          m_broadcastSummaries.append(summary);
          appendLog(QString("Broadcast %1 terminato: %2")
            .arg(cameraId, summary.value("status").toString()));
          m_broadcastWorkers.removeAll(worker);
          worker->deleteLater();
          --m_activeBroadcastWorkers;
          if (m_activeBroadcastWorkers == 0)
          {
            m_broadcastRunning = false;
            m_startButton->setEnabled(true);
            m_stopButton->setEnabled(false);
            m_stateLabel->setText(QString(
              "Broadcast completato: %1 telecamere, %2 risultati")
              .arg(m_broadcastSummaries.size())
              .arg(m_broadcastTable->rowCount()));
          }
        },
        [this](const QString& cameraId, const TestResult& result) {
          appendBroadcastResult(cameraId, result);
        });
    }
  }

void TestVisionWindow::syncStrategyFromRecipe()
  {
    if (!m_strategyCombo || !m_recipeCombo || !m_cameraCombo)
    {
      return;
    }

    const QString recipeId = m_recipeCombo->currentText().trimmed();
    const QString cameraId = m_cameraCombo->currentData().toString();
    if (recipeId.isEmpty() || cameraId.isEmpty() || cameraId == "SELECTED")
    {
      return;
    }

    const QDir recipesDirectory(QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("recipes"));
    const QString cameraPath = recipesDirectory.filePath(recipeId + "/cameras/" + cameraId + ".json");
    QFile cameraFile(cameraPath);
    if (!cameraFile.open(QIODevice::ReadOnly))
    {
      return;
    }

    const QJsonObject root = QJsonDocument::fromJson(cameraFile.readAll()).object();
    const QString method = root.value("tools").toObject()
      .value("surfaceLocalization").toObject()
      .value("method").toString();
    const int strategyIndex = m_strategyCombo->findData(method);
    if (strategyIndex >= 0 && m_strategyCombo->currentIndex() != strategyIndex)
    {
      m_strategyCombo->setCurrentIndex(strategyIndex);
      appendLog("Strategia allineata alla ricetta: " + method);
    }
  }

void TestVisionWindow::startTest()
  {
    if (!m_isCampaignWorker)
    {
      const QJsonArray broadcastTargets = selectedBroadcastItems();
      if (broadcastTargets.size() > 1)
      {
        startBroadcastTest(broadcastTargets);
        return;
      }
      if (broadcastTargets.size() == 1)
      {
        const QJsonObject target = broadcastTargets.first().toObject();
        const int cameraIndex = m_cameraCombo->findData(
          target.value("cameraId").toString());
        if (cameraIndex >= 0)
        {
          m_cameraCombo->setCurrentIndex(cameraIndex);
        }
        const int strategyIndex = m_strategyCombo->findData(
          target.value("strategyId").toString());
        if (strategyIndex >= 0)
        {
          m_strategyCombo->setCurrentIndex(strategyIndex);
        }
      }
    }
    QString error;
    if (!buildPlan(&error))
    {
      QMessageBox::warning(this, "TestVision", error);
      return;
    }
    syncStrategyFromRecipe();
    closePipe();
    m_currentIndex = -1;
    m_currentFrameId = 0;
    m_results.clear();
    m_hasBaseline = false;
    m_baselineResult = {};
    m_baselinePose = {};
    m_baselineMeasurementPixels.clear();
    m_recipeMeasurementNominals = testVisionLoadRecipeMeasurementNominals(
      m_recipeCombo->currentText().trimmed(),
      m_cameraCombo->currentData().toString());
    refreshAnalysis(true);
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    if (m_stateLabel)
    {
      m_stateLabel->setProperty("state", "running");
      m_stateLabel->style()->unpolish(m_stateLabel);
      m_stateLabel->style()->polish(m_stateLabel);
    }
    m_sendOnlyMode = m_sendOnlyCheck && m_sendOnlyCheck->isChecked();
    m_stateLabel->setText(
      m_sendOnlyMode ? "Connessione a Vision (solo invio)..." : "Connessione a Vision...");
    m_pendingSample = false;
    connectPipe();
  }

void TestVisionWindow::stopTest(const QString& reason)
  {
    m_sendTimer.stop();
    std::sort(m_results.begin(), m_results.end(), [](const TestResult& a, const TestResult& b) {
      return a.frameId < b.frameId;
    });
    if (m_table)
    {
      m_table->setRowCount(0);
      for (const TestResult& result : m_results)
      {
        appendResultRow(result);
      }
    }
    refreshAnalysis(true);

    if (!m_results.isEmpty())
    {
      saveReport();
    }
    appendLog(reason);
    m_stateLabel->setText(reason);
    m_pollTimer.stop();
    closePipe();
    m_state = State::Idle;
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    if (m_stateLabel)
    {
      const bool failed = reason.contains("fallit") || reason.contains("Arrestato") ||
                          reason.contains("non valid") || reason.contains("persa");
      const bool ok = reason.startsWith("Test completato") || reason.startsWith("Invio completato") ||
                      reason.startsWith("Campione ricevuto");
      m_stateLabel->setProperty("state", ok ? "ok" : (failed ? "error" : "idle"));
      m_stateLabel->style()->unpolish(m_stateLabel);
      m_stateLabel->style()->polish(m_stateLabel);
    }
    if (m_isCampaignWorker && !m_workerCompleted)
    {
      m_workerCompleted = true;
      QString failure = m_workerFailure;
      if (!reason.startsWith("Test completato") &&
          !reason.startsWith("Invio completato") &&
          failure.isEmpty())
      {
        failure = reason;
      }
      const QJsonObject summary =
        buildCampaignSummary(m_workerItem, m_workerCycle, failure);
      if (m_workerCompletion)
      {
        m_workerCompletion(summary);
      }
    }
  }

void TestVisionWindow::closePipe()
  {
    if (m_pipe != nullptr)
    {
      CloseHandle(reinterpret_cast<HANDLE>(m_pipe));
      m_pipe = nullptr;
    }
    m_buffer.clear();
  }

void TestVisionWindow::pollPipe()
  {
    DWORD available = 0;
    if (!PeekNamedPipe(reinterpret_cast<HANDLE>(m_pipe), nullptr, 0, nullptr, &available, nullptr))
    {
      stopTest(QString("Connessione persa, errore Windows %1").arg(GetLastError()));
      return;
    }
    if (available > 0)
    {
      std::vector<char> chunk(std::min<DWORD>(available, 64 * 1024));
      DWORD bytesRead = 0;
      if (!ReadFile(reinterpret_cast<HANDLE>(m_pipe), chunk.data(), static_cast<DWORD>(chunk.size()), &bytesRead, nullptr))
      {
        stopTest(QString("Lettura fallita, errore Windows %1").arg(GetLastError()));
        return;
      }
      m_buffer.append(chunk.data(), static_cast<qsizetype>(bytesRead));
    }
    while (true)
    {
      const qsizetype newline = m_buffer.indexOf('\n');
      if (newline < 0) break;
      const QByteArray line = m_buffer.left(newline).trimmed();
      m_buffer.remove(0, newline + 1);
      const QJsonDocument document = QJsonDocument::fromJson(line);
      if (document.isObject()) processMessage(document.object());
    }
  }

void TestVisionWindow::processMessage(const QJsonObject& message)
  {
    const QString type = message.value("type").toString();
    if (type == "hello" && m_state == State::WaitingHello)
    {
      appendLog("Vision collegato");
      requestRecipe();
      return;
    }
    if (type == "recipeAccepted" && m_state == State::WaitingRecipe)
    {
      appendLog("Ricetta confermata da Vision: " + message.value("recipeId").toString());
      if (m_pendingSample) writeSample();
      else sendNextFrame();
      return;
    }
    if (type == "recipeRejected" && m_state == State::WaitingRecipe)
    {
      stopTest("Ricetta rifiutata da Vision: " + message.value("message").toString());
      return;
    }
    if (type == "sampleAccepted")
    {
      m_pendingSample = false;
      stopTest("Campione ricevuto da Vision");
      return;
    }
    if (type == "frameAccepted")
    {
      if (m_sendOnlyMode)
      {
        return;
      }
      int fid = message.value("frameId").toInt();
      m_state = State::WaitingResult;
      m_stateLabel->setText(QString("Frame %1 accettato; attendo Vision").arg(fid));
      return;
    }
    if (type == "waitingStart" || type == "frameScheduled" ||
        type == "processingStarted" || type == "processingCompleted" ||
        type == "processingError")
    {
      if (m_sendOnlyMode && type != "processingError")
      {
        return;
      }
      int fid = message.value("frameId").toInt();
      const QString text = message.value("message").toString(type);
      if (type == "processingError")
      {
        m_lastProcessingError = text;
        if (m_campaignRunning)
        {
          m_abortCurrentCampaignItem = true;
          m_campaignItemFailure = text;
        }
        if (m_isCampaignWorker)
        {
          m_abortCurrentCampaignItem = true;
          m_workerFailure = text;
        }
      }
      m_stateLabel->setText(QString("Frame %1: %2").arg(fid).arg(text));
      appendLog(QString("%1 (Frame %2): %3").arg(type).arg(fid).arg(text));
      return;
    }
    if (type == "result")
    {
      if (m_sendOnlyMode)
      {
        return;
      }
      int resultFrameId = message.value("frameId").toInt();
      handleResult(message);
      if (resultFrameId == 1)
      {
        if (m_hasBaseline)
        {
          sendNextFrame();
        }
        else
        {
          stopTest("Impossibile stabilire il riferimento/baseline (Frame 1 non valido)");
        }
      }
      else
      {
        if (m_results.size() == m_plan.size() || m_abortCurrentCampaignItem)
        {
          QTimer::singleShot(0, this, [this]() { finishTest(); });
        }
      }
      return;
    }
    if (type == "error")
    {
      stopTest("Errore Vision: " + message.value("message").toString());
    }
  }

void TestVisionWindow::sendNextFrame()
  {
    ++m_currentIndex;
    if (m_currentIndex >= m_plan.size())
    {
      m_sendTimer.stop();
      return;
    }
    ++m_currentFrameId;
    m_currentPose = m_plan[m_currentIndex];
    const double pxPerMm = 1.0 / m_pixelSize->value();
    const double txPx = m_currentPose.xMm * pxPerMm;
    const double tyPx = m_currentPose.yMm * pxPerMm;
    m_currentImage = testVisionTransformImage(m_master, m_currentPose.angleDeg, txPx, tyPx);
    m_preview->setPixmap(testVisionMatToPixmap(m_currentImage).scaled(
      m_preview->size(), Qt::KeepAspectRatio, Qt::FastTransformation));

    if (m_hasBaseline)
    {
      const double deltaAngle =
        m_currentPose.angleDeg - m_baselinePose.angleDeg;
      const QPointF expectedCenter = testVisionExpectedImageCenter(
        m_baselineResult.actualX,
        m_baselineResult.actualY,
        m_baselinePose.angleDeg,
        m_baselinePose.xMm,
        m_baselinePose.yMm,
        m_currentPose.angleDeg,
        m_currentPose.xMm,
        m_currentPose.yMm,
        m_master.cols * 0.5,
        m_master.rows * 0.5,
        pxPerMm);
      m_expectedX = expectedCenter.x();
      m_expectedY = expectedCenter.y();
      m_expectedAngle =
        m_baselineResult.actualAngle -
        deltaAngle;
    }
    else
    {
      m_expectedX = 0.0;
      m_expectedY = 0.0;
      m_expectedAngle = 0.0;
    }
    m_frameLabel->setText(QString("%1 / %2 | posa %3 | giro %4")
      .arg(m_currentIndex + 1).arg(m_plan.size())
      .arg(m_currentPose.poseIndex).arg(m_currentPose.pass));
    m_expectedLabel->setText(
      m_hasBaseline
        ? QString("X=%1 Y=%2 A=%3° (orbita rotazione + traslazione)")
            .arg(m_expectedX, 0, 'f', 2)
            .arg(m_expectedY, 0, 'f', 2)
            .arg(m_expectedAngle, 0, 'f', 2)
        : QString("Frame zero: acquisizione riferimento Vision"));
    m_actualLabel->setText("-");

    QJsonObject frame;
    frame["type"] = "frame";
    frame["protocolVersion"] = m_scenario.value("protocolVersion").toInt(1);
    frame["scenarioId"] = m_scenario.value("name").toString();
    const QString targetCam = m_cameraCombo->currentData().toString();
    frame["cameraId"] = targetCam;
    frame["channel"] = targetCam;
    int targetSlot = 1;
    if (targetCam != "SELECTED")
    {
      targetSlot = targetCam.mid(3).toInt();
    }
    else
    {
      targetSlot = m_scenario.value("slot").toInt(1);
    }
    frame["slot"] = targetSlot;
    frame["frameId"] = m_currentFrameId;
    frame["strategyId"] = m_strategyCombo->currentData().toString();
    frame["recipeId"] = m_expectedRecipeId;
    frame["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    frame["imageFormat"] = SimulatorProtocol::kPreferredImageFormat;
    frame["imageBase64"] = SimulatorProtocol::encodeImageBase64(m_currentImage);
    m_lastProcessingError.clear();
    m_abortCurrentCampaignItem = false;

    // Record the sent frame info so we can reconstruct results asynchronously
    SentFrameInfo info;
    info.pose = m_currentPose;
    info.expectedX = m_expectedX;
    info.expectedY = m_expectedY;
    info.expectedAngle = m_expectedAngle;
    if (!m_sendOnlyMode)
    {
      m_sentFrames.insert(m_currentFrameId, info);
    }

    QByteArray payload = QJsonDocument(frame).toJson(QJsonDocument::Compact);
    payload.append('\n');
    DWORD written = 0;
    if (!WriteFile(reinterpret_cast<HANDLE>(m_pipe), payload.constData(), static_cast<DWORD>(payload.size()), &written, nullptr))
    {
      stopTest(QString("Invio fallito, errore Windows %1").arg(GetLastError()));
      return;
    }
    m_state = State::WaitingAccepted;
    m_stateLabel->setText(
      m_sendOnlyMode
        ? QString("Frame %1 inviato (%2/%3)")
            .arg(m_currentFrameId)
            .arg(m_currentIndex + 1)
            .arg(m_plan.size())
        : QString("Frame %1 inviato").arg(m_currentFrameId));

    if (m_sendOnlyMode)
    {
      if (m_currentIndex + 1 < m_plan.size())
      {
        m_sendTimer.start(m_intervalMs->value());
      }
      else
      {
        QTimer::singleShot(0, this, [this]() { finishSendOnlyTest(); });
      }
    }
    else if (m_hasBaseline)
    {
      m_sendTimer.start(m_intervalMs->value());
    }
  }

void TestVisionWindow::finishSendOnlyTest()
  {
    m_sendTimer.stop();
    stopTest(QString("Invio completato: %1 frame").arg(m_currentFrameId));
    if (m_campaignRunning)
    {
      recordCampaignSummary();
      QTimer::singleShot(500, this, [this]() { runNextCampaignItem(); });
    }
  }

void TestVisionWindow::requestRecipe()
  {
    QJsonObject request;
    request["type"] = "setRecipe";
    request["protocolVersion"] = m_scenario.value("protocolVersion").toInt(1);
    request["recipeId"] = m_expectedRecipeId;
    QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    payload.append('\n');
    DWORD written = 0;
    if (!WriteFile(reinterpret_cast<HANDLE>(m_pipe), payload.constData(), static_cast<DWORD>(payload.size()), &written, nullptr))
    {
      stopTest(QString("Cambio ricetta fallito, errore Windows %1").arg(GetLastError()));
      return;
    }
    m_state = State::WaitingRecipe;
    m_stateLabel->setText("Caricamento ricetta " + m_expectedRecipeId);
  }

void TestVisionWindow::handleResult(const QJsonObject& message)
  {
    int resultFrameId = message.value("frameId").toInt();
    if (!m_sentFrames.contains(resultFrameId))
    {
      appendLog(QString("Risultato ricevuto per frame %1 non configurato").arg(resultFrameId));
      return;
    }
    const SentFrameInfo info = m_sentFrames.value(resultFrameId);

    const QJsonObject pose = message.value("pose").toObject();
    TestResult result;
    result.pose = info.pose;
    result.frameId = resultFrameId;
    result.valid = pose.value("valid").toBool();
    result.actualX = pose.value("x").toDouble();
    result.actualY = pose.value("y").toDouble();
    result.actualAngle = pose.value("angleDeg").toDouble();
    result.measurements = testVisionParseMeasurementReadings(message.value("measurements").toArray());

    const bool captureBaseline = !m_hasBaseline && result.valid;
    testVisionApplyMeasurementExpectations(
      result.measurements,
      m_baselineMeasurementPixels,
      m_recipeMeasurementNominals,
      captureBaseline);
    if (captureBaseline)
    {
      for (const TestMeasurementReading& reading : result.measurements)
      {
        if (reading.valid)
        {
          m_baselineMeasurementPixels.insert(reading.id, reading.valuePixels);
        }
      }
    }

    if (!m_hasBaseline && result.valid)
    {
      m_hasBaseline = true;
      m_baselinePose = info.pose;
      m_baselineResult = result;
      m_expectedX = result.actualX;
      m_expectedY = result.actualY;
      m_expectedAngle = result.actualAngle;
      result.expectedX = result.actualX;
      result.expectedY = result.actualY;
      result.expectedAngle = result.actualAngle;
      appendLog(QString("Frame zero Vision: X=%1 Y=%2 A=%3°")
        .arg(result.actualX, 0, 'f', 3)
        .arg(result.actualY, 0, 'f', 3)
        .arg(result.actualAngle, 0, 'f', 3));
    }
    else
    {
      result.expectedX = info.expectedX;
      result.expectedY = info.expectedY;
      result.expectedAngle = info.expectedAngle;
    }

    result.centerError = std::hypot(result.actualX - result.expectedX, result.actualY - result.expectedY);
    result.angleError = testVisionPcaAngleError(result.actualAngle, result.expectedAngle);
    result.processingMs = message.value("processingMs").toDouble();
    m_results.append(result);
    if (m_workerResultCallback)
    {
      m_workerResultCallback(
        m_workerItem.value("cameraId").toString(
          m_cameraCombo->currentData().toString()),
        result);
    }

    m_actualLabel->setText(QString("Frame %1 | X=%2 Y=%3 A=%4° | Ec=%5 px Ea=%6°")
      .arg(resultFrameId)
      .arg(result.actualX, 0, 'f', 2).arg(result.actualY, 0, 'f', 2)
      .arg(result.actualAngle, 0, 'f', 2).arg(result.centerError, 0, 'f', 2)
      .arg(result.angleError, 0, 'f', 2));
    m_stateLabel->setText(
      result.valid
        ? QString("Frame %1: Risultato ricevuto").arg(resultFrameId)
        : (m_lastProcessingError.isEmpty()
             ? QString("Frame %1: Posa non valida").arg(resultFrameId)
             : QString("Frame %1: Posa non valida: %2").arg(resultFrameId).arg(m_lastProcessingError)));
    if (!result.valid && !m_lastProcessingError.isEmpty())
    {
      appendLog(QString("Motivo posa non valida Frame %1: %2").arg(resultFrameId).arg(m_lastProcessingError));
    }
    m_lastProcessingError.clear();
    appendResultRow(result);
    appendMeasurementRowsForResult(result);
    scheduleDeferredAnalysisRefresh();
  }

void TestVisionWindow::scheduleDeferredAnalysisRefresh()
  {
    if (!m_analysisRefreshTimer.isActive())
    {
      m_analysisRefreshTimer.start();
    }
  }

void TestVisionWindow::appendMeasurementRowsForResult(const TestResult& result)
  {
    if (!m_measurementTable)
    {
      return;
    }

    m_measurementTable->setUpdatesEnabled(false);
    for (const TestMeasurementReading& reading : result.measurements)
    {
      const int row = m_measurementTable->rowCount();
      m_measurementTable->insertRow(row);
      const QStringList values = {
        QString::number(result.pose.poseIndex),
        QString::number(result.pose.pass),
        QString::number(result.expectedAngle, 'f', 2),
        reading.id,
        reading.type,
        QString::number(reading.valuePixels, 'f', 3),
        reading.hasExpectedBaseline
          ? QString::number(reading.expectedBaselinePixels, 'f', 3)
          : "-",
        reading.hasExpectedBaseline
          ? QString::number(reading.baselineErrorPixels, 'f', 3)
          : "-",
        reading.hasExpectedRecipe
          ? QString::number(reading.expectedRecipePixels, 'f', 3)
          : "-",
        reading.hasExpectedRecipe
          ? QString::number(reading.recipeErrorPixels, 'f', 3)
          : "-",
        QString::number(result.centerError, 'f', 3),
        QString::number(result.angleError, 'f', 3),
        QString::number(result.processingMs, 'f', 1)
      };
      for (int column = 0; column < values.size(); ++column)
      {
        auto* item = new QTableWidgetItem(values[column]);
        if (!result.valid || !reading.valid)
        {
          item->setBackground(QColor("#7a2633"));
        }
        m_measurementTable->setItem(row, column, item);
      }
    }
    m_measurementTable->setUpdatesEnabled(true);
  }

void TestVisionWindow::refreshMeasurementAnalysis(bool rebuildTable)
  {
    if (!m_measurementTable)
    {
      return;
    }

    if (rebuildTable)
    {
      m_measurementTable->setUpdatesEnabled(false);
      m_measurementTable->setRowCount(0);
    }

    if (m_results.isEmpty())
    {
      if (m_measurementSummaryLabel)
      {
        m_measurementSummaryLabel->setText("Nessun dato misura");
      }
      if (m_measurementPlot)
      {
        m_measurementPlot->setResults({});
      }
      if (rebuildTable)
      {
        m_measurementTable->setUpdatesEnabled(true);
      }
      return;
    }

    double maxBaselineError = 0.0;
    double maxRecipeError = 0.0;
    double sumBaselineError = 0.0;
    int baselineCount = 0;
    QVector<double> centerErrors;
    QVector<double> measurementErrors;
    QHash<QString, double> maxErrorById;

    for (const TestResult& result : m_results)
    {
      if (rebuildTable)
      {
        for (const TestMeasurementReading& reading : result.measurements)
        {
          const int row = m_measurementTable->rowCount();
          m_measurementTable->insertRow(row);
          const QStringList values = {
            QString::number(result.pose.poseIndex),
            QString::number(result.pose.pass),
            QString::number(result.expectedAngle, 'f', 2),
            reading.id,
            reading.type,
            QString::number(reading.valuePixels, 'f', 3),
            reading.hasExpectedBaseline
              ? QString::number(reading.expectedBaselinePixels, 'f', 3)
              : "-",
            reading.hasExpectedBaseline
              ? QString::number(reading.baselineErrorPixels, 'f', 3)
              : "-",
            reading.hasExpectedRecipe
              ? QString::number(reading.expectedRecipePixels, 'f', 3)
              : "-",
            reading.hasExpectedRecipe
              ? QString::number(reading.recipeErrorPixels, 'f', 3)
              : "-",
            QString::number(result.centerError, 'f', 3),
            QString::number(result.angleError, 'f', 3),
            QString::number(result.processingMs, 'f', 1)
          };
          for (int column = 0; column < values.size(); ++column)
          {
            auto* item = new QTableWidgetItem(values[column]);
            if (!result.valid || !reading.valid)
            {
              item->setBackground(QColor("#7a2633"));
            }
            m_measurementTable->setItem(row, column, item);
          }
        }
      }

      for (const TestMeasurementReading& reading : result.measurements)
      {
        if (result.valid && reading.valid && reading.hasExpectedBaseline)
        {
          sumBaselineError += reading.baselineErrorPixels;
          ++baselineCount;
          maxBaselineError = std::max(maxBaselineError, reading.baselineErrorPixels);
          centerErrors.append(result.centerError);
          measurementErrors.append(reading.baselineErrorPixels);
          maxErrorById[reading.id] =
            std::max(maxErrorById.value(reading.id, 0.0), reading.baselineErrorPixels);
        }
        if (reading.valid && reading.hasExpectedRecipe)
        {
          maxRecipeError = std::max(maxRecipeError, reading.recipeErrorPixels);
        }
      }
    }

    if (rebuildTable)
    {
      m_measurementTable->setUpdatesEnabled(true);
    }

    const double correlation = testVisionPearsonCorrelation(centerErrors, measurementErrors);
    QString summary =
      QString("Misure analizzate: %1 righe | Δ baseline medio/max: %2 / %3 px")
        .arg(baselineCount)
        .arg(baselineCount > 0 ? sumBaselineError / baselineCount : 0.0, 0, 'f', 3)
        .arg(maxBaselineError, 0, 'f', 3);
    if (!m_recipeMeasurementNominals.isEmpty())
    {
      summary += QString(" | Δ ricetta max: %1 px").arg(maxRecipeError, 0, 'f', 3);
    }
    if (baselineCount >= 2)
    {
      summary += QString(" | Correlazione err. centro ↔ err. misura: %1")
                   .arg(correlation, 0, 'f', 3);
      if (std::abs(correlation) > 0.6)
      {
        summary += " (la misura segue l'errore di posa: verificare localizzazione/edge)";
      }
      else if (maxBaselineError > 1.0)
      {
        summary += " (drift misura non spiegato solo dalla posa: verificare geometrie)";
      }
      else
      {
        summary += " (misure stabili rispetto al pezzo)";
      }
    }
    for (auto it = maxErrorById.cbegin(); it != maxErrorById.cend(); ++it)
    {
      summary += QString(" | %1 max Δ=%2 px").arg(it.key()).arg(it.value(), 0, 'f', 3);
    }
    if (m_measurementSummaryLabel)
    {
      m_measurementSummaryLabel->setText(summary);
    }
    if (m_measurementPlot)
    {
      m_measurementPlot->setResults(m_results);
    }
  }

void TestVisionWindow::appendResultRow(const TestResult& result)
  {
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    const QStringList values = {
      QString::number(result.pose.poseIndex),
      QString::number(result.pose.pass),
      QString::number(result.pose.xMm, 'f', 3),
      QString::number(result.pose.yMm, 'f', 3),
      QString::number(result.pose.angleDeg, 'f', 3),
      QString::number(result.expectedX, 'f', 3),
      QString::number(result.expectedY, 'f', 3),
      QString::number(result.expectedAngle, 'f', 3),
      QString::number(result.actualX, 'f', 3),
      QString::number(result.actualY, 'f', 3),
      QString::number(result.actualAngle, 'f', 3),
      QString::number(result.centerError, 'f', 3),
      QString::number(result.angleError, 'f', 3),
      QString::number(result.processingMs, 'f', 1)
    };
    for (int column = 0; column < values.size(); ++column)
    {
      auto* item = new QTableWidgetItem(values[column]);
      if (!result.valid)
      {
        item->setBackground(QColor("#7a2633"));
      }
      m_table->setItem(row, column, item);
    }
  }

void TestVisionWindow::refreshAnalysis(bool rebuildTables)
  {
    if (m_results.isEmpty())
    {
      if (rebuildTables && m_table)
      {
        m_table->setRowCount(0);
      }
      m_summaryLabel->setText("Nessun dato");
      m_anglePlot->setResults({});
      m_centerPlot->setResults({});
      m_repeatabilityPlot->setResults({});
      refreshMeasurementAnalysis(rebuildTables);
      return;
    }

    double centerSum = 0.0;
    double angleSum = 0.0;
    double centerMax = 0.0;
    double angleMax = 0.0;
    int invalidCount = 0;
    for (const TestResult& result : m_results)
    {
      if (!result.valid)
      {
        ++invalidCount;
      }
      centerSum += result.centerError;
      angleSum += result.angleError;
      centerMax = std::max(centerMax, result.centerError);
      angleMax = std::max(angleMax, result.angleError);
    }

    std::map<int, std::vector<TestResult>> grouped;
    for (const TestResult& result : m_results)
    {
      grouped[result.pose.poseIndex].push_back(result);
    }
    double worstRepeatCenter = 0.0;
    double worstRepeatAngle = 0.0;
    for (const auto& [poseIndex, values] : grouped)
    {
      std::vector<double> xs;
      std::vector<double> ys;
      std::vector<double> angles;
      for (const TestResult& result : values)
      {
        xs.push_back(result.actualX);
        ys.push_back(result.actualY);
        angles.push_back(testVisionNormalizePcaAngleNear(
          result.actualAngle,
          result.expectedAngle));
      }
      worstRepeatCenter = std::max(
        worstRepeatCenter,
        std::hypot(testVisionStandardDeviation(xs), testVisionStandardDeviation(ys)));
      worstRepeatAngle = std::max(worstRepeatAngle, testVisionStandardDeviation(angles));
    }

    m_summaryLabel->setText(
      QString("Frame: %1 (Falliti/Scartati: %2) | Errore centro medio/max: %3 / %4 px | "
              "Errore angolo medio/max: %5 / %6° | "
              "Ripetibilità peggiore σ centro/angolo: %7 px / %8°")
        .arg(m_results.size())
        .arg(invalidCount)
        .arg(centerSum / m_results.size(), 0, 'f', 3)
        .arg(centerMax, 0, 'f', 3)
        .arg(angleSum / m_results.size(), 0, 'f', 3)
        .arg(angleMax, 0, 'f', 3)
        .arg(worstRepeatCenter, 0, 'f', 4)
        .arg(worstRepeatAngle, 0, 'f', 4));
    m_anglePlot->setResults(m_results);
    m_centerPlot->setResults(m_results);
    m_repeatabilityPlot->setResults(m_results);
    refreshMeasurementAnalysis(rebuildTables);
  }

void TestVisionWindow::finishTest()
  {
    stopTest(QString("Test completato: %1 frame").arg(m_results.size()));
    if (m_campaignRunning)
    {
      recordCampaignSummary();
      QTimer::singleShot(500, this, [this]() { runNextCampaignItem(); });
    }
  }

void TestVisionWindow::saveReport()
  {
    QJsonArray frames;
    for (const TestResult& result : m_results)
    {
      QJsonObject item;
      item["poseIndex"] = result.pose.poseIndex;
      item["pass"] = result.pose.pass;
      item["xMm"] = result.pose.xMm;
      item["yMm"] = result.pose.yMm;
      item["angleExpectedDeg"] = result.expectedAngle;
      item["expectedX"] = result.expectedX;
      item["expectedY"] = result.expectedY;
      item["actualX"] = result.actualX;
      item["actualY"] = result.actualY;
      item["actualAngleDeg"] = result.actualAngle;
      item["centerErrorPx"] = result.centerError;
      item["angleErrorDeg"] = result.angleError;
      item["processingMs"] = result.processingMs;
      item["valid"] = result.valid;
      QJsonArray measurements;
      for (const TestMeasurementReading& reading : result.measurements)
      {
        QJsonObject measurement;
        measurement["id"] = reading.id;
        measurement["type"] = reading.type;
        measurement["valid"] = reading.valid;
        measurement["valuePixels"] = reading.valuePixels;
        measurement["baselineErrorPixels"] = reading.baselineErrorPixels;
        measurement["recipeErrorPixels"] = reading.recipeErrorPixels;
        measurement["expectedBaselinePixels"] = reading.expectedBaselinePixels;
        measurement["expectedRecipePixels"] = reading.expectedRecipePixels;
        if (reading.sampleCount > 0)
        {
          measurement["sampleCount"] = reading.sampleCount;
        }
        if (reading.pointCount > 0)
        {
          measurement["pointCount"] = reading.pointCount;
        }
        if (!reading.diagnostic.isEmpty())
        {
          measurement["diagnostic"] = reading.diagnostic;
        }
        measurements.append(measurement);
      }
      item["measurements"] = measurements;
      frames.append(item);
    }
    QJsonObject report;
    report["scenario"] = m_scenario.value("name");
    report["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    report["pixelSizeMm"] = m_pixelSize->value();
    report["passes"] = m_passes->value();
    report["strategyId"] = m_strategyCombo->currentData().toString();
    report["recipeId"] = m_recipeCombo->currentText().trimmed();
    report["cameraId"] = m_cameraCombo->currentData().toString();
    QJsonObject recipeNominals;
    for (auto it = m_recipeMeasurementNominals.cbegin(); it != m_recipeMeasurementNominals.cend(); ++it)
    {
      recipeNominals[it.key()] = it.value();
    }
    report["recipeMeasurementNominalsPx"] = recipeNominals;
    report["frames"] = frames;
    const QDir scenarioDir = QFileInfo(m_scenarioPath).dir();
    const QString path = scenarioDir.absoluteFilePath(m_scenario.value("output").toString());
    QString error;
    const QString versionedPath = saveVersionedTestReport(path, report, &error);
    if (versionedPath.isEmpty())
    {
      appendLog(error);
      return;
    }
    appendLog("Report salvato: " + QDir::toNativeSeparators(versionedPath));
  }

void TestVisionWindow::appendLog(const QString& message)
  {
    m_log->appendPlainText(QString("%1  %2")
      .arg(QTime::currentTime().toString("HH:mm:ss.zzz"), message));
  }
