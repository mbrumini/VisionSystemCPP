#pragma once

#include <QPixmap>
#include <QString>

#include <opencv2/core.hpp>

cv::Mat testVisionCreateShape(const QString& id, int width, int height, int background);
cv::Mat testVisionTransformImage(const cv::Mat& master, double angleDeg, double txPx, double tyPx);
QPixmap testVisionMatToPixmap(const cv::Mat& image);
