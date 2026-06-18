#pragma once

#include <QColor>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QVector>

struct GeometryOverlayPoint
{
  QPointF imagePoint;
  QString label;
  QColor color = QColor("#ff4fd8");
};

struct GeometryOverlayLine
{
  QPointF imageStart;
  QPointF imageEnd;
  QColor color = QColor("#ff4fd8");
  int width = 3;
};

struct GeometryOverlayBand
{
  QPointF imageCenter;
  QSizeF imageSize;
  double angleDegrees = 0.0;
  QColor outlineColor = QColor("#00d2ff");
  QColor fillColor = QColor(0, 210, 255, 35);
};

struct GeometryOverlayDimension
{
  QPointF imageStart;
  QPointF imageEnd;
  QString label;
  QColor color = QColor("#ff8a00");
  int width = 3;
  QString id;
  QPointF labelPoint;
  bool hasLabelPoint = false;
  QColor labelColor = QColor("#ff8a00");
};

struct GeometryOverlayAngle
{
  QPointF imageCenter;
  QPointF imageArmA;
  QPointF imageArmB;
  QString label;
  QColor color = QColor("#ff8a00");
  int width = 3;
  QColor labelColor = QColor("#ff8a00");
};

struct GeometryOverlay
{
  QVector<GeometryOverlayPoint> points;
  QVector<GeometryOverlayLine> lines;
  QVector<GeometryOverlayBand> bands;
  QVector<GeometryOverlayDimension> dimensions;
  QVector<GeometryOverlayAngle> angles;

  void clear();
  bool empty() const;
};
