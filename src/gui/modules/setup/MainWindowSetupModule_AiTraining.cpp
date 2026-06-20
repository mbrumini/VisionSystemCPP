#include "gui/modules/MainWindowSetupModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/modules/setup/AiClassificationPaths.h"
#include "gui/modules/setup/AiModelComparison.h"
#include "gui/modules/setup/AiPythonRuntime.h"
#include "gui/modules/setup/AiTrainingGraph.h"
#include "gui/modules/setup/AiLocalizationPaths.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>

void MainWindowSetupModule::prepareAiClassificationDataset(const CameraConfig& camera)
{
  const QString script = aiProjectPath("tools/ai/prepare_classification_dataset.py");
  const QString source = QDir::cleanPath(aiClassificationSourcePath(recipes(), camera.id));
  const QString output = QDir::cleanPath(aiClassificationDatasetPath(recipes(), camera.id));
  startAiProcess(
    tr("actions.prepareAiDataset"),
    aiPythonProgram(),
    aiPythonArguments({
      script,
      "--source", source,
      "--output", output,
      "--val-ratio", "0.2",
      "--seed", "42",
      "--clean"
    }));
}

void MainWindowSetupModule::startAiClassificationTraining(
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

  stopAiInferenceWorker();
  m_aiTrainingCameraId = camera.id;
  m_aiTrainingPreviousModelPath = aiClassificationModelFilePath(recipes(), camera.id);

  const QString python = aiPythonProgram();
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
      parts.append(aiPowerShellQuote(item));
    }
    for (const QString& item : args)
    {
      parts.append(aiPowerShellQuote(item));
    }
    return "& " + parts.join(' ');
  };

  const QString prepareScript = aiProjectPath("tools/ai/prepare_classification_dataset.py");
  const QString script = aiProjectPath("tools/ai/train_classification_yolo.py");
  const QString source = QDir::cleanPath(aiClassificationSourcePath(recipes(), camera.id));
  const QString data = QDir::cleanPath(aiClassificationDatasetPath(recipes(), camera.id));
  const QString project = QDir::cleanPath(aiClassificationModelRunsPath(recipes(), camera.id));

  const QString prepareCommand = pythonCommand({
    prepareScript,
    "--source", source,
    "--output", data,
    "--val-ratio", QString::number(valRatio, 'f', 2),
    "--seed", "42",
    "--clean"
  });
  const QString trainCommand = pythonCommand({
    script,
    "--data", data,
    "--model", baseModel,
    "--epochs", QString::number(epochs),
    "--imgsz", QString::number(imageSize),
    "--batch", QString::number(batchSize),
    "--device", device,
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
      if (!m_aiLocalizationTrainingCameraId.isEmpty())
      {
        stopAiTrainingGraphUpdates();
        if (code == 0)
        {
          log(QString("Training localizzazione AI completato: %1 -> %2")
            .arg(m_aiLocalizationTrainingCameraId)
            .arg(aiLocalizationModelsPath(recipes(), m_aiLocalizationTrainingCameraId)));
        }
        m_aiLocalizationTrainingCameraId.clear();
        return;
      }
      handleAiTrainingFinished(code);
    });
  }

  log(QString("AI process start: %1 -> %2 %3").arg(label, program, arguments.join(' ')));
  m_aiProcess->start(program, arguments);
}

void MainWindowSetupModule::chooseAiClassificationModel(const CameraConfig& camera)
{
  const QString modelRoot = QDir(RecipeManager::recipesRootPath()).filePath(
    QString("%1/models/classification").arg(recipes().recipeId()));
  const QString selected = QFileDialog::getOpenFileName(
    window(),
    "Scegli modello classificazione",
    modelRoot,
    "YOLO model (*.pt);;All files (*.*)");

  if (selected.isEmpty())
  {
    return;
  }

  QString error;
  if (!recipes().saveAiClassificationActiveModelPath(camera.id, selected, &error))
  {
    QMessageBox::warning(window(), "Scegli modello", error);
    return;
  }

  stopAiInferenceWorker();
  log(QString("AI active model selected: %1 -> %2").arg(camera.id, selected));
  showAiClassificationPanel(camera);
}

void MainWindowSetupModule::handleAiTrainingFinished(int code)
{
  stopAiTrainingGraphUpdates();
  if (m_aiTrainingCameraId.isEmpty())
  {
    return;
  }

  const QString cameraId = m_aiTrainingCameraId;
  const QString previousModelPath = m_aiTrainingPreviousModelPath;
  m_aiTrainingCameraId.clear();
  m_aiTrainingPreviousModelPath.clear();

  if (code != 0)
  {
    return;
  }

  const QString newestModelPath = aiNewestClassificationModelFilePath(recipes(), cameraId);
  if (newestModelPath.isEmpty() || !QFileInfo::exists(newestModelPath) || newestModelPath == previousModelPath)
  {
    return;
  }

  QString comparisonText;
  QString recommendedModelPath;
  QString comparisonError;
  if (!previousModelPath.isEmpty() && QFileInfo::exists(previousModelPath))
  {
    comparisonText = compareAiClassificationModels(
      previousModelPath,
      newestModelPath,
      aiClassificationValidationPath(recipes(), cameraId),
      &recommendedModelPath,
      &comparisonError);
  }

  if (comparisonText.isEmpty())
  {
    comparisonText = comparisonError.isEmpty()
      ? "Confronto automatico non disponibile."
      : "Confronto automatico non disponibile:\n" + comparisonError;
    recommendedModelPath = newestModelPath;
  }

  const QMessageBox::StandardButton choice = QMessageBox::question(
    window(),
    "Scelta modello",
    QString("%1\n\nNuovo modello:\n%2\n\nModello precedente:\n%3\n\nUsare il nuovo modello come modello attivo?")
      .arg(comparisonText)
      .arg(newestModelPath)
      .arg(previousModelPath.isEmpty() ? QString("nessuno") : previousModelPath),
    QMessageBox::Yes | QMessageBox::No,
    recommendedModelPath == newestModelPath ? QMessageBox::Yes : QMessageBox::No);

  if (choice != QMessageBox::Yes)
  {
    log(QString("AI training model kept previous: %1").arg(previousModelPath));
    return;
  }

  QString error;
  if (!recipes().saveAiClassificationActiveModelPath(cameraId, newestModelPath, &error))
  {
    QMessageBox::warning(window(), "Training completato", error);
    return;
  }

  stopAiInferenceWorker();
  log(QString("AI training model promoted: %1 -> %2").arg(cameraId, newestModelPath));
}

void MainWindowSetupModule::updateAiTrainingGraph(const QString& cameraId)
{
  if (cameraId.isEmpty() || !largeImage())
  {
    return;
  }

  selectedPreview() = renderAiClassificationTrainingGraph(recipes(), cameraId);
  largeImage()->setImage(selectedPreview());
}

void MainWindowSetupModule::updateAiLocalizationTrainingGraph(const QString& cameraId)
{
  if (cameraId.isEmpty() || !largeImage())
  {
    return;
  }
  selectedPreview() = renderAiLocalizationTrainingGraph(recipes(), cameraId);
  largeImage()->setImage(selectedPreview());
}

void MainWindowSetupModule::stopAiTrainingGraphUpdates()
{
  if (m_aiTrainingGraphTimer)
  {
    m_aiTrainingGraphTimer->stop();
  }
  m_aiTrainingGraphCameraId.clear();
  m_aiTrainingGraphIsLocalization = false;
}

