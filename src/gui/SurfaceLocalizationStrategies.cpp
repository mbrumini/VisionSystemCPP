#include "SurfaceLocalizationStrategies.h"

namespace
{
struct StrategyTemplate
{
  QString id;
  QString labelKey;
  QString noteKey;
  bool implemented = false;
};

const QVector<StrategyTemplate>& strategies()
{
  static const QVector<StrategyTemplate> result = {
    {"threshold", "strategies.threshold", "strategyNotes.threshold", true},
    {"edge", "strategies.edge", "strategyNotes.edge", true},
    {"two_circles_axis", "strategies.two_circles_axis", "strategyNotes.two_circles_axis", true},
    {"edgePca", "strategies.edgePca", "strategyNotes.edgePca", true},
    {"massPca", "strategies.massPca", "strategyNotes.massPca", true},
    {"model", "strategies.model", "strategyNotes.model", false},
    {"aiYolo", "strategies.aiYolo", "strategyNotes.aiYolo", false}
  };

  return result;
}
}

QVector<SurfaceLocalizationStrategyDefinition> SurfaceLocalizationStrategies::all(const TranslationManager& translations)
{
  QVector<SurfaceLocalizationStrategyDefinition> result;

  for (const StrategyTemplate& item : strategies())
  {
    result.append({item.id, translations.text(item.labelKey), translations.text(item.noteKey), item.implemented});
  }

  return result;
}

SurfaceLocalizationStrategyDefinition SurfaceLocalizationStrategies::strategy(const QString& id, const TranslationManager& translations)
{
  for (const SurfaceLocalizationStrategyDefinition& item : all(translations))
  {
    if (item.id == id)
    {
      return item;
    }
  }

  return {id, id, translations.text("labels.placeholderNote"), false};
}
