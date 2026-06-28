#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/geometry/GeometryPanelNavigation.h"
#include "gui/TouchIconButton.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>

void MainWindowGeometryModule::showGeometryArcPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Arc;
  largeImage()->setGeometryOverlayPointEditingEnabled(false);
  restoreCleanGeometryImage(camera);
  largeImage()->clearCircles();
  loadGeometryArcsRecipe(camera);

  QVector<GeometryArcRuntimeConfig>& arcConfigs = m_arcConfigs[camera.id];
  GeometryArcRuntimeConfig& arcConfig = activeGeometryArcConfig(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("actions.arcGeometry"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* poseLabel = new QLabel(pose.valid ? tr("labels.partPose") : tr("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* arcSelector = new QComboBox(panel);
  for (int i = 0; i < arcConfigs.size(); ++i)
  {
    const QString display = arcConfigs[i].alias.trimmed().isEmpty()
      ? arcConfigs[i].id
      : QString("%1 (%2)").arg(arcConfigs[i].alias.trimmed(), arcConfigs[i].id);
    arcSelector->addItem(display, i);
  }
  arcSelector->setCurrentIndex(qBound(0, m_activeArcIndexes.value(camera.id, 0), arcConfigs.size() - 1));
  auto* newArcButton = createTouchIconButton("arcGeometry", tr("actions.newGeometryArc"), panel);
  auto* editArcButton = createTouchIconButton("arcGeometry", tr("actions.editGeometry"), panel);
  auto* deleteArcButton = createTouchIconButton("delete", tr("actions.deleteGeometryArc"), panel);

  auto* top = new QWidget(panel);
  auto* topLayout = new QGridLayout(top);
  topLayout->setContentsMargins(0, 0, 0, 0);
  topLayout->setHorizontalSpacing(6);
  topLayout->addWidget(new QLabel(tr("actions.arcGeometry"), top), 0, 0);
  topLayout->addWidget(arcSelector, 0, 1);
  topLayout->addWidget(newArcButton, 0, 2);
  topLayout->addWidget(editArcButton, 0, 3);
  topLayout->addWidget(deleteArcButton, 0, 4);
  topLayout->setColumnStretch(1, 1);
  layout->addWidget(top);

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(6, 6, 6, 6);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);
  auto* innerBand = new QSlider(Qt::Horizontal, form);
  innerBand->setRange(1, 500);
  innerBand->setValue(arcConfig.innerBand);
  auto* innerBandValue = new QLabel(QString("%1 px").arg(arcConfig.innerBand), form);
  innerBandValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  innerBandValue->setMinimumWidth(52);
  auto* outerBand = new QSlider(Qt::Horizontal, form);
  outerBand->setRange(1, 500);
  outerBand->setValue(arcConfig.outerBand);
  auto* outerBandValue = new QLabel(QString("%1 px").arg(arcConfig.outerBand), form);
  outerBandValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  outerBandValue->setMinimumWidth(52);
  auto* sensitivity = new QSlider(Qt::Horizontal, form);
  sensitivity->setRange(1, 255);
  sensitivity->setValue(arcConfig.edgeSensitivity);
  auto* sensitivityValue = new QLabel(QString::number(arcConfig.edgeSensitivity), form);
  sensitivityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  sensitivityValue->setMinimumWidth(36);
  auto* cleanup = new QSlider(Qt::Horizontal, form);
  cleanup->setRange(0, 100);
  cleanup->setValue(arcConfig.edgeCleanupDerivative);
  auto* cleanupValue = new QLabel(QString("%1 px").arg(arcConfig.edgeCleanupDerivative), form);
  cleanupValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  cleanupValue->setMinimumWidth(52);
  auto* statFilter = new QSlider(Qt::Horizontal, form);
  statFilter->setRange(0, 100);
  statFilter->setValue(arcConfig.edgeStatisticalFilter);
  auto* statFilterValue = new QLabel(QString("%1 px").arg(arcConfig.edgeStatisticalFilter), form);
  statFilterValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  statFilterValue->setMinimumWidth(52);
  auto* subpixel = new QCheckBox(tr("labels.subpixelEdge"), form);
  subpixel->setChecked(arcConfig.useSubpixel);
  auto* scanDirection = new QComboBox(form);
  scanDirection->setObjectName("geometryChoice");
  scanDirection->setMinimumHeight(38);
  scanDirection->addItem(tr("labels.scanNormalPositive"), "normal_positive");
  scanDirection->addItem(tr("labels.scanNormalNegative"), "normal_negative");
  scanDirection->setCurrentIndex(arcConfig.scanDirection == EdgeLineScanDirection::NormalNegative ? 1 : 0);
  auto* transition = new QComboBox(form);
  transition->setObjectName("geometryChoice");
  transition->setMinimumHeight(38);
  transition->addItem(tr("labels.transitionLightToDark"), "light_to_dark");
  transition->addItem(tr("labels.transitionDarkToLight"), "dark_to_light");
  transition->setCurrentIndex(arcConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);
  auto* pickMode = new QComboBox(form);
  pickMode->setObjectName("geometryChoice");
  pickMode->setMinimumHeight(38);
  pickMode->addItem(tr("labels.edgePickFirst"), "first");
  pickMode->addItem(tr("labels.edgePickLast"), "last");
  pickMode->addItem(tr("labels.edgePickBest"), "best");
  pickMode->setCurrentIndex(static_cast<int>(arcConfig.pickMode));
  auto* aliasEdit = new QLineEdit(arcConfig.alias, form);
  aliasEdit->setPlaceholderText(tr("labels.operatorAlias"));

  int row = 0;
  formLayout->addWidget(new QLabel(tr("labels.edgeBandInner"), form), row, 0);
  formLayout->addWidget(innerBandValue, row, 1);
  formLayout->addWidget(new QLabel(tr("labels.edgeBandOuter"), form), row, 2);
  formLayout->addWidget(outerBandValue, row++, 3);
  formLayout->addWidget(innerBand, row, 0, 1, 2);
  formLayout->addWidget(outerBand, row++, 2, 1, 2);
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

  QObject::connect(arcSelector, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    if (index < 0)
    {
      return;
    }

    m_activeArcIndexes[camera.id] = index;
    showGeometryArcPanel(camera);
  });
  QObject::connect(newArcButton, &QPushButton::clicked, window(), [this, camera]() {
    addGeometryArc(camera);
    showGeometryArcPanel(camera);
    activateGeometryArcDrawing(camera);
  });
  QObject::connect(editArcButton, &QPushButton::clicked, window(), [this, camera]() {
    largeImage()->setGeometryOverlayPointEditingEnabled(true);
    testConfiguredGeometryLines(camera);
    showConfiguredGeometryArcs(camera);
  });
  QObject::connect(deleteArcButton, &QPushButton::clicked, window(), [this, camera]() { removeActiveGeometryArc(camera); });
  QObject::connect(innerBand, &QSlider::valueChanged, window(), [this, camera, innerBandValue](int value) {
    innerBandValue->setText(QString("%1 px").arg(value));
    activeGeometryArcConfig(camera.id).innerBand = value;
    testGeometryArc(camera);
  });
  QObject::connect(outerBand, &QSlider::valueChanged, window(), [this, camera, outerBandValue](int value) {
    outerBandValue->setText(QString("%1 px").arg(value));
    activeGeometryArcConfig(camera.id).outerBand = value;
    testGeometryArc(camera);
  });
  QObject::connect(sensitivity, &QSlider::valueChanged, window(), [this, camera, sensitivityValue](int value) {
    sensitivityValue->setText(QString::number(value));
    activeGeometryArcConfig(camera.id).edgeSensitivity = value;
    testGeometryArc(camera);
  });
  QObject::connect(cleanup, &QSlider::valueChanged, window(), [this, camera, cleanupValue](int value) {
    cleanupValue->setText(QString("%1 px").arg(value));
    activeGeometryArcConfig(camera.id).edgeCleanupDerivative = value;
    testGeometryArc(camera);
  });
  QObject::connect(statFilter, &QSlider::valueChanged, window(), [this, camera, statFilterValue](int value) {
    statFilterValue->setText(QString("%1 px").arg(value));
    activeGeometryArcConfig(camera.id).edgeStatisticalFilter = value;
    testGeometryArc(camera);
  });
  QObject::connect(subpixel, &QCheckBox::toggled, window(), [this, camera](bool checked) {
    activeGeometryArcConfig(camera.id).useSubpixel = checked;
    testGeometryArc(camera);
  });
  QObject::connect(scanDirection, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryArcConfig(camera.id).scanDirection =
      index == 1 ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
    testGeometryArc(camera);
  });
  QObject::connect(transition, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryArcConfig(camera.id).transition = index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    testGeometryArc(camera);
  });
  QObject::connect(pickMode, qOverload<int>(&QComboBox::currentIndexChanged), window(), [this, camera](int index) {
    activeGeometryArcConfig(camera.id).pickMode = index == 1 ? EdgeLinePickMode::Last : (index == 2 ? EdgeLinePickMode::Best : EdgeLinePickMode::First);
    testGeometryArc(camera);
  });
  QObject::connect(aliasEdit, &QLineEdit::editingFinished, window(), [this, camera, aliasEdit]() {
    activeGeometryArcConfig(camera.id).alias = aliasEdit->text().trimmed();
    syncRuntimeGeometryLabels(camera);
    showConfiguredGeometryArcs(camera);
    refreshMeasurementOverlay(camera);
  });

  auto* saveButton = createTouchIconButton("saveSample", tr("actions.saveGeometry"), panel);
  auto* testButton = createTouchIconButton("testGeometry", tr("actions.testGeometry"), panel);
  auto* backButton = createTouchIconButton(
    "back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera]() {
    saveGeometryArcsRecipe(camera);
    largeImage()->setGeometryOverlayPointEditingEnabled(true);
    showConfiguredGeometryArcs(camera);
    if (context().updateMeasurementResults)
    {
      context().updateMeasurementResults();
    }
  });
  QObject::connect(testButton, &QPushButton::clicked, window(), [this, camera]() { testGeometryArc(camera); });
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (!GeometryPanelNavigation::returnToSetup(context(), camera))
    {
      showGeometryPanel(camera);
    }
  });
  layout->addWidget(saveButton);
  layout->addWidget(testButton);
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  showConfiguredGeometryArcs(camera);
}
