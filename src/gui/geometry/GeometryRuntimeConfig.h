#pragma once

#include "processing/geometry/EdgeLineDetector.h"

#include <QString>

#include <opencv2/core/types.hpp>

struct GeometryLineRuntimeConfig
{
  QString id = "line_1";
  QString alias;
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
  bool anchorInImageSpace = false;
  bool hasImageLine = false;
  bool hasLine = false;
};

struct GeometryPointRuntimeConfig
{
  QString id = "point_1";
  QString alias;
  bool enabled = true;
  cv::Point2d imageStart;
  cv::Point2d imageEnd;
  cv::Point2d partStart;
  cv::Point2d partEnd;
  int edgeSensitivity = 60;
  bool useSubpixel = true;
  EdgeLineTransition transition = EdgeLineTransition::LightToDark;
  EdgeLinePickMode pickMode = EdgeLinePickMode::First;
  bool anchorInImageSpace = false;
  bool hasImageGuide = false;
  bool hasGuide = false;
};

struct GeometryCircleRuntimeConfig
{
  QString id = "circle_1";
  QString alias;
  bool enabled = true;
  cv::Point2d imageCenter;
  cv::Point2d partCenter;
  double radius = 0.0;
  int innerBand = 20;
  int outerBand = 20;
  int edgeSensitivity = 60;
  int edgeCleanupDerivative = 12;
  int edgeStatisticalFilter = 0;
  bool useSubpixel = true;
  EdgeLineScanDirection scanDirection = EdgeLineScanDirection::NormalPositive;
  EdgeLineTransition transition = EdgeLineTransition::LightToDark;
  EdgeLinePickMode pickMode = EdgeLinePickMode::First;
  bool anchorInImageSpace = false;
  bool hasImageCircle = false;
  bool hasCircle = false;
};

struct GeometryArcRuntimeConfig
{
  QString id = "arc_1";
  QString alias;
  bool enabled = true;
  cv::Point2d imageCenter;
  cv::Point2d imageStart;
  cv::Point2d imageEnd;
  cv::Point2d imageThrough;
  cv::Point2d partCenter;
  cv::Point2d partStart;
  cv::Point2d partEnd;
  cv::Point2d partThrough;
  double radius = 0.0;
  double startAngleRadians = 0.0;
  double endAngleRadians = 0.0;
  double partStartAngleRadians = 0.0;
  double partEndAngleRadians = 0.0;
  int innerBand = 20;
  int outerBand = 20;
  int edgeSensitivity = 60;
  int edgeCleanupDerivative = 12;
  int edgeStatisticalFilter = 0;
  bool useSubpixel = true;
  EdgeLineScanDirection scanDirection = EdgeLineScanDirection::NormalPositive;
  EdgeLineTransition transition = EdgeLineTransition::LightToDark;
  EdgeLinePickMode pickMode = EdgeLinePickMode::First;
  bool anchorInImageSpace = false;
  bool hasImageThrough = false;
  bool hasImageArc = false;
  bool hasArc = false;
};
