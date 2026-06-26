#include "TestVisionArtifacts.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

#include <opencv2/imgcodecs.hpp>

QString testVisionTimestamp()
{
  return QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
}

QString saveVersionedTestReport(
  const QString& configuredOutputPath,
  const QJsonObject& report,
  QString* errorMessage)
{
  const QFileInfo configuredInfo(configuredOutputPath);
  const QDir outputDirectory(configuredInfo.dir());
  if (!QDir().mkpath(outputDirectory.absolutePath()))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare cartella report: " + outputDirectory.absolutePath();
    }
    return {};
  }

  const QString timestamp = testVisionTimestamp();
  const QString reportPath = outputDirectory.filePath(timestamp + ".json");

  const QByteArray data = QJsonDocument(report).toJson(QJsonDocument::Indented);
  QFile reportFile(reportPath);
  if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      reportFile.write(data) != data.size())
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile salvare report: " + reportPath;
    }
    return {};
  }
  return reportPath;
}

QString saveTestVisionDataset(
  const QString& datasetsRoot,
  const QString& scenarioName,
  const QVector<TestVisionDatasetImage>& images,
  QString* errorMessage)
{
  if (images.isEmpty())
  {
    if (errorMessage)
    {
      *errorMessage = "Nessuna immagine da salvare nel dataset.";
    }
    return {};
  }

  QString safeScenarioName = scenarioName.trimmed();
  safeScenarioName.replace(QRegularExpression(R"([^A-Za-z0-9_.-]+)"), "_");
  if (safeScenarioName.isEmpty())
  {
    safeScenarioName = "scenario";
  }
  const QString datasetDirectory = QDir(datasetsRoot).filePath(
    safeScenarioName + "/" + testVisionTimestamp());
  const QString imagesDirectory = QDir(datasetDirectory).filePath("images");
  if (!QDir().mkpath(imagesDirectory))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare dataset: " + datasetDirectory;
    }
    return {};
  }

  QJsonArray manifestImages;
  for (const TestVisionDatasetImage& item : images)
  {
    const QString imagePath = QDir(imagesDirectory).filePath(item.fileName);
    if (item.image.empty() || !cv::imwrite(imagePath.toStdString(), item.image))
    {
      if (errorMessage)
      {
        *errorMessage = "Impossibile salvare immagine dataset: " + imagePath;
      }
      return {};
    }
    QJsonObject metadata = item.metadata;
    metadata["file"] = "images/" + item.fileName;
    manifestImages.append(metadata);
  }

  QJsonObject manifest;
  manifest["scenario"] = scenarioName;
  manifest["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
  manifest["images"] = manifestImages;
  QFile manifestFile(QDir(datasetDirectory).filePath("manifest.json"));
  const QByteArray manifestData = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
  if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      manifestFile.write(manifestData) != manifestData.size())
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile salvare manifest dataset.";
    }
    return {};
  }
  return datasetDirectory;
}

QString saveTestVisionLabelingBatch(
  const QString& rawImagesDirectory,
  const QString& scenarioName,
  const QVector<TestVisionDatasetImage>& images,
  QString* errorMessage)
{
  if (images.isEmpty())
  {
    if (errorMessage)
    {
      *errorMessage = "Nessuna immagine da salvare per il labeling.";
    }
    return {};
  }
  if (!QDir().mkpath(rawImagesDirectory))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare cartella raw: " + rawImagesDirectory;
    }
    return {};
  }

  QString safeScenarioName = scenarioName.trimmed();
  safeScenarioName.replace(QRegularExpression(R"([^A-Za-z0-9_.-]+)"), "_");
  if (safeScenarioName.isEmpty())
  {
    safeScenarioName = "scenario";
  }
  const QString batchId = testVisionTimestamp();
  QJsonArray manifestImages;
  for (const TestVisionDatasetImage& item : images)
  {
    const QString fileName = QString("%1_%2_%3")
      .arg(safeScenarioName, batchId, item.fileName);
    const QString imagePath = QDir(rawImagesDirectory).filePath(fileName);
    if (item.image.empty() || !cv::imwrite(imagePath.toStdString(), item.image))
    {
      if (errorMessage)
      {
        *errorMessage = "Impossibile salvare immagine raw: " + imagePath;
      }
      return {};
    }
    QJsonObject metadata = item.metadata;
    metadata["file"] = fileName;
    manifestImages.append(metadata);
  }

  const QString manifestsDirectory = QDir(rawImagesDirectory).absoluteFilePath("../manifests");
  if (!QDir().mkpath(manifestsDirectory))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare cartella manifest: " + manifestsDirectory;
    }
    return {};
  }
  QJsonObject manifest;
  manifest["scenario"] = scenarioName;
  manifest["batchId"] = batchId;
  manifest["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
  manifest["rawDirectory"] = QDir::toNativeSeparators(rawImagesDirectory);
  manifest["images"] = manifestImages;
  const QString manifestPath = QDir(manifestsDirectory).filePath(batchId + ".json");
  QFile manifestFile(manifestPath);
  const QByteArray manifestData = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
  if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      manifestFile.write(manifestData) != manifestData.size())
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile salvare manifest: " + manifestPath;
    }
    return {};
  }
  return manifestPath;
}
