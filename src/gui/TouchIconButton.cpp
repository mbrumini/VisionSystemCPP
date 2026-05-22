#include "gui/TouchIconButton.h"

#include "gui/IconCatalog.h"

#include <QPushButton>

QPushButton* createTouchIconButton(const QString& iconId, const QString& label, QWidget* parent)
{
  auto* button = new QPushButton(parent);
  button->setObjectName("touchIconButton");
  button->setIcon(IconCatalog::icon(iconId));
  button->setIconSize(QSize(28, 28));
  button->setToolTip(label);
  button->setAccessibleName(label);
  button->setMinimumSize(58, 58);
  button->setMaximumSize(68, 68);
  return button;
}

QString iconIdForActionKey(const QString& actionKey)
{
  if (actionKey.contains("Point") || actionKey.contains("point") || actionKey.contains("punto"))
  {
    return "pointGeometry";
  }
  if (actionKey.contains("Line") || actionKey.contains("line") || actionKey.contains("linea"))
  {
    return "lineGeometry";
  }
  if (actionKey.contains("Circle") || actionKey.contains("circle") || actionKey.contains("cerchio"))
  {
    return "circleGeometry";
  }
  if (actionKey.contains("Arc") || actionKey.contains("arc") || actionKey.contains("arco"))
  {
    return "arcGeometry";
  }
  if (actionKey.contains("Angle") || actionKey.contains("angle") || actionKey.contains("angolo"))
  {
    return "angle";
  }
  if (actionKey.contains("Distance") || actionKey.contains("distance") || actionKey.contains("distanza"))
  {
    return "distance";
  }
  if (actionKey.contains("Diameter") || actionKey.contains("diameter") || actionKey.contains("diametro"))
  {
    return "diameter";
  }
  if (actionKey.contains("Intersection") || actionKey.contains("intersection") || actionKey.contains("intersezione"))
  {
    return "lineLineIntersection";
  }
  if (actionKey.contains("Midpoint") || actionKey.contains("midpoint") || actionKey.contains("medio"))
  {
    return "pointGeometry";
  }
  if (actionKey.contains("back") || actionKey.contains("Torna"))
  {
    return "back";
  }
  if (actionKey.contains("test") || actionKey.contains("Prova"))
  {
    return "start";
  }
  return "geometries";
}
