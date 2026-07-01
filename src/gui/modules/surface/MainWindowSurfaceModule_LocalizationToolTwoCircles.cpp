#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/TouchIconButton.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

void MainWindowSurfaceModule::showTwoCirclesLocalizationPanel(const CameraConfig& camera)
{
  QString error;
  if (!recipes().saveSurfaceLocalizationMethod(camera.id, "two_circles_axis", &error))
  {
    log(error);
    return;
  }

  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  const SurfaceLocalizationStrategyConfig strategyConfig = recipes().loadSurfaceLocalizationStrategy(camera.id);
  const SurfaceLocalizationStrategyDefinition strategyDef = SurfaceLocalizationStrategies::strategy("two_circles_axis", translations());

  // Find existing feature configurations (or defaults)
  SurfaceStrategyFeatureConfig featureA;
  featureA.id = "circle_a";
  featureA.thresholdMax = 80;
  SurfaceStrategyFeatureConfig featureB;
  featureB.id = "circle_b";
  featureB.thresholdMax = 80;

  for (const SurfaceStrategyFeatureConfig& f : strategyConfig.features)
  {
    if (f.id == "circle_a") featureA = f;
    else if (f.id == "circle_b") featureB = f;
  }

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  // Title
  auto* title = new QLabel(strategyDef.label + " | " + QString("%1 %2").arg(tr("labels.camera")).arg(camera.slot), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  // Note
  auto* note = new QLabel(strategyDef.note, panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  // --- Group Circle A ---
  auto* groupA = new QGroupBox(tr("labels.surfaceCircleA") + " (circle_a)", panel);
  auto* layoutA = new QVBoxLayout(groupA);
  layoutA->setContentsMargins(8, 8, 8, 8);
  layoutA->setSpacing(6);

  auto* btnDrawA = createTouchIconButton("surfaceOuterCircle", tr("actions.surfaceCircleADraw"), groupA);
  QObject::connect(btnDrawA, &QPushButton::clicked, window(), [this, camera]() {
    activateSurfaceCircleADrawing(camera);
  });
  layoutA->addWidget(btnDrawA);

  auto* threshLayoutA = new QHBoxLayout();
  auto* lblThreshTextA = new QLabel(tr("labels.threshold"), groupA);
  lblThreshTextA->setObjectName("toolPanelNote");
  auto* lblThreshValA = new QLabel(QString::number(featureA.thresholdMax), groupA);
  lblThreshValA->setObjectName("toolPanelNote");
  lblThreshValA->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  threshLayoutA->addWidget(lblThreshTextA);
  threshLayoutA->addWidget(lblThreshValA);
  layoutA->addLayout(threshLayoutA);

  auto* sliderThreshA = new QSlider(Qt::Horizontal, groupA);
  sliderThreshA->setRange(0, 255);
  sliderThreshA->setValue(featureA.thresholdMax);
  sliderThreshA->setMinimumHeight(22);
  QObject::connect(sliderThreshA, &QSlider::valueChanged, window(), [this, camera, lblThreshValA](int val) {
    lblThreshValA->setText(QString::number(val));
    saveSurfaceStrategyThresholdAndPreview(camera, "circle_a", val);
  });
  layoutA->addWidget(sliderThreshA);
  layout->addWidget(groupA);

  // --- Group Circle B ---
  auto* groupB = new QGroupBox(tr("labels.surfaceCircleB") + " (circle_b)", panel);
  auto* layoutB = new QVBoxLayout(groupB);
  layoutB->setContentsMargins(8, 8, 8, 8);
  layoutB->setSpacing(6);

  auto* btnDrawB = createTouchIconButton("surfaceOuterCircle", tr("actions.surfaceCircleBDraw"), groupB);
  QObject::connect(btnDrawB, &QPushButton::clicked, window(), [this, camera]() {
    activateSurfaceCircleBDrawing(camera);
  });
  layoutB->addWidget(btnDrawB);

  auto* threshLayoutB = new QHBoxLayout();
  auto* lblThreshTextB = new QLabel(tr("labels.threshold"), groupB);
  lblThreshTextB->setObjectName("toolPanelNote");
  auto* lblThreshValB = new QLabel(QString::number(featureB.thresholdMax), groupB);
  lblThreshValB->setObjectName("toolPanelNote");
  lblThreshValB->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  threshLayoutB->addWidget(lblThreshTextB);
  threshLayoutB->addWidget(lblThreshValB);
  layoutB->addLayout(threshLayoutB);

  auto* sliderThreshB = new QSlider(Qt::Horizontal, groupB);
  sliderThreshB->setRange(0, 255);
  sliderThreshB->setValue(featureB.thresholdMax);
  sliderThreshB->setMinimumHeight(22);
  QObject::connect(sliderThreshB, &QSlider::valueChanged, window(), [this, camera, lblThreshValB](int val) {
    lblThreshValB->setText(QString::number(val));
    saveSurfaceStrategyThresholdAndPreview(camera, "circle_b", val);
  });
  layoutB->addWidget(sliderThreshB);
  layout->addWidget(groupB);

  // --- Actions ---
  auto* actionsGroup = new QWidget(panel);
  auto* actionsLayout = new QGridLayout(actionsGroup);
  actionsLayout->setContentsMargins(0, 0, 0, 0);
  actionsLayout->setSpacing(6);

  auto* maskButton = createTouchIconButton("surfaceAddExclusion", tr("actions.surfaceAddExclusion"), actionsGroup);
  auto* clearLocalizationButton = createTouchIconButton("clear", tr("actions.clearLocalization"), actionsGroup);
  auto* testButton = createTouchIconButton("testStrategy", tr("actions.testSurfaceStrategy"), actionsGroup);

  QObject::connect(maskButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectExclusionDrawing(camera); });
  QObject::connect(clearLocalizationButton, &QPushButton::clicked, window(), [this, camera]() { clearSurfaceLocalization(camera); });
  QObject::connect(testButton, &QPushButton::clicked, window(), [this, camera]() { testSurfaceLocalizationStrategy(camera); });

  actionsLayout->addWidget(maskButton, 0, 0, 1, 2);
  actionsLayout->addWidget(clearLocalizationButton, 1, 0, 1, 2);
  actionsLayout->addWidget(testButton, 2, 0, 1, 2);
  layout->addWidget(actionsGroup);

  // --- Back Button ---
  auto* backButton = createTouchIconButton("back", tr("groups.localizationStrategies"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    context().deactivateImageDrawingTools();
    context().showLocalizationStrategyList(camera);
    log(tr("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::SurfaceDefects;
  log(tr("log.toolPanel") + ": " + strategyDef.label);

  showStoredSurfaceStrategyGeometry(camera, strategyConfig);
  testSurfaceLocalizationStrategy(camera);
}
