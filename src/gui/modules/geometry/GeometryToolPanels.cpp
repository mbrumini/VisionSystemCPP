#include "gui/modules/QtCompat.h"

#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/geometry/GeometryPanelNavigation.h"
#include "gui/TouchIconButton.h"

#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "processing/geometry/EdgePointDetector.h"
#include "util/AsyncExecutor.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

using AsyncExecutor::runAsyncTask;

void MainWindowGeometryModule::showGeometryPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.geometries"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* buttonGrid = new QWidget(panel);
  auto* buttonLayout = new QGridLayout(buttonGrid);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(6);

  auto* pointButton = createTouchIconButton("pointGeometry", tr("actions.pointGeometry"), panel);
  QObject::connect(pointButton, &QPushButton::clicked, window(), [this, camera]() { showGeometryPointPanel(camera); });
  buttonLayout->addWidget(pointButton, 0, 0);

  auto* lineButton = createTouchIconButton("lineGeometry", tr("actions.lineGeometry"), panel);
  QObject::connect(lineButton, &QPushButton::clicked, window(), [this, camera]() { showGeometryLinePanel(camera); });
  buttonLayout->addWidget(lineButton, 0, 1);

  auto* circleButton = createTouchIconButton("circleGeometry", tr("actions.circleGeometry"), panel);
  QObject::connect(circleButton, &QPushButton::clicked, window(), [this, camera]() { showGeometryCirclePanel(camera); });
  buttonLayout->addWidget(circleButton, 0, 2);

  auto* arcButton = createTouchIconButton("arcGeometry", tr("actions.arcGeometry"), panel);
  QObject::connect(arcButton, &QPushButton::clicked, window(), [this, camera]() { showGeometryArcPanel(camera); });
  buttonLayout->addWidget(arcButton, 0, 3);

  auto* backButton = createTouchIconButton("back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (!GeometryPanelNavigation::returnToSetup(context(), camera))
    {
      context().showCameraToolList(camera);
    }
  });
  buttonLayout->addWidget(backButton, 1, 0);
  layout->addWidget(buttonGrid);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}

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

void MainWindowGeometryModule::showGeometryLinePanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Line;
  if (context().setup) { context().setup->refreshPoseForCurrentFrame(camera); }
  restoreCleanGeometryImage(camera);
  largeImage()->clearCircles();
  loadGeometryLinesRecipe(camera);

  QVector<GeometryLineRuntimeConfig>& lineConfigs = m_lineConfigs[camera.id];
  GeometryLineRuntimeConfig& lineConfig = activeGeometryLineConfig(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("actions.lineGeometry"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* poseLabel = new QLabel(pose.valid ? tr("labels.partPose") : tr("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* lineSelector = new QComboBox(panel);
  for (int i = 0; i < lineConfigs.size(); ++i)
  {
    lineSelector->addItem(lineConfigs[i].id, i);
  }
  lineSelector->setCurrentIndex(qBound(0, m_activeLineIndexes.value(camera.id, 0), lineConfigs.size() - 1));
  auto* newLineButton = createTouchIconButton("new", tr("actions.newGeometryLine"), panel);
  auto* deleteLineButton = createTouchIconButton("delete", tr("actions.deleteGeometryLine"), panel);

  auto* bandHalfWidth = new QSpinBox(panel);
  bandHalfWidth->setRange(2, 500);
  bandHalfWidth->setValue(lineConfig.bandHalfWidth);
  bandHalfWidth->setSuffix(" px");
  bandHalfWidth->setFixedWidth(82);
  auto* edgeSensitivity = new QSpinBox(panel);
  edgeSensitivity->setRange(1, 255);
  edgeSensitivity->setValue(lineConfig.edgeSensitivity);
  edgeSensitivity->setFixedWidth(68);
  auto* edgeCleanupDerivative = new QSpinBox(panel);
  edgeCleanupDerivative->setRange(0, 100);
  edgeCleanupDerivative->setValue(lineConfig.edgeCleanupDerivative);
  edgeCleanupDerivative->setSuffix(" px");
  edgeCleanupDerivative->setFixedWidth(82);
  auto* edgeStatisticalFilter = new QSpinBox(panel);
  edgeStatisticalFilter->setRange(0, 100);
  edgeStatisticalFilter->setValue(lineConfig.edgeStatisticalFilter);
  edgeStatisticalFilter->setSuffix(" px");
  edgeStatisticalFilter->setFixedWidth(82);
  auto* subpixelEdge = new QCheckBox(tr("labels.subpixelEdge"), panel);
  subpixelEdge->setChecked(lineConfig.useSubpixel);
  auto* scanDirection = new QComboBox(panel);
  scanDirection->addItem(tr("labels.scanNormalPositive"), "normal_positive");
  scanDirection->addItem(tr("labels.scanNormalNegative"), "normal_negative");
  scanDirection->setCurrentIndex(lineConfig.scanDirection == EdgeLineScanDirection::NormalNegative ? 1 : 0);
  auto* edgeTransition = new QComboBox(panel);
  edgeTransition->addItem(tr("labels.transitionLightToDark"), "light_to_dark");
  edgeTransition->addItem(tr("labels.transitionDarkToLight"), "dark_to_light");
  edgeTransition->setCurrentIndex(lineConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);
  auto* edgePickMode = new QComboBox(panel);
  edgePickMode->addItem(tr("labels.edgePickFirst"), "first");
  edgePickMode->addItem(tr("labels.edgePickLast"), "last");
  edgePickMode->addItem(tr("labels.edgePickBest"), "best");
  edgePickMode->setCurrentIndex(static_cast<int>(lineConfig.pickMode));

  auto* lineControls = new QWidget(panel);
  auto* lineControlsLayout = new QGridLayout(lineControls);
  lineControlsLayout->setContentsMargins(0, 0, 0, 0);
  lineControlsLayout->setHorizontalSpacing(6);
  lineControlsLayout->setVerticalSpacing(4);
  lineControlsLayout->addWidget(new QLabel(tr("labels.geometryLine"), lineControls), 0, 0);
  lineControlsLayout->addWidget(lineSelector, 0, 1);
  lineControlsLayout->addWidget(newLineButton, 0, 2);
  lineControlsLayout->addWidget(deleteLineButton, 0, 3);
  lineControlsLayout->addWidget(new QLabel(tr("labels.geometryLineBand"), lineControls), 1, 0);
  lineControlsLayout->addWidget(bandHalfWidth, 1, 1);
  lineControlsLayout->addWidget(new QLabel(tr("labels.edgeSensitivity"), lineControls), 1, 2);
  lineControlsLayout->addWidget(edgeSensitivity, 1, 3);
  lineControlsLayout->setColumnStretch(1, 1);
  layout->addWidget(lineControls);

  auto* filterControls = new QWidget(panel);
  auto* filterControlsLayout = new QGridLayout(filterControls);
  filterControlsLayout->setContentsMargins(0, 0, 0, 0);
  filterControlsLayout->setHorizontalSpacing(6);
  filterControlsLayout->setVerticalSpacing(4);
  filterControlsLayout->addWidget(new QLabel(tr("labels.edgeCleanupDerivative"), filterControls), 0, 0);
  filterControlsLayout->addWidget(edgeCleanupDerivative, 0, 1);
  filterControlsLayout->addWidget(new QLabel(tr("labels.edgeStatisticalFilter"), filterControls), 0, 2);
  filterControlsLayout->addWidget(edgeStatisticalFilter, 0, 3);
  filterControlsLayout->setColumnStretch(3, 1);
  layout->addWidget(filterControls);

  if (MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    layout->addWidget(subpixelEdge);
  }

  auto* scanControls = new QWidget(panel);
  auto* scanControlsLayout = new QGridLayout(scanControls);
  scanControlsLayout->setContentsMargins(0, 0, 0, 0);
  scanControlsLayout->setHorizontalSpacing(6);
  scanControlsLayout->setVerticalSpacing(4);
  scanControlsLayout->addWidget(new QLabel(tr("labels.scanDirection"), scanControls), 0, 0);
  scanControlsLayout->addWidget(scanDirection, 0, 1);
  scanControlsLayout->addWidget(new QLabel(tr("labels.edgeTransition"), scanControls), 0, 2);
  scanControlsLayout->addWidget(edgeTransition, 0, 3);
  scanControlsLayout->addWidget(new QLabel(tr("labels.edgePickMode"), scanControls), 1, 0);
  scanControlsLayout->addWidget(edgePickMode, 1, 1, 1, 3);
  scanControlsLayout->setColumnStretch(1, 1);
  scanControlsLayout->setColumnStretch(3, 1);
  layout->addWidget(scanControls);

  QObject::connect(lineSelector, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    if (index < 0)
    {
      return;
    }

    m_activeLineIndexes[camera.id] = index;
    showGeometryLinePanel(camera);
  });
  QObject::connect(newLineButton, &QPushButton::clicked, window(), [this, camera]() {
    addGeometryLine(camera);
    saveGeometryLinesRecipe(camera);
    showGeometryLinePanel(camera);
    activateGeometryLineDrawing(camera);
  });
  QObject::connect(deleteLineButton, &QPushButton::clicked, window(), [this, camera]() {
    removeActiveGeometryLine(camera);
  });
  QObject::connect(bandHalfWidth, qOverload<int>(&QSpinBox::valueChanged), window(), [this, camera](int value) {
    activeGeometryLineConfig(camera.id).bandHalfWidth = value;
    m_lineMouseControllers[camera.id].setBandHalfWidth(value);
    updateGeometryLineOverlay(camera);
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  QObject::connect(edgeSensitivity, qOverload<int>(&QSpinBox::valueChanged), window(), [this, camera](int value) {
    activeGeometryLineConfig(camera.id).edgeSensitivity = value;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  QObject::connect(edgeCleanupDerivative, qOverload<int>(&QSpinBox::valueChanged), window(), [this, camera](int value) {
    activeGeometryLineConfig(camera.id).edgeCleanupDerivative = value;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  QObject::connect(edgeStatisticalFilter, qOverload<int>(&QSpinBox::valueChanged), window(), [this, camera](int value) {
    activeGeometryLineConfig(camera.id).edgeStatisticalFilter = value;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  QObject::connect(subpixelEdge, &QCheckBox::toggled, window(), [this, camera](bool checked) {
    activeGeometryLineConfig(camera.id).useSubpixel = checked;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  QObject::connect(scanDirection, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryLineConfig(camera.id).scanDirection =
      index == 1 ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  QObject::connect(edgeTransition, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryLineConfig(camera.id).transition =
      index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  QObject::connect(edgePickMode, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    if (index == 1)
    {
      activeGeometryLineConfig(camera.id).pickMode = EdgeLinePickMode::Last;
    }
    else if (index == 2)
    {
      activeGeometryLineConfig(camera.id).pickMode = EdgeLinePickMode::Best;
    }
    else
    {
      activeGeometryLineConfig(camera.id).pickMode = EdgeLinePickMode::First;
    }
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });

  auto* testButton = createTouchIconButton("start", tr("actions.testGeometry"), panel);
  auto* backButton = createTouchIconButton("back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);

  QObject::connect(testButton, &QPushButton::clicked, window(), [this, camera]() { testGeometryLine(camera); });
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

  bool shouldRefreshLine = false;
  if (lineConfig.hasLine && pose.valid)
  {
    const cv::Point2d imageStart = partToImage(pose, lineConfig.partStart);
    const cv::Point2d imageEnd = partToImage(pose, lineConfig.partEnd);
    LineGeometryMouseController& controller = m_lineMouseControllers[camera.id];
    controller.setLine(QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y), lineConfig.bandHalfWidth);
    *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
    m_drawingTarget = DrawingTarget::Line;
    updateGeometryLineOverlay(camera);
    largeImage()->setGeometryOverlayPointEditingEnabled(true);
    shouldRefreshLine = true;
  }
  else if (lineConfig.hasImageLine)
  {
    LineGeometryMouseController& controller = m_lineMouseControllers[camera.id];
    controller.setLine(
      QPointF(lineConfig.imageStart.x, lineConfig.imageStart.y),
      QPointF(lineConfig.imageEnd.x, lineConfig.imageEnd.y),
      lineConfig.bandHalfWidth);
    *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
    m_drawingTarget = DrawingTarget::Line;
    largeImage()->setGeometryOverlayPointEditingEnabled(true);
    shouldRefreshLine = true;
  }

  toolsLayout()->addWidget(panel);
  if (shouldRefreshLine)
  {
    testGeometryLine(camera);
  }
  else
  {
    updateGeometryLineOverlay(camera);
  }
}

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
