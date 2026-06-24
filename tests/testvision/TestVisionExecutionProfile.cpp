#include "TestVisionExecutionProfile.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

#include "simulator/SimulatorProtocol.h"

namespace
{
double readDouble(const QJsonObject& object, const char* key, double fallback)
{
  return object.contains(key) ? object.value(key).toDouble(fallback) : fallback;
}

int readInt(const QJsonObject& object, const char* key, int fallback)
{
  return object.contains(key) ? object.value(key).toInt(fallback) : fallback;
}

QString readString(const QJsonObject& object, const char* key, const QString& fallback)
{
  return object.contains(key) ? object.value(key).toString(fallback) : fallback;
}

bool saveJsonObject(const QString& path, const QJsonObject& object, QString* error)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
    if (error)
    {
      *error = "Impossibile salvare: " + path;
    }
    return false;
  }

  file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
  return true;
}

QJsonObject loadJsonObject(const QString& path, QString* error)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
  {
    if (error)
    {
      *error = "Impossibile aprire: " + path;
    }
    return {};
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    if (error)
    {
      *error = "JSON non valido: " + parseError.errorString();
    }
    return {};
  }

  return document.object();
}

TestVisionExecutionProfile profileFromLegacySequence(
  const QJsonObject& sequence,
  const TestVisionExecutionProfile& defaults)
{
  TestVisionExecutionProfile profile = defaults;
  profile.xMinMm = readDouble(sequence, "translationX", profile.xMinMm);
  profile.xMaxMm = readDouble(sequence, "translationX", profile.xMaxMm);
  profile.yMinMm = readDouble(sequence, "translationY", profile.yMinMm);
  profile.yMaxMm = readDouble(sequence, "translationY", profile.yMaxMm);
  profile.passes = qMax(1, readInt(sequence, "repetitions", profile.passes));

  const QJsonArray angles = sequence.value("anglesDeg").toArray();
  if (!angles.isEmpty())
  {
    double minAngle = angles.first().toDouble();
    double maxAngle = angles.first().toDouble();
    for (const QJsonValue& value : angles)
    {
      const double angle = value.toDouble();
      minAngle = qMin(minAngle, angle);
      maxAngle = qMax(maxAngle, angle);
    }
    profile.angleMinDeg = minAngle;
    profile.angleMaxDeg = maxAngle;
    if (angles.size() > 1)
    {
      profile.angleStepDeg = qAbs(angles.at(1).toDouble() - angles.at(0).toDouble());
      if (profile.angleStepDeg < 0.000001)
      {
        profile.angleStepDeg = defaults.angleStepDeg;
      }
    }
  }

  return profile;
}
}

bool testVisionCanvasFitsProtocol(int width, int height)
{
  if (width <= 0 || height <= 0)
  {
    return false;
  }

  const qint64 bytes =
    static_cast<qint64>(width) * static_cast<qint64>(height) * 3LL + 54LL;
  return bytes <= SimulatorProtocol::kMaximumImagePayloadBytes;
}

QString testVisionCanvasSizeWarning(int width, int height)
{
  if (testVisionCanvasFitsProtocol(width, height))
  {
    const qint64 bytes =
      static_cast<qint64>(width) * static_cast<qint64>(height) * 3LL + 54LL;
    return QString("BMP stimato: %1 KB").arg((bytes + 1023) / 1024);
  }

  return QString(
    "Attenzione: %1×%2 supera il limite BMP di 12 MB per il simulatore")
    .arg(width)
    .arg(height);
}

QString testVisionFrameDeliveryModeToString(TestVisionFrameDeliveryMode mode)
{
  return mode == TestVisionFrameDeliveryMode::SendOnly ? "sendOnly" : "collectResults";
}

TestVisionFrameDeliveryMode testVisionFrameDeliveryModeFromString(const QString& value)
{
  if (value.compare("sendOnly", Qt::CaseInsensitive) == 0 ||
      value.compare("send_only", Qt::CaseInsensitive) == 0)
  {
    return TestVisionFrameDeliveryMode::SendOnly;
  }
  return TestVisionFrameDeliveryMode::CollectResults;
}

TestVisionExecutionProfile testVisionExecutionProfileFromJson(
  const QJsonObject& source,
  const TestVisionExecutionProfile& defaults)
{
  TestVisionExecutionProfile profile = defaults;
  if (source.isEmpty())
  {
    return profile;
  }

  profile.shapeId = readString(source, "shapeId", profile.shapeId);
  profile.strategyId = readString(source, "strategyId", profile.strategyId);
  profile.recipeId = readString(source, "recipeId", profile.recipeId);
  profile.cameraId = readString(source, "cameraId", profile.cameraId);
  profile.pixelSizeMm = readDouble(source, "pixelSizeMm", profile.pixelSizeMm);
  profile.passes = qMax(1, readInt(source, "passes", profile.passes));
  profile.intervalMs = qMax(0, readInt(source, "intervalMs", profile.intervalMs));
  if (source.contains("frameDeliveryMode"))
  {
    profile.frameDeliveryMode =
      testVisionFrameDeliveryModeFromString(source.value("frameDeliveryMode").toString());
  }
  else if (source.contains("sendOnly"))
  {
    profile.frameDeliveryMode = source.value("sendOnly").toBool(false)
      ? TestVisionFrameDeliveryMode::SendOnly
      : TestVisionFrameDeliveryMode::CollectResults;
  }

  profile.xMinMm = readDouble(source, "xMinMm", profile.xMinMm);
  profile.xMaxMm = readDouble(source, "xMaxMm", profile.xMaxMm);
  profile.xStepMm = readDouble(source, "xStepMm", profile.xStepMm);
  profile.yMinMm = readDouble(source, "yMinMm", profile.yMinMm);
  profile.yMaxMm = readDouble(source, "yMaxMm", profile.yMaxMm);
  profile.yStepMm = readDouble(source, "yStepMm", profile.yStepMm);
  profile.angleMinDeg = readDouble(source, "angleMinDeg", profile.angleMinDeg);
  profile.angleMaxDeg = readDouble(source, "angleMaxDeg", profile.angleMaxDeg);
  profile.angleStepDeg = readDouble(source, "angleStepDeg", profile.angleStepDeg);
  profile.canvasWidth = qMax(1, readInt(source, "canvasWidth", profile.canvasWidth));
  profile.canvasHeight = qMax(1, readInt(source, "canvasHeight", profile.canvasHeight));
  profile.canvasBackground = readInt(source, "canvasBackground", profile.canvasBackground);
  return profile;
}

TestVisionExecutionProfile testVisionExecutionProfileFromScenario(const QJsonObject& scenario)
{
  TestVisionExecutionProfile defaults;
  defaults.strategyId = scenario.value("strategyId").toString();
  defaults.recipeId = scenario.value("recipeId").toString();
  defaults.cameraId = scenario.value("cameraId").toString();

  const QJsonObject canvas = scenario.value("canvas").toObject();
  defaults.canvasWidth = qMax(1, canvas.value("width").toInt(defaults.canvasWidth));
  defaults.canvasHeight = qMax(1, canvas.value("height").toInt(defaults.canvasHeight));
  defaults.canvasBackground = canvas.value("background").toInt(defaults.canvasBackground);

  if (scenario.contains("executionProfile"))
  {
    return testVisionExecutionProfileFromJson(
      scenario.value("executionProfile").toObject(), defaults);
  }

  if (scenario.contains("sequence"))
  {
    return profileFromLegacySequence(
      scenario.value("sequence").toObject(), defaults);
  }

  return defaults;
}

QJsonObject testVisionExecutionProfileToJson(const TestVisionExecutionProfile& profile)
{
  QJsonObject object;
  if (!profile.shapeId.isEmpty())
  {
    object["shapeId"] = profile.shapeId;
  }
  if (!profile.strategyId.isEmpty())
  {
    object["strategyId"] = profile.strategyId;
  }
  if (!profile.recipeId.isEmpty())
  {
    object["recipeId"] = profile.recipeId;
  }
  if (!profile.cameraId.isEmpty())
  {
    object["cameraId"] = profile.cameraId;
  }
  object["pixelSizeMm"] = profile.pixelSizeMm;
  object["passes"] = profile.passes;
  object["intervalMs"] = profile.intervalMs;
  object["frameDeliveryMode"] = testVisionFrameDeliveryModeToString(profile.frameDeliveryMode);
  object["sendOnly"] = profile.frameDeliveryMode == TestVisionFrameDeliveryMode::SendOnly;
  object["xMinMm"] = profile.xMinMm;
  object["xMaxMm"] = profile.xMaxMm;
  object["xStepMm"] = profile.xStepMm;
  object["yMinMm"] = profile.yMinMm;
  object["yMaxMm"] = profile.yMaxMm;
  object["yStepMm"] = profile.yStepMm;
  object["angleMinDeg"] = profile.angleMinDeg;
  object["angleMaxDeg"] = profile.angleMaxDeg;
  object["angleStepDeg"] = profile.angleStepDeg;
  object["canvasWidth"] = profile.canvasWidth;
  object["canvasHeight"] = profile.canvasHeight;
  object["canvasBackground"] = profile.canvasBackground;
  return object;
}

bool testVisionLoadExecutionProfileFile(
  const QString& path,
  TestVisionExecutionProfile* profile,
  QString* error)
{
  if (!profile)
  {
    return false;
  }

  const QJsonObject root = loadJsonObject(path, error);
  if (root.isEmpty())
  {
    return false;
  }

  const QJsonObject source = root.contains("executionProfile")
    ? root.value("executionProfile").toObject()
    : root;
  *profile = testVisionExecutionProfileFromJson(source);
  return true;
}

bool testVisionSaveExecutionProfileFile(
  const QString& path,
  const TestVisionExecutionProfile& profile,
  QString* error)
{
  QJsonObject root;
  root["name"] = QFileInfo(path).completeBaseName();
  root["executionProfile"] = testVisionExecutionProfileToJson(profile);
  return saveJsonObject(path, root, error);
}

bool testVisionSaveScenarioExecutionProfile(
  const QString& scenarioPath,
  const TestVisionExecutionProfile& profile,
  QString* error)
{
  QJsonObject scenario = loadJsonObject(scenarioPath, error);
  if (scenario.isEmpty())
  {
    return false;
  }

  scenario["executionProfile"] = testVisionExecutionProfileToJson(profile);

  QJsonObject canvas = scenario.value("canvas").toObject();
  canvas["width"] = profile.canvasWidth;
  canvas["height"] = profile.canvasHeight;
  canvas["background"] = profile.canvasBackground;
  scenario["canvas"] = canvas;

  return saveJsonObject(scenarioPath, scenario, error);
}
