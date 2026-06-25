#include "processing/thread/ThreadProfileExtractor.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace
{
cv::Rect clampRect(const cv::Rect& rect, const cv::Size& imageSize)
{
  return rect & cv::Rect(0, 0, imageSize.width, imageSize.height);
}

cv::Mat toGray(const cv::Mat& input)
{
  if (input.channels() == 1)
  {
    return input;
  }

  cv::Mat gray;
  cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
  return gray;
}

void removeSpeckComponents(cv::Mat& mask, int minArea, int maxArea)
{
  if (minArea <= 1 && maxArea <= 0)
  {
    return;
  }

  cv::Mat labels;
  cv::Mat stats;
  cv::Mat centroids;
  const int count = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);
  cv::Mat filtered = cv::Mat::zeros(mask.size(), CV_8UC1);

  for (int label = 1; label < count; ++label)
  {
    const int area = stats.at<int>(label, cv::CC_STAT_AREA);
    if (area < minArea)
    {
      continue;
    }
    if (maxArea > 0 && area > maxArea)
    {
      continue;
    }

    filtered.setTo(255, labels == label);
  }

  mask = filtered;
}

std::vector<double> movingAverage(const std::vector<double>& values, int radius)
{
  if (radius <= 0 || values.empty())
  {
    return values;
  }

  std::vector<double> smoothed(values.size(), 0.0);
  for (size_t i = 0; i < values.size(); ++i)
  {
    const int start = static_cast<int>(std::max<size_t>(0, i - static_cast<size_t>(radius)));
    const int end = static_cast<int>(std::min(values.size() - 1, i + static_cast<size_t>(radius)));
    double sum = 0.0;
    int count = 0;
    for (int j = start; j <= end; ++j)
    {
      if (!std::isfinite(values[j]))
      {
        continue;
      }
      sum += values[j];
      count += 1;
    }
    smoothed[i] = count > 0 ? sum / static_cast<double>(count) : values[i];
  }
  return smoothed;
}

std::vector<double> rejectOutliers(const std::vector<double>& values, double sigmaFactor)
{
  if (sigmaFactor <= 0.0 || values.empty())
  {
    return values;
  }

  std::vector<double> finite;
  finite.reserve(values.size());
  for (double value : values)
  {
    if (std::isfinite(value))
    {
      finite.push_back(value);
    }
  }
  if (finite.empty())
  {
    return values;
  }

  const double median = finite[finite.size() / 2];
  std::vector<double> deviations;
  deviations.reserve(finite.size());
  for (double value : finite)
  {
    deviations.push_back(std::abs(value - median));
  }
  std::nth_element(deviations.begin(), deviations.begin() + deviations.size() / 2, deviations.end());
  const double mad = deviations[deviations.size() / 2];
  const double threshold = std::max(1.5, sigmaFactor * std::max(1.0, mad * 1.4826));

  std::vector<double> filtered = values;
  for (size_t i = 0; i < filtered.size(); ++i)
  {
    if (!std::isfinite(filtered[i]))
    {
      continue;
    }
    if (std::abs(filtered[i] - median) > threshold)
    {
      filtered[i] = std::numeric_limits<double>::quiet_NaN();
    }
  }

  return movingAverage(filtered, 2);
}

double findTopEdgeY(const cv::Mat& columnMask)
{
  for (int row = 0; row < columnMask.rows; ++row)
  {
    if (columnMask.at<uchar>(row) != 0)
    {
      return static_cast<double>(row);
    }
  }
  return std::numeric_limits<double>::quiet_NaN();
}

double findBottomEdgeY(const cv::Mat& columnMask)
{
  for (int row = columnMask.rows - 1; row >= 0; --row)
  {
    if (columnMask.at<uchar>(row) != 0)
    {
      return static_cast<double>(row);
    }
  }
  return std::numeric_limits<double>::quiet_NaN();
}
}

ThreadProfileResult ThreadProfileExtractor::extract(
  const cv::Mat& input,
  const cv::Rect& imageRoi,
  const ThreadInspectionSettings& settings) const
{
  ThreadProfileResult result;

  if (input.empty())
  {
    result.error = "empty_image";
    return result;
  }

  const cv::Rect roi = clampRect(imageRoi, input.size());
  if (roi.width < 8 || roi.height < 8)
  {
    result.error = "invalid_roi";
    return result;
  }

  const cv::Mat gray = toGray(input);
  cv::Mat roiGray = gray(roi);

  cv::Mat mask;
  const int thresholdMin = std::min(settings.thresholdMin, settings.thresholdMax);
  const int thresholdMax = std::max(settings.thresholdMin, settings.thresholdMax);
  cv::inRange(roiGray, cv::Scalar(thresholdMin), cv::Scalar(thresholdMax), mask);

  const int openRadius = std::clamp(settings.morphOpenRadius, 0, 15);
  if (openRadius > 0)
  {
    cv::Mat kernel = cv::getStructuringElement(
      cv::MORPH_ELLIPSE,
      cv::Size(openRadius * 2 + 1, openRadius * 2 + 1));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
  }

  removeSpeckComponents(mask, std::max(1, settings.minSpeckAreaPx), settings.maxSpeckAreaPx);

  result.columns.reserve(static_cast<size_t>(roi.width));
  std::vector<double> crestProfile;
  std::vector<double> rootProfile;
  crestProfile.reserve(static_cast<size_t>(roi.width));
  rootProfile.reserve(static_cast<size_t>(roi.width));

  for (int col = 0; col < roi.width; ++col)
  {
    const cv::Mat columnMask = mask.col(col);
    ThreadProfileColumn column;
    column.x = roi.x + col;
    column.crestY = findTopEdgeY(columnMask);
    column.rootY = findBottomEdgeY(columnMask);
    column.valid = std::isfinite(column.crestY) && std::isfinite(column.rootY) && column.rootY > column.crestY;
    result.columns.push_back(column);
    crestProfile.push_back(column.valid ? column.crestY : std::numeric_limits<double>::quiet_NaN());
    rootProfile.push_back(column.valid ? column.rootY : std::numeric_limits<double>::quiet_NaN());
  }

  const int smoothRadius = std::clamp(settings.profileSmoothRadius, 0, 15);
  crestProfile = movingAverage(crestProfile, smoothRadius);
  rootProfile = movingAverage(rootProfile, smoothRadius);
  crestProfile = rejectOutliers(crestProfile, settings.outlierRejectSigma);
  rootProfile = rejectOutliers(rootProfile, settings.outlierRejectSigma);
  crestProfile = movingAverage(crestProfile, smoothRadius);
  rootProfile = movingAverage(rootProfile, smoothRadius);

  result.validColumns = 0;
  for (size_t i = 0; i < result.columns.size(); ++i)
  {
    ThreadProfileColumn& column = result.columns[i];
    if (std::isfinite(crestProfile[i]) && std::isfinite(rootProfile[i]) && rootProfile[i] > crestProfile[i])
    {
      column.crestY = crestProfile[i] + roi.y;
      column.rootY = rootProfile[i] + roi.y;
      column.valid = true;
      result.validColumns += 1;
    }
    else
    {
      column.valid = false;
    }
  }

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  cv::rectangle(result.diagnosticImage, roi, cv::Scalar(0, 200, 255), 3);
  for (const ThreadProfileColumn& column : result.columns)
  {
    if (!column.valid)
    {
      continue;
    }

    cv::circle(result.diagnosticImage, cv::Point(column.x, static_cast<int>(std::round(column.crestY))), 1, cv::Scalar(80, 220, 80), -1, cv::LINE_AA);
    cv::circle(result.diagnosticImage, cv::Point(column.x, static_cast<int>(std::round(column.rootY))), 1, cv::Scalar(60, 200, 255), -1, cv::LINE_AA);
  }

  for (size_t i = 1; i < result.columns.size(); ++i)
  {
    const ThreadProfileColumn& previous = result.columns[i - 1];
    const ThreadProfileColumn& current = result.columns[i];
    if (!previous.valid || !current.valid)
    {
      continue;
    }

    cv::line(
      result.diagnosticImage,
      cv::Point(previous.x, static_cast<int>(std::round(previous.crestY))),
      cv::Point(current.x, static_cast<int>(std::round(current.crestY))),
      cv::Scalar(80, 220, 80),
      1,
      cv::LINE_AA);
    cv::line(
      result.diagnosticImage,
      cv::Point(previous.x, static_cast<int>(std::round(previous.rootY))),
      cv::Point(current.x, static_cast<int>(std::round(current.rootY))),
      cv::Scalar(60, 200, 255),
      1,
      cv::LINE_AA);
  }

  result.valid = result.validColumns >= std::max(8, roi.width / 6);
  if (!result.valid)
  {
    result.error = "insufficient_profile";
  }

  return result;
}
