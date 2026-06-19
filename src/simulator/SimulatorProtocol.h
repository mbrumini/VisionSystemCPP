#pragma once

#include <QDateTime>
#include <QString>

#include <opencv2/core/mat.hpp>

struct SimulatorFrameMetadata
{
  int protocolVersion = 1;
  QString scenarioId;
  QString cameraId;
  QString channel;
  QString strategyId;
  QString recipeId;
  int slot = 0;
  qint64 frameId = -1;
  QDateTime timestamp;

  bool valid() const
  {
    return frameId >= 0 && !channel.isEmpty();
  }
};

struct SimulatorFrame
{
  SimulatorFrameMetadata metadata;
  cv::Mat image;
};
