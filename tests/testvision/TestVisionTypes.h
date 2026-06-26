#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVector>

struct TestPose
{
  int poseIndex = 0;
  int pass = 1;
  double xMm = 0.0;
  double yMm = 0.0;
  double angleDeg = 0.0;
};

struct SentFrameInfo
{
  TestPose pose;
  double expectedX = 0.0;
  double expectedY = 0.0;
  double expectedAngle = 0.0;
};

struct TestMeasurementReading
{
  QString id;
  QString type;
  QString unit;
  bool valid = false;
  double valuePixels = 0.0;
  double valueReal = 0.0;
  bool hasRealValue = false;
  double expectedBaselinePixels = 0.0;
  double expectedRecipePixels = 0.0;
  bool hasExpectedBaseline = false;
  bool hasExpectedRecipe = false;
  double baselineErrorPixels = 0.0;
  double recipeErrorPixels = 0.0;
  int sampleCount = 0;
  int pointCount = 0;
  QString diagnostic;
};

struct TestResult
{
  TestPose pose;
  int frameId = 0;
  bool valid = false;
  double expectedX = 0.0;
  double expectedY = 0.0;
  double expectedAngle = 0.0;
  double actualX = 0.0;
  double actualY = 0.0;
  double actualAngle = 0.0;
  double centerError = 0.0;
  double angleError = 0.0;
  double processingMs = 0.0;
  QVector<TestMeasurementReading> measurements;
};

struct TestPoseSummary
{
  double centerMean = 0.0;
  double centerMax = 0.0;
  double angleMean = 0.0;
  double angleMax = 0.0;
  double timeMean = 0.0;
  double steadyTimeMean = 0.0;
  double coldStartMs = 0.0;
  double timeMax = 0.0;
  int validCount = 0;
  int frameCount = 0;
  int invalidCount = 0;
  double worstRepeatCenter = 0.0;
  double worstRepeatAngle = 0.0;
};

struct TestMeasurementSummary
{
  int rowCount = 0;
  double sumBaselineError = 0.0;
  double maxBaselineError = 0.0;
  double maxRecipeError = 0.0;
  double correlation = 0.0;
  QHash<QString, double> maxErrorById;
};
