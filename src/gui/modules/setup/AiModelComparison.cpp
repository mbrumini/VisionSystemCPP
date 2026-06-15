#include "gui/modules/setup/AiModelComparison.h"

#include "config/RecipeJsonUtils.h"
#include "gui/modules/setup/AiPythonRuntime.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

namespace
{
QString modelComparisonLine(const QString& label, const QJsonObject& object)
{
  return QString("%1: accuracy=%2  corretti=%3/%4  confidenza media=%5")
    .arg(label)
    .arg(object.value("accuracy").toDouble(), 0, 'f', 4)
    .arg(object.value("correct").toInt())
    .arg(object.value("total").toInt())
    .arg(object.value("avg_confidence").toDouble(), 0, 'f', 4);
}
}

QString compareAiClassificationModels(
  const QString& oldModelPath,
  const QString& newModelPath,
  const QString& validationPath,
  QString* recommendedModelPath,
  QString* errorMessage)
{
  const QString script = aiProjectPath("tools/ai/compare_classification_models.py");
  if (!QFileInfo::exists(script))
  {
    if (errorMessage)
    {
      *errorMessage = "Script confronto modelli mancante: " + script;
    }
    return {};
  }

  QProcess process;
  process.setWorkingDirectory(RecipeJsonUtils::appRootPath());
  process.setProgram(aiPythonProgram());
  process.setArguments(aiPythonArguments({
    script,
    "--old", oldModelPath,
    "--new", newModelPath,
    "--data", validationPath,
    "--device", "0"
  }));
  process.start();
  if (!process.waitForStarted(5000) || !process.waitForFinished(120000))
  {
    if (errorMessage)
    {
      *errorMessage = "Confronto modelli non completato.";
    }
    return {};
  }

  const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
  const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  if (process.exitCode() != 0)
  {
    if (errorMessage)
    {
      *errorMessage = stderrText.isEmpty() ? stdoutText : stderrText;
    }
    return {};
  }

  const QJsonDocument document = QJsonDocument::fromJson(stdoutText.toUtf8());
  if (!document.isObject())
  {
    if (errorMessage)
    {
      *errorMessage = "Output confronto modelli non valido.";
    }
    return {};
  }

  const QJsonObject root = document.object();
  const QJsonObject oldResult = root.value("old").toObject();
  const QJsonObject newResult = root.value("new").toObject();
  const double oldAccuracy = oldResult.value("accuracy").toDouble();
  const double newAccuracy = newResult.value("accuracy").toDouble();
  const double oldConfidence = oldResult.value("avg_confidence").toDouble();
  const double newConfidence = newResult.value("avg_confidence").toDouble();
  const bool newIsBetter =
    newAccuracy > oldAccuracy ||
    (qFuzzyCompare(newAccuracy, oldAccuracy) && newConfidence > oldConfidence);

  if (recommendedModelPath)
  {
    *recommendedModelPath = newIsBetter ? newModelPath : oldModelPath;
  }

  return QString("Confronto su validation set:\n%1\n%2\n\nConsiglio: %3")
    .arg(modelComparisonLine("Vecchio", oldResult))
    .arg(modelComparisonLine("Nuovo", newResult))
    .arg(newIsBetter ? "usa il nuovo modello" : "mantieni il modello precedente");
}
