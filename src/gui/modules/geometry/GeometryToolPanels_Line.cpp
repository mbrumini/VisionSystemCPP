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
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <opencv2/imgproc.hpp>

void MainWindowGeometryModule::showGeometryLinePanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Line;
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
    const QString display = lineConfigs[i].alias.trimmed().isEmpty()
      ? lineConfigs[i].id
      : QString("%1 (%2)").arg(lineConfigs[i].alias.trimmed(), lineConfigs[i].id);
    lineSelector->addItem(display, i);
  }
  lineSelector->setCurrentIndex(qBound(0, m_activeLineIndexes.value(camera.id, 0), lineConfigs.size() - 1));
  auto* newLineButton = createTouchIconButton("new", tr("actions.newGeometryLine"), panel);
  auto* deleteLineButton = createTouchIconButton("delete", tr("actions.deleteGeometryLine"), panel);

  auto* bandHalfWidth = new QSlider(Qt::Horizontal, panel);
  bandHalfWidth->setRange(2, 500);
  bandHalfWidth->setValue(lineConfig.bandHalfWidth);
  auto* bandHalfWidthValue = new QLabel(QString("%1 px").arg(lineConfig.bandHalfWidth), panel);
  bandHalfWidthValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  bandHalfWidthValue->setMinimumWidth(52);
  auto* edgeSensitivity = new QSlider(Qt::Horizontal, panel);
  edgeSensitivity->setRange(1, 255);
  edgeSensitivity->setValue(lineConfig.edgeSensitivity);
  auto* edgeSensitivityValue = new QLabel(QString::number(lineConfig.edgeSensitivity), panel);
  edgeSensitivityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  edgeSensitivityValue->setMinimumWidth(36);
  auto* edgeCleanupDerivative = new QSlider(Qt::Horizontal, panel);
  edgeCleanupDerivative->setRange(0, 100);
  edgeCleanupDerivative->setValue(lineConfig.edgeCleanupDerivative);
  auto* edgeCleanupDerivativeValue = new QLabel(QString("%1 px").arg(lineConfig.edgeCleanupDerivative), panel);
  edgeCleanupDerivativeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  edgeCleanupDerivativeValue->setMinimumWidth(52);
  auto* edgeStatisticalFilter = new QSlider(Qt::Horizontal, panel);
  edgeStatisticalFilter->setRange(0, 100);
  edgeStatisticalFilter->setValue(lineConfig.edgeStatisticalFilter);
  auto* edgeStatisticalFilterValue = new QLabel(QString("%1 px").arg(lineConfig.edgeStatisticalFilter), panel);
  edgeStatisticalFilterValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  edgeStatisticalFilterValue->setMinimumWidth(52);
  auto* subpixelEdge = new QCheckBox(tr("labels.subpixelEdge"), panel);
  subpixelEdge->setChecked(lineConfig.useSubpixel);
  auto* scanDirection = new QComboBox(panel);
  scanDirection->setObjectName("geometryChoice");
  scanDirection->setMinimumHeight(38);
  scanDirection->addItem(tr("labels.scanNormalPositive"), "normal_positive");
  scanDirection->addItem(tr("labels.scanNormalNegative"), "normal_negative");
  scanDirection->setCurrentIndex(lineConfig.scanDirection == EdgeLineScanDirection::NormalNegative ? 1 : 0);
  auto* edgeTransition = new QComboBox(panel);
  edgeTransition->setObjectName("geometryChoice");
  edgeTransition->setMinimumHeight(38);
  edgeTransition->addItem(tr("labels.transitionLightToDark"), "light_to_dark");
  edgeTransition->addItem(tr("labels.transitionDarkToLight"), "dark_to_light");
  edgeTransition->setCurrentIndex(lineConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);
  auto* edgePickMode = new QComboBox(panel);
  edgePickMode->setObjectName("geometryChoice");
  edgePickMode->setMinimumHeight(38);
  edgePickMode->addItem(tr("labels.edgePickFirst"), "first");
  edgePickMode->addItem(tr("labels.edgePickLast"), "last");
  edgePickMode->addItem(tr("labels.edgePickBest"), "best");
  edgePickMode->setCurrentIndex(static_cast<int>(lineConfig.pickMode));
  auto* aliasEdit = new QLineEdit(lineConfig.alias, panel);
  aliasEdit->setPlaceholderText(tr("labels.operatorAlias"));

  auto* lineControls = new QWidget(panel);
  auto* lineControlsLayout = new QGridLayout(lineControls);
  lineControlsLayout->setContentsMargins(0, 0, 0, 0);
  lineControlsLayout->setHorizontalSpacing(6);
  lineControlsLayout->setVerticalSpacing(6);
  lineControlsLayout->addWidget(new QLabel(tr("labels.geometryLine"), lineControls), 0, 0);
  lineControlsLayout->addWidget(lineSelector, 0, 1);
  lineControlsLayout->addWidget(newLineButton, 0, 2);
  lineControlsLayout->addWidget(deleteLineButton, 0, 3);
  lineControlsLayout->addWidget(new QLabel(tr("labels.geometryLineBand"), lineControls), 1, 0);
  lineControlsLayout->addWidget(bandHalfWidthValue, 1, 1);
  lineControlsLayout->addWidget(new QLabel(tr("labels.edgeSensitivity"), lineControls), 1, 2);
  lineControlsLayout->addWidget(edgeSensitivityValue, 1, 3);
  lineControlsLayout->addWidget(bandHalfWidth, 2, 0, 1, 2);
  lineControlsLayout->addWidget(edgeSensitivity, 2, 2, 1, 2);
  lineControlsLayout->setColumnStretch(1, 1);
  layout->addWidget(lineControls);

  auto* filterControls = new QWidget(panel);
  auto* filterControlsLayout = new QGridLayout(filterControls);
  filterControlsLayout->setContentsMargins(0, 0, 0, 0);
  filterControlsLayout->setHorizontalSpacing(6);
  filterControlsLayout->setVerticalSpacing(10);
  filterControlsLayout->addWidget(new QLabel(tr("labels.edgeCleanupDerivative"), filterControls), 0, 0, 1, 3);
  filterControlsLayout->addWidget(edgeCleanupDerivativeValue, 0, 3);
  filterControlsLayout->addWidget(edgeCleanupDerivative, 1, 0, 1, 4);
  filterControlsLayout->addWidget(new QLabel(tr("labels.edgeStatisticalFilter"), filterControls), 2, 0, 1, 3);
  filterControlsLayout->addWidget(edgeStatisticalFilterValue, 2, 3);
  filterControlsLayout->addWidget(edgeStatisticalFilter, 3, 0, 1, 4);
  filterControlsLayout->setColumnStretch(3, 1);
  auto* scanControls = new QWidget(panel);
  auto* scanControlsLayout = new QGridLayout(scanControls);
  scanControlsLayout->setContentsMargins(0, 0, 0, 0);
  scanControlsLayout->setHorizontalSpacing(6);
  scanControlsLayout->setVerticalSpacing(6);
  int scanRow = 0;
  scanControlsLayout->addWidget(new QLabel(tr("labels.scanDirection"), scanControls), scanRow++, 0, 1, 3);
  scanControlsLayout->addWidget(scanDirection, scanRow++, 0, 1, 3);
  scanControlsLayout->addWidget(new QLabel(tr("labels.edgeTransition"), scanControls), scanRow++, 0, 1, 3);
  scanControlsLayout->addWidget(edgeTransition, scanRow++, 0, 1, 3);
  scanControlsLayout->addWidget(new QLabel(tr("labels.edgePickMode"), scanControls), scanRow++, 0, 1, 3);
  scanControlsLayout->addWidget(edgePickMode, scanRow++, 0, 1, 3);
  layout->addWidget(scanControls);

  auto* advancedButton = new QToolButton(panel);
  advancedButton->setText(tr("groups.strategyControls"));
  advancedButton->setCheckable(true);
  advancedButton->setChecked(false);
  advancedButton->setArrowType(Qt::RightArrow);
  advancedButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  auto* advancedPanel = new QWidget(panel);
  auto* advancedLayout = new QVBoxLayout(advancedPanel);
  advancedLayout->setContentsMargins(0, 0, 0, 0);
  advancedLayout->setSpacing(8);
  advancedLayout->addWidget(filterControls);
  if (MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    advancedLayout->addWidget(subpixelEdge);
  }
  auto* aliasLabel = new QLabel(tr("labels.operatorAlias"), advancedPanel);
  aliasEdit->setMinimumHeight(34);
  advancedLayout->addWidget(aliasLabel);
  advancedLayout->addWidget(aliasEdit);
  advancedPanel->setVisible(false);

  QObject::connect(advancedButton, &QToolButton::toggled, window(), [advancedButton, advancedPanel](bool checked) {
    advancedButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    advancedPanel->setVisible(checked);
  });
  layout->addWidget(advancedButton);
  layout->addWidget(advancedPanel);

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
  QObject::connect(bandHalfWidth, &QSlider::valueChanged, window(), [this, camera, bandHalfWidthValue](int value) {
    bandHalfWidthValue->setText(QString("%1 px").arg(value));
    activeGeometryLineConfig(camera.id).bandHalfWidth = value;
    m_lineMouseControllers[camera.id].setBandHalfWidth(value);
    updateGeometryLineOverlay(camera);
    saveGeometryLinesRecipe(camera);
    updateGeometryLineOverlay(camera);
  });
  QObject::connect(edgeSensitivity, &QSlider::valueChanged, window(), [this, camera, edgeSensitivityValue](int value) {
    edgeSensitivityValue->setText(QString::number(value));
    activeGeometryLineConfig(camera.id).edgeSensitivity = value;
    saveGeometryLinesRecipe(camera);
    updateGeometryLineOverlay(camera);
  });
  QObject::connect(edgeCleanupDerivative, &QSlider::valueChanged, window(), [this, camera, edgeCleanupDerivativeValue](int value) {
    edgeCleanupDerivativeValue->setText(QString("%1 px").arg(value));
    activeGeometryLineConfig(camera.id).edgeCleanupDerivative = value;
    saveGeometryLinesRecipe(camera);
    updateGeometryLineOverlay(camera);
  });
  QObject::connect(edgeStatisticalFilter, &QSlider::valueChanged, window(), [this, camera, edgeStatisticalFilterValue](int value) {
    edgeStatisticalFilterValue->setText(QString("%1 px").arg(value));
    activeGeometryLineConfig(camera.id).edgeStatisticalFilter = value;
    saveGeometryLinesRecipe(camera);
    updateGeometryLineOverlay(camera);
  });
  QObject::connect(subpixelEdge, &QCheckBox::toggled, window(), [this, camera](bool checked) {
    activeGeometryLineConfig(camera.id).useSubpixel = checked;
    saveGeometryLinesRecipe(camera);
    updateGeometryLineOverlay(camera);
  });
  QObject::connect(scanDirection, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryLineConfig(camera.id).scanDirection =
      index == 1 ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
    saveGeometryLinesRecipe(camera);
    updateGeometryLineOverlay(camera);
  });
  QObject::connect(edgeTransition, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryLineConfig(camera.id).transition =
      index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryLinesRecipe(camera);
    updateGeometryLineOverlay(camera);
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
    updateGeometryLineOverlay(camera);
  });
  QObject::connect(aliasEdit, &QLineEdit::editingFinished, window(), [this, camera, aliasEdit]() {
    activeGeometryLineConfig(camera.id).alias = aliasEdit->text().trimmed();
    saveGeometryLinesRecipe(camera);
    testConfiguredGeometryLines(camera);
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
    updateGeometryLineOverlay(camera);
  }
  else
  {
    updateGeometryLineOverlay(camera);
  }
}

