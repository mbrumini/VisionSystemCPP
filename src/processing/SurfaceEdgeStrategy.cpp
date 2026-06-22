#include "SurfaceEdgeStrategy.h"

#include "processing/CircleFit.h"
#include "processing/SurfaceProcessingUtils.h"

SurfaceDefectResult SurfaceEdgeStrategy::locateAnnulus(
  const cv::Mat& input,
  const SurfaceAnnulusThresholdConfig& config,
  const std::vector<cv::Rect>& exclusionRects,
  bool createDiagnosticImage,
  bool drawContours) const
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

  cv::Mat blurred;
  cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0.0);

  const int high = std::clamp(config.edgeSensitivity, 1, 255);
  const int low = std::max(1, high / 2);
  cv::Mat edgeMask;
  cv::Canny(blurred, edgeMask, low, high);

  cv::Mat annulusMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  cv::circle(annulusMask, config.center, config.outerRadius, cv::Scalar(255), -1);
  cv::circle(annulusMask, config.center, config.innerRadius, cv::Scalar(0), -1);
  cv::bitwise_and(edgeMask, annulusMask, edgeMask);

  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect maskExclusion = clampSurfaceRect(exclusionRect, edgeMask.size());

    if (!maskExclusion.empty())
    {
      edgeMask(maskExclusion).setTo(0);
    }
  }

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(edgeMask, edgeMask, cv::MORPH_CLOSE, kernel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(edgeMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  if (createDiagnosticImage)
  {
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
  }

  std::vector<cv::Point> edgePixels;
  cv::findNonZero(edgeMask, edgePixels);

  if (createDiagnosticImage && drawContours)
  {
    // Draw contours with shadow
    for (const std::vector<cv::Point>& contour : contours)
    {
      cv::drawContours(result.diagnosticImage, std::vector<std::vector<cv::Point>>{contour}, 0, cv::Scalar(16, 16, 16), 4, cv::LINE_AA);
      cv::drawContours(result.diagnosticImage, std::vector<std::vector<cv::Point>>{contour}, 0, cv::Scalar(94, 197, 34), 2, cv::LINE_AA);
    }
  }

  if (!edgePixels.empty())
  {
    const cv::Moments moments = cv::moments(edgeMask, true);
    const cv::Point2d massCenter = moments.m00 == 0.0
      ? cv::Point2d(config.center)
      : cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);
    CircleFitSettings fitSettings;
    fitSettings.maxRadialError = config.edgeFitMaxError;
    const CircleFitResult fit = CircleFit::fit(edgePixels, fitSettings);

    SurfaceBlob mergedBlob;
    mergedBlob.area = static_cast<double>(edgePixels.size());
    mergedBlob.center = fit.found ? fit.center : massCenter;
    mergedBlob.boundingRect = cv::boundingRect(edgePixels);

    const std::vector<cv::Point>& contourPoints = fit.found && !fit.inliers.empty() ? fit.inliers : edgePixels;
    if (contourPoints.size() >= 3)
    {
      cv::convexHull(contourPoints, mergedBlob.contour);
    }

    result.totalArea = mergedBlob.area;
    result.blobs.push_back(mergedBlob);

    if (createDiagnosticImage && drawContours)
    {
      cv::rectangle(result.diagnosticImage, mergedBlob.boundingRect, cv::Scalar(255, 0, 0), 2);

      // Draw the inlier points used for fitting with high visibility (fine dots with shadow)
      for (const cv::Point& pt : contourPoints)
      {
        cv::circle(result.diagnosticImage, pt, 3, cv::Scalar(16, 16, 16), -1, cv::LINE_AA);
        cv::circle(result.diagnosticImage, pt, 2, cv::Scalar(0, 255, 255), -1, cv::LINE_AA);
      }
    }

    if (fit.found)
    {
      const cv::Point centerPt = surfacePoint(fit.center);
      const int radiusPt = static_cast<int>(std::round(fit.radius));
      if (createDiagnosticImage && drawContours)
      {
        // Draw fitted circle with shadow
        cv::circle(result.diagnosticImage, centerPt, radiusPt, cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
        cv::circle(result.diagnosticImage, centerPt, radiusPt, cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
      }
      result.localization.found = true;
      result.localization.method = "edge_circle_fit";
      result.localization.center = fit.center;
      result.localization.radius = fit.radius;
      result.localization.meanError = fit.meanError;
      result.localization.inputPoints = fit.inputPoints;
      result.localization.usedPoints = fit.usedPoints;
      result.localization.score = fit.inputPoints <= 0 ? 0.0 : static_cast<double>(fit.usedPoints) / static_cast<double>(fit.inputPoints);
    }
    else
    {
      result.localization.found = true;
      result.localization.method = "edge_mass_center";
      result.localization.center = massCenter;
      result.localization.inputPoints = static_cast<int>(edgePixels.size());
      result.localization.usedPoints = static_cast<int>(edgePixels.size());
      result.localization.score = 1.0;
    }
    if (createDiagnosticImage)
    {
      drawStyledCenterOfMass(result.diagnosticImage, mergedBlob.center);
    }
  }

  result.processed = true;
  return result;
}
