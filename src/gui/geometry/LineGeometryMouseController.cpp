#include "LineGeometryMouseController.h"

void LineGeometryMouseController::begin(double bandHalfWidth)
{
  m_editor.reset();
  m_editor.setBandHalfWidth(bandHalfWidth);
}

bool LineGeometryMouseController::handleClick(const QPointF& imagePoint)
{
  return m_editor.handlePointClick(imagePoint);
}

void LineGeometryMouseController::handleMove(const QPointF& imagePoint)
{
  m_editor.handlePreviewPoint(imagePoint);
}

void LineGeometryMouseController::setLine(const QPointF& imageStart, const QPointF& imageEnd, double bandHalfWidth)
{
  m_editor.setLine(imageStart, imageEnd);
  m_editor.setBandHalfWidth(bandHalfWidth);
}

void LineGeometryMouseController::setBandHalfWidth(double bandHalfWidth)
{
  m_editor.setBandHalfWidth(bandHalfWidth);
}

void LineGeometryMouseController::movePoint(int pointIndex, const QPointF& imagePoint)
{
  m_editor.movePoint(pointIndex, imagePoint);
}

bool LineGeometryMouseController::complete() const
{
  return m_editor.complete();
}

const LineGeometryEditorState& LineGeometryMouseController::state() const
{
  return m_editor.state();
}

GeometryOverlay LineGeometryMouseController::overlay() const
{
  return m_editor.overlay();
}
