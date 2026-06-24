#include "gui/geometry/ConfiguredGeometryDetector.h"

#include "gui/geometry/ArcGuideMath.h"
#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryGuideRuntime.h"
#include "gui/geometry/GeometryOverlayPrimitives.h"
#include "processing/geometry/EdgeCircleDetector.h"
#include "processing/geometry/EdgeLineDetector.h"
#include "processing/geometry/EdgePointDetector.h"
#include "runtime/PartPose.h"

#include <QColor>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace
{
double normalizedSetupArcAngle(double angle)
{
  while (angle < 0.0)
  {
    angle += 2.0 * CV_PI;
  }
  while (angle >= 2.0 * CV_PI)
  {
    angle -= 2.0 * CV_PI;
  }
  return angle;
}
}

ConfiguredGeometryDetectOutput detectConfiguredGeometries(const ConfiguredGeometryDetectInput& input)
{
  ConfiguredGeometryDetectOutput output;
  if (input.image.empty())
  {
    return output;
  }

  cv::Mat diagnostic;
  if (input.buildDiagnostic)
  {
    if (input.image.channels() == 1)
    {
      cv::cvtColor(input.image, diagnostic, cv::COLOR_GRAY2BGR);
    }
    else
    {
      input.image.copyTo(diagnostic);
    }
  }

  const cv::Mat& image = input.image;
  const PartPose& pose = input.pose;
  const cv::Size imageSize = image.size();
  const QSize referenceSize = input.guideReferenceSize;

  for (const GeometryLineRuntimeConfig& line : input.lines)
  {
    if (!line.enabled)
    {
      continue;
    }

    const bool usePartLine = GeometryGuideRuntime::shouldUsePartGuide(
      pose.valid,
      line.hasLine,
      line.hasImageLine,
      line.anchorInImageSpace,
      referenceSize,
      imageSize);
    if (!usePartLine && !line.hasImageLine)
    {
      continue;
    }

    const cv::Point2d imageStart = GeometryGuideRuntime::resolveImagePoint(
      pose, usePartLine, line.partStart, line.imageStart, referenceSize, imageSize);
    const cv::Point2d imageEnd = GeometryGuideRuntime::resolveImagePoint(
      pose, usePartLine, line.partEnd, line.imageEnd, referenceSize, imageSize);

    EdgeLineDetectorConfig config;
    config.id = line.id;
    config.label = line.alias.trimmed().isEmpty() ? line.id : line.alias.trimmed();
    config.guideStart = imageStart;
    config.guideEnd = imageEnd;
    config.bandHalfWidth = line.bandHalfWidth;
    config.edgeSensitivity = line.edgeSensitivity;
    config.edgeCleanupDerivative = line.edgeCleanupDerivative;
    config.edgeStatisticalFilter = line.edgeStatisticalFilter;
    config.useSubpixel = input.useSubpixelDimensional && line.useSubpixel;
    config.scanDirection = line.scanDirection;
    config.transition = line.transition;
    config.pickMode = line.pickMode;

    EdgeLineDetector detector;
    const EdgeLineDetectorResult result = detector.detect(image, config);
    if (!result.processed || !result.found)
    {
      continue;
    }

    output.lines.append(result.line);
    if (!diagnostic.empty())
    {
      cv::line(
        diagnostic,
        cv::Point(static_cast<int>(std::round(result.line.start.x)), static_cast<int>(std::round(result.line.start.y))),
        cv::Point(static_cast<int>(std::round(result.line.end.x)), static_cast<int>(std::round(result.line.end.y))),
        cv::Scalar(0, 255, 0),
        4,
        cv::LINE_AA);
    }
    if (input.buildGuideOverlay)
    {
      output.guideOverlay.lines.append({
        QPointF(result.line.start.x, result.line.start.y),
        QPointF(result.line.end.x, result.line.end.y),
        QColor("#35c46a"),
        6
      });
    }
  }

  for (const GeometryPointRuntimeConfig& point : input.points)
  {
    if (!point.enabled || point.anchorInImageSpace || !point.hasImageGuide)
    {
      continue;
    }

    const bool usePartGuide = GeometryGuideRuntime::shouldUsePartGuide(
      pose.valid,
      point.hasGuide,
      point.hasImageGuide,
      point.anchorInImageSpace,
      referenceSize,
      imageSize);
    if (!usePartGuide && !point.hasImageGuide)
    {
      continue;
    }

    const cv::Point2d imageStart = GeometryGuideRuntime::resolveImagePoint(
      pose, usePartGuide, point.partStart, point.imageStart, referenceSize, imageSize);
    const cv::Point2d imageEnd = GeometryGuideRuntime::resolveImagePoint(
      pose, usePartGuide, point.partEnd, point.imageEnd, referenceSize, imageSize);

    EdgePointDetectorConfig config;
    config.id = point.id;
    config.label = point.alias.trimmed().isEmpty() ? point.id : point.alias.trimmed();
    config.scanStart = imageStart;
    config.scanEnd = imageEnd;
    config.edgeSensitivity = point.edgeSensitivity;
    config.useSubpixel = input.useSubpixelDimensional && point.useSubpixel;
    config.transition = point.transition;
    config.pickMode = point.pickMode;

    EdgePointDetector detector;
    const EdgePointDetectorResult result = detector.detect(image, config);
    if (!result.processed || !result.found)
    {
      continue;
    }

    output.points.append(result.point);
    if (!diagnostic.empty())
    {
      GeometryDiagnosticDrawing::drawOrangePointCross(diagnostic, result.point.point);
    }
    if (input.buildGuideOverlay)
    {
      GeometryDiagnosticDrawing::appendOrangePointCross(output.guideOverlay, result.point.point);
    }
  }

  for (const GeometryCircleRuntimeConfig& circle : input.circles)
  {
    if (!circle.enabled)
    {
      continue;
    }

    const bool usePartCircle = GeometryGuideRuntime::shouldUsePartGuide(
      pose.valid,
      circle.hasCircle,
      circle.hasImageCircle,
      circle.anchorInImageSpace,
      referenceSize,
      imageSize);
    if (!usePartCircle && !circle.hasImageCircle)
    {
      continue;
    }

    cv::Point2d guideCenter = GeometryGuideRuntime::resolveImagePoint(
      pose, usePartCircle, circle.partCenter, circle.imageCenter, referenceSize, imageSize);
    double guideRadius = circle.radius;
    if (!usePartCircle && referenceSize.isValid() && referenceSize.width() > 0 && referenceSize.height() > 0 &&
        imageSize.width > 0 && imageSize.height > 0 &&
        (referenceSize.width() != imageSize.width || referenceSize.height() != imageSize.height))
    {
      const double scale = 0.5 *
                           (static_cast<double>(imageSize.width) / static_cast<double>(referenceSize.width()) +
                            static_cast<double>(imageSize.height) / static_cast<double>(referenceSize.height()));
      guideRadius = circle.radius * scale;
    }
    const int guideRadiusInt = static_cast<int>(std::round(guideRadius));
    if (guideRadiusInt <= 0)
    {
      continue;
    }

    if (input.buildGuideOverlay)
    {
      appendGeometryCirclePolyline(
        output.guideOverlay, guideCenter, std::max(1.0, guideRadius - circle.innerBand), QColor(0, 210, 255, 90), 1);
      appendGeometryCirclePolyline(output.guideOverlay, guideCenter, guideRadius, QColor(0, 210, 255, 170), 2);
      appendGeometryCirclePolyline(
        output.guideOverlay, guideCenter, guideRadius + circle.outerBand, QColor(0, 210, 255, 90), 1);
    }

    EdgeCircleDetectorConfig config;
    config.id = circle.id;
    config.label = circle.alias.trimmed().isEmpty() ? circle.id : circle.alias.trimmed();
    config.guideCenter = guideCenter;
    config.guideRadius = guideRadius;
    config.innerBand = circle.innerBand;
    config.outerBand = circle.outerBand;
    config.edgeSensitivity = circle.edgeSensitivity;
    config.edgeCleanupDerivative = circle.edgeCleanupDerivative;
    config.edgeStatisticalFilter = circle.edgeStatisticalFilter;
    config.useSubpixel = input.useSubpixelDimensional && circle.useSubpixel;
    config.scanDirection = circle.scanDirection;
    config.transition = circle.transition;
    config.pickMode = circle.pickMode;

    EdgeCircleDetector detector;
    const EdgeCircleDetectorResult result = detector.detect(image, config);
    if (!result.processed || !result.found)
    {
      continue;
    }

    output.circles.append(result.circle);
    if (input.buildGuideOverlay)
    {
      appendGeometryCirclePolyline(output.guideOverlay, result.circle.center, result.circle.radius, QColor("#00d2ff"), 7);
    }
    if (!diagnostic.empty())
    {
      cv::circle(
        diagnostic,
        cv::Point(static_cast<int>(std::round(result.circle.center.x)), static_cast<int>(std::round(result.circle.center.y))),
        static_cast<int>(std::round(result.circle.radius)),
        cv::Scalar(255, 255, 0),
        4,
        cv::LINE_AA);
    }
  }

  for (const GeometryArcRuntimeConfig& arc : input.arcs)
  {
    if (!arc.enabled)
    {
      continue;
    }

    ResolvedArcGuide guide;
    if (!ArcGuideMath::resolveArcGuide(arc, pose, guide, referenceSize, imageSize))
    {
      continue;
    }

    const cv::Point2d guideCenter = guide.center;
    const double guideRadius = guide.radius;
    const double guideStartAngle = normalizedSetupArcAngle(guide.startAngle);
    const double guideEndAngle = normalizedSetupArcAngle(guide.endAngle);
    if (input.buildGuideOverlay)
    {
      appendGeometryArcPolyline(
        output.guideOverlay, guideCenter, std::max(1.0, guideRadius - arc.innerBand), guideStartAngle, guideEndAngle, QColor(255, 79, 216, 90), 1);
      appendGeometryArcPolyline(
        output.guideOverlay, guideCenter, guideRadius, guideStartAngle, guideEndAngle, QColor(255, 79, 216, 170), 2);
      appendGeometryArcPolyline(
        output.guideOverlay, guideCenter, guideRadius + arc.outerBand, guideStartAngle, guideEndAngle, QColor(255, 79, 216, 90), 1);
    }

    EdgeCircleDetectorConfig config;
    config.id = arc.id;
    config.label = arc.alias.trimmed().isEmpty() ? arc.id : arc.alias.trimmed();
    config.guideCenter = guideCenter;
    config.guideRadius = guideRadius;
    config.innerBand = arc.innerBand;
    config.outerBand = arc.outerBand;
    config.edgeSensitivity = arc.edgeSensitivity;
    config.edgeCleanupDerivative = arc.edgeCleanupDerivative;
    config.edgeStatisticalFilter = arc.edgeStatisticalFilter;
    config.useSubpixel = input.useSubpixelDimensional && arc.useSubpixel;
    config.scanDirection = arc.scanDirection;
    config.transition = arc.transition;
    config.pickMode = arc.pickMode;
    config.useArc = true;
    config.startAngleRadians = guideStartAngle;
    config.endAngleRadians = guideEndAngle;

    EdgeCircleDetector detector;
    const EdgeCircleDetectorResult result = detector.detect(image, config);
    if (!result.processed || !result.found)
    {
      continue;
    }

    output.arcs.append(result.arc);
    if (input.buildGuideOverlay)
    {
      appendGeometryArcPolyline(
        output.guideOverlay,
        result.arc.center,
        result.arc.radius,
        result.arc.startAngleRadians,
        result.arc.endAngleRadians,
        QColor("#ff4fd8"),
        7);
    }
    if (!diagnostic.empty())
    {
      double startDegrees = config.startAngleRadians * 180.0 / CV_PI;
      double endDegrees = config.endAngleRadians * 180.0 / CV_PI;
      if (endDegrees < startDegrees)
      {
        endDegrees += 360.0;
      }
      cv::ellipse(
        diagnostic,
        cv::Point(static_cast<int>(std::round(result.arc.center.x)), static_cast<int>(std::round(result.arc.center.y))),
        cv::Size(static_cast<int>(std::round(result.arc.radius)), static_cast<int>(std::round(result.arc.radius))),
        0.0,
        startDegrees,
        endDegrees,
        cv::Scalar(216, 79, 255),
        4,
        cv::LINE_AA);
    }
  }

  output.diagnostic = std::move(diagnostic);
  return output;
}
