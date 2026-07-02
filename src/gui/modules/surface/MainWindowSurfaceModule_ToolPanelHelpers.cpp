#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/TouchIconButton.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

QWidget* MainWindowSurfaceModule::createSurfaceLocalizationToolPanel(
  const CameraConfig& camera,
  const SurfaceLocalizationStrategyDefinition& strategy,
  QVBoxLayout** layout)
{
  context().clearToolPanel();

  auto* panel = new QWidget(toolsContainer());
  auto* panelLayout = new QVBoxLayout(panel);
  panelLayout->setContentsMargins(0, 0, 0, 0);
  panelLayout->setSpacing(6);

  auto* title = new QLabel(strategy.label + " | " + QString("%1 %2").arg(tr("labels.camera")).arg(camera.slot), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  panelLayout->addWidget(title);

  auto* note = new QLabel(strategy.note, panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  panelLayout->addWidget(note);

  if (layout)
  {
    *layout = panelLayout;
  }
  return panel;
}

void MainWindowSurfaceModule::addSurfaceDefectAoeButtons(
  const CameraConfig& camera,
  QVBoxLayout* layout,
  const std::function<void()>& testAction,
  bool includeMaskButton)
{
  auto* panel = qobject_cast<QWidget*>(layout->parentWidget());
  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setSpacing(6);

  auto* roiButton = createTouchIconButton("surfaceSearchRoi", tr("actions.surfaceSearchRoi"), buttons);
  auto* polygonButton = createTouchIconButton("polygon", tr("actions.polygon"), buttons);
  auto* clearLocalizationButton = createTouchIconButton("clear", tr("actions.clearLocalization"), buttons);
  auto* testButton = createTouchIconButton("testStrategy", tr("actions.testStrategy"), buttons);
  QObject::connect(roiButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectRoiDrawing(camera); });
  QObject::connect(polygonButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectPolygonDrawing(camera); });
  QObject::connect(clearLocalizationButton, &QPushButton::clicked, window(), [this, camera]() { clearSurfaceLocalization(camera); });
  QObject::connect(testButton, &QPushButton::clicked, window(), [testAction]() { testAction(); });

  buttonsLayout->addWidget(roiButton, 0, 0);
  buttonsLayout->addWidget(polygonButton, 0, 1);
  int row = 1;
  if (includeMaskButton)
  {
    auto* maskButton = createTouchIconButton("surfaceAddExclusion", tr("actions.surfaceAddExclusion"), buttons);
    QObject::connect(maskButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectExclusionDrawing(camera); });
    buttonsLayout->addWidget(maskButton, row++, 0, 1, 2);
  }
  buttonsLayout->addWidget(clearLocalizationButton, row++, 0, 1, 2);
  buttonsLayout->addWidget(testButton, row, 0, 1, 2);
  layout->addWidget(buttons);
}

void MainWindowSurfaceModule::addSurfaceLocalizationBackButton(const CameraConfig& camera, QVBoxLayout* layout)
{
  auto* panel = qobject_cast<QWidget*>(layout->parentWidget());
  auto* backButton = createTouchIconButton("back", tr("groups.localizationStrategies"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    context().showLocalizationStrategyList(camera);
    log(tr("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);
}
