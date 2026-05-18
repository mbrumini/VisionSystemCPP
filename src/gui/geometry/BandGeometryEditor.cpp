#include "BandGeometryEditor.h"

#include <algorithm>
#include <cmath>

void BandGeometryEditor::setHalfWidth(double halfWidth)
{
  m_halfWidth = std::max(2.0, halfWidth);
}

double BandGeometryEditor::halfWidth() const
{
  return m_halfWidth;
}

GeometryOverlayBand BandGeometryEditor::bandForSegment(
  const QPointF& imageStart,
  const QPointF& imageEnd,
  const QColor& outlineColor,
  const QColor& fillColor) const
{
  const QPointF center = (imageStart + imageEnd) * 0.5;
  const QPointF delta = imageEnd - imageStart;
  const double length = std::hypot(delta.x(), delta.y());
  const double angleDegrees = std::atan2(delta.y(), delta.x()) * 180.0 / 3.14159265358979323846;

  return {center, QSizeF(length, m_halfWidth * 2.0), angleDegrees, outlineColor, fillColor};
}
