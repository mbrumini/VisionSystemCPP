#pragma once

#include <QJsonObject>
#include <QString>

enum class TestVisionFrameDeliveryMode
{
  CollectResults,
  SendOnly
};

struct TestVisionExecutionProfile
{
  QString shapeId = "cross";
  QString strategyId;
  QString recipeId;
  QString cameraId;
  double pixelSizeMm = 0.0654;
  int passes = 3;
  int intervalMs = 200;
  TestVisionFrameDeliveryMode frameDeliveryMode = TestVisionFrameDeliveryMode::CollectResults;
  double xMinMm = 0.0;
  double xMaxMm = 0.0;
  double xStepMm = 1.0;
  double yMinMm = 0.0;
  double yMaxMm = 0.0;
  double yStepMm = 1.0;
  double angleMinDeg = 0.0;
  double angleMaxDeg = 45.0;
  double angleStepDeg = 15.0;
  int canvasWidth = 640;
  int canvasHeight = 480;
  int canvasBackground = 240;
};

QString testVisionFrameDeliveryModeToString(TestVisionFrameDeliveryMode mode);
TestVisionFrameDeliveryMode testVisionFrameDeliveryModeFromString(const QString& value);

TestVisionExecutionProfile testVisionExecutionProfileFromJson(
  const QJsonObject& source,
  const TestVisionExecutionProfile& defaults = {});

QJsonObject testVisionExecutionProfileToJson(const TestVisionExecutionProfile& profile);

TestVisionExecutionProfile testVisionExecutionProfileFromScenario(const QJsonObject& scenario);

bool testVisionLoadExecutionProfileFile(
  const QString& path,
  TestVisionExecutionProfile* profile,
  QString* error);

bool testVisionSaveExecutionProfileFile(
  const QString& path,
  const TestVisionExecutionProfile& profile,
  QString* error);

bool testVisionSaveScenarioExecutionProfile(
  const QString& scenarioPath,
  const TestVisionExecutionProfile& profile,
  QString* error);

bool testVisionCanvasFitsProtocol(int width, int height);
QString testVisionCanvasSizeWarning(int width, int height);
