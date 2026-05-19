#pragma once

#include <QPoint>
#include <QPointF>
#include <QSizeF>

struct ImageCircle
{
  QPoint center;
  int radius = 0;
};

struct ImageRotatedRect
{
  QPointF center;
  QSizeF size;
  double angleDegrees = 0.0;
};

struct ImageLine
{
  QPointF start;
  QPointF end;
};
