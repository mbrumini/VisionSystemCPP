#include "MainWindow.h"

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

void MainWindow::showGeometryPanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(trText("tools.geometries"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* pointButton = new QPushButton(trText("actions.pointGeometry"), panel);
  connect(pointButton, &QPushButton::clicked, this, [this, camera]() { showGeometryPointPanel(camera); });
  layout->addWidget(pointButton);

  auto* lineButton = new QPushButton(trText("actions.lineGeometry"), panel);
  connect(lineButton, &QPushButton::clicked, this, [this, camera]() { showGeometryLinePanel(camera); });
  layout->addWidget(lineButton);

  auto* circleButton = new QPushButton(trText("actions.circleGeometry"), panel);
  connect(circleButton, &QPushButton::clicked, this, [this, camera]() { showGeometryCirclePanel(camera); });
  layout->addWidget(circleButton);

  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);
  connect(backButton, &QPushButton::clicked, this, [this, camera]() { showCameraToolList(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  m_toolsLayout->addWidget(panel);
}

void MainWindow::showGeometryPointPanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Point;
  refreshPoseForCurrentFrame(camera);
  restoreCleanGeometryImage(camera);
  m_largeImage->clearCircles();
  loadGeometryPointRecipe(camera);

  QVector<GeometryPointRuntimeConfig>& pointConfigs = m_geometryPointConfigs[camera.id];
  GeometryPointRuntimeConfig& pointConfig = activeGeometryPointConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(trText("actions.pointGeometry"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* poseLabel = new QLabel(pose.valid ? trText("labels.partPose") : trText("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* pointSelector = new QComboBox(panel);
  for (int i = 0; i < pointConfigs.size(); ++i)
  {
    pointSelector->addItem(pointConfigs[i].id, i);
  }
  pointSelector->setCurrentIndex(qBound(0, m_activeGeometryPointIndexes.value(camera.id, 0), pointConfigs.size() - 1));
  auto* newPointButton = new QPushButton(trText("actions.newGeometryPoint"), panel);
  auto* deletePointButton = new QPushButton(trText("actions.deleteGeometryPoint"), panel);

  auto* pointControls = new QWidget(panel);
  auto* pointControlsLayout = new QGridLayout(pointControls);
  pointControlsLayout->setContentsMargins(0, 0, 0, 0);
  pointControlsLayout->setHorizontalSpacing(6);
  pointControlsLayout->setVerticalSpacing(4);
  pointControlsLayout->addWidget(new QLabel(trText("actions.pointGeometry"), pointControls), 0, 0);
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

  auto* subpixelEdge = new QCheckBox(trText("labels.subpixelEdge"), form);
  subpixelEdge->setChecked(pointConfig.useSubpixel);

  auto* edgeTransition = new QComboBox(form);
  edgeTransition->addItem(trText("labels.transitionLightToDark"), "light_to_dark");
  edgeTransition->addItem(trText("labels.transitionDarkToLight"), "dark_to_light");
  edgeTransition->setCurrentIndex(pointConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);

  auto* edgePickMode = new QComboBox(form);
  edgePickMode->addItem(trText("labels.edgePickFirst"), "first");
  edgePickMode->addItem(trText("labels.edgePickLast"), "last");
  edgePickMode->addItem(trText("labels.edgePickBest"), "best");
  edgePickMode->setCurrentIndex(static_cast<int>(pointConfig.pickMode));

  int row = 0;
  formLayout->addWidget(new QLabel(trText("labels.edgeSensitivity"), form), row, 0);
  formLayout->addWidget(edgeSensitivity, row++, 1);
  if (isBwDimensionalCamera(camera))
  {
    formLayout->addWidget(subpixelEdge, row++, 0, 1, 2);
  }
  formLayout->addWidget(new QLabel(trText("labels.edgeTransition"), form), row, 0);
  formLayout->addWidget(edgeTransition, row++, 1);
  formLayout->addWidget(new QLabel(trText("labels.edgePickMode"), form), row, 0);
  formLayout->addWidget(edgePickMode, row++, 1);
  layout->addWidget(form);

  connect(pointSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    if (index < 0)
    {
      return;
    }

    m_activeGeometryPointIndexes[camera.id] = index;
    showGeometryPointPanel(camera);
  });
  connect(newPointButton, &QPushButton::clicked, this, [this, camera]() {
    addGeometryPoint(camera);
    saveGeometryPointRecipe(camera);
    showGeometryPointPanel(camera);
    activateGeometryPointDrawing(camera);
  });
  connect(deletePointButton, &QPushButton::clicked, this, [this, camera]() {
    removeActiveGeometryPoint(camera);
  });

  connect(edgeSensitivity, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryPointConfig(camera.id).edgeSensitivity = value;
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });
  connect(subpixelEdge, &QCheckBox::toggled, this, [this, camera](bool checked) {
    activeGeometryPointConfig(camera.id).useSubpixel = checked;
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });
  connect(edgeTransition, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    activeGeometryPointConfig(camera.id).transition =
      index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });
  connect(edgePickMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
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

  auto* testButton = new QPushButton(trText("actions.testGeometry"), panel);
  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);

  connect(testButton, &QPushButton::clicked, this, [this, camera]() { testGeometryPoint(camera); });
  connect(backButton, &QPushButton::clicked, this, [this, camera]() { showGeometryPanel(camera); });

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setHorizontalSpacing(8);
  buttonsLayout->setVerticalSpacing(8);
  buttonsLayout->addWidget(testButton, 0, 0, 1, 2);
  buttonsLayout->addWidget(backButton, 1, 0, 1, 2);
  layout->addWidget(buttons);
  layout->addStretch(1);

  if (pointConfig.hasGuide && pose.valid)
  {
    const cv::Point2d imageStart = partToImage(pose, pointConfig.partStart);
    const cv::Point2d imageEnd = partToImage(pose, pointConfig.partEnd);
    m_pointGeometryMouseControllers[camera.id].setLine(
      QPointF(imageStart.x, imageStart.y),
      QPointF(imageEnd.x, imageEnd.y),
      3.0);
    m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
    m_geometryDrawingTarget = GeometryDrawingTarget::Point;
    m_largeImage->setGeometryOverlayPointEditingEnabled(true);
    testGeometryPoint(camera);
  }
  else if (pointConfig.hasImageGuide)
  {
    m_pointGeometryMouseControllers[camera.id].setLine(
      QPointF(pointConfig.imageStart.x, pointConfig.imageStart.y),
      QPointF(pointConfig.imageEnd.x, pointConfig.imageEnd.y),
      3.0);
    m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
    m_geometryDrawingTarget = GeometryDrawingTarget::Point;
    m_largeImage->setGeometryOverlayPointEditingEnabled(true);
    testGeometryPoint(camera);
  }
  else
  {
    updateGeometryPointOverlay(camera);
  }

  m_toolsLayout->addWidget(panel);
}

void MainWindow::showGeometryLinePanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Line;
  refreshPoseForCurrentFrame(camera);
  restoreCleanGeometryImage(camera);
  m_largeImage->clearCircles();
  loadGeometryLinesRecipe(camera);

  QVector<GeometryLineRuntimeConfig>& lineConfigs = m_geometryLineConfigs[camera.id];
  GeometryLineRuntimeConfig& lineConfig = activeGeometryLineConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(trText("actions.lineGeometry"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* poseLabel = new QLabel(pose.valid ? trText("labels.partPose") : trText("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* lineSelector = new QComboBox(panel);
  for (int i = 0; i < lineConfigs.size(); ++i)
  {
    lineSelector->addItem(lineConfigs[i].id, i);
  }
  lineSelector->setCurrentIndex(qBound(0, m_activeGeometryLineIndexes.value(camera.id, 0), lineConfigs.size() - 1));
  auto* newLineButton = new QPushButton(trText("actions.newGeometryLine"), panel);
  auto* deleteLineButton = new QPushButton(trText("actions.deleteGeometryLine"), panel);

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
  auto* subpixelEdge = new QCheckBox(trText("labels.subpixelEdge"), panel);
  subpixelEdge->setChecked(lineConfig.useSubpixel);
  auto* scanDirection = new QComboBox(panel);
  scanDirection->addItem(trText("labels.scanNormalPositive"), "normal_positive");
  scanDirection->addItem(trText("labels.scanNormalNegative"), "normal_negative");
  scanDirection->setCurrentIndex(lineConfig.scanDirection == EdgeLineScanDirection::NormalNegative ? 1 : 0);
  auto* edgeTransition = new QComboBox(panel);
  edgeTransition->addItem(trText("labels.transitionLightToDark"), "light_to_dark");
  edgeTransition->addItem(trText("labels.transitionDarkToLight"), "dark_to_light");
  edgeTransition->setCurrentIndex(lineConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);
  auto* edgePickMode = new QComboBox(panel);
  edgePickMode->addItem(trText("labels.edgePickFirst"), "first");
  edgePickMode->addItem(trText("labels.edgePickLast"), "last");
  edgePickMode->addItem(trText("labels.edgePickBest"), "best");
  edgePickMode->setCurrentIndex(static_cast<int>(lineConfig.pickMode));

  auto* lineControls = new QWidget(panel);
  auto* lineControlsLayout = new QGridLayout(lineControls);
  lineControlsLayout->setContentsMargins(0, 0, 0, 0);
  lineControlsLayout->setHorizontalSpacing(6);
  lineControlsLayout->setVerticalSpacing(4);
  lineControlsLayout->addWidget(new QLabel(trText("labels.geometryLine"), lineControls), 0, 0);
  lineControlsLayout->addWidget(lineSelector, 0, 1);
  lineControlsLayout->addWidget(newLineButton, 0, 2);
  lineControlsLayout->addWidget(deleteLineButton, 0, 3);
  lineControlsLayout->addWidget(new QLabel(trText("labels.geometryLineBand"), lineControls), 1, 0);
  lineControlsLayout->addWidget(bandHalfWidth, 1, 1);
  lineControlsLayout->addWidget(new QLabel(trText("labels.edgeSensitivity"), lineControls), 1, 2);
  lineControlsLayout->addWidget(edgeSensitivity, 1, 3);
  lineControlsLayout->setColumnStretch(1, 1);
  layout->addWidget(lineControls);

  auto* filterControls = new QWidget(panel);
  auto* filterControlsLayout = new QGridLayout(filterControls);
  filterControlsLayout->setContentsMargins(0, 0, 0, 0);
  filterControlsLayout->setHorizontalSpacing(6);
  filterControlsLayout->setVerticalSpacing(4);
  filterControlsLayout->addWidget(new QLabel(trText("labels.edgeCleanupDerivative"), filterControls), 0, 0);
  filterControlsLayout->addWidget(edgeCleanupDerivative, 0, 1);
  filterControlsLayout->addWidget(new QLabel(trText("labels.edgeStatisticalFilter"), filterControls), 0, 2);
  filterControlsLayout->addWidget(edgeStatisticalFilter, 0, 3);
  filterControlsLayout->setColumnStretch(3, 1);
  layout->addWidget(filterControls);

  if (isBwDimensionalCamera(camera))
  {
    layout->addWidget(subpixelEdge);
  }

  auto* scanControls = new QWidget(panel);
  auto* scanControlsLayout = new QGridLayout(scanControls);
  scanControlsLayout->setContentsMargins(0, 0, 0, 0);
  scanControlsLayout->setHorizontalSpacing(6);
  scanControlsLayout->setVerticalSpacing(4);
  scanControlsLayout->addWidget(new QLabel(trText("labels.scanDirection"), scanControls), 0, 0);
  scanControlsLayout->addWidget(scanDirection, 0, 1);
  scanControlsLayout->addWidget(new QLabel(trText("labels.edgeTransition"), scanControls), 0, 2);
  scanControlsLayout->addWidget(edgeTransition, 0, 3);
  scanControlsLayout->addWidget(new QLabel(trText("labels.edgePickMode"), scanControls), 1, 0);
  scanControlsLayout->addWidget(edgePickMode, 1, 1, 1, 3);
  scanControlsLayout->setColumnStretch(1, 1);
  scanControlsLayout->setColumnStretch(3, 1);
  layout->addWidget(scanControls);

  connect(lineSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    if (index < 0)
    {
      return;
    }

    m_activeGeometryLineIndexes[camera.id] = index;
    showGeometryLinePanel(camera);
  });
  connect(newLineButton, &QPushButton::clicked, this, [this, camera]() {
    addGeometryLine(camera);
    saveGeometryLinesRecipe(camera);
    showGeometryLinePanel(camera);
    activateGeometryLineDrawing(camera);
  });
  connect(deleteLineButton, &QPushButton::clicked, this, [this, camera]() {
    removeActiveGeometryLine(camera);
  });
  connect(bandHalfWidth, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryLineConfig(camera.id).bandHalfWidth = value;
    m_lineGeometryMouseControllers[camera.id].setBandHalfWidth(value);
    updateGeometryLineOverlay(camera);
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(edgeSensitivity, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryLineConfig(camera.id).edgeSensitivity = value;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(edgeCleanupDerivative, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryLineConfig(camera.id).edgeCleanupDerivative = value;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(edgeStatisticalFilter, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryLineConfig(camera.id).edgeStatisticalFilter = value;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(subpixelEdge, &QCheckBox::toggled, this, [this, camera](bool checked) {
    activeGeometryLineConfig(camera.id).useSubpixel = checked;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(scanDirection, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    activeGeometryLineConfig(camera.id).scanDirection =
      index == 1 ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(edgeTransition, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    activeGeometryLineConfig(camera.id).transition =
      index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(edgePickMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
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

  auto* testButton = new QPushButton(trText("actions.testGeometry"), panel);
  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);

  connect(testButton, &QPushButton::clicked, this, [this, camera]() { testGeometryLine(camera); });
  connect(backButton, &QPushButton::clicked, this, [this, camera]() { showGeometryPanel(camera); });

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setHorizontalSpacing(8);
  buttonsLayout->setVerticalSpacing(8);
  buttonsLayout->addWidget(testButton, 0, 0, 1, 2);
  buttonsLayout->addWidget(backButton, 1, 0, 1, 2);
  layout->addWidget(buttons);
  layout->addStretch(1);

  bool shouldRefreshLine = false;
  if (lineConfig.hasLine && pose.valid)
  {
    const cv::Point2d imageStart = partToImage(pose, lineConfig.partStart);
    const cv::Point2d imageEnd = partToImage(pose, lineConfig.partEnd);
    LineGeometryMouseController& controller = m_lineGeometryMouseControllers[camera.id];
    controller.setLine(QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y), lineConfig.bandHalfWidth);
    m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
    m_geometryDrawingTarget = GeometryDrawingTarget::Line;
    updateGeometryLineOverlay(camera);
    m_largeImage->setGeometryOverlayPointEditingEnabled(true);
    shouldRefreshLine = true;
  }
  else if (lineConfig.hasImageLine)
  {
    LineGeometryMouseController& controller = m_lineGeometryMouseControllers[camera.id];
    controller.setLine(
      QPointF(lineConfig.imageStart.x, lineConfig.imageStart.y),
      QPointF(lineConfig.imageEnd.x, lineConfig.imageEnd.y),
      lineConfig.bandHalfWidth);
    m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
    m_geometryDrawingTarget = GeometryDrawingTarget::Line;
    m_largeImage->setGeometryOverlayPointEditingEnabled(true);
    shouldRefreshLine = true;
  }

  m_toolsLayout->addWidget(panel);
  if (shouldRefreshLine)
  {
    testGeometryLine(camera);
  }
  else
  {
    updateGeometryLineOverlay(camera);
  }
}

void MainWindow::showGeometryCirclePanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Circle;
  refreshPoseForCurrentFrame(camera);
  restoreCleanGeometryImage(camera);
  m_largeImage->clearCircles();
  loadGeometryCirclesRecipe(camera);

  QVector<GeometryCircleRuntimeConfig>& circleConfigs = m_geometryCircleConfigs[camera.id];
  GeometryCircleRuntimeConfig& circleConfig = activeGeometryCircleConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(trText("actions.circleGeometry"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* poseLabel = new QLabel(pose.valid ? trText("labels.partPose") : trText("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* circleSelector = new QComboBox(panel);
  for (int i = 0; i < circleConfigs.size(); ++i)
  {
    circleSelector->addItem(circleConfigs[i].id, i);
  }
  circleSelector->setCurrentIndex(qBound(0, m_activeGeometryCircleIndexes.value(camera.id, 0), circleConfigs.size() - 1));
  auto* newCircleButton = new QPushButton(trText("actions.newGeometryCircle"), panel);
  auto* deleteCircleButton = new QPushButton(trText("actions.deleteGeometryCircle"), panel);

  auto* top = new QWidget(panel);
  auto* topLayout = new QGridLayout(top);
  topLayout->setContentsMargins(0, 0, 0, 0);
  topLayout->setHorizontalSpacing(6);
  topLayout->addWidget(new QLabel(trText("actions.circleGeometry"), top), 0, 0);
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
  auto* subpixel = new QCheckBox(trText("labels.subpixelEdge"), form);
  subpixel->setChecked(circleConfig.useSubpixel);
  auto* transition = new QComboBox(form);
  transition->addItem(trText("labels.transitionLightToDark"), "light_to_dark");
  transition->addItem(trText("labels.transitionDarkToLight"), "dark_to_light");
  transition->setCurrentIndex(circleConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);
  auto* pickMode = new QComboBox(form);
  pickMode->addItem(trText("labels.edgePickFirst"), "first");
  pickMode->addItem(trText("labels.edgePickLast"), "last");
  pickMode->addItem(trText("labels.edgePickBest"), "best");
  pickMode->setCurrentIndex(static_cast<int>(circleConfig.pickMode));

  int row = 0;
  formLayout->addWidget(new QLabel(trText("labels.edgeBandInner"), form), row, 0);
  formLayout->addWidget(innerBand, row, 1);
  formLayout->addWidget(new QLabel(trText("labels.edgeBandOuter"), form), row, 2);
  formLayout->addWidget(outerBand, row++, 3);
  formLayout->addWidget(new QLabel(trText("labels.edgeSensitivity"), form), row, 0);
  formLayout->addWidget(sensitivity, row, 1);
  formLayout->addWidget(new QLabel(trText("labels.edgeCleanupDerivative"), form), row, 2);
  formLayout->addWidget(cleanup, row++, 3);
  formLayout->addWidget(new QLabel(trText("labels.edgeStatisticalFilter"), form), row, 0);
  formLayout->addWidget(statFilter, row++, 1);
  if (isBwDimensionalCamera(camera))
  {
    formLayout->addWidget(subpixel, row++, 0, 1, 4);
  }
  formLayout->addWidget(new QLabel(trText("labels.edgeTransition"), form), row, 0);
  formLayout->addWidget(transition, row, 1);
  formLayout->addWidget(new QLabel(trText("labels.edgePickMode"), form), row, 2);
  formLayout->addWidget(pickMode, row++, 3);
  layout->addWidget(form);

  connect(circleSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    if (index < 0)
    {
      return;
    }

    m_activeGeometryCircleIndexes[camera.id] = index;
    showGeometryCirclePanel(camera);
  });
  connect(newCircleButton, &QPushButton::clicked, this, [this, camera]() {
    addGeometryCircle(camera);
    saveGeometryCirclesRecipe(camera);
    showGeometryCirclePanel(camera);
    activateGeometryCircleDrawing(camera);
  });
  connect(deleteCircleButton, &QPushButton::clicked, this, [this, camera]() { removeActiveGeometryCircle(camera); });
  connect(innerBand, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).innerBand = value;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  connect(outerBand, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).outerBand = value;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  connect(sensitivity, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).edgeSensitivity = value;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  connect(cleanup, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).edgeCleanupDerivative = value;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  connect(statFilter, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).edgeStatisticalFilter = value;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  connect(subpixel, &QCheckBox::toggled, this, [this, camera](bool checked) {
    activeGeometryCircleConfig(camera.id).useSubpixel = checked;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  connect(transition, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    activeGeometryCircleConfig(camera.id).transition = index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  connect(pickMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    activeGeometryCircleConfig(camera.id).pickMode = index == 1 ? EdgeLinePickMode::Last : (index == 2 ? EdgeLinePickMode::Best : EdgeLinePickMode::First);
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });

  auto* testButton = new QPushButton(trText("actions.testGeometry"), panel);
  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);
  connect(testButton, &QPushButton::clicked, this, [this, camera]() { testGeometryCircle(camera); });
  connect(backButton, &QPushButton::clicked, this, [this, camera]() { showGeometryPanel(camera); });
  layout->addWidget(testButton);
  layout->addWidget(backButton);
  layout->addStretch(1);
  m_toolsLayout->addWidget(panel);
  showConfiguredGeometryCircles(camera);
}
