#pragma once

#include <opencv2/core.hpp>

#include <QRect>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>

struct PartPose
{
  bool valid = false;
  QString cameraId;
  QString method;
  cv::Point2d origin;
  double angleRadians = 0.0;
  double score = 0.0;
  cv::Point2d xAxis = {1.0, 0.0};
  cv::Point2d yAxis = {0.0, 1.0};
};

inline PartPose makeInvalidPartPose(const QString& cameraId = {})
{
  PartPose pose;
  pose.cameraId = cameraId;
  return pose;
}

inline cv::Point2d normalizedOrDefault(const cv::Point2d& vector, const cv::Point2d& fallback)
{
  const double length = std::hypot(vector.x, vector.y);
  if (length <= 0.000001)
  {
    return fallback;
  }

  return {vector.x / length, vector.y / length};
}

inline cv::Point2d imageToPart(const PartPose& pose, const cv::Point2d& imagePoint)
{
  if (!pose.valid)
  {
    return imagePoint;
  }

  const cv::Point2d delta = imagePoint - pose.origin;
  return {
    delta.x * pose.xAxis.x + delta.y * pose.xAxis.y,
    delta.x * pose.yAxis.x + delta.y * pose.yAxis.y
  };
}

inline cv::Point2d partToImage(const PartPose& pose, const cv::Point2d& partPoint)
{
  if (!pose.valid)
  {
    return partPoint;
  }

  return pose.origin + pose.xAxis * partPoint.x + pose.yAxis * partPoint.y;
}

inline QRect imageRectToPartRect(const PartPose& pose, const QRect& imageRect)
{
  if (!pose.valid)
  {
    return imageRect;
  }

  const QVector<cv::Point2d> points = {
    imageToPart(pose, cv::Point2d(imageRect.left(), imageRect.top())),
    imageToPart(pose, cv::Point2d(imageRect.right(), imageRect.top())),
    imageToPart(pose, cv::Point2d(imageRect.right(), imageRect.bottom())),
    imageToPart(pose, cv::Point2d(imageRect.left(), imageRect.bottom()))
  };

  double minX = points.first().x;
  double maxX = points.first().x;
  double minY = points.first().y;
  double maxY = points.first().y;
  for (const cv::Point2d& point : points)
  {
    minX = std::min(minX, point.x);
    maxX = std::max(maxX, point.x);
    minY = std::min(minY, point.y);
    maxY = std::max(maxY, point.y);
  }

  return QRect(
    static_cast<int>(std::floor(minX)),
    static_cast<int>(std::floor(minY)),
    static_cast<int>(std::ceil(maxX - minX)),
    static_cast<int>(std::ceil(maxY - minY)));
}

inline QRect partRectToImageBoundingRect(const PartPose& pose, const QRect& partRect)
{
  if (!pose.valid)
  {
    return partRect;
  }

  const QVector<cv::Point2d> points = {
    partToImage(pose, cv::Point2d(partRect.left(), partRect.top())),
    partToImage(pose, cv::Point2d(partRect.right(), partRect.top())),
    partToImage(pose, cv::Point2d(partRect.right(), partRect.bottom())),
    partToImage(pose, cv::Point2d(partRect.left(), partRect.bottom()))
  };

  double minX = points.first().x;
  double maxX = points.first().x;
  double minY = points.first().y;
  double maxY = points.first().y;
  for (const cv::Point2d& point : points)
  {
    minX = std::min(minX, point.x);
    maxX = std::max(maxX, point.x);
    minY = std::min(minY, point.y);
    maxY = std::max(maxY, point.y);
  }

  return QRect(
    static_cast<int>(std::floor(minX)),
    static_cast<int>(std::floor(minY)),
    static_cast<int>(std::ceil(maxX - minX)),
    static_cast<int>(std::ceil(maxY - minY)));
}
