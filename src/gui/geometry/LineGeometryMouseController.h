#pragma once

#include "gui/geometry/LineGeometryEditor.h"

class LineGeometryMouseController
{
public:
  void begin(double bandHalfWidth);
  bool handleClick(const QPointF& imagePoint);
  void handleMove(const QPointF& imagePoint);
  void setLine(const QPointF& imageStart, const QPointF& imageEnd, double bandHalfWidth);
  void setBandHalfWidth(double bandHalfWidth);
  void movePoint(int pointIndex, const QPointF& imagePoint);

  bool complete() const;
  const LineGeometryEditorState& state() const;
  GeometryOverlay overlay() const;

private:
  LineGeometryEditor m_editor;
};
