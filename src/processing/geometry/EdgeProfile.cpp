#include "processing/geometry/EdgeProfile.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kEpsilon = 0.000001;

cv::Point2d interpolatedPoint(
  const EdgeProfileSample& previous,
  const EdgeProfileSample& current,
  bool useSubpixel)
{
  const double delta = current.value - previous.value;
  if (!useSubpixel || std::abs(delta) <= kEpsilon)
  {
    return current.point;
  }

  const double target = (previous.value + current.value) * 0.5;
  const double t = std::clamp((target - previous.value) / delta, 0.0, 1.0);
  return previous.point + (current.point - previous.point) * t;
}
}

bool EdgeProfileSampler::pointInsideImage(const cv::Mat& image, const cv::Point2d& point)
{
  return point.x >= 0.0 && point.y >= 0.0 && point.x < image.cols && point.y < image.rows;
}

double EdgeProfileSampler::sampleBilinear(const cv::Mat& gray, const cv::Point2d& point)
{
  const double x = std::clamp(point.x, 0.0, static_cast<double>(gray.cols - 1));
  const double y = std::clamp(point.y, 0.0, static_cast<double>(gray.rows - 1));
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(x0 + 1, gray.cols - 1);
  const int y1 = std::min(y0 + 1, gray.rows - 1);
  const double tx = x - static_cast<double>(x0);
  const double ty = y - static_cast<double>(y0);
  const double top = gray.at<uchar>(y0, x0) * (1.0 - tx) + gray.at<uchar>(y0, x1) * tx;
  const double bottom = gray.at<uchar>(y1, x0) * (1.0 - tx) + gray.at<uchar>(y1, x1) * tx;
  return top * (1.0 - ty) + bottom * ty;
}

std::vector<EdgeProfileSample> EdgeProfileSampler::sampleLine(
  const cv::Mat& gray,
  const cv::Point2d& start,
  const cv::Point2d& end,
  int steps)
{
  std::vector<EdgeProfileSample> profile;
  if (gray.empty() || steps <= 0)
  {
    return profile;
  }

  profile.reserve(static_cast<size_t>(steps) + 1);
  for (int step = 0; step <= steps; ++step)
  {
    const double t = static_cast<double>(step) / static_cast<double>(steps);
    const cv::Point2d point = start + (end - start) * t;
    if (!pointInsideImage(gray, point))
    {
      continue;
    }

    profile.push_back({point, sampleBilinear(gray, point)});
  }
  return profile;
}

EdgeProfileCandidate SubpixelEdgeLocator::locate(
  const std::vector<EdgeProfileSample>& profile,
  EdgeLineTransition transition,
  EdgeLinePickMode pickMode,
  int gradientThreshold,
  bool useSubpixel)
{
  EdgeProfileCandidate best;
  if (profile.size() < 2)
  {
    return best;
  }

  for (int i = 1; i < static_cast<int>(profile.size()); ++i)
  {
    const double delta = profile[i].value - profile[i - 1].value;
    const bool matchesTransition =
      (transition == EdgeLineTransition::DarkToLight && delta >= gradientThreshold) ||
      (transition == EdgeLineTransition::LightToDark && delta <= -gradientThreshold);
    if (!matchesTransition)
    {
      continue;
    }

    EdgeProfileCandidate candidate;
    candidate.found = true;
    candidate.point = interpolatedPoint(profile[i - 1], profile[i], useSubpixel);
    candidate.strength = std::abs(delta);
    candidate.index = i;

    if (pickMode == EdgeLinePickMode::First)
    {
      return candidate;
    }

    if (!best.found || pickMode == EdgeLinePickMode::Last || candidate.strength > best.strength)
    {
      best = candidate;
    }
  }

  return best;
}
