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

#include <opencv2/imgproc.hpp>

void MainWindowGeometryModule::showGeometryCirclePanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Circle;
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
    const QString display = circleConfigs[i].alias.trimmed().isEmpty()
      ? circleConfigs[i].id
      : QString("%1 (%2)").arg(circleConfigs[i].alias.trimmed(), circleConfigs[i].id);
    circleSelector->addItem(display, i);
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
  formLayout->setContentsMargins(6, 6, 6, 6);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);
  auto* sensitivity = new QSlider(Qt::Horizontal, form);
  sensitivity->setRange(1, 255);
  sensitivity->setValue(circleConfig.edgeSensitivity);
  auto* sensitivityValue = new QLabel(QString::number(circleConfig.edgeSensitivity), form);
  sensitivityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  sensitivityValue->setMinimumWidth(36);
  auto* cleanup = new QSlider(Qt::Horizontal, form);
  cleanup->setRange(0, 100);
  cleanup->setValue(circleConfig.edgeCleanupDerivative);
  auto* cleanupValue = new QLabel(QString("%1 px").arg(circleConfig.edgeCleanupDerivative), form);
  cleanupValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  cleanupValue->setMinimumWidth(52);
  auto* statFilter = new QSlider(Qt::Horizontal, form);
  statFilter->setRange(0, 100);
  statFilter->setValue(circleConfig.edgeStatisticalFilter);
  auto* statFilterValue = new QLabel(QString("%1 px").arg(circleConfig.edgeStatisticalFilter), form);
  statFilterValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  statFilterValue->setMinimumWidth(52);
  auto* subpixel = new QCheckBox(tr("labels.subpixelEdge"), form);
  subpixel->setChecked(circleConfig.useSubpixel);
  auto* scanDirection = new QComboBox(form);
  scanDirection->setObjectName("geometryChoice");
  scanDirection->setMinimumHeight(38);
  scanDirection->addItem(tr("labels.scanNormalPositive"), "normal_positive");
  scanDirection->addItem(tr("labels.scanNormalNegative"), "normal_negative");
  scanDirection->setCurrentIndex(circleConfig.scanDirection == EdgeLineScanDirection::NormalNegative ? 1 : 0);
  auto* transition = new QComboBox(form);
  transition->setObjectName("geometryChoice");
  transition->setMinimumHeight(38);
  transition->addItem(tr("labels.transitionLightToDark"), "light_to_dark");
  transition->addItem(tr("labels.transitionDarkToLight"), "dark_to_light");
  transition->setCurrentIndex(circleConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);
  auto* pickMode = new QComboBox(form);
  pickMode->setObjectName("geometryChoice");
  pickMode->setMinimumHeight(38);
  pickMode->addItem(tr("labels.edgePickFirst"), "first");
  pickMode->addItem(tr("labels.edgePickLast"), "last");
  pickMode->addItem(tr("labels.edgePickBest"), "best");
  pickMode->setCurrentIndex(static_cast<int>(circleConfig.pickMode));
  auto* aliasEdit = new QLineEdit(circleConfig.alias, form);
  aliasEdit->setPlaceholderText(tr("labels.operatorAlias"));

  int row = 0;
  formLayout->addWidget(new QLabel(tr("labels.edgeSensitivity"), form), row, 0, 1, 3);
  formLayout->addWidget(sensitivityValue, row++, 3);
  formLayout->addWidget(sensitivity, row++, 0, 1, 4);
  formLayout->addWidget(new QLabel(tr("labels.scanDirection"), form), row++, 0, 1, 4);
  formLayout->addWidget(scanDirection, row++, 0, 1, 4);
  formLayout->addWidget(new QLabel(tr("labels.edgeTransition"), form), row++, 0, 1, 4);
  formLayout->addWidget(transition, row++, 0, 1, 4);
  formLayout->addWidget(new QLabel(tr("labels.edgePickMode"), form), row++, 0, 1, 4);
  formLayout->addWidget(pickMode, row++, 0, 1, 4);
  layout->addWidget(form);

  auto* advancedButton = new QToolButton(panel);
  advancedButton->setText(tr("groups.strategyControls"));
  advancedButton->setCheckable(true);
  advancedButton->setArrowType(Qt::RightArrow);
  advancedButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  auto* advancedPanel = new QWidget(panel);
  auto* advancedLayout = new QGridLayout(advancedPanel);
  advancedLayout->setContentsMargins(6, 6, 6, 6);
  advancedLayout->setVerticalSpacing(8);
  int advancedRow = 0;
  advancedLayout->addWidget(new QLabel(tr("labels.edgeCleanupDerivative"), advancedPanel), advancedRow, 0, 1, 3);
  advancedLayout->addWidget(cleanupValue, advancedRow++, 3);
  advancedLayout->addWidget(cleanup, advancedRow++, 0, 1, 4);
  advancedLayout->addWidget(new QLabel(tr("labels.edgeStatisticalFilter"), advancedPanel), advancedRow, 0, 1, 3);
  advancedLayout->addWidget(statFilterValue, advancedRow++, 3);
  advancedLayout->addWidget(statFilter, advancedRow++, 0, 1, 4);
  if (MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    advancedLayout->addWidget(subpixel, advancedRow++, 0, 1, 4);
  }
  advancedLayout->addWidget(new QLabel(tr("labels.operatorAlias"), advancedPanel), advancedRow++, 0, 1, 4);
  aliasEdit->setMinimumHeight(34);
  advancedLayout->addWidget(aliasEdit, advancedRow++, 0, 1, 4);
  advancedPanel->setVisible(false);
  QObject::connect(advancedButton, &QToolButton::toggled, window(), [advancedButton, advancedPanel](bool checked) {
    advancedButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    advancedPanel->setVisible(checked);
  });
  layout->addWidget(advancedButton);
  layout->addWidget(advancedPanel);

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
  QObject::connect(sensitivity, &QSlider::valueChanged, window(), [this, camera, sensitivityValue](int value) {
    sensitivityValue->setText(QString::number(value));
    activeGeometryCircleConfig(camera.id).edgeSensitivity = value;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  QObject::connect(cleanup, &QSlider::valueChanged, window(), [this, camera, cleanupValue](int value) {
    cleanupValue->setText(QString("%1 px").arg(value));
    activeGeometryCircleConfig(camera.id).edgeCleanupDerivative = value;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  QObject::connect(statFilter, &QSlider::valueChanged, window(), [this, camera, statFilterValue](int value) {
    statFilterValue->setText(QString("%1 px").arg(value));
    activeGeometryCircleConfig(camera.id).edgeStatisticalFilter = value;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  QObject::connect(subpixel, &QCheckBox::toggled, window(), [this, camera](bool checked) {
    activeGeometryCircleConfig(camera.id).useSubpixel = checked;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  QObject::connect(scanDirection, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryCircleConfig(camera.id).scanDirection =
      index == 1 ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  QObject::connect(transition, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryCircleConfig(camera.id).transition = index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  QObject::connect(pickMode, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryCircleConfig(camera.id).pickMode = index == 1 ? EdgeLinePickMode::Last : (index == 2 ? EdgeLinePickMode::Best : EdgeLinePickMode::First);
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  QObject::connect(aliasEdit, &QLineEdit::editingFinished, window(), [this, camera, aliasEdit]() {
    activeGeometryCircleConfig(camera.id).alias = aliasEdit->text().trimmed();
    saveGeometryCirclesRecipe(camera);
    syncRuntimeGeometryLabels(camera);
    showConfiguredGeometryCircles(camera);
    refreshMeasurementOverlay(camera);
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
