#pragma once

#include "gui/ImagePrimitives.h"

#include <QPoint>
#include <QVector>

namespace GeometryMath
{
bool circleFromThreePoints(const QVector<QPoint>& points, ImageCircle& circle);
}
