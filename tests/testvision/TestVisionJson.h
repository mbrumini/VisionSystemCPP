#pragma once

#include "TestVisionTypes.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

QJsonObject testVisionLoadJson(const QString& path, QString* error = nullptr);
QString testVisionResolveRecipesRoot();
QHash<QString, double> testVisionLoadRecipeMeasurementNominals(
  const QString& recipeId,
  const QString& cameraId);
QVector<TestMeasurementReading> testVisionParseMeasurementReadings(const QJsonArray& measurements);
void testVisionApplyMeasurementExpectations(
  QVector<TestMeasurementReading>& readings,
  const QHash<QString, double>& baselinePixels,
  const QHash<QString, double>& recipeNominals,
  bool captureBaseline);
