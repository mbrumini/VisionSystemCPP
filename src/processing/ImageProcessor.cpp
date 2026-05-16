#include "ImageProcessor.h"

#include <opencv2/imgproc.hpp>

bool ImageProcessor::process(const cv::Mat& input, cv::Mat& output)
{
  if (input.empty())
  {
    return false;
  }

  cv::Mat grayscale;
  cv::cvtColor(input, grayscale, cv::COLOR_BGR2GRAY);
  cv::cvtColor(grayscale, output, cv::COLOR_GRAY2BGR);

  return true;
}
