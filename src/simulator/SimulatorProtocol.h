#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <opencv2/core/mat.hpp>

namespace SimulatorProtocol
{
constexpr int kVersion = 1;
constexpr qint64 kMaximumImagePayloadBytes = 12LL * 1024 * 1024;
constexpr qint64 kMaximumDecodedImageBytes = 12LL * 1024 * 1024;
constexpr const char* kPreferredImageFormat = "bmp";

struct DecodeImageResult
{
  bool ok = false;
  cv::Mat image;
  QString errorCode;
  QString errorMessage;
};

QString encodeImageBase64(const cv::Mat& image);
DecodeImageResult decodeImageFromBase64(const QByteArray& encodedImage);
}

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
