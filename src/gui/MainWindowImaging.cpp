#include "MainWindow.h"

#include "gui/SurfaceLocalizationPanelWidget.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"
#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "processing/SurfaceModelTrainer.h"
#include "processing/geometry/EdgeLineDetector.h"
#include "processing/geometry/EdgePointDetector.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QSettings>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>
#include <type_traits>
#include "util/AsyncExecutor.h"
#include <thread>
#include <memory>

using AsyncExecutor::runAsyncTask;

namespace
{
QString projectPath(const QString& relativePath)
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(relativePath);
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


PartPose MainWindow::partPoseFromLocalizationResult(const CameraConfig& camera, const LocalizationResult& result) const
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

PartPose MainWindow::partPoseFromSurfaceReference(const CameraConfig& camera, const SurfaceLocalizationReference& reference) const
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

QPixmap MainWindow::loadCameraPreview(const CameraConfig& camera) const
{
  const QString imagePath = cameraSampleImagePath(camera);

  if (imagePath.isEmpty())
  {
    return {};
  }

  return QPixmap(imagePath);
}

void MainWindow::reloadCameraReferenceImage(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId || !m_largeImage)
  {
    return;
  }

  m_selectedImagePath = cameraSampleImagePath(camera);
  m_selectedPreview = m_selectedImagePath.isEmpty() ? QPixmap() : QPixmap(m_selectedImagePath);
  if (m_selectedPreview.isNull())
  {
    m_largeImage->clearImage();
    return;
  }

  m_largeImage->setImage(m_selectedPreview);
  updateLargePreview();
}

QPixmap MainWindow::matToPixmap(const cv::Mat& image) const
{
  if (image.empty())
  {
    return {};
  }

  cv::Mat rgb;

  if (image.channels() == 1)
  {
    cv::cvtColor(image, rgb, cv::COLOR_GRAY2RGB);
  }
  else
  {
    cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
  }

  QImage qimage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
  return QPixmap::fromImage(qimage.copy());
}

cv::Mat MainWindow::currentInputImage(const CameraConfig& camera, QString* errorMessage) const
{
  const auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt != m_cameraRuntime.end() && !runtimeIt->second.currentFrame().empty())
  {
    return runtimeIt->second.currentFrame().clone();
  }

  QString imagePath = camera.id == m_selectedCameraId ? m_selectedImagePath : QString();
  if (imagePath.isEmpty())
  {
    imagePath = cameraSampleImagePath(camera);
  }

  if (imagePath.isEmpty())
  {
    if (errorMessage)
    {
      *errorMessage = trText("log.imageMissing") + ": " + camera.id;
    }

    return {};
  }

  cv::Mat input = cv::imread(imagePath.toStdString(), cv::IMREAD_COLOR);
  if (input.empty() && errorMessage)
  {
    *errorMessage = trText("log.imageMissing") + ": " + imagePath;
  }

  return input;
}

QString MainWindow::cameraSampleImagePath(const CameraConfig& camera) const
{
  const QString recipeSample = m_recipeManager.firstCameraSampleImagePath(camera.id);
  if (!recipeSample.isEmpty())
  {
    return recipeSample;
  }

  return firstImageInFolder(camera.folder);
}

QString MainWindow::cameraTestImagesFolder(const CameraConfig& camera) const
{
  if (!m_recipeManager.firstCameraTestImagePath(camera.id).isEmpty())
  {
    return m_recipeManager.cameraTestImagesPath(camera.id);
  }

  return resolvedCameraFolder(camera);
}

QString MainWindow::resolvedCameraFolder(const CameraConfig& camera) const
{
  if (camera.folder.isEmpty())
  {
    return projectPath("data/images");
  }

  return resolveProjectPath(camera.folder);
}

QString MainWindow::firstImageInFolder(const QString& folder) const
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


