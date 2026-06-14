#include "ImageViewWidget.h"

#include <QPolygon>
#include <QPolygonF>

int ImageViewWidget::polygonVertexAt(const QPoint& widgetPoint) const
{
  for (int i = m_searchPolygon.size() - 1; i >= 0; --i)
  {
    const QPoint widgetVertex = imagePointToWidget(m_searchPolygon[i]);
    if (QRect(widgetVertex - QPoint(8, 8), QSize(16, 16)).contains(widgetPoint))
    {
      return i;
    }
  }

  return -1;
}

bool ImageViewWidget::polygonContains(const QPoint& widgetPoint) const
{
  if (m_searchPolygon.size() < 3)
  {
    return false;
  }

  QPolygonF polygon;
  for (const QPoint& point : m_searchPolygon)
  {
    polygon << imagePointToWidget(point);
  }
  return polygon.containsPoint(widgetPoint, Qt::OddEvenFill);
}

void ImageViewWidget::finishPolygonDrawing()
{
  if (m_pendingPolygon.size() < 3)
  {
    return;
  }

  m_searchPolygon = m_pendingPolygon;
  m_pendingPolygon.clear();
  m_selectedPolygonVertexIndex = -1;
  m_hasRoi = true;
  m_roi = QPolygon(m_searchPolygon).boundingRect().normalized();

  if (m_polygonChangedHandler)
  {
    m_polygonChangedHandler(m_searchPolygon);
  }

  update();
}
