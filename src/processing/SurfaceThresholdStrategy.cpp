#include "SurfaceThresholdStrategy.h"

#include "processing/SurfaceProcessingUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
std::vector<cv::Point> localPolygon(const std::vector<cv::Point>& polygon, const cv::Rect& roi)
{
  std::vector<cv::Point> result;
  result.reserve(polygon.size());
  for (const cv::Point& point : polygon)
  {
    result.emplace_back(point.x - roi.x, point.y - roi.y);
  }
  return result;
}
}

SurfaceDefectResult SurfaceThresholdStrategy::detectInRoi(
  const cv::Mat& input,
  const cv::Rect& searchRoi,
  const std::vector<cv::Rect>& exclusionRects,
  const SurfaceThresholdSettings& settings) const
{
  SurfaceDefectResult result;

  if (input.empty())
  {
    return result;
  }

  cv::Mat gray;

  if (input.channels() == 1)
  {
    gray = input;
  }
  else
  {
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
  }

  const cv::Rect roi = clampSurfaceRect(searchRoi, gray.size());

  if (roi.width < 4 || roi.height < 4)
  {
    return result;
  }

  const int minValue = std::clamp(settings.minValue, 0, 255);
  const int maxValue = std::clamp(settings.maxValue, minValue, 255);
  const cv::Mat grayRoi = gray(roi);

  cv::Mat mask;
  cv::inRange(grayRoi, cv::Scalar(minValue), cv::Scalar(maxValue), mask);

  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect clampedExclusion = clampSurfaceRect(exclusionRect, gray.size());
    const cv::Rect localExclusion(
      clampedExclusion.x - roi.x,
      clampedExclusion.y - roi.y,
      clampedExclusion.width,
      clampedExclusion.height);
    const cv::Rect maskExclusion = clampSurfaceRect(localExclusion, mask.size());

    if (!maskExclusion.empty())
    {
      mask(maskExclusion).setTo(0);
    }
  }

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  // cv::rectangle(result.diagnosticImage, roi, cv::Scalar(0, 255, 255), 2);

  // Determine if there is at least one significant contour that does not span the entire ROI
  bool hasSmallSignificantContour = false;
  for (const auto& c : contours)
  {
    const double area = cv::contourArea(c);
    if (area > 100.0)
    {
      const cv::Rect bounding = cv::boundingRect(c);
      if (bounding.width <= 0.95 * roi.width || bounding.height <= 0.95 * roi.height)
      {
        hasSmallSignificantContour = true;
        break;
      }
    }
  }

  for (std::vector<cv::Point>& contour : contours)
  {
    const double area = cv::contourArea(contour);

    if (area <= 0.0)
    {
      continue;
    }

    if (hasSmallSignificantContour)
    {
      const cv::Rect bounding = cv::boundingRect(contour);
      if (bounding.width > 0.95 * roi.width && bounding.height > 0.95 * roi.height)
      {
        continue;
      }
    }

    for (cv::Point& point : contour)
    {
      point.x += roi.x;
      point.y += roi.y;
    }

    const cv::Moments moments = cv::moments(contour);
    SurfaceBlob blob;
    blob.area = area;
    blob.center = moments.m00 == 0.0 ? cv::Point2d() : cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);
    blob.boundingRect = cv::boundingRect(contour);
    blob.contour = contour;
    result.totalArea += area;
    result.blobs.push_back(blob);
  }

  sortSurfaceBlobsByArea(result);
  drawSurfaceBlobs(result.diagnosticImage, result.blobs);
  if (!result.blobs.empty())
  {
    const SurfaceBlob& mainBlob = result.blobs.front();
    result.localization.found = true;
    result.localization.method = "threshold_blob_mass_pca";
    result.localization.center = mainBlob.center;
    result.localization.radius = mainBlob.area > 0.0 ? std::sqrt(mainBlob.area / CV_PI) : 0.0;
    result.localization.score = 1.0;
    result.localization.inputPoints = static_cast<int>(mainBlob.contour.size());
    result.localization.usedPoints = result.localization.inputPoints;

    if (mainBlob.contour.size() >= 5)
    {
      cv::Mat data(static_cast<int>(mainBlob.contour.size()), 2, CV_64F);
      for (int i = 0; i < static_cast<int>(mainBlob.contour.size()); ++i)
      {
        data.at<double>(i, 0) = mainBlob.contour[i].x;
        data.at<double>(i, 1) = mainBlob.contour[i].y;
      }

      const cv::PCA pca(data, cv::Mat(), cv::PCA::DATA_AS_ROW);
      const cv::Point2d xDirection(pca.eigenvectors.at<double>(0, 0), pca.eigenvectors.at<double>(0, 1));
      double angle = std::atan2(xDirection.y, xDirection.x);

      if (settings.resolveAmbiguity)
      {
        std::vector<std::vector<cv::Point>> allContours;
        cv::findContours(mask.clone(), allContours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        int bestInternalIndex = -1;
        double bestInternalArea = 0.0;
        for (int i = 0; i < static_cast<int>(allContours.size()); ++i)
        {
          const double area = std::abs(cv::contourArea(allContours[i]));
          if (area < 0.8 * mainBlob.area && area > 8.0)
          {
            if (area > bestInternalArea)
            {
              bestInternalIndex = i;
              bestInternalArea = area;
            }
          }
        }

        if (bestInternalIndex >= 0)
        {
          std::vector<cv::Point> internalContour = allContours[bestInternalIndex];
          for (cv::Point& pt : internalContour)
          {
            pt.x += roi.x;
            pt.y += roi.y;
          }

          cv::Moments m = cv::moments(internalContour);
          cv::Point2d internalCenter;
          if (std::abs(m.m00) > 1e-6)
          {
            internalCenter = cv::Point2d(m.m10 / m.m00, m.m01 / m.m00);
          }
          else
          {
            cv::Rect r = cv::boundingRect(internalContour);
            internalCenter = cv::Point2d(r.x + r.width / 2.0, r.y + r.height / 2.0);
          }

          cv::drawContours(result.diagnosticImage, std::vector<std::vector<cv::Point>>{internalContour}, -1, cv::Scalar(0, 165, 255), 2);

          const cv::Point2d toInternal = internalCenter - mainBlob.center;
          const double dot = toInternal.x * xDirection.x + toInternal.y * xDirection.y;
          if (dot < 0.0)
          {
            angle = angle + CV_PI;
            if (angle > CV_PI)
            {
              angle -= 2.0 * CV_PI;
            }
            else if (angle <= -CV_PI)
            {
              angle += 2.0 * CV_PI;
            }
          }
        }
      }

      const cv::Point2d xDir(std::cos(angle), std::sin(angle));
      const cv::Point2d yDir(-xDir.y, xDir.x);
      const cv::Rect bounding = mainBlob.boundingRect;
      const double axisLength = std::max(20.0, 0.55 * std::max(bounding.width, bounding.height));
      result.localization.angleRadians = angle;
      result.localization.xAxisStart = mainBlob.center - xDir * axisLength;
      result.localization.xAxisEnd = mainBlob.center + xDir * axisLength;
      result.localization.yAxisStart = mainBlob.center - yDir * axisLength;
      result.localization.yAxisEnd = mainBlob.center + yDir * axisLength;

      drawStyledAxes(result.diagnosticImage, mainBlob.center, result.localization.xAxisStart, result.localization.xAxisEnd, result.localization.yAxisStart, result.localization.yAxisEnd);
    }
  }

  result.processed = true;
  return result;
}

SurfaceDefectResult SurfaceThresholdStrategy::detectInPolygon(
  const cv::Mat& input,
  const std::vector<cv::Point>& searchPolygon,
  const std::vector<cv::Rect>& exclusionRects,
  const SurfaceThresholdSettings& settings) const
{
  if (searchPolygon.size() < 3)
  {
    return detectInRoi(input, cv::Rect(), exclusionRects, settings);
  }

  SurfaceDefectResult result;
  if (input.empty())
  {
    return result;
  }

  cv::Mat gray;
  if (input.channels() == 1)
  {
    gray = input;
  }
  else
  {
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
  }

  const cv::Rect roi = clampSurfaceRect(cv::boundingRect(searchPolygon), gray.size());
  if (roi.width < 4 || roi.height < 4)
  {
    return result;
  }

  const int minValue = std::clamp(settings.minValue, 0, 255);
  const int maxValue = std::clamp(settings.maxValue, minValue, 255);
  cv::Mat mask;
  cv::inRange(gray(roi), cv::Scalar(minValue), cv::Scalar(maxValue), mask);

  cv::Mat polygonMask = cv::Mat::zeros(mask.size(), CV_8UC1);
  const std::vector<std::vector<cv::Point>> fillPolygons{localPolygon(searchPolygon, roi)};
  cv::fillPoly(polygonMask, fillPolygons, cv::Scalar(255));
  cv::bitwise_and(mask, polygonMask, mask);

  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect clampedExclusion = clampSurfaceRect(exclusionRect, gray.size());
    const cv::Rect localExclusion(
      clampedExclusion.x - roi.x,
      clampedExclusion.y - roi.y,
      clampedExclusion.width,
      clampedExclusion.height);
    const cv::Rect maskExclusion = clampSurfaceRect(localExclusion, mask.size());
    if (!maskExclusion.empty())
    {
      mask(maskExclusion).setTo(0);
    }
  }

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  // cv::polylines(result.diagnosticImage, std::vector<std::vector<cv::Point>>{searchPolygon}, true, cv::Scalar(0, 255, 255), 2);

  // Determine if there is at least one significant contour that does not span the entire ROI
  bool hasSmallSignificantContour = false;
  for (const auto& c : contours)
  {
    const double area = cv::contourArea(c);
    if (area > 100.0)
    {
      const cv::Rect bounding = cv::boundingRect(c);
      if (bounding.width <= 0.95 * roi.width || bounding.height <= 0.95 * roi.height)
      {
        hasSmallSignificantContour = true;
        break;
      }
    }
  }

  for (std::vector<cv::Point>& contour : contours)
  {
    const double area = cv::contourArea(contour);
    if (area <= 0.0)
    {
      continue;
    }

    if (hasSmallSignificantContour)
    {
      const cv::Rect bounding = cv::boundingRect(contour);
      if (bounding.width > 0.95 * roi.width && bounding.height > 0.95 * roi.height)
      {
        continue;
      }
    }

    for (cv::Point& point : contour)
    {
      point.x += roi.x;
      point.y += roi.y;
    }

    const cv::Moments moments = cv::moments(contour);
    SurfaceBlob blob;
    blob.area = area;
    blob.center = moments.m00 == 0.0 ? cv::Point2d() : cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);
    blob.boundingRect = cv::boundingRect(contour);
    blob.contour = contour;
    result.totalArea += area;
    result.blobs.push_back(blob);
  }

  sortSurfaceBlobsByArea(result);
  drawSurfaceBlobs(result.diagnosticImage, result.blobs);
  if (!result.blobs.empty())
  {
    const SurfaceBlob& mainBlob = result.blobs.front();
    result.localization.found = true;
    result.localization.method = "threshold_polygon_mass_pca";
    result.localization.center = mainBlob.center;
    result.localization.radius = mainBlob.area > 0.0 ? std::sqrt(mainBlob.area / CV_PI) : 0.0;
    result.localization.score = 1.0;
    result.localization.inputPoints = static_cast<int>(mainBlob.contour.size());
    result.localization.usedPoints = result.localization.inputPoints;

    if (mainBlob.contour.size() >= 5)
    {
      cv::Mat data(static_cast<int>(mainBlob.contour.size()), 2, CV_64F);
      for (int i = 0; i < static_cast<int>(mainBlob.contour.size()); ++i)
      {
        data.at<double>(i, 0) = mainBlob.contour[i].x;
        data.at<double>(i, 1) = mainBlob.contour[i].y;
      }
      const cv::PCA pca(data, cv::Mat(), cv::PCA::DATA_AS_ROW);
      const cv::Point2d xDirection(pca.eigenvectors.at<double>(0, 0), pca.eigenvectors.at<double>(0, 1));
      double angle = std::atan2(xDirection.y, xDirection.x);

      if (settings.resolveAmbiguity)
      {
        std::vector<std::vector<cv::Point>> allContours;
        cv::findContours(mask.clone(), allContours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        int bestInternalIndex = -1;
        double bestInternalArea = 0.0;
        for (int i = 0; i < static_cast<int>(allContours.size()); ++i)
        {
          const double area = std::abs(cv::contourArea(allContours[i]));
          if (area < 0.8 * mainBlob.area && area > 8.0)
          {
            if (area > bestInternalArea)
            {
              bestInternalIndex = i;
              bestInternalArea = area;
            }
          }
        }

        if (bestInternalIndex >= 0)
        {
          std::vector<cv::Point> internalContour = allContours[bestInternalIndex];
          for (cv::Point& pt : internalContour)
          {
            pt.x += roi.x;
            pt.y += roi.y;
          }

          cv::Moments m = cv::moments(internalContour);
          cv::Point2d internalCenter;
          if (std::abs(m.m00) > 1e-6)
          {
            internalCenter = cv::Point2d(m.m10 / m.m00, m.m01 / m.m00);
          }
          else
          {
            cv::Rect r = cv::boundingRect(internalContour);
            internalCenter = cv::Point2d(r.x + r.width / 2.0, r.y + r.height / 2.0);
          }

          cv::drawContours(result.diagnosticImage, std::vector<std::vector<cv::Point>>{internalContour}, -1, cv::Scalar(0, 165, 255), 2);

          const cv::Point2d toInternal = internalCenter - mainBlob.center;
          const double dot = toInternal.x * xDirection.x + toInternal.y * xDirection.y;
          if (dot < 0.0)
          {
            angle = angle + CV_PI;
            if (angle > CV_PI)
            {
              angle -= 2.0 * CV_PI;
            }
            else if (angle <= -CV_PI)
            {
              angle += 2.0 * CV_PI;
            }
          }
        }
      }

      const cv::Point2d xDir(std::cos(angle), std::sin(angle));
      const cv::Point2d yDir(-xDir.y, xDir.x);
      const double axisLength = std::max(20.0, 0.55 * std::max(mainBlob.boundingRect.width, mainBlob.boundingRect.height));
      result.localization.angleRadians = angle;
      result.localization.xAxisStart = mainBlob.center - xDir * axisLength;
      result.localization.xAxisEnd = mainBlob.center + xDir * axisLength;
      result.localization.yAxisStart = mainBlob.center - yDir * axisLength;
      result.localization.yAxisEnd = mainBlob.center + yDir * axisLength;
      drawStyledAxes(result.diagnosticImage, mainBlob.center, result.localization.xAxisStart, result.localization.xAxisEnd, result.localization.yAxisStart, result.localization.yAxisEnd);
    }
  }

  result.processed = true;
  return result;
}

SurfaceDefectResult SurfaceThresholdStrategy::locateAnnulus(
  const cv::Mat& input,
  const SurfaceAnnulusThresholdConfig& config,
  const std::vector<cv::Rect>& exclusionRects) const
{
  SurfaceDefectResult result;

  if (input.empty() || config.outerRadius <= 0 || config.innerRadius < 0 || config.innerRadius >= config.outerRadius)
  {
    return result;
  }

  cv::Mat gray;

  if (input.channels() == 1)
  {
    gray = input;
  }
  else
  {
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
  }

  cv::Mat thresholdMask;
  const int minValue = std::clamp(config.threshold.minValue, 0, 255);
  const int maxValue = std::clamp(config.threshold.maxValue, minValue, 255);
  cv::inRange(gray, cv::Scalar(minValue), cv::Scalar(maxValue), thresholdMask);

  cv::Mat annulusMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  cv::circle(annulusMask, config.center, config.outerRadius, cv::Scalar(255), -1);
  cv::circle(annulusMask, config.center, config.innerRadius, cv::Scalar(0), -1);
  cv::bitwise_and(thresholdMask, annulusMask, thresholdMask);

  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect maskExclusion = clampSurfaceRect(exclusionRect, thresholdMask.size());

    if (!maskExclusion.empty())
    {
      thresholdMask(maskExclusion).setTo(0);
    }
  }

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(thresholdMask, thresholdMask, cv::MORPH_OPEN, kernel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(thresholdMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  // Draw outer circle with shadow
  cv::circle(result.diagnosticImage, config.center, config.outerRadius, cv::Scalar(16, 16, 16), 4, cv::LINE_AA);
  cv::circle(result.diagnosticImage, config.center, config.outerRadius, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

  // Draw inner circle with shadow
  cv::circle(result.diagnosticImage, config.center, config.innerRadius, cv::Scalar(16, 16, 16), 4, cv::LINE_AA);
  cv::circle(result.diagnosticImage, config.center, config.innerRadius, cv::Scalar(0, 165, 255), 2, cv::LINE_AA);

  for (std::vector<cv::Point>& contour : contours)
  {
    const double area = cv::contourArea(contour);

    if (area <= 0.0)
    {
      continue;
    }

    const cv::Moments moments = cv::moments(contour);
    SurfaceBlob blob;
    blob.area = area;
    blob.center = moments.m00 == 0.0 ? cv::Point2d() : cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);
    blob.boundingRect = cv::boundingRect(contour);
    blob.contour = contour;
    result.totalArea += area;
    result.blobs.push_back(blob);
  }

  sortSurfaceBlobsByArea(result);
  drawSurfaceBlobs(result.diagnosticImage, result.blobs);
  if (!result.blobs.empty())
  {
    const SurfaceBlob& mainBlob = result.blobs.front();
    result.localization.found = true;
    result.localization.method = "threshold_annulus_center";
    result.localization.center = mainBlob.center;
    result.localization.radius = mainBlob.area > 0.0 ? std::sqrt(mainBlob.area / CV_PI) : 0.0;
    result.localization.score = 1.0;
    result.localization.inputPoints = static_cast<int>(mainBlob.contour.size());
    result.localization.usedPoints = result.localization.inputPoints;

    // Draw equivalent radius circle with shadow
    if (result.localization.radius > 0.0)
    {
      const cv::Point centerPt = surfacePoint(mainBlob.center);
      const int radiusPt = static_cast<int>(std::round(result.localization.radius));
      cv::circle(result.diagnosticImage, centerPt, radiusPt, cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
      cv::circle(result.diagnosticImage, centerPt, radiusPt, cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
    }
  }

  result.processed = true;
  return result;
}
