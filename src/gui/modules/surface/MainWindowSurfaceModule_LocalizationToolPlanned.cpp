#include "gui/modules/MainWindowSurfaceModule.h"

#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/TouchIconButton.h"

#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

void MainWindowSurfaceModule::showPlannedLocalizationPanel(const CameraConfig& camera, const QString& strategyId)
{
  const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, translations());
  QVBoxLayout* layout = nullptr;
  auto* panel = createSurfaceLocalizationToolPanel(camera, strategy, &layout);

  auto* controlsBox = new QGroupBox(tr("groups.strategyControls"), panel);
  auto* controlsLayout = new QVBoxLayout(controlsBox);
  controlsLayout->setSpacing(8);
  controlsLayout->addWidget(new QLabel(tr("labels.strategyPlanned"), controlsBox));
  controlsLayout->addWidget(createTouchIconButton("configureStrategy", tr("actions.configureStrategy"), controlsBox));
  controlsLayout->addWidget(createTouchIconButton("testStrategy", tr("actions.testStrategy"), controlsBox));
  layout->addWidget(controlsBox);

  addSurfaceLocalizationBackButton(camera, layout);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + strategy.label);
}
