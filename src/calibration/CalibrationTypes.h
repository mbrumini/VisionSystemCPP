#pragma once

#include <QPointF>
#include <QSize>
#include <QString>
#include <QVector>

enum class CalibrationPatternType
{
  Checkerboard,
  DotGrid,
  CircleGrid,
  Custom
};

struct CalibrationPatternConfig
{
  CalibrationPatternType type = CalibrationPatternType::Checkerboard;
  QSize gridSize;
  double pitchMm = 1.0;
  QString assetPath;
};

struct CalibrationFrame
{
  QString imagePath;
  QVector<QPointF> imagePoints;
  QVector<QPointF> patternPointsMm;
  bool accepted = false;
};

struct CameraCalibrationModel
{
  QString cameraId;
  QString modelId;
  QString calibrationType = "none";
  QString sourceFile;
  double pixelSizeXMm = 0.0;
  double pixelSizeYMm = 0.0;
  double rotationDegrees = 0.0;
  double rmsErrorPixels = 0.0;
  double averageHorizontalPitchPixels = 0.0;
  double averageVerticalPitchPixels = 0.0;
  QPointF originImagePoint;
  QPointF originWorldMm;
  bool valid = false;
};
