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

