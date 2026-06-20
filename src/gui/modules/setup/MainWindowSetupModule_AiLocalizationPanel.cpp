#include "gui/modules/MainWindowSetupModule.h"

#include "config/RecipeJsonUtils.h"
#include "ai/AiMaskLabelStorage.h"
#include "gui/modules/MainWindowCameraConfigModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/setup/AiLocalizationPaths.h"
#include "gui/modules/setup/AiPythonRuntime.h"
#include "gui/modules/setup/AiTrainingGraph.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

void MainWindowSetupModule::showAiLocalizationPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  QDir().mkpath(aiLocalizationRawPath(recipes(), camera.id));
  QDir().mkpath(aiLocalizationMasksPath(recipes(), camera.id));
  QDir().mkpath(aiLocalizationLabelsPath(recipes(), camera.id));
  QDir().mkpath(aiLocalizationDatasetPath(recipes(), camera.id));
  QDir().mkpath(aiLocalizationModelsPath(recipes(), camera.id));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.aiLocalization"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* note = new QLabel(tr("labels.aiLocalizationNote"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* steps = new QWidget(panel);
  auto* grid = new QGridLayout(steps);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setSpacing(6);

  auto makeButton = [steps](const QString& text) {
    auto* button = new QPushButton(text, steps);
    button->setObjectName("touchButton");
    button->setMinimumHeight(48);
    return button;
  };

  auto* acquire = makeButton(tr("actions.acquireAiRawImage") + " 1 shot");
  QObject::connect(acquire, &QPushButton::clicked, window(), [this, camera]() {
    context().cameraConfig->acquireCameraAiLocalizationRawImage(camera);
  });
  grid->addWidget(acquire, 0, 0);

  auto* openFolder = makeButton(tr("actions.openAiLocalizationFolder"));
  QObject::connect(openFolder, &QPushButton::clicked, window(), [this, camera]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(aiLocalizationRootPath(recipes(), camera.id)));
  });
  grid->addWidget(openFolder, 0, 1);

  const QDir rawDirectory(aiLocalizationRawPath(recipes(), camera.id));
  const QFileInfoList rawImages = rawDirectory.entryInfoList(
    RecipeJsonUtils::imageNameFilters(),
    QDir::Files,
    QDir::Name);
  int labeledCount = 0;
  for (const QFileInfo& rawImage : rawImages)
  {
    if (QFileInfo(QDir(aiLocalizationLabelsPath(recipes(), camera.id))
                    .filePath(rawImage.completeBaseName() + ".txt")).exists())
    {
      ++labeledCount;
    }
  }
  auto* labeling = makeButton(
    QString("%1 (%2/%3)").arg(tr("actions.labelPieceMasks")).arg(labeledCount).arg(rawImages.size()));
  labeling->setEnabled(!rawImages.isEmpty());
  labeling->setToolTip(
    rawImages.isEmpty()
      ? tr("log.imageMissing")
      : "Apre il lotto dalla prima immagine non etichettata. Doppio click per chiudere il poligono.");
  QObject::connect(labeling, &QPushButton::clicked, window(), [this, camera]() {
    startAiLocalizationLabeling(camera);
  });
  grid->addWidget(labeling, 1, 0);

  auto* training = makeButton(tr("actions.trainAiModel"));
  training->setEnabled(labeledCount >= 2);
  training->setToolTip(
    labeledCount >= 2
      ? QString("Avvia YOLO segmentation usando %1 immagini etichettate.").arg(labeledCount)
      : "Servono almeno 2 immagini etichettate.");
  QObject::connect(training, &QPushButton::clicked, window(), [this, camera]() {
    showAiLocalizationTrainingPanel(camera);
  });
  grid->addWidget(training, 1, 1);

  auto* inference = makeButton(tr("actions.runInference"));
  const QString localizationModelPath = aiNewestLocalizationModelPath(recipes(), camera.id);
  inference->setEnabled(!localizationModelPath.isEmpty());
  inference->setToolTip(
    localizationModelPath.isEmpty()
      ? "Nessun modello addestrato disponibile."
      : QDir::toNativeSeparators(localizationModelPath));
  QObject::connect(inference, &QPushButton::clicked, window(), [this, camera]() {
    runAiLocalizationInference(camera);
  });
  grid->addWidget(inference, 2, 0, 1, 2);
  layout->addWidget(steps);

  m_aiLocalizationInferenceResultLabel = new QLabel(
    localizationModelPath.isEmpty()
      ? "Inferenza: modello non disponibile"
      : "Inferenza: pronta",
    panel);
  m_aiLocalizationInferenceResultLabel->setObjectName("toolPanelNote");
  m_aiLocalizationInferenceResultLabel->setWordWrap(true);
  layout->addWidget(m_aiLocalizationInferenceResultLabel);

  auto* navigation = new QWidget(panel);
  auto* navigationLayout = new QGridLayout(navigation);
  navigationLayout->setContentsMargins(0, 0, 0, 0);
  navigationLayout->setSpacing(6);
  m_aiLocalizationPreviousButton = makeButton("Immagine precedente");
  m_aiLocalizationNextButton = makeButton("Immagine successiva");
  m_aiLocalizationPreviousButton->setEnabled(false);
  m_aiLocalizationNextButton->setEnabled(false);
  QObject::connect(m_aiLocalizationPreviousButton, &QPushButton::clicked, window(), [this]() {
    loadAiLocalizationLabelingImage(m_aiLocalizationLabelingIndex - 1);
  });
  QObject::connect(m_aiLocalizationNextButton, &QPushButton::clicked, window(), [this]() {
    loadAiLocalizationLabelingImage(m_aiLocalizationLabelingIndex + 1);
  });
  navigationLayout->addWidget(m_aiLocalizationPreviousButton, 0, 0);
  navigationLayout->addWidget(m_aiLocalizationNextButton, 0, 1);
  m_aiLocalizationLabelingStatus = new QLabel(
    QString("Etichettate: %1 / %2").arg(labeledCount).arg(rawImages.size()),
    navigation);
  m_aiLocalizationLabelingStatus->setObjectName("toolPanelNote");
  navigationLayout->addWidget(m_aiLocalizationLabelingStatus, 1, 0, 1, 2);
  layout->addWidget(navigation);

  auto* paths = new QLabel(
    QString("raw: %1\nmasks: %2\ndataset: %3\nmodels: %4")
      .arg(
        QDir::toNativeSeparators(aiLocalizationRawPath(recipes(), camera.id)),
        QDir::toNativeSeparators(aiLocalizationMasksPath(recipes(), camera.id)),
        QDir::toNativeSeparators(aiLocalizationDatasetPath(recipes(), camera.id)),
        QDir::toNativeSeparators(aiLocalizationModelsPath(recipes(), camera.id))),
    panel);
  paths->setObjectName("toolPanelNote");
  paths->setWordWrap(true);
  layout->addWidget(paths);

  auto* back = makeButton(tr("groups.localizationStrategies"));
  QObject::connect(back, &QPushButton::clicked, window(), [this, camera]() {
    context().showLocalizationStrategyList(camera);
  });
  layout->addWidget(back);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tr("tools.aiLocalization"));
}

void MainWindowSetupModule::showAiLocalizationTrainingPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  const QFileInfoList rawImages = QDir(aiLocalizationRawPath(recipes(), camera.id)).entryInfoList(
    RecipeJsonUtils::imageNameFilters(), QDir::Files, QDir::Name);
  int labeledCount = 0;
  for (const QFileInfo& rawImage : rawImages)
  {
    const QString labelPath = QDir(aiLocalizationLabelsPath(recipes(), camera.id))
      .filePath(rawImage.completeBaseName() + ".txt");
    if (QFileInfo::exists(labelPath))
    {
      ++labeledCount;
    }
  }

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(
    QString("Training localizzazione AI | %1").arg(camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* note = new QLabel(
    "Imposta i parametri e poi avvia. Il grafico live sostituira' l'immagine durante il training.",
    panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setSpacing(6);
  auto* epochsSpin = new QSpinBox(form);
  epochsSpin->setRange(1, 1000);
  epochsSpin->setValue(100);
  auto* imageSizeSpin = new QSpinBox(form);
  imageSizeSpin->setRange(128, 2048);
  imageSizeSpin->setSingleStep(32);
  imageSizeSpin->setValue(640);
  auto* batchSpin = new QSpinBox(form);
  batchSpin->setRange(-1, 256);
  batchSpin->setValue(-1);
  batchSpin->setSpecialValueText("Auto");
  auto* valSplitSpin = new QDoubleSpinBox(form);
  valSplitSpin->setRange(0.05, 0.5);
  valSplitSpin->setSingleStep(0.05);
  valSplitSpin->setDecimals(2);
  valSplitSpin->setValue(0.20);
  auto* deviceEdit = new QLineEdit("0", form);
  auto* modelEdit = new QLineEdit("yolo11n-seg.pt", form);

  int row = 0;
  formLayout->addWidget(new QLabel("Epoch", form), row, 0);
  formLayout->addWidget(epochsSpin, row++, 1);
  formLayout->addWidget(new QLabel("Dimensione immagine", form), row, 0);
  formLayout->addWidget(imageSizeSpin, row++, 1);
  formLayout->addWidget(new QLabel("Batch size", form), row, 0);
  formLayout->addWidget(batchSpin, row++, 1);
  formLayout->addWidget(new QLabel("Quota validation", form), row, 0);
  formLayout->addWidget(valSplitSpin, row++, 1);
  formLayout->addWidget(new QLabel("GPU / device", form), row, 0);
  formLayout->addWidget(deviceEdit, row++, 1);
  formLayout->addWidget(new QLabel("Modello base", form), row, 0);
  formLayout->addWidget(modelEdit, row++, 1);
  layout->addWidget(form);

  auto* summary = new QLabel(
    QString("Immagini raw: %1\nEtichettate utilizzabili: %2\nNon etichettate: %3\n\nOutput:\n%4")
      .arg(rawImages.size())
      .arg(labeledCount)
      .arg(rawImages.size() - labeledCount)
      .arg(QDir::toNativeSeparators(aiLocalizationModelsPath(recipes(), camera.id))),
    panel);
  summary->setObjectName("toolPanelNote");
  summary->setWordWrap(true);
  layout->addWidget(summary);

  auto* startButton = new QPushButton("Avvia training", panel);
  startButton->setObjectName("touchButton");
  startButton->setEnabled(labeledCount >= 2);
  QObject::connect(startButton, &QPushButton::clicked, window(),
    [this, camera, labeledCount, rawCount = rawImages.size(), epochsSpin,
     imageSizeSpin, batchSpin, deviceEdit, valSplitSpin, modelEdit]() {
      if (labeledCount < rawCount)
      {
        const auto choice = QMessageBox::question(
          window(),
          "Training preliminare",
          QString("Sono etichettate %1 immagini su %2.\n"
                  "Il training usera' solo quelle complete. Continuare?")
            .arg(labeledCount)
            .arg(rawCount),
          QMessageBox::Yes | QMessageBox::No,
          QMessageBox::Yes);
        if (choice != QMessageBox::Yes)
        {
          return;
        }
      }
      startAiLocalizationTraining(
        camera,
        epochsSpin->value(),
        imageSizeSpin->value(),
        batchSpin->value(),
        deviceEdit->text().trimmed().isEmpty() ? QString("0") : deviceEdit->text().trimmed(),
        valSplitSpin->value(),
        modelEdit->text().trimmed().isEmpty() ? QString("yolo11n-seg.pt") : modelEdit->text().trimmed());
    });
  layout->addWidget(startButton);

  auto* backButton = new QPushButton("Indietro", panel);
  backButton->setObjectName("touchButton");
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiLocalizationPanel(camera);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
}

void MainWindowSetupModule::startAiLocalizationTraining(
  const CameraConfig& camera,
  int epochs,
  int imageSize,
  int batchSize,
  const QString& device,
  double valRatio,
  const QString& baseModel)
{
  if (m_aiProcess && m_aiProcess->state() != QProcess::NotRunning)
  {
    log(tr("log.aiProcessBusy"));
    return;
  }

  stopAiLocalizationInferenceWorker();
  m_aiLocalizationTrainingCameraId = camera.id;
  m_aiTrainingGraphCameraId = camera.id;
  m_aiTrainingGraphIsLocalization = true;
  if (!m_aiTrainingGraphTimer)
  {
    m_aiTrainingGraphTimer = new QTimer(window());
    QObject::connect(m_aiTrainingGraphTimer, &QTimer::timeout, window(), [this]() {
      if (m_aiTrainingGraphIsLocalization)
      {
        updateAiLocalizationTrainingGraph(m_aiTrainingGraphCameraId);
      }
      else
      {
        updateAiTrainingGraph(m_aiTrainingGraphCameraId);
      }
    });
  }
  m_aiTrainingGraphTimer->start(2000);
  updateAiLocalizationTrainingGraph(camera.id);
  const QString prepareScript = aiProjectPath(
    "tools/ai/prepare_localization_segmentation_dataset.py");
  const QString trainScript = aiProjectPath(
    "tools/ai/train_localization_segmentation_yolo.py");
  const QString raw = QDir::cleanPath(aiLocalizationRawPath(recipes(), camera.id));
  const QString labels = QDir::cleanPath(aiLocalizationLabelsPath(recipes(), camera.id));
  const QString dataset = QDir::cleanPath(aiLocalizationDatasetPath(recipes(), camera.id));
  const QString models = QDir::cleanPath(aiLocalizationModelsPath(recipes(), camera.id));

  QStringList pythonPrefix = aiPythonProgram() == "py"
    ? QStringList{"py", "-3.11"}
    : QStringList{aiPythonProgram()};
  auto pythonCommand = [&pythonPrefix](const QStringList& args) {
    QStringList parts;
    for (const QString& item : pythonPrefix)
    {
      parts.append(aiPowerShellQuote(item));
    }
    for (const QString& item : args)
    {
      parts.append(aiPowerShellQuote(item));
    }
    return "& " + parts.join(' ');
  };

  const QString prepareCommand = pythonCommand({
    prepareScript,
    "--raw", raw,
    "--labels", labels,
    "--output", dataset,
    "--val-ratio", QString::number(valRatio, 'f', 2),
    "--seed", "42",
    "--clean"
  });
  const QString trainCommand = pythonCommand({
    trainScript,
    "--data", QDir(dataset).filePath("data.yaml"),
    "--model", baseModel,
    "--epochs", QString::number(epochs),
    "--imgsz", QString::number(imageSize),
    "--batch", QString::number(batchSize),
    "--device", device,
    "--project", models,
    "--name", camera.id + "_yolo11n_seg",
    "--export-onnx"
  });
  startAiProcess(
    tr("actions.trainAiModel"),
    "powershell",
    {
      "-NoProfile",
      "-ExecutionPolicy", "Bypass",
      "-Command",
      prepareCommand + "; if ($LASTEXITCODE -eq 0) { " + trainCommand + " }"
    });
}

void MainWindowSetupModule::startAiLocalizationLabeling(const CameraConfig& camera)
{
  const QFileInfoList rawImages = QDir(aiLocalizationRawPath(recipes(), camera.id)).entryInfoList(
    RecipeJsonUtils::imageNameFilters(), QDir::Files, QDir::Name);
  m_aiLocalizationLabelingCamera = camera;
  m_aiLocalizationLabelingImages.clear();
  int firstUnlabeled = -1;
  for (const QFileInfo& rawImage : rawImages)
  {
    m_aiLocalizationLabelingImages.append(rawImage.absoluteFilePath());
    const QString labelPath = QDir(aiLocalizationLabelsPath(recipes(), camera.id))
      .filePath(rawImage.completeBaseName() + ".txt");
    if (firstUnlabeled < 0 && !QFileInfo::exists(labelPath))
    {
      firstUnlabeled = m_aiLocalizationLabelingImages.size() - 1;
    }
  }
  if (m_aiLocalizationLabelingImages.isEmpty())
  {
    return;
  }
  loadAiLocalizationLabelingImage(firstUnlabeled >= 0 ? firstUnlabeled : 0);
}

void MainWindowSetupModule::loadAiLocalizationLabelingImage(int index)
{
  if (index < 0 || index >= m_aiLocalizationLabelingImages.size())
  {
    return;
  }
  const QString rawPath = m_aiLocalizationLabelingImages.at(index);
  selectedPreview() = QPixmap(rawPath);
  if (selectedPreview().isNull())
  {
    log(tr("log.imageMissing") + ": " + rawPath);
    return;
  }
  m_aiLocalizationLabelingIndex = index;
  selectedImagePath() = rawPath;
  largeImage()->setImage(selectedPreview());
  largeImage()->clearRoi();
  largeImage()->clearExclusionRects();
  largeImage()->clearCircles();
  largeImage()->clearGeometryOverlay();
  QString labelError;
  const QVector<QPoint> storedPolygon = AiMaskLabelStorage::loadPolygon(
    rawPath,
    aiLocalizationLabelsPath(recipes(), m_aiLocalizationLabelingCamera.id),
    &labelError);
  if (!labelError.isEmpty())
  {
    log(labelError);
  }
  largeImage()->setSearchPolygon(storedPolygon);
  largeImage()->setPolygonDrawingEnabled(storedPolygon.size() < 3);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::AiLocalization;
  if (context().largeTitle)
  {
    context().largeTitle->setText(QString("%1 | %2")
      .arg(tr("actions.labelPieceMasks"), QFileInfo(rawPath).fileName()));
  }
  updateAiLocalizationLabelingStatus();
}

void MainWindowSetupModule::advanceAiLocalizationLabeling()
{
  if (m_aiLocalizationLabelingIndex + 1 < m_aiLocalizationLabelingImages.size())
  {
    loadAiLocalizationLabelingImage(m_aiLocalizationLabelingIndex + 1);
    return;
  }
  updateAiLocalizationLabelingStatus();
  log("Labeling AI completato per il lotto corrente.");
}

void MainWindowSetupModule::updateAiLocalizationLabelingStatus()
{
  int labeledCount = 0;
  for (const QString& rawPath : m_aiLocalizationLabelingImages)
  {
    const QString labelPath = QDir(aiLocalizationLabelsPath(
      recipes(), m_aiLocalizationLabelingCamera.id))
      .filePath(QFileInfo(rawPath).completeBaseName() + ".txt");
    if (QFileInfo::exists(labelPath))
    {
      ++labeledCount;
    }
  }
  if (m_aiLocalizationLabelingStatus)
  {
    m_aiLocalizationLabelingStatus->setText(
      QString("Immagine %1/%2 | Etichettate %3 | Rimanenti %4")
        .arg(m_aiLocalizationLabelingIndex + 1)
        .arg(m_aiLocalizationLabelingImages.size())
        .arg(labeledCount)
        .arg(m_aiLocalizationLabelingImages.size() - labeledCount));
  }
  if (m_aiLocalizationPreviousButton)
  {
    m_aiLocalizationPreviousButton->setEnabled(m_aiLocalizationLabelingIndex > 0);
  }
  if (m_aiLocalizationNextButton)
  {
    m_aiLocalizationNextButton->setEnabled(
      m_aiLocalizationLabelingIndex >= 0 &&
      m_aiLocalizationLabelingIndex + 1 < m_aiLocalizationLabelingImages.size());
  }
}
