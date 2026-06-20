#pragma once

#include <QPoint>
#include <QString>
#include <QVector>

class QSize;

struct AiMaskLabelPaths
{
  QString maskPath;
  QString labelPath;
};

class AiMaskLabelStorage
{
public:
  static bool savePolygon(
    const QString& sourceImagePath,
    const QString& masksDirectory,
    const QString& labelsDirectory,
    const QVector<QPoint>& polygon,
    AiMaskLabelPaths* savedPaths = nullptr,
    QString* errorMessage = nullptr);

  static QString yoloSegmentationLine(
    const QVector<QPoint>& polygon,
    const QSize& imageSize,
    int classId = 0);

  static QVector<QPoint> loadPolygon(
    const QString& sourceImagePath,
    const QString& labelsDirectory,
    QString* errorMessage = nullptr);
};
