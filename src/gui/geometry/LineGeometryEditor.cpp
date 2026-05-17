#include "LineGeometryEditor.h"

#include <algorithm>
#include <cmath>

void LineGeometryEditor::reset()
{
  m_state = {};
}

void LineGeometryEditor::setBandHalfWidth(double halfWidth)
{
  m_state.bandHalfWidth = std::max(2.0, halfWidth);
}

void LineGeometryEditor::setLine(const QPointF& imageStart, const QPointF& imageEnd)
{
  m_state.imageStart = imageStart;
  m_state.imageEnd = imageEnd;
  m_state.hasStart = true;
  m_state.hasEnd = true;
  m_state.hasPreview = false;
}

void LineGeometryEditor::movePoint(int pointIndex, const QPointF& imagePoint)
{
  if (pointIndex == 0 && m_state.hasStart)
  {
    m_state.imageStart = imagePoint;
  }
  else if (pointIndex == 1 && m_state.hasEnd)
  {
    m_state.imageEnd = imagePoint;
  }

  m_state.hasPreview = false;
}

bool LineGeometryEditor::handlePointClick(const QPointF& imagePoint)
{
  if (!m_state.hasStart || (m_state.hasStart && m_state.hasEnd))
  {
    m_state.imageStart = imagePoint;
    m_state.hasStart = true;
    m_state.hasEnd = false;
    m_state.hasPreview = false;
    return false;
  }

  m_state.imageEnd = imagePoint;
  m_state.hasEnd = true;
  m_state.hasPreview = false;
  return true;
}

void LineGeometryEditor::handlePreviewPoint(const QPointF& imagePoint)
{
  if (!m_state.hasStart || m_state.hasEnd)
  {
    return;
  }

  m_state.imagePreview = imagePoint;
  m_state.hasPreview = true;
}

bool LineGeometryEditor::complete() const
{
  return m_state.hasStart && m_state.hasEnd;
}

const LineGeometryEditorState& LineGeometryEditor::state() const
{
  return m_state;
}

GeometryOverlay LineGeometryEditor::overlay() const
{
  GeometryOverlay overlay;

  if (m_state.hasStart)
  {
    overlay.points.append({m_state.imageStart, "1"});
  }

  const bool drawLine = m_state.hasEnd || m_state.hasPreview;
  if (drawLine)
  {
    const QPointF endPoint = m_state.hasEnd ? m_state.imageEnd : m_state.imagePreview;
    overlay.points.append({endPoint, "2"});
    overlay.lines.append({m_state.imageStart, endPoint});

    const QPointF center = (m_state.imageStart + endPoint) * 0.5;
    const QPointF delta = endPoint - m_state.imageStart;
    const double length = std::hypot(delta.x(), delta.y());
    const double angleDegrees = std::atan2(delta.y(), delta.x()) * 180.0 / 3.14159265358979323846;
    overlay.bands.append({center, QSizeF(length, m_state.bandHalfWidth * 2.0), angleDegrees});
  }

  return overlay;
}
