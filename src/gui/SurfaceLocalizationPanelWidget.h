#pragma once

#include "config/RecipeManager.h"
#include "config/TranslationManager.h"

#include <QWidget>
#include <functional>

class SurfaceLocalizationPanelWidget : public QWidget
{
public:
  struct Handlers
  {
    std::function<void()> drawOuterCircle;
    std::function<void()> drawInnerCircle;
    std::function<void()> drawEdgeCircle;
    std::function<void()> drawThreePointCircle;
    std::function<void()> addExclusion;
    std::function<void()> clearExclusions;
    std::function<void(const QString&)> methodChanged;
    std::function<void(int)> thresholdChanged;
    std::function<void(int)> edgeSensitivityChanged;
    std::function<void(int, int)> edgeBandChanged;
    std::function<void(int)> edgeFitMaxErrorChanged;
    std::function<void()> back;
  };

  SurfaceLocalizationPanelWidget(
    const QString& titleText,
    const SurfaceAnnulusLocalizationConfig& annulus,
    const TranslationManager& translations,
    Handlers handlers,
    QWidget* parent = nullptr);
};
