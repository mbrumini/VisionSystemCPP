#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>

class IconCatalog
{
public:
  static QIcon icon(const QString& id);
  static QString iconSet();
  static void setIconSet(const QString& iconSet);
  static QStringList iconSetIds();
  static QString iconSetLabel(const QString& iconSet);
};
