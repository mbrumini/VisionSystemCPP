#pragma once

#include "config/RecipeManager.h"

#include <opencv2/core.hpp>

#include <QString>
#include <vector>

struct ThreadProfileColumn
{
  int x = 0;
  double crestY = 0.0;
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
};

class ThreadProfileExtractor
{
public:
  ThreadProfileResult extract(const cv::Mat& input, const cv::Rect& imageRoi, const ThreadInspectionSettings& settings) const;
};
