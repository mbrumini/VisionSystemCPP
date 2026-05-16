#pragma once

#include "config/TranslationManager.h"

#include <QString>
#include <QVector>

struct SurfaceLocalizationStrategyDefinition
{
  QString id;
  QString label;
  QString note;
  bool implemented = false;
};

class SurfaceLocalizationStrategies
{
public:
  static QVector<SurfaceLocalizationStrategyDefinition> all(const TranslationManager& translations);
  static SurfaceLocalizationStrategyDefinition strategy(const QString& id, const TranslationManager& translations);
};
