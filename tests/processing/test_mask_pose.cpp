#include "ai/AiMaskLabelStorage.h"
#include "processing/MaskPoseEstimator.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <iostream>

namespace
{
double angleError180(double actualRadians, double expectedRadians)
{
  double error = std::fmod(std::abs(actualRadians - expectedRadians), CV_PI);
  return std::min(error, CV_PI - error);
}

bool check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << message << '\n';
  }
  return condition;
}
}

int main()
{
  cv::Mat mask = cv::Mat::zeros(480, 640, CV_8UC1);
  const cv::RotatedRect expected(
    cv::Point2f(360.0F, 205.0F),
    cv::Size2f(220.0F, 80.0F),
    32.0F);
  cv::Point2f corners[4];
  expected.points(corners);
  std::vector<cv::Point> polygon;
  for (const cv::Point2f& corner : corners)
  {
    polygon.emplace_back(cvRound(corner.x), cvRound(corner.y));
  }
  cv::fillConvexPoly(mask, polygon, cv::Scalar(255));

  const MaskPoseResult result = MaskPoseEstimator().estimate(mask);
  bool ok = true;
  ok &= check(result.found, "Maschera nota non localizzata");
  ok &= check(std::hypot(result.center.x - 360.0, result.center.y - 205.0) < 1.0,
              "Centroide maschera fuori tolleranza");
  ok &= check(angleError180(result.angleRadians, 32.0 * CV_PI / 180.0) < 1.5 * CV_PI / 180.0,
              "Orientamento PCA fuori tolleranza");
  ok &= check(result.area > 16000.0 && result.area < 19000.0,
              "Area maschera fuori tolleranza");

  const MaskPoseResult empty = MaskPoseEstimator().estimate(cv::Mat::zeros(100, 100, CV_8UC1));
  ok &= check(!empty.found, "Maschera vuota deve produrre found=false");

  QTemporaryDir temporary;
  const QString rawPath = temporary.filePath("raw/sample.png");
  const QString maskDirectory = temporary.filePath("masks");
  const QString labelDirectory = temporary.filePath("labels");
  QDir().mkpath(QFileInfo(rawPath).dir().absolutePath());
  cv::imwrite(rawPath.toStdString(), cv::Mat::zeros(100, 200, CV_8UC3));
  const QVector<QPoint> labelPolygon = {
    {20, 10}, {180, 10}, {180, 90}, {20, 90}
  };
  AiMaskLabelPaths savedPaths;
  QString storageError;
  ok &= check(
    AiMaskLabelStorage::savePolygon(
      rawPath,
      maskDirectory,
      labelDirectory,
      labelPolygon,
      &savedPaths,
      &storageError),
    "Salvataggio mask/label fallito");
  ok &= check(QFileInfo::exists(savedPaths.maskPath), "File maschera non creato");
  ok &= check(QFileInfo::exists(savedPaths.labelPath), "File label non creato");
  const QVector<QPoint> loadedPolygon =
    AiMaskLabelStorage::loadPolygon(rawPath, labelDirectory, &storageError);
  ok &= check(loadedPolygon.size() == labelPolygon.size(), "Ricaricamento label fallito");
  if (loadedPolygon.size() == labelPolygon.size())
  {
    for (int i = 0; i < labelPolygon.size(); ++i)
    {
      ok &= check(
        (loadedPolygon[i] - labelPolygon[i]).manhattanLength() <= 1,
        "Coordinate label ricaricate fuori tolleranza");
    }
  }
  return ok ? 0 : 1;
}
