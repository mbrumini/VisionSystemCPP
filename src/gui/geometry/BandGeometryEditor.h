#pragma once

#include "gui/geometry/GeometryOverlay.h"

#include <QPointF>

class BandGeometryEditor
{
public:
  void setHalfWidth(double halfWidth);
  double halfWidth() const;

  GeometryOverlayBand bandForSegment(
    const QPointF& imageStart,
    const QPointF& imageEnd,
    const QColor& outlineColor = QColor("#00d2ff"),
    const QColor& fillColor = QColor(0, 210, 255, 35)) const;

private:
  double m_halfWidth = 20.0;
};
