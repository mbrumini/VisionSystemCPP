#pragma once

#include "gui/geometry/BandGeometryEditor.h"
#include "gui/geometry/GeometryOverlay.h"

#include <QPointF>

struct LineGeometryEditorState
{
  bool hasStart = false;
  bool hasEnd = false;
  bool hasPreview = false;
  QPointF imageStart;
  QPointF imageEnd;
  QPointF imagePreview;
  double bandHalfWidth = 20.0;
};

class LineGeometryEditor
{
public:
  void reset();
  void setBandHalfWidth(double halfWidth);
  void setLine(const QPointF& imageStart, const QPointF& imageEnd);
  void movePoint(int pointIndex, const QPointF& imagePoint);
  bool handlePointClick(const QPointF& imagePoint);
  void handlePreviewPoint(const QPointF& imagePoint);

  bool complete() const;
  const LineGeometryEditorState& state() const;
  GeometryOverlay overlay() const;

private:
  LineGeometryEditorState m_state;
  BandGeometryEditor m_bandEditor;
};
