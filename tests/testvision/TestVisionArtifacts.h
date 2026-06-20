#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <opencv2/core.hpp>

struct TestVisionDatasetImage
{
  QString fileName;
  cv::Mat image;
  QJsonObject metadata;
};

QString testVisionTimestamp();

QString saveVersionedTestReport(
  const QString& configuredOutputPath,
  const QJsonObject& report,
  QString* errorMessage = nullptr);

QString saveTestVisionDataset(
  const QString& datasetsRoot,
  const QString& scenarioName,
  const QVector<TestVisionDatasetImage>& images,
  QString* errorMessage = nullptr);

QString saveTestVisionLabelingBatch(
  const QString& rawImagesDirectory,
  const QString& scenarioName,
  const QVector<TestVisionDatasetImage>& images,
  QString* errorMessage = nullptr);
