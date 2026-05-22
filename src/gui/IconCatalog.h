#pragma once

#include <QIcon>
#include <QString>

class IconCatalog
{
public:
  static QIcon icon(const QString& id);
};
