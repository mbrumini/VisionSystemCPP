#pragma once

#include <opencv2/core/mat.hpp>

class ImageProcessor
{
public:
  bool process(const cv::Mat& input, cv::Mat& output);
};
