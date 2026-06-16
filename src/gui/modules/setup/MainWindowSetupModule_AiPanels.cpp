#include "gui/modules/MainWindowSetupModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/modules/MainWindowCameraConfigModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/setup/AiTrainingGraph.h"

#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

void MainWindowSetupModule::showAiClassificationPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.aiClassification"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(tr("labels.aiClassificationNote"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  const QString activeModelPath = recipes().loadAiClassificationActiveModelPath(camera.id);
  auto* modelLabel = new QLabel(
    activeModelPath.isEmpty()
      ? "Modello attivo: automatico / non selezionato"
      : QString("Modello attivo: %1").arg(QFileInfo(activeModelPath).fileName()),
    panel);
  modelLabel->setObjectName("toolPanelNote");
  modelLabel->setWordWrap(true);
  modelLabel->setToolTip(activeModelPath);
  layout->addWidget(modelLabel);

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setSpacing(6);

  auto* rawButton = new QPushButton(tr("actions.acquireAiRawImage") + " 1 shot", buttons);
  rawButton->setObjectName("touchButton");
  QObject::connect(rawButton, &QPushButton::clicked, window(), [this, camera]() {
    context().cameraConfig->acquireCameraAiClassificationRawImage(camera);
  });
  buttonsLayout->addWidget(rawButton, 0, 0);

  const bool rawCaptureActive =
    m_aiClassificationCaptureCameraId == camera.id &&
    !m_aiClassificationCaptureToClass;
  auto* rawPlayButton = new QPushButton((rawCaptureActive ? tr("commands.stop") : tr("commands.start")) + " raw", buttons);
  rawPlayButton->setObjectName("touchButton");
  QObject::connect(rawPlayButton, &QPushButton::clicked, window(), [this, camera]() {
    if (m_aiClassificationCaptureCameraId == camera.id && !m_aiClassificationCaptureToClass)
    {
      stopAiClassificationCapture();
    }
    else
    {
      startAiClassificationCapture(camera, false);
    }
    showAiClassificationPanel(camera);
  });
  buttonsLayout->addWidget(rawPlayButton, 0, 1);

  auto* addClassButton = new QPushButton(tr("actions.addAiClass"), buttons);
  addClassButton->setObjectName("touchButton");
  QObject::connect(addClassButton, &QPushButton::clicked, window(), [this, camera]() {
    bool ok = false;
    const QString className = QInputDialog::getText(
      window(),
      tr("actions.addAiClass"),
      tr("labels.aiClassName"),
      QLineEdit::Normal,
      {},
      &ok).trimmed();
    if (!ok || className.isEmpty())
    {
      return;
    }

    QString error;
    if (!recipes().addAiClassificationClass(camera.id, className, nullptr, &error))
    {
      log(error);
      return;
    }

    showAiClassificationPanel(camera);
  });
  buttonsLayout->addWidget(addClassButton, 0, 2, 1, 2);

  auto* prepareDatasetButton = new QPushButton(tr("actions.prepareAiDataset"), buttons);
  prepareDatasetButton->setObjectName("touchButton");
  QObject::connect(prepareDatasetButton, &QPushButton::clicked, window(), [this, camera]() {
    prepareAiClassificationDataset(camera);
  });
  buttonsLayout->addWidget(prepareDatasetButton, 1, 0, 1, 2);

  auto* trainButton = new QPushButton(tr("actions.trainAiModel") + " GPU", buttons);
  trainButton->setObjectName("touchButton");
  QObject::connect(trainButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiClassificationTrainingPanel(camera);
  });
  buttonsLayout->addWidget(trainButton, 1, 2, 1, 2);

  auto* inferenceButton = new QPushButton(tr("actions.runInference"), buttons);
  inferenceButton->setObjectName("touchButton");
  QObject::connect(inferenceButton, &QPushButton::clicked, window(), [this, camera]() {
    runAiClassificationInference(camera);
  });
  buttonsLayout->addWidget(inferenceButton, 2, 0, 1, 4);

  const QVector<AiClassificationClassConfig> classes = recipes().loadAiClassificationClasses(camera.id);
  int index = 0;
  for (const AiClassificationClassConfig& classConfig : classes)
  {
    auto* classButton = new QPushButton(
      QString("%1 - %2 1 shot").arg(classConfig.id, 3, 10, QChar('0')).arg(classConfig.name),
      buttons);
    classButton->setObjectName("touchButton");
    QObject::connect(classButton, &QPushButton::clicked, window(), [this, camera, classConfig]() {
      context().cameraConfig->acquireCameraAiClassificationClassImage(camera, classConfig);
    });
    const bool classCaptureActive =
      m_aiClassificationCaptureCameraId == camera.id &&
      m_aiClassificationCaptureToClass &&
      m_aiClassificationCaptureClass.id == classConfig.id;
    auto* classPlayButton = new QPushButton(
      QString("%1 %2").arg(classCaptureActive ? tr("commands.stop") : tr("commands.start"), classConfig.name),
      buttons);
    classPlayButton->setObjectName("touchButton");
    QObject::connect(classPlayButton, &QPushButton::clicked, window(), [this, camera, classConfig]() {
      if (m_aiClassificationCaptureCameraId == camera.id &&
          m_aiClassificationCaptureToClass &&
          m_aiClassificationCaptureClass.id == classConfig.id)
      {
        stopAiClassificationCapture();
      }
      else
      {
        startAiClassificationCapture(camera, true, classConfig);
      }
      showAiClassificationPanel(camera);
    });
    buttonsLayout->addWidget(classButton, 3 + index, 0, 1, 2);
    buttonsLayout->addWidget(classPlayButton, 3 + index, 2, 1, 2);
    ++index;
  }

  layout->addWidget(buttons);

  m_aiInferenceResultLabel = new QLabel("Inferenza: nessun risultato", panel);
  m_aiInferenceResultLabel->setObjectName("toolPanelNote");
  m_aiInferenceResultLabel->setWordWrap(true);
  layout->addWidget(m_aiInferenceResultLabel);

  auto* backButton = new QPushButton(tr("commands.backToCameraTools"), panel);
  backButton->setObjectName("touchButton");
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (m_aiClassificationCaptureCameraId == camera.id)
    {
      stopAiClassificationCapture();
    }
    showAiPanel(camera);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tr("tools.aiClassification"));
}

void MainWindowSetupModule::showAiClassificationTrainingPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  if (context().largeTitle)
  {
    context().largeTitle->setText(QString("Training AI | %1").arg(camera.id));
  }
  selectedPreview() = renderAiClassificationTrainingGraph(recipes(), camera.id);
  largeImage()->show();
  largeImage()->setImage(selectedPreview());

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("Training classificazione | %1").arg(camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(
    "Valori consigliati gia' impostati per il primo training. Regola solo se serve, poi avvia. "
    "Il grafico resta a sinistra.",
    panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(6);

  auto* epochsSpin = new QSpinBox(form);
  epochsSpin->setRange(1, 1000);
  epochsSpin->setValue(80);
  epochsSpin->setToolTip("Consigliato: 80. Aumenta se il dataset cresce o la validazione e' instabile.");
  auto* imageSizeSpin = new QSpinBox(form);
  imageSizeSpin->setRange(64, 2048);
  imageSizeSpin->setSingleStep(32);
  imageSizeSpin->setValue(224);
  imageSizeSpin->setToolTip("Consigliato: 224 per classificazione veloce. Aumenta solo se i dettagli sono piccoli.");
  auto* batchSpin = new QSpinBox(form);
  batchSpin->setRange(1, 256);
  batchSpin->setValue(8);
  batchSpin->setToolTip("Consigliato: 8. Riduci se la GPU finisce memoria.");
  auto* valSplitSpin = new QDoubleSpinBox(form);
  valSplitSpin->setRange(0.05, 0.5);
  valSplitSpin->setSingleStep(0.05);
  valSplitSpin->setDecimals(2);
  valSplitSpin->setValue(0.20);
  valSplitSpin->setToolTip("Consigliato: 0.20, cioe' 20% immagini per validazione.");
  auto* deviceEdit = new QLineEdit("0", form);
  deviceEdit->setToolTip("Consigliato: 0 per la prima GPU CUDA. Usa cpu solo per diagnostica.");
  auto* modelEdit = new QLineEdit("yolo11s-cls.pt", form);
  modelEdit->setToolTip("Consigliato: yolo11s-cls.pt. Modello base leggero per classificazione.");

  int row = 0;
  formLayout->addWidget(new QLabel("Epoch consigliate", form), row, 0);
  formLayout->addWidget(epochsSpin, row++, 1);
  formLayout->addWidget(new QLabel("Image size consigliata", form), row, 0);
  formLayout->addWidget(imageSizeSpin, row++, 1);
  formLayout->addWidget(new QLabel("Batch size consigliato", form), row, 0);
  formLayout->addWidget(batchSpin, row++, 1);
  formLayout->addWidget(new QLabel("Validation split consigliato", form), row, 0);
  formLayout->addWidget(valSplitSpin, row++, 1);
  formLayout->addWidget(new QLabel("GPU / device consigliato", form), row, 0);
  formLayout->addWidget(deviceEdit, row++, 1);
  formLayout->addWidget(new QLabel("Modello base consigliato", form), row, 0);
  formLayout->addWidget(modelEdit, row++, 1);
  layout->addWidget(form);

  const QVector<AiClassificationClassConfig> classes = recipes().loadAiClassificationClasses(camera.id);
  QStringList classLines;
  for (const AiClassificationClassConfig& classConfig : classes)
  {
    const QDir folder(recipes().cameraAiClassificationClassImagesPath(camera.id, classConfig));
    const int count = folder.entryInfoList(RecipeJsonUtils::imageNameFilters(), QDir::Files).size();
    classLines.append(QString("%1 - %2: %3 immagini").arg(classConfig.id, 3, 10, QChar('0')).arg(classConfig.name).arg(count));
  }
  auto* classSummary = new QLabel(classLines.isEmpty() ? "Classi: nessuna immagine" : classLines.join('\n'), panel);
  classSummary->setObjectName("toolPanelNote");
  classSummary->setWordWrap(true);
  layout->addWidget(classSummary);

  auto* runSummary = new QLabel(
    QString("Output training temporaneo:\n%1\n\nModello attivo stabile: verra' aggiornato solo se promuovi il nuovo training.")
      .arg(QDir(RecipeManager::recipesRootPath()).filePath(QString("%1/models/classification/runs").arg(recipes().recipeId()))),
    panel);
  runSummary->setObjectName("toolPanelNote");
  runSummary->setWordWrap(true);
  layout->addWidget(runSummary);

  auto* startButton = new QPushButton("Start training", panel);
  startButton->setObjectName("touchButton");
  QObject::connect(startButton, &QPushButton::clicked, window(), [this, camera, epochsSpin, imageSizeSpin, batchSpin, deviceEdit, valSplitSpin, modelEdit]() {
    m_aiTrainingGraphCameraId = camera.id;
    if (!m_aiTrainingGraphTimer)
    {
      m_aiTrainingGraphTimer = new QTimer(window());
      QObject::connect(m_aiTrainingGraphTimer, &QTimer::timeout, window(), [this]() {
        updateAiTrainingGraph(m_aiTrainingGraphCameraId);
      });
    }
    m_aiTrainingGraphTimer->start(2000);
    updateAiTrainingGraph(camera.id);
    startAiClassificationTraining(
      camera,
      epochsSpin->value(),
      imageSizeSpin->value(),
      batchSpin->value(),
      deviceEdit->text().trimmed().isEmpty() ? QString("0") : deviceEdit->text().trimmed(),
      valSplitSpin->value(),
      modelEdit->text().trimmed().isEmpty() ? QString("yolo11s-cls.pt") : modelEdit->text().trimmed());
  });
  layout->addWidget(startButton);

  auto* backButton = new QPushButton(tr("commands.backToCameraTools"), panel);
  backButton->setObjectName("touchButton");
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiClassificationPanel(camera);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(QString("AI training setup panel: %1").arg(camera.id));
}
