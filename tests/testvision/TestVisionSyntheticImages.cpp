#include "TestVisionSyntheticImages.h"

#include <QImage>

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <vector>

namespace
{
cv::Mat createAsymmetricCross(int width, int height, int background)
{
  cv::Mat image(height, width, CV_8UC3, cv::Scalar(background, background, background));
  const cv::Point center(width / 2, height / 2);
  const cv::Scalar dark(20, 20, 20);
  cv::line(image, center + cv::Point(-150, 0), center + cv::Point(185, 0), dark, 26, cv::LINE_AA);
  cv::line(image, center + cv::Point(0, -120), center + cv::Point(0, 145), dark, 26, cv::LINE_AA);
  cv::circle(image, center + cv::Point(145, 0), 22, cv::Scalar(80, 80, 80), -1, cv::LINE_AA);
  cv::circle(image, center, 14, cv::Scalar(240, 240, 240), -1, cv::LINE_AA);
  cv::line(image, center, center + cv::Point(95, -70), dark, 12, cv::LINE_AA);
  return image;
}

cv::Mat createScrewShank(int width, int height, int background)
{
  cv::Mat image(height, width, CV_8UC3, cv::Scalar(background, background, background));
  const cv::Scalar dark(12, 12, 12);
  const int centerY = height / 2;
  const int coreHalf = std::max(16, height / 28);
  const int threadAmp = std::max(7, coreHalf / 2);
  const int pitch = std::max(12, coreHalf);

  const int leftX = 0;
  const int threadStart = width * 24 / 100;
  const int threadEnd = width * 66 / 100;
  const int rightEnd = width * 79 / 100;

  cv::rectangle(
    image,
    cv::Point(leftX, centerY - coreHalf),
    cv::Point(threadStart, centerY + coreHalf),
    dark,
    cv::FILLED,
    cv::LINE_AA);

  std::vector<cv::Point> threadProfile;
  threadProfile.emplace_back(threadStart, centerY - coreHalf);
  for (int x = threadStart; x < threadEnd; x += pitch)
  {
    const int peakX = std::min(x + pitch / 2, threadEnd);
    const int nextX = std::min(x + pitch, threadEnd);
    threadProfile.emplace_back(peakX, centerY - coreHalf - threadAmp);
    threadProfile.emplace_back(nextX, centerY - coreHalf);
  }
  threadProfile.emplace_back(threadEnd, centerY + coreHalf);
  for (int x = threadEnd; x > threadStart; x -= pitch)
  {
    const int peakX = std::max(x - pitch / 2, threadStart);
    const int prevX = std::max(x - pitch, threadStart);
    threadProfile.emplace_back(peakX, centerY + coreHalf + threadAmp);
    threadProfile.emplace_back(prevX, centerY + coreHalf);
  }
  cv::fillPoly(image, std::vector<std::vector<cv::Point>>{threadProfile}, dark, cv::LINE_AA);

  const int tipStart = std::max(threadEnd, rightEnd - coreHalf * 2);
  cv::rectangle(
    image,
    cv::Point(threadEnd, centerY - coreHalf),
    cv::Point(tipStart, centerY + coreHalf),
    dark,
    cv::FILLED,
    cv::LINE_AA);
  const std::vector<cv::Point> tip = {
    cv::Point(tipStart, centerY - coreHalf),
    cv::Point(rightEnd, centerY),
    cv::Point(tipStart, centerY + coreHalf)
  };
  cv::fillConvexPoly(image, tip, dark, cv::LINE_AA);

  return image;
}
}

cv::Mat testVisionCreateShape(const QString& id, int width, int height, int background)
{
  if (id == "cross")
  {
    return createAsymmetricCross(width, height, background);
  }

  cv::Mat image(height, width, CV_8UC3, cv::Scalar(background, background, background));
  const cv::Point center(width / 2, height / 2);
  const cv::Scalar dark(20, 20, 20);
  if (id == "rectangle")
  {
    cv::RotatedRect rect(center, cv::Size2f(300, 170), 0.0F);
    cv::Point2f points[4];
    rect.points(points);
    for (int i = 0; i < 4; ++i)
    {
      cv::line(image, points[i], points[(i + 1) % 4], dark, 24, cv::LINE_AA);
    }
    cv::circle(image, center + cv::Point(95, -45), 24, dark, -1, cv::LINE_AA);
  }
  else if (id == "circles")
  {
    cv::circle(image, center, 145, dark, 22, cv::LINE_AA);
    cv::circle(image, center, 75, dark, 18, cv::LINE_AA);
    cv::circle(image, center + cv::Point(95, 0), 16, dark, -1, cv::LINE_AA);
  }
  else if (id == "plate")
  {
    cv::rectangle(image, center + cv::Point(-170, -110), center + cv::Point(170, 110), dark, 20, cv::LINE_AA);
    for (const cv::Point& offset : {
           cv::Point(-105, -55), cv::Point(100, -55),
           cv::Point(-105, 55), cv::Point(100, 55)})
    {
      cv::circle(image, center + offset, 25, dark, -1, cv::LINE_AA);
    }
    cv::circle(image, center + cv::Point(35, 0), 42, dark, 15, cv::LINE_AA);
  }
  else if (id == "l_profile")
  {
    std::vector<cv::Point> polygon = {
      center + cv::Point(-150, -120),
      center + cv::Point(-70, -120),
      center + cv::Point(-70, 45),
      center + cv::Point(155, 45),
      center + cv::Point(155, 120),
      center + cv::Point(-150, 120)
    };
    cv::fillPoly(image, std::vector<std::vector<cv::Point>>{polygon}, dark);
    cv::circle(image, center + cv::Point(80, 82), 22, cv::Scalar(background, background, background), -1, cv::LINE_AA);
  }
  else if (id == "screw_shank")
  {
    return createScrewShank(width, height, background);
  }
  else if (id == "gear")
  {
    const int teeth = 12;
    std::vector<cv::Point> polygon;
    for (int i = 0; i < teeth * 2; ++i)
    {
      const double angle = i * CV_PI / teeth;
      const double radius = (i % 2 == 0) ? 145.0 : 112.0;
      polygon.emplace_back(
        center.x + static_cast<int>(std::round(radius * std::cos(angle))),
        center.y + static_cast<int>(std::round(radius * std::sin(angle))));
    }
    cv::fillPoly(image, std::vector<std::vector<cv::Point>>{polygon}, dark);
    cv::circle(image, center, 48, cv::Scalar(background, background, background), -1, cv::LINE_AA);
    cv::circle(image, center + cv::Point(72, 0), 15, cv::Scalar(background, background, background), -1, cv::LINE_AA);
  }
  return image;
}

cv::Mat testVisionTransformImage(const cv::Mat& master, double angleDeg, double txPx, double tyPx)
{
  const cv::Point2f center(master.cols * 0.5F, master.rows * 0.5F);
  cv::Mat matrix = cv::getRotationMatrix2D(center, angleDeg, 1.0);
  matrix.at<double>(0, 2) += txPx;
  matrix.at<double>(1, 2) += tyPx;
  cv::Mat output;
  cv::warpAffine(
    master,
    output,
    matrix,
    master.size(),
    cv::INTER_LINEAR,
    cv::BORDER_CONSTANT,
    cv::Scalar(240, 240, 240));
  return output;
}

QPixmap testVisionMatToPixmap(const cv::Mat& image)
{
  if (image.empty())
  {
    return {};
  }

  cv::Mat rgb;
  cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
  return QPixmap::fromImage(
    QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy());
}
