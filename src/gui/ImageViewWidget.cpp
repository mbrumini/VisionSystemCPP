#include "ImageViewWidget.h"

#include <QPolygonF>

#include <algorithm>
#include <cmath>

ImageViewWidget::ImageViewWidget(QWidget* parent)
  : QWidget(parent)
{
  setMinimumSize(320, 240);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
}

