#pragma once

#include "config/RecipeManager.h"
#include "runtime/PartPose.h"

#include <opencv2/core.hpp>

#include <QString>
#include <array>
#include <vector>

struct ThreadProfileColumn
{
  double warpedAlong = 0.0;
  double warpedCrestY = 0.0;
  double warpedRootY = 0.0;
  double crestX = 0.0;
  double crestY = 0.0;
  double rootX = 0.0;
  double rootY = 0.0;
  bool valid = false;
};

struct ThreadProfileResult
{
  bool valid = false;
  QString error;
  std::vector<ThreadProfileColumn> columns;
  cv::Mat diagnosticImage;
  int validColumns = 0;
  std::array<cv::Point2f, 4> imageQuad{};
  int warpedWidth = 0;
  int warpedHeight = 0;
};

class ThreadProfileExtractor
{
public:
  ThreadProfileResult extract(const cv::Mat& input, const cv::Rect& imageRoi, const ThreadInspectionSettings& settings) const;
  ThreadProfileResult extractOriented(
    const cv::Mat& input,
    const PartPose& pose,
    const ThreadInspectionSettings& settings) const;
};
