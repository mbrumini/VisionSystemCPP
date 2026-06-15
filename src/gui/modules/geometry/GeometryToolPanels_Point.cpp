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

void MainWindowGeometryModule::showGeometryPointPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Point;
  if (context().setup) { context().setup->refreshPoseForCurrentFrame(camera); }
  restoreCleanGeometryImage(camera);
  largeImage()->clearCircles();
  loadGeometryPointRecipe(camera);

  QVector<GeometryPointRuntimeConfig>& pointConfigs = m_pointConfigs[camera.id];
  GeometryPointRuntimeConfig& pointConfig = activeGeometryPointConfig(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("actions.pointGeometry"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* poseLabel = new QLabel(pose.valid ? tr("labels.partPose") : tr("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* pointSelector = new QComboBox(panel);
  for (int i = 0; i < pointConfigs.size(); ++i)
  {
    pointSelector->addItem(pointConfigs[i].id, i);
  }
  pointSelector->setCurrentIndex(qBound(0, m_activePointIndexes.value(camera.id, 0), pointConfigs.size() - 1));
  auto* newPointButton = createTouchIconButton("new", tr("actions.newGeometryPoint"), panel);
  auto* deletePointButton = createTouchIconButton("delete", tr("actions.deleteGeometryPoint"), panel);

  auto* pointControls = new QWidget(panel);
  auto* pointControlsLayout = new QGridLayout(pointControls);
  pointControlsLayout->setContentsMargins(0, 0, 0, 0);
  pointControlsLayout->setHorizontalSpacing(6);
  pointControlsLayout->setVerticalSpacing(4);
  pointControlsLayout->addWidget(new QLabel(tr("actions.pointGeometry"), pointControls), 0, 0);
  pointControlsLayout->addWidget(pointSelector, 0, 1);
  pointControlsLayout->addWidget(newPointButton, 0, 2);
  pointControlsLayout->addWidget(deletePointButton, 0, 3);
  pointControlsLayout->setColumnStretch(1, 1);
  layout->addWidget(pointControls);

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(6);

  auto* edgeSensitivity = new QSpinBox(form);
  edgeSensitivity->setRange(1, 255);
  edgeSensitivity->setValue(pointConfig.edgeSensitivity);

  auto* subpixelEdge = new QCheckBox(tr("labels.subpixelEdge"), form);
  subpixelEdge->setChecked(pointConfig.useSubpixel);

  auto* edgeTransition = new QComboBox(form);
  edgeTransition->addItem(tr("labels.transitionLightToDark"), "light_to_dark");
  edgeTransition->addItem(tr("labels.transitionDarkToLight"), "dark_to_light");
  edgeTransition->setCurrentIndex(pointConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);

  auto* edgePickMode = new QComboBox(form);
  edgePickMode->addItem(tr("labels.edgePickFirst"), "first");
  edgePickMode->addItem(tr("labels.edgePickLast"), "last");
  edgePickMode->addItem(tr("labels.edgePickBest"), "best");
  edgePickMode->setCurrentIndex(static_cast<int>(pointConfig.pickMode));

  int row = 0;
  formLayout->addWidget(new QLabel(tr("labels.edgeSensitivity"), form), row, 0);
  formLayout->addWidget(edgeSensitivity, row++, 1);
  if (MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    formLayout->addWidget(subpixelEdge, row++, 0, 1, 2);
  }
  formLayout->addWidget(new QLabel(tr("labels.edgeTransition"), form), row, 0);
  formLayout->addWidget(edgeTransition, row++, 1);
  formLayout->addWidget(new QLabel(tr("labels.edgePickMode"), form), row, 0);
  formLayout->addWidget(edgePickMode, row++, 1);
  layout->addWidget(form);

  QObject::connect(pointSelector, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    if (index < 0)
    {
      return;
    }

    m_activePointIndexes[camera.id] = index;
    showGeometryPointPanel(camera);
  });
  QObject::connect(newPointButton, &QPushButton::clicked, window(), [this, camera]() {
    addGeometryPoint(camera);
    saveGeometryPointRecipe(camera);
    showGeometryPointPanel(camera);
    activateGeometryPointDrawing(camera);
  });
  QObject::connect(deletePointButton, &QPushButton::clicked, window(), [this, camera]() {
    removeActiveGeometryPoint(camera);
  });

  QObject::connect(edgeSensitivity, qOverload<int>(&QSpinBox::valueChanged), window(), [this, camera](int value) {
    activeGeometryPointConfig(camera.id).edgeSensitivity = value;
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });
  QObject::connect(subpixelEdge, &QCheckBox::toggled, window(), [this, camera](bool checked) {
    activeGeometryPointConfig(camera.id).useSubpixel = checked;
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });
  QObject::connect(edgeTransition, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryPointConfig(camera.id).transition =
      index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });
  QObject::connect(edgePickMode, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    if (index == 1)
    {
      activeGeometryPointConfig(camera.id).pickMode = EdgeLinePickMode::Last;
    }
    else if (index == 2)
    {
      activeGeometryPointConfig(camera.id).pickMode = EdgeLinePickMode::Best;
    }
    else
    {
      activeGeometryPointConfig(camera.id).pickMode = EdgeLinePickMode::First;
    }
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });

  auto* testButton = createTouchIconButton("start", tr("actions.testGeometry"), panel);
  auto* backButton = createTouchIconButton("back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);

  QObject::connect(testButton, &QPushButton::clicked, window(), [this, camera]() { testGeometryPoint(camera); });
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (!GeometryPanelNavigation::returnToSetup(context(), camera))
    {
      showGeometryPanel(camera);
    }
  });

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setHorizontalSpacing(8);
  buttonsLayout->setVerticalSpacing(8);
  buttonsLayout->addWidget(testButton, 0, 0);
  buttonsLayout->addWidget(backButton, 0, 1);
  layout->addWidget(buttons);
  layout->addStretch(1);

  if (pointConfig.hasGuide && pose.valid)
  {
    const cv::Point2d imageStart = partToImage(pose, pointConfig.partStart);
    const cv::Point2d imageEnd = partToImage(pose, pointConfig.partEnd);
    m_pointMouseControllers[camera.id].setLine(
      QPointF(imageStart.x, imageStart.y),
      QPointF(imageEnd.x, imageEnd.y),
      3.0);
    *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
    m_drawingTarget = DrawingTarget::Point;
    largeImage()->setGeometryOverlayPointEditingEnabled(true);
    testGeometryPoint(camera);
  }
  else if (pointConfig.hasImageGuide)
  {
    m_pointMouseControllers[camera.id].setLine(
      QPointF(pointConfig.imageStart.x, pointConfig.imageStart.y),
      QPointF(pointConfig.imageEnd.x, pointConfig.imageEnd.y),
      3.0);
    *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
    m_drawingTarget = DrawingTarget::Point;
    largeImage()->setGeometryOverlayPointEditingEnabled(true);
    testGeometryPoint(camera);
  }
  else
  {
    updateGeometryPointOverlay(camera);
  }

  toolsLayout()->addWidget(panel);
}

