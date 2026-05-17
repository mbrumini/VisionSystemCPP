#pragma once

#include <opencv2/core.hpp>

#include <QString>
#include <QRect>

#include <vector>

enum class GeometryPolarity
{
  DarkOnLight,
  LightOnDark
};

struct GeometryDetectorRoi
{
  QRect rect;
  std::vector<cv::Rect> exclusions;
};

struct GeometryThresholdRange
{
  int minValue = 0;
  int maxValue = 80;
  GeometryPolarity polarity = GeometryPolarity::DarkOnLight;
};

struct GeometryDetectorResult
{
  bool processed = false;
  bool found = false;
  QString message;
  cv::Mat diagnosticImage;
};
