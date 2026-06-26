#include "TestVisionJson.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

QJsonObject testVisionLoadJson(const QString& path, QString* error)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
  {
    if (error)
    {
      *error = "Impossibile aprire scenario: " + path;
    }
    return {};
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    if (error)
    {
      *error = "Scenario JSON non valido: " + parseError.errorString();
    }
    return {};
  }
  return document.object();
}

QString testVisionResolveRecipesRoot()
{
  const QString fromExe =
    QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../recipes");
  if (QDir(fromExe).exists())
  {
    return QDir(fromExe).canonicalPath();
  }

  const QString fromCwd = QDir::current().absoluteFilePath("recipes");
  if (QDir(fromCwd).exists())
  {
    return QDir(fromCwd).canonicalPath();
  }

  return fromExe;
}

QHash<QString, double> testVisionLoadRecipeMeasurementNominals(
  const QString& recipeId,
  const QString& cameraId)
{
  QHash<QString, double> nominals;
  if (recipeId.trimmed().isEmpty() || cameraId.trimmed().isEmpty())
  {
    return nominals;
  }

  const QString path = QDir(testVisionResolveRecipesRoot()).filePath(
    QString("%1/cameras/%2.json").arg(recipeId, cameraId));
  QString error;
  const QJsonObject root = testVisionLoadJson(path, &error);
  if (root.isEmpty())
  {
    return nominals;
  }

  const QJsonObject tools = root.value("tools").toObject();
  const QJsonArray items = tools.value("measurements").toObject().value("items").toArray();
  for (const QJsonValue& value : items)
  {
    const QJsonObject item = value.toObject();
    const QString id = item.value("id").toString();
    const double samplePixels = item.value("samplePixels").toDouble(0.0);
    if (!id.isEmpty() && samplePixels > 0.0)
    {
      nominals.insert(id, samplePixels);
    }
  }

  const QJsonObject threadInspection = tools.value("threadInspection").toObject();
  if (threadInspection.value("enabled").toBool())
  {
    static const QHash<QString, QString> threadKeys = {
      {"major", "thread_major"},
      {"pitch", "thread_pitch"},
      {"minor", "thread_minor"}
    };
    const QJsonObject threadMeasurements = threadInspection.value("measurements").toObject();
    for (auto it = threadKeys.cbegin(); it != threadKeys.cend(); ++it)
    {
      const QJsonObject limits = threadMeasurements.value(it.key()).toObject();
      const double samplePixels = limits.value("samplePixels").toDouble(0.0);
      if (samplePixels > 0.0)
      {
        nominals.insert(it.value(), samplePixels);
      }
    }
  }

  return nominals;
}

QVector<TestMeasurementReading> testVisionParseMeasurementReadings(const QJsonArray& measurements)
{
  QVector<TestMeasurementReading> readings;
  for (const QJsonValue& value : measurements)
  {
    const QJsonObject item = value.toObject();
    TestMeasurementReading reading;
    reading.id = item.value("id").toString();
    reading.type = item.value("type").toString();
    reading.unit = item.value("unit").toString("px");
    reading.valid = item.value("valid").toBool();
    reading.valuePixels = item.value("valuePixels").toDouble();
    reading.hasRealValue = item.value("hasRealValue").toBool();
    reading.valueReal = item.value("valueReal").toDouble();
    reading.sampleCount = item.value("sampleCount").toInt(0);
    reading.pointCount = item.value("pointCount").toInt(0);
    reading.diagnostic = item.value("diagnostic").toString();
    if (!reading.id.isEmpty())
    {
      readings.append(reading);
    }
  }
  return readings;
}

void testVisionApplyMeasurementExpectations(
  QVector<TestMeasurementReading>& readings,
  const QHash<QString, double>& baselinePixels,
  const QHash<QString, double>& recipeNominals,
  bool captureBaseline)
{
  for (TestMeasurementReading& reading : readings)
  {
    if (!reading.valid)
    {
      continue;
    }

    if (recipeNominals.contains(reading.id))
    {
      reading.expectedRecipePixels = recipeNominals.value(reading.id);
      reading.hasExpectedRecipe = true;
      reading.recipeErrorPixels =
        std::abs(reading.valuePixels - reading.expectedRecipePixels);
    }

    if (captureBaseline)
    {
      reading.expectedBaselinePixels = reading.valuePixels;
      reading.hasExpectedBaseline = true;
      reading.baselineErrorPixels = 0.0;
      continue;
    }

    if (baselinePixels.contains(reading.id))
    {
      reading.expectedBaselinePixels = baselinePixels.value(reading.id);
      reading.hasExpectedBaseline = true;
      reading.baselineErrorPixels =
        std::abs(reading.valuePixels - reading.expectedBaselinePixels);
    }
  }
}
