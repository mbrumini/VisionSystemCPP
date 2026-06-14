#include "gui/modules/MainWindowSetupModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/IconCatalog.h"
#include "gui/modules/MainWindowCameraConfigModule.h"
#include "gui/modules/MainWindowContext.h"

#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
QPushButton* createAiButton(const QString& iconId, const QString& label, QWidget* parent)
{
  auto* button = new QPushButton(IconCatalog::icon(iconId), label, parent);
  button->setObjectName("touchButton");
  button->setMinimumHeight(48);
  return button;
}

QString projectPath(const QString& relativePath)
{
  return RecipeJsonUtils::appPath(relativePath);
}

QString pythonProgram()
{
  const QString venvPython = projectPath(".venv-ai/Scripts/python.exe");
  if (QFileInfo::exists(venvPython))
  {
    return QDir::toNativeSeparators(venvPython);
  }
  return "py";
}

QStringList pythonArguments(const QStringList& scriptArguments)
{
  if (pythonProgram() == "py")
  {
    QStringList args = {"-3.11"};
    args.append(scriptArguments);
    return args;
  }
  return scriptArguments;
}

QString psQuote(const QString& text)
{
  QString escaped = QDir::toNativeSeparators(text);
  escaped.replace("'", "''");
  return "'" + escaped + "'";
}

QString classificationSourcePath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(recipes.cameraAiClassificationRawImagesPath(cameraId)).absoluteFilePath("../");
}

QString classificationDatasetPath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(RecipeManager::recipesRootPath()).filePath(
    QString("%1/datasets/%2/classification_yolo").arg(recipes.recipeId(), cameraId));
}

QString classificationModelPath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(RecipeManager::recipesRootPath()).filePath(
    QString("%1/models/classification/runs").arg(recipes.recipeId()));
}
}

void MainWindowSetupModule::showAiPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.ai"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(tr("labels.aiNote"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* grid = new QWidget(panel);
  auto* gridLayout = new QGridLayout(grid);
  gridLayout->setContentsMargins(0, 0, 0, 0);
  gridLayout->setSpacing(6);

  auto* classificationButton = createAiButton("aiClassification", tr("tools.aiClassification"), grid);
  QObject::connect(classificationButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiClassificationPanel(camera);
  });
  gridLayout->addWidget(classificationButton, 0, 0);

  auto* anomalyButton = createAiButton("aiAnomaly", tr("tools.aiAnomaly"), grid);
  QObject::connect(anomalyButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiPlaceholderPanel(camera, "aiAnomaly");
  });
  gridLayout->addWidget(anomalyButton, 0, 1);

  auto* segmentationButton = createAiButton("aiSegmentation", tr("tools.aiSegmentation"), grid);
  QObject::connect(segmentationButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiPlaceholderPanel(camera, "aiSegmentation");
  });
  gridLayout->addWidget(segmentationButton, 1, 0);

  layout->addWidget(grid);

  auto* backButton = createAiButton("back", tr("commands.backToCameraTools"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    context().showCameraToolList(camera);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tr("tools.ai"));
}

void MainWindowSetupModule::showAiPlaceholderPanel(const CameraConfig& camera, const QString& toolId)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools." + toolId), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(tr("labels.aiToolPlanned"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* labelsButton = createAiButton("classes", tr("actions.configureAiLabels"), panel);
  QObject::connect(labelsButton, &QPushButton::clicked, window(), [this, toolId]() {
    log(tr("log.placeholder") + ": " + tr("actions.configureAiLabels") + " -> " + tr("tools." + toolId));
  });
  layout->addWidget(labelsButton);

  auto* trainingButton = createAiButton("aiTrain", tr("actions.trainAiModel"), panel);
  QObject::connect(trainingButton, &QPushButton::clicked, window(), [this, toolId]() {
    log(tr("log.placeholder") + ": " + tr("actions.trainAiModel") + " -> " + tr("tools." + toolId));
  });
  layout->addWidget(trainingButton);

  auto* backButton = createAiButton("back", tr("tools.ai"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiPanel(camera);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tr("tools." + toolId));
}

void MainWindowSetupModule::prepareAiClassificationDataset(const CameraConfig& camera)
{
  const QString script = projectPath("tools/ai/prepare_classification_dataset.py");
  const QString source = QDir::cleanPath(classificationSourcePath(recipes(), camera.id));
  const QString output = QDir::cleanPath(classificationDatasetPath(recipes(), camera.id));
  startAiProcess(
    tr("actions.prepareAiDataset"),
    pythonProgram(),
    pythonArguments({
      script,
      "--source", source,
      "--output", output,
      "--val-ratio", "0.2",
      "--seed", "42",
      "--clean"
    }));
}

void MainWindowSetupModule::startAiClassificationTraining(const CameraConfig& camera)
{
  const QString python = pythonProgram();
  QStringList pythonPrefix;
  if (python == "py")
  {
    pythonPrefix = {"py", "-3.11"};
  }
  else
  {
    pythonPrefix = {python};
  }

  auto pythonCommand = [&pythonPrefix](const QStringList& args) {
    QStringList parts;
    for (const QString& item : pythonPrefix)
    {
      parts.append(psQuote(item));
    }
    for (const QString& item : args)
    {
      parts.append(psQuote(item));
    }
    return "& " + parts.join(' ');
  };

  const QString prepareScript = projectPath("tools/ai/prepare_classification_dataset.py");
  const QString script = projectPath("tools/ai/train_classification_yolo.py");
  const QString source = QDir::cleanPath(classificationSourcePath(recipes(), camera.id));
  const QString data = QDir::cleanPath(classificationDatasetPath(recipes(), camera.id));
  const QString project = QDir::cleanPath(classificationModelPath(recipes(), camera.id));

  const QString prepareCommand = pythonCommand({
    prepareScript,
    "--source", source,
    "--output", data,
    "--val-ratio", "0.2",
    "--seed", "42",
    "--clean"
  });
  const QString trainCommand = pythonCommand({
    script,
    "--data", data,
    "--model", "yolo11s-cls.pt",
    "--epochs", "100",
    "--imgsz", "224",
    "--batch", "8",
    "--device", "0",
    "--project", project,
    "--name", camera.id + "_yolo11s_cls",
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

void MainWindowSetupModule::startAiProcess(
  const QString& label,
  const QString& program,
  const QStringList& arguments)
{
  if (m_aiProcess && m_aiProcess->state() != QProcess::NotRunning)
  {
    log(tr("log.aiProcessBusy"));
    return;
  }

  if (!m_aiProcess)
  {
    m_aiProcess = new QProcess(window());
    m_aiProcess->setWorkingDirectory(RecipeJsonUtils::appRootPath());
    m_aiProcess->setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(m_aiProcess, &QProcess::readyReadStandardOutput, window(), [this]() {
      const QString text = QString::fromLocal8Bit(m_aiProcess->readAllStandardOutput()).trimmed();
      for (const QString& line : text.split('\n', Qt::SkipEmptyParts))
      {
        log("AI: " + line.trimmed());
      }
    });
    QObject::connect(m_aiProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), window(), [this](int code, QProcess::ExitStatus status) {
      log(QString("AI process finished: code=%1 status=%2").arg(code).arg(status == QProcess::NormalExit ? "normal" : "crash"));
    });
  }

  log(QString("AI process start: %1 -> %2 %3").arg(label, program, arguments.join(' ')));
  m_aiProcess->start(program, arguments);
}
