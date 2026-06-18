#include "calibration/CalibrationRecipe.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

CameraCalibrationModel CalibrationRecipe::load(const QString& cameraRecipePath) const
{
  CameraCalibrationModel model;
  QFile file(cameraRecipePath);
  if (!file.open(QIODevice::ReadOnly))
  {
    return model;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    return model;
  }

  const QJsonObject root = document.object();
  model.cameraId = root.value("cameraId").toString();
  model.modelId = root.value("modelId").toString();
  model.calibrationType = root.value("calibrationType").toString("none");
  model.sourceFile = root.value("sourceFile").toString();
  model.pixelSizeXMm = root.value("pixelSizeXMm").toDouble();
  model.pixelSizeYMm = root.value("pixelSizeYMm").toDouble(model.pixelSizeXMm);
  model.rotationDegrees = root.value("rotationDegrees").toDouble();
  model.rmsErrorPixels = root.value("rmsErrorPixels").toDouble();
  model.averageHorizontalPitchPixels = root.value("averageHorizontalPitchPixels").toDouble();
  model.averageVerticalPitchPixels = root.value("averageVerticalPitchPixels").toDouble();
  model.valid = root.value("valid").toBool(false);
  const QJsonObject originImage = root.value("originImagePoint").toObject();
  model.originImagePoint = QPointF(originImage.value("x").toDouble(), originImage.value("y").toDouble());
  const QJsonObject originWorld = root.value("originWorldMm").toObject();
  model.originWorldMm = QPointF(originWorld.value("x").toDouble(), originWorld.value("y").toDouble());
  return model;
}

bool CalibrationRecipe::save(const QString& cameraRecipePath, const CameraCalibrationModel& model, QString* errorMessage) const
{
  QFile file(cameraRecipePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile scrivere calibrazione: " + cameraRecipePath;
    }
    return false;
  }

  QJsonObject root;
  root["cameraId"] = model.cameraId;
  root["modelId"] = model.modelId;
  root["calibrationType"] = model.calibrationType;
  root["sourceFile"] = model.sourceFile;
  root["pixelSizeXMm"] = model.pixelSizeXMm;
  root["pixelSizeYMm"] = model.pixelSizeYMm;
  root["rotationDegrees"] = model.rotationDegrees;
  root["rmsErrorPixels"] = model.rmsErrorPixels;
  root["averageHorizontalPitchPixels"] = model.averageHorizontalPitchPixels;
  root["averageVerticalPitchPixels"] = model.averageVerticalPitchPixels;
  root["valid"] = model.valid;
  QJsonObject originImage;
  originImage["x"] = model.originImagePoint.x();
  originImage["y"] = model.originImagePoint.y();
  root["originImagePoint"] = originImage;
  QJsonObject originWorld;
  originWorld["x"] = model.originWorldMm.x();
  originWorld["y"] = model.originWorldMm.y();
  root["originWorldMm"] = originWorld;

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
}
