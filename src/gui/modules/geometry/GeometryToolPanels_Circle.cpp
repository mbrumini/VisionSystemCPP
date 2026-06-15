#include "gui/modules/QtCompat.h"

#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/geometry/GeometryPanelNavigation.h"
#include "gui/TouchIconButton.h"

#include "processing/geometry/EdgePointDetector.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include <opencv2/imgproc.hpp>

void MainWindowGeometryModule::showGeometryCirclePanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Circle;
  if (context().setup) { context().setup->refreshPoseForCurrentFrame(camera); }
  restoreCleanGeometryImage(camera);
  largeImage()->clearCircles();
  loadGeometryCirclesRecipe(camera);

  QVector<GeometryCircleRuntimeConfig>& circleConfigs = m_circleConfigs[camera.id];
  GeometryCircleRuntimeConfig& circleConfig = activeGeometryCircleConfig(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("actions.circleGeometry"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* poseLabel = new QLabel(pose.valid ? tr("labels.partPose") : tr("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* circleSelector = new QComboBox(panel);
  for (int i = 0; i < circleConfigs.size(); ++i)
  {
    circleSelector->addItem(circleConfigs[i].id, i);
  }
  circleSelector->setCurrentIndex(qBound(0, m_activeCircleIndexes.value(camera.id, 0), circleConfigs.size() - 1));
  auto* newCircleButton = createTouchIconButton("new", tr("actions.newGeometryCircle"), panel);
  auto* deleteCircleButton = createTouchIconButton("delete", tr("actions.deleteGeometryCircle"), panel);

  auto* top = new QWidget(panel);
  auto* topLayout = new QGridLayout(top);
  topLayout->setContentsMargins(0, 0, 0, 0);
  topLayout->setHorizontalSpacing(6);
  topLayout->addWidget(new QLabel(tr("actions.circleGeometry"), top), 0, 0);
  topLayout->addWidget(circleSelector, 0, 1);
  topLayout->addWidget(newCircleButton, 0, 2);
  topLayout->addWidget(deleteCircleButton, 0, 3);
  topLayout->setColumnStretch(1, 1);
  layout->addWidget(top);

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(6);
  auto* innerBand = new QSpinBox(form);
  innerBand->setRange(1, 500);
  innerBand->setSuffix(" px");
  innerBand->setValue(circleConfig.innerBand);
  auto* outerBand = new QSpinBox(form);
  outerBand->setRange(1, 500);
  outerBand->setSuffix(" px");
  outerBand->setValue(circleConfig.outerBand);
  auto* sensitivity = new QSpinBox(form);
  sensitivity->setRange(1, 255);
  sensitivity->setValue(circleConfig.edgeSensitivity);
  auto* cleanup = new QSpinBox(form);
  cleanup->setRange(0, 100);
  cleanup->setSuffix(" px");
  cleanup->setValue(circleConfig.edgeCleanupDerivative);
  auto* statFilter = new QSpinBox(form);
  statFilter->setRange(0, 100);
  statFilter->setSuffix(" px");
  statFilter->setValue(circleConfig.edgeStatisticalFilter);
  auto* subpixel = new QCheckBox(tr("labels.subpixelEdge"), form);
  subpixel->setChecked(circleConfig.useSubpixel);
  auto* scanDirection = new QComboBox(form);
  scanDirection->addItem(tr("labels.scanNormalPositive"), "normal_positive");
  scanDirection->addItem(tr("labels.scanNormalNegative"), "normal_negative");
  scanDirection->setCurrentIndex(circleConfig.scanDirection == EdgeLineScanDirection::NormalNegative ? 1 : 0);
  auto* transition = new QComboBox(form);
  transition->addItem(tr("labels.transitionLightToDark"), "light_to_dark");
  transition->addItem(tr("labels.transitionDarkToLight"), "dark_to_light");
  transition->setCurrentIndex(circleConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);
  auto* pickMode = new QComboBox(form);
  pickMode->addItem(tr("labels.edgePickFirst"), "first");
  pickMode->addItem(tr("labels.edgePickLast"), "last");
  pickMode->addItem(tr("labels.edgePickBest"), "best");
  pickMode->setCurrentIndex(static_cast<int>(circleConfig.pickMode));

  int row = 0;
  formLayout->addWidget(new QLabel(tr("labels.edgeBandInner"), form), row, 0);
  formLayout->addWidget(innerBand, row, 1);
  formLayout->addWidget(new QLabel(tr("labels.edgeBandOuter"), form), row, 2);
  formLayout->addWidget(outerBand, row++, 3);
  formLayout->addWidget(new QLabel(tr("labels.edgeSensitivity"), form), row, 0);
  formLayout->addWidget(sensitivity, row, 1);
  formLayout->addWidget(new QLabel(tr("labels.edgeCleanupDerivative"), form), row, 2);
  formLayout->addWidget(cleanup, row++, 3);
  formLayout->addWidget(new QLabel(tr("labels.edgeStatisticalFilter"), form), row, 0);
  formLayout->addWidget(statFilter, row++, 1);
  if (MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    formLayout->addWidget(subpixel, row++, 0, 1, 4);
  }
  formLayout->addWidget(new QLabel(tr("labels.scanDirection"), form), row, 0);
  formLayout->addWidget(scanDirection, row, 1);
  formLayout->addWidget(new QLabel(tr("labels.edgeTransition"), form), row, 2);
  formLayout->addWidget(transition, row++, 3);
  formLayout->addWidget(new QLabel(tr("labels.edgePickMode"), form), row, 0);
  formLayout->addWidget(pickMode, row++, 1, 1, 3);
  layout->addWidget(form);

  QObject::connect(circleSelector, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    if (index < 0)
    {
      return;
    }

    m_activeCircleIndexes[camera.id] = index;
    showGeometryCirclePanel(camera);
  });
  QObject::connect(newCircleButton, &QPushButton::clicked, window(), [this, camera]() {
    addGeometryCircle(camera);
    saveGeometryCirclesRecipe(camera);
    showGeometryCirclePanel(camera);
    activateGeometryCircleDrawing(camera);
  });
  QObject::connect(deleteCircleButton, &QPushButton::clicked, window(), [this, camera]() { removeActiveGeometryCircle(camera); });
  QObject::connect(innerBand, qOverload<int>(&QSpinBox::valueChanged), window(), [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).innerBand = value;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
    testGeometryCircle(camera);
  });
  QObject::connect(outerBand, qOverload<int>(&QSpinBox::valueChanged), window(), [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).outerBand = value;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
    testGeometryCircle(camera);
  });
  QObject::connect(sensitivity, qOverload<int>(&QSpinBox::valueChanged), window(), [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).edgeSensitivity = value;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  QObject::connect(cleanup, qOverload<int>(&QSpinBox::valueChanged), window(), [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).edgeCleanupDerivative = value;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  QObject::connect(statFilter, qOverload<int>(&QSpinBox::valueChanged), window(), [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).edgeStatisticalFilter = value;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  QObject::connect(subpixel, &QCheckBox::toggled, window(), [this, camera](bool checked) {
    activeGeometryCircleConfig(camera.id).useSubpixel = checked;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  QObject::connect(scanDirection, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryCircleConfig(camera.id).scanDirection =
      index == 1 ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  QObject::connect(transition, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryCircleConfig(camera.id).transition = index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  QObject::connect(pickMode, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryCircleConfig(camera.id).pickMode = index == 1 ? EdgeLinePickMode::Last : (index == 2 ? EdgeLinePickMode::Best : EdgeLinePickMode::First);
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });

  auto* testButton = createTouchIconButton("start", tr("actions.testGeometry"), panel);
  auto* backButton = createTouchIconButton("back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);
  QObject::connect(testButton, &QPushButton::clicked, window(), [this, camera]() { testGeometryCircle(camera); });
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (!GeometryPanelNavigation::returnToSetup(context(), camera))
    {
      showGeometryPanel(camera);
    }
  });
  layout->addWidget(testButton);
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  showConfiguredGeometryCircles(camera);
}
