#include "processing/MaskPoseEstimator.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

MaskPoseResult MaskPoseEstimator::estimate(const cv::Mat& mask, double minArea) const
{
  MaskPoseResult result;
  if (mask.empty())
  {
    return result;
  }

  cv::Mat gray;
  if (mask.channels() == 1)
  {
    gray = mask;
  }
  else
  {
    cv::cvtColor(mask, gray, cv::COLOR_BGR2GRAY);
  }

  cv::Mat binary;
  cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY);
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  int bestIndex = -1;
  double bestArea = std::max(0.0, minArea);
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    const double area = std::abs(cv::contourArea(contours[i]));
    if (area >= bestArea)
    {
      bestArea = area;
      bestIndex = i;
    }
  }
  if (bestIndex < 0 || contours[bestIndex].size() < 3)
  {
    return result;
  }

  const std::vector<cv::Point>& contour = contours[bestIndex];
  const cv::Moments moments = cv::moments(contour);
  if (std::abs(moments.m00) <= std::numeric_limits<double>::epsilon())
  {
    return result;
  }

  cv::Mat data(static_cast<int>(contour.size()), 2, CV_64F);
  for (int i = 0; i < static_cast<int>(contour.size()); ++i)
  {
    data.at<double>(i, 0) = contour[i].x;
    data.at<double>(i, 1) = contour[i].y;
  }
  const cv::PCA pca(data, cv::Mat(), cv::PCA::DATA_AS_ROW);

  result.found = true;
  result.center = {
    moments.m10 / moments.m00,
    moments.m01 / moments.m00
  };
  result.angleRadians = std::atan2(
    pca.eigenvectors.at<double>(0, 1),
    pca.eigenvectors.at<double>(0, 0));
  result.area = bestArea;
  result.boundingRect = cv::boundingRect(contour);
  result.contour = contour;
  return result;
}
