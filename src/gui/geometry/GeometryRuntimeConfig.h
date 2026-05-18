#pragma once

#include "processing/geometry/EdgeLineDetector.h"

#include <QString>

#include <opencv2/core/types.hpp>

struct GeometryLineRuntimeConfig
{
  QString id = "line_1";
  bool enabled = true;
  cv::Point2d imageStart;
  cv::Point2d imageEnd;
  cv::Point2d partStart;
  cv::Point2d partEnd;
  int bandHalfWidth = 20;
  int edgeSensitivity = 60;
  int edgeCleanupDerivative = 12;
  int edgeStatisticalFilter = 0;
  bool useSubpixel = true;
  EdgeLineScanDirection scanDirection = EdgeLineScanDirection::NormalPositive;
  EdgeLineTransition transition = EdgeLineTransition::LightToDark;
  EdgeLinePickMode pickMode = EdgeLinePickMode::First;
  bool hasImageLine = false;
  bool hasLine = false;
};

struct GeometryPointRuntimeConfig
{
  QString id = "point_1";
  bool enabled = true;
  cv::Point2d imageStart;
  cv::Point2d imageEnd;
  cv::Point2d partStart;
  cv::Point2d partEnd;
  int edgeSensitivity = 60;
  bool useSubpixel = true;
  EdgeLineTransition transition = EdgeLineTransition::LightToDark;
  EdgeLinePickMode pickMode = EdgeLinePickMode::First;
  bool hasImageGuide = false;
  bool hasGuide = false;
};
