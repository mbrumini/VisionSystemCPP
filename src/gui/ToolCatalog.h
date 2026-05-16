#pragma once

#include "config/TranslationManager.h"

#include <QString>
#include <QVector>

struct ToolActionDefinition
{
  QString id;
  QString label;
};

struct ToolDefinition
{
  QString id;
  QString label;
  QVector<ToolActionDefinition> actions;
};

class ToolCatalog
{
public:
  static ToolDefinition tool(const QString& toolId, const TranslationManager& translations);
  static QString label(const QString& toolId, const TranslationManager& translations);
};
