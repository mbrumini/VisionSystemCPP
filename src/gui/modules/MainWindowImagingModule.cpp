#include "gui/modules/MainWindowImagingModule.h"

#include "config/RecipeJsonUtils.h"
#include "processing/SurfaceDefectProcessor.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace
{
QString projectPath(const QString& relativePath)
{
  return RecipeJsonUtils::appPath(relativePath);
}

QString resolveProjectPath(const QString& path)
{
  if (QFileInfo(path).isAbsolute())
  {
    return path;
  }

  return projectPath(path);
}
}

MainWindowImagingModule::MainWindowImagingModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

PartPose MainWindowImagingModule::partPoseFromLocalizationResult(const CameraConfig& camera, const LocalizationResult& result) const
{
  if (!result.found)
  {
    return makeInvalidPartPose(camera.id);
  }

  PartPose pose;
  pose.valid = true;
  pose.cameraId = camera.id;
  pose.method = "bw_localization";
  pose.origin = result.center;
  pose.angleRadians = result.angleRadians;
  pose.score = result.area;
  pose.xAxis = normalizedOrDefault(result.xAxisEnd - result.xAxisStart, {std::cos(result.angleRadians), std::sin(result.angleRadians)});
  pose.yAxis = normalizedOrDefault(result.yAxisEnd - result.yAxisStart, {-pose.xAxis.y, pose.xAxis.x});
  return pose;
}

PartPose MainWindowImagingModule::partPoseFromSurfaceReference(const CameraConfig& camera, const SurfaceLocalizationReference& reference) const
{
  if (!reference.found)
  {
    return makeInvalidPartPose(camera.id);
  }

  PartPose pose;
  pose.valid = true;
  pose.cameraId = camera.id;
  pose.method = QString::fromStdString(reference.method);
  pose.origin = reference.center;
  pose.angleRadians = reference.angleRadians;
  pose.score = reference.score;

  const cv::Point2d fallbackX(std::cos(reference.angleRadians), std::sin(reference.angleRadians));
  pose.xAxis = normalizedOrDefault(reference.xAxisEnd - reference.xAxisStart, fallbackX);
  pose.yAxis = normalizedOrDefault(reference.yAxisEnd - reference.yAxisStart, {-pose.xAxis.y, pose.xAxis.x});
  return pose;
}

QPixmap MainWindowImagingModule::loadCameraPreview(const CameraConfig& camera) const
{
  const auto runtimeIt = cameraRuntime().find(camera.id);
  if (runtimeIt != cameraRuntime().end() && !runtimeIt->second.currentFrame().empty())
  {
    return matToPixmap(runtimeIt->second.currentFrame());
  }

  const QString imagePath = cameraSampleImagePath(camera);
  if (imagePath.isEmpty())
  {
    return {};
  }

  return QPixmap(imagePath);
}

QPixmap MainWindowImagingModule::loadCameraSamplePreview(const CameraConfig& camera) const
{
  const QString imagePath = cameraSampleImagePath(camera);
  if (imagePath.isEmpty())
  {
    return {};
  }

  QPixmap preview(imagePath);
  if (!preview.isNull())
  {
    return preview;
  }

  const cv::Mat input = cv::imread(imagePath.toStdString(), cv::IMREAD_COLOR);
  return matToPixmap(input);
}

void MainWindowImagingModule::reloadCameraReferenceImage(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId() || !largeImage())
  {
    return;
  }

  ensureReferenceImageVisible(camera);
}

bool MainWindowImagingModule::ensureReferenceImageVisible(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId() || !largeImage())
  {
    return false;
  }

  selectedImagePath() = cameraSampleImagePath(camera);
  selectedPreview() = loadCameraSamplePreview(camera);
  if (selectedPreview().isNull())
  {
    if (largeImage())
    {
      largeImage()->clearImage();
    }
    if (context().updateLargePreview)
    {
      context().updateLargePreview();
    }
    return false;
  }

  largeImage()->setImage(selectedPreview());
  if (context().updateLargePreview)
  {
    context().updateLargePreview();
  }
  return true;
}

void MainWindowImagingModule::restoreSampleWorkspace(const CameraConfig& camera)
{
  const auto runtimeIt = cameraRuntime().find(camera.id);
  if (runtimeIt != cameraRuntime().end())
  {
    runtimeIt->second.stop();
    runtimeIt->second.clearCurrentFrame();
    runtimeIt->second.clearCurrentPose(camera.id);
  }

  if (camera.id == selectedCameraId() && context().simulationTimer)
  {
    context().simulationTimer->stop();
  }

  if (context().lastSurfaceLocalizationResults)
  {
    context().lastSurfaceLocalizationResults->remove(camera.id);
  }
  if (context().lastLocalizationResults)
  {
    context().lastLocalizationResults->remove(camera.id);
  }

  reloadCameraReferenceImage(camera);
  if (camera.id == selectedCameraId() && largeImage())
  {
    largeImage()->clearRoi();
    largeImage()->clearExclusionRects();
    largeImage()->clearCircles();
    largeImage()->clearGeometryArea();
    largeImage()->clearGeometryPoints();
    largeImage()->clearGeometryLines();
    largeImage()->clearGeometryOverlay();
    largeImage()->clearDetectedCircle();
  }
}

QPixmap MainWindowImagingModule::matToPixmap(const cv::Mat& image, const QSize& targetSize) const
{
  if (image.empty())
  {
    return {};
  }

  cv::Mat processed = image;
  if (targetSize.isValid() && (image.cols > targetSize.width() || image.rows > targetSize.height()))
  {
    double scale = std::min(static_cast<double>(targetSize.width()) / image.cols,
                            static_cast<double>(targetSize.height()) / image.rows);
    int newWidth = std::max(1, static_cast<int>(image.cols * scale));
    int newHeight = std::max(1, static_cast<int>(image.rows * scale));
    cv::resize(image, processed, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_AREA);
  }

  cv::Mat rgb;
  if (processed.channels() == 1)
  {
    cv::cvtColor(processed, rgb, cv::COLOR_GRAY2RGB);
  }
  else
  {
    cv::cvtColor(processed, rgb, cv::COLOR_BGR2RGB);
  }

  QImage qimage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
  return QPixmap::fromImage(qimage.copy());
}

cv::Mat MainWindowImagingModule::sampleInputImage(const CameraConfig& camera, QString* errorMessage) const
{
  const QString imagePath = cameraSampleImagePath(camera);
  if (imagePath.isEmpty())
  {
    if (errorMessage)
    {
      *errorMessage = tr("log.imageMissing") + ": " + camera.id;
    }
    return {};
  }

  cv::Mat input = cv::imread(imagePath.toStdString(), cv::IMREAD_COLOR);
  if (input.empty() && errorMessage)
  {
    *errorMessage = tr("log.imageMissing") + ": " + imagePath;
  }

  return input;
}

cv::Mat MainWindowImagingModule::validationInputImage(const CameraConfig& camera, QString* errorMessage) const
{
  return currentInputImage(camera, errorMessage);
}

cv::Mat MainWindowImagingModule::currentInputImage(const CameraConfig& camera, QString* errorMessage) const
{
  const auto runtimeIt = cameraRuntime().find(camera.id);
  if (runtimeIt != cameraRuntime().end() && !runtimeIt->second.currentFrame().empty())
  {
    return runtimeIt->second.currentFrame().clone();
  }

  QString imagePath = camera.id == selectedCameraId() ? selectedImagePath() : QString();
  if (imagePath.isEmpty())
  {
    imagePath = cameraSampleImagePath(camera);
  }

  if (imagePath.isEmpty())
  {
    if (errorMessage)
    {
      *errorMessage = tr("log.imageMissing") + ": " + camera.id;
    }
    return {};
  }

  cv::Mat input = cv::imread(imagePath.toStdString(), cv::IMREAD_COLOR);
  if (input.empty() && errorMessage)
  {
    *errorMessage = tr("log.imageMissing") + ": " + imagePath;
  }

  return input;
}

QString MainWindowImagingModule::cameraSampleImagePath(const CameraConfig& camera) const
{
  const QString recipeSample = recipes().firstCameraSampleImagePath(camera.id);
  if (!recipeSample.isEmpty())
  {
    return recipeSample;
  }

  if (camera.type != "file")
  {
    return {};
  }

  return firstImageInFolder(camera.folder);
}

QString MainWindowImagingModule::cameraTestImagesFolder(const CameraConfig& camera) const
{
  if (!recipes().firstCameraTestImagePath(camera.id).isEmpty())
  {
    return recipes().cameraTestImagesPath(camera.id);
  }

  return resolvedCameraFolder(camera);
}

QString MainWindowImagingModule::resolvedCameraFolder(const CameraConfig& camera) const
{
  if (camera.type != "file")
  {
    return {};
  }

  if (camera.folder.isEmpty())
  {
    return projectPath("data/images");
  }

  return resolveProjectPath(camera.folder);
}

QString MainWindowImagingModule::firstImageInFolder(const QString& folder) const
{
  if (folder.isEmpty())
  {
    return {};
  }

  QDir directory(resolveProjectPath(folder));
  const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tif", "*.tiff"};
  const QFileInfoList files = directory.entryInfoList(filters, QDir::Files, QDir::Name);
  if (files.isEmpty())
  {
    return {};
  }

  return files.first().absoluteFilePath();
}
