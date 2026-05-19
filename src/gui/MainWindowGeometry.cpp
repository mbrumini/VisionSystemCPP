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

namespace
{
bool circleFromThreePoints(const QVector<QPoint>& points, ImageCircle& circle)
{
  if (points.size() < 3)
  {
    return false;
  }

  const double x1 = points[0].x();
  const double y1 = points[0].y();
  const double x2 = points[1].x();
  const double y2 = points[1].y();
  const double x3 = points[2].x();
  const double y3 = points[2].y();
  const double d = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));

  if (std::abs(d) < 0.001)
  {
    return false;
  }

  const double ux = ((x1 * x1 + y1 * y1) * (y2 - y3) +
                     (x2 * x2 + y2 * y2) * (y3 - y1) +
                     (x3 * x3 + y3 * y3) * (y1 - y2)) / d;
  const double uy = ((x1 * x1 + y1 * y1) * (x3 - x2) +
                     (x2 * x2 + y2 * y2) * (x1 - x3) +
                     (x3 * x3 + y3 * y3) * (x2 - x1)) / d;
  const QPoint center(qRound(ux), qRound(uy));
  const int radius = qRound(std::hypot(x1 - ux, y1 - uy));

  if (radius <= 2)
  {
    return false;
  }

  circle = {center, radius};
  return true;
}

QString scanDirectionToRecipe(EdgeLineScanDirection direction)
{
  return direction == EdgeLineScanDirection::NormalNegative ? "normal_negative" : "normal_positive";
}

EdgeLineScanDirection scanDirectionFromRecipe(const QString& value)
{
  return value == "normal_negative" ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
}

QString transitionToRecipe(EdgeLineTransition transition)
{
  return transition == EdgeLineTransition::DarkToLight ? "dark_to_light" : "light_to_dark";
}

EdgeLineTransition transitionFromRecipe(const QString& value)
{
  return value == "dark_to_light" ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
}

QString pickModeToRecipe(EdgeLinePickMode mode)
{
  if (mode == EdgeLinePickMode::Last)
  {
    return "last";
  }

  if (mode == EdgeLinePickMode::Best)
  {
    return "best";
  }

  return "first";
}

EdgeLinePickMode pickModeFromRecipe(const QString& value)
{
  if (value == "last")
  {
    return EdgeLinePickMode::Last;
  }

  if (value == "best")
  {
    return EdgeLinePickMode::Best;
  }

  return EdgeLinePickMode::First;
}
}
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

GeometryLineRuntimeConfig& MainWindow::activeGeometryLineConfig(const QString& cameraId)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[cameraId];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig first;
    first.id = "line_1";
    lines.append(first);
    m_activeGeometryLineIndexes[cameraId] = 0;
    return lines[0];
  }

  int index = qBound(0, m_activeGeometryLineIndexes.value(cameraId, 0), lines.size() - 1);
  m_activeGeometryLineIndexes[cameraId] = index;
  return lines[index];
}

const GeometryLineRuntimeConfig& MainWindow::activeGeometryLineConfig(const QString& cameraId) const
{
  const QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs.value(cameraId);
  static const GeometryLineRuntimeConfig fallback;
  if (lines.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeGeometryLineIndexes.value(cameraId, 0), lines.size() - 1);
  return lines[index];
}

void MainWindow::addGeometryLine(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig first;
    first.id = "line_1";
    lines.append(first);
    m_activeGeometryLineIndexes[camera.id] = 0;
    return;
  }

  GeometryLineRuntimeConfig& activeLine = activeGeometryLineConfig(camera.id);
  if (!activeLine.hasImageLine && !activeLine.hasLine)
  {
    return;
  }

  GeometryLineRuntimeConfig line = activeGeometryLineConfig(camera.id);
  line.id = QString("line_%1").arg(lines.size() + 1);
  line.hasImageLine = false;
  line.hasLine = false;
  lines.append(line);
  m_activeGeometryLineIndexes[camera.id] = lines.size() - 1;
}

void MainWindow::removeActiveGeometryLine(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
    m_activeGeometryLineIndexes[camera.id] = 0;
    showGeometryLinePanel(camera);
    return;
  }

  const int index = qBound(0, m_activeGeometryLineIndexes.value(camera.id, 0), lines.size() - 1);
  lines.removeAt(index);
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
  }

  m_activeGeometryLineIndexes[camera.id] = qBound(0, index, lines.size() - 1);
  m_lineGeometryMouseControllers[camera.id].begin(activeGeometryLineConfig(camera.id).bandHalfWidth);
  saveGeometryLinesRecipe(camera);
  updateGeometryLineOverlay(camera);
  showGeometryLinePanel(camera);
}

void MainWindow::loadGeometryPointRecipe(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  if (!points.isEmpty())
  {
    return;
  }

  const QVector<GeometryPointRecipeConfig> recipes = m_recipeManager.loadGeometryPoints(camera.id);
  for (const GeometryPointRecipeConfig& recipe : recipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryPointRuntimeConfig point;
    point.enabled = recipe.enabled;
    point.id = recipe.id;
    point.partStart = cv::Point2d(recipe.partStart.x(), recipe.partStart.y());
    point.partEnd = cv::Point2d(recipe.partEnd.x(), recipe.partEnd.y());
    point.edgeSensitivity = recipe.edgeSensitivity;
    point.useSubpixel = recipe.useSubpixel;
    point.transition = transitionFromRecipe(recipe.transition);
    point.pickMode = pickModeFromRecipe(recipe.pickMode);
    point.hasGuide = true;
    points.append(point);
  }

  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }
  m_activeGeometryPointIndexes[camera.id] = qBound(0, m_activeGeometryPointIndexes.value(camera.id, 0), points.size() - 1);
}

void MainWindow::loadGeometryCirclesRecipe(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[camera.id];
  if (!circles.isEmpty())
  {
    return;
  }

  const QVector<GeometryCircleRecipeConfig> recipes = m_recipeManager.loadGeometryCircles(camera.id);
  for (const GeometryCircleRecipeConfig& recipe : recipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryCircleRuntimeConfig circle;
    circle.enabled = recipe.enabled;
    circle.id = recipe.id;
    circle.partCenter = cv::Point2d(recipe.partCenter.x(), recipe.partCenter.y());
    circle.radius = recipe.radius;
    circle.innerBand = recipe.innerBand;
    circle.outerBand = recipe.outerBand;
    circle.edgeSensitivity = recipe.edgeSensitivity;
    circle.edgeCleanupDerivative = recipe.edgeCleanupDerivative;
    circle.edgeStatisticalFilter = recipe.edgeStatisticalFilter;
    circle.useSubpixel = recipe.useSubpixel;
    circle.transition = transitionFromRecipe(recipe.transition);
    circle.pickMode = pickModeFromRecipe(recipe.pickMode);
    circle.hasCircle = true;
    circles.append(circle);
  }

  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }
  m_activeGeometryCircleIndexes[camera.id] = qBound(0, m_activeGeometryCircleIndexes.value(camera.id, 0), circles.size() - 1);
}

void MainWindow::saveGeometryPointRecipe(const CameraConfig& camera)
{
  QVector<GeometryPointRecipeConfig> recipes;
  for (const GeometryPointRuntimeConfig& point : m_geometryPointConfigs[camera.id])
  {
    if (!point.hasGuide)
    {
      continue;
    }

    GeometryPointRecipeConfig recipe;
    recipe.enabled = point.enabled;
    recipe.id = point.id;
    recipe.partStart = QPointF(point.partStart.x, point.partStart.y);
    recipe.partEnd = QPointF(point.partEnd.x, point.partEnd.y);
    recipe.edgeSensitivity = point.edgeSensitivity;
    recipe.useSubpixel = point.useSubpixel;
    recipe.transition = transitionToRecipe(point.transition);
    recipe.pickMode = pickModeToRecipe(point.pickMode);
    recipes.append(recipe);
  }

  QString error;
  if (!m_recipeManager.saveGeometryPoints(camera.id, recipes, &error))
  {
    appendLog(error);
  }
}

void MainWindow::saveGeometryCirclesRecipe(const CameraConfig& camera)
{
  QVector<GeometryCircleRecipeConfig> recipes;
  for (const GeometryCircleRuntimeConfig& circle : m_geometryCircleConfigs[camera.id])
  {
    if (!circle.hasCircle)
    {
      continue;
    }

    GeometryCircleRecipeConfig recipe;
    recipe.enabled = circle.enabled;
    recipe.id = circle.id;
    recipe.partCenter = QPointF(circle.partCenter.x, circle.partCenter.y);
    recipe.radius = circle.radius;
    recipe.innerBand = circle.innerBand;
    recipe.outerBand = circle.outerBand;
    recipe.edgeSensitivity = circle.edgeSensitivity;
    recipe.edgeCleanupDerivative = circle.edgeCleanupDerivative;
    recipe.edgeStatisticalFilter = circle.edgeStatisticalFilter;
    recipe.useSubpixel = circle.useSubpixel;
    recipe.transition = transitionToRecipe(circle.transition);
    recipe.pickMode = pickModeToRecipe(circle.pickMode);
    recipes.append(recipe);
  }

  QString error;
  if (!m_recipeManager.saveGeometryCircles(camera.id, recipes, &error))
  {
    appendLog(error);
  }
}

void MainWindow::addGeometryPoint(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
    m_activeGeometryPointIndexes[camera.id] = 0;
    return;
  }

  GeometryPointRuntimeConfig& activePoint = activeGeometryPointConfig(camera.id);
  if (!activePoint.hasImageGuide && !activePoint.hasGuide)
  {
    return;
  }

  GeometryPointRuntimeConfig point = activeGeometryPointConfig(camera.id);
  point.id = QString("point_%1").arg(points.size() + 1);
  point.hasImageGuide = false;
  point.hasGuide = false;
  points.append(point);
  m_activeGeometryPointIndexes[camera.id] = points.size() - 1;
}

void MainWindow::addGeometryCircle(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[camera.id];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
    m_activeGeometryCircleIndexes[camera.id] = 0;
    return;
  }

  GeometryCircleRuntimeConfig& activeCircle = activeGeometryCircleConfig(camera.id);
  if (!activeCircle.hasImageCircle && !activeCircle.hasCircle)
  {
    return;
  }

  GeometryCircleRuntimeConfig circle = activeCircle;
  circle.id = QString("circle_%1").arg(circles.size() + 1);
  circle.hasImageCircle = false;
  circle.hasCircle = false;
  circles.append(circle);
  m_activeGeometryCircleIndexes[camera.id] = circles.size() - 1;
}


void MainWindow::removeActiveGeometryPoint(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
    m_activeGeometryPointIndexes[camera.id] = 0;
    showGeometryPointPanel(camera);
    return;
  }

  const int index = qBound(0, m_activeGeometryPointIndexes.value(camera.id, 0), points.size() - 1);
  points.removeAt(index);
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }

  m_activeGeometryPointIndexes[camera.id] = qBound(0, index, points.size() - 1);
  m_pointGeometryMouseControllers[camera.id].begin(3.0);
  saveGeometryPointRecipe(camera);
  updateGeometryPointOverlay(camera);
  showGeometryPointPanel(camera);
}

void MainWindow::removeActiveGeometryCircle(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[camera.id];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
    m_activeGeometryCircleIndexes[camera.id] = 0;
    showGeometryCirclePanel(camera);
    return;
  }

  const int index = qBound(0, m_activeGeometryCircleIndexes.value(camera.id, 0), circles.size() - 1);
  circles.removeAt(index);
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }

  m_activeGeometryCircleIndexes[camera.id] = qBound(0, index, circles.size() - 1);
  saveGeometryCirclesRecipe(camera);
  showGeometryCirclePanel(camera);
}

GeometryPointRuntimeConfig& MainWindow::activeGeometryPointConfig(const QString& cameraId)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[cameraId];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }

  int index = qBound(0, m_activeGeometryPointIndexes.value(cameraId, 0), points.size() - 1);
  m_activeGeometryPointIndexes[cameraId] = index;
  return points[index];
}

GeometryCircleRuntimeConfig& MainWindow::activeGeometryCircleConfig(const QString& cameraId)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[cameraId];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }

  int index = qBound(0, m_activeGeometryCircleIndexes.value(cameraId, 0), circles.size() - 1);
  m_activeGeometryCircleIndexes[cameraId] = index;
  return circles[index];
}

const GeometryPointRuntimeConfig& MainWindow::activeGeometryPointConfig(const QString& cameraId) const
{
  const QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs.value(cameraId);
  static const GeometryPointRuntimeConfig fallback;
  if (points.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeGeometryPointIndexes.value(cameraId, 0), points.size() - 1);
  return points[index];
}

const GeometryCircleRuntimeConfig& MainWindow::activeGeometryCircleConfig(const QString& cameraId) const
{
  const QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs.value(cameraId);
  static const GeometryCircleRuntimeConfig fallback;
  if (circles.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeGeometryCircleIndexes.value(cameraId, 0), circles.size() - 1);
  return circles[index];
}

void MainWindow::loadGeometryLinesRecipe(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  if (!lines.isEmpty())
  {
    return;
  }

  const QVector<GeometryLineRecipeConfig> recipes = m_recipeManager.loadGeometryLines(camera.id);
  for (const GeometryLineRecipeConfig& recipe : recipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryLineRuntimeConfig line;
    line.id = recipe.id;
    line.enabled = recipe.enabled;
    line.partStart = cv::Point2d(recipe.partStart.x(), recipe.partStart.y());
    line.partEnd = cv::Point2d(recipe.partEnd.x(), recipe.partEnd.y());
    line.bandHalfWidth = recipe.bandHalfWidth;
    line.edgeSensitivity = recipe.edgeSensitivity;
    line.edgeCleanupDerivative = recipe.edgeCleanupDerivative;
    line.edgeStatisticalFilter = recipe.edgeStatisticalFilter;
    line.useSubpixel = recipe.useSubpixel;
    line.scanDirection = scanDirectionFromRecipe(recipe.scanDirection);
    line.transition = transitionFromRecipe(recipe.transition);
    line.pickMode = pickModeFromRecipe(recipe.pickMode);
    line.hasLine = true;
    lines.append(line);
  }

  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
  }
  m_activeGeometryLineIndexes[camera.id] = qBound(0, m_activeGeometryLineIndexes.value(camera.id, 0), lines.size() - 1);
}

void MainWindow::saveGeometryLinesRecipe(const CameraConfig& camera)
{
  QVector<GeometryLineRecipeConfig> recipes;
  for (const GeometryLineRuntimeConfig& line : m_geometryLineConfigs[camera.id])
  {
    if (!line.hasLine)
    {
      continue;
    }

    GeometryLineRecipeConfig recipe;
    recipe.enabled = line.enabled;
    recipe.id = line.id;
    recipe.partStart = QPointF(line.partStart.x, line.partStart.y);
    recipe.partEnd = QPointF(line.partEnd.x, line.partEnd.y);
    recipe.bandHalfWidth = line.bandHalfWidth;
    recipe.edgeSensitivity = line.edgeSensitivity;
    recipe.edgeCleanupDerivative = line.edgeCleanupDerivative;
    recipe.edgeStatisticalFilter = line.edgeStatisticalFilter;
    recipe.useSubpixel = line.useSubpixel;
    recipe.scanDirection = scanDirectionToRecipe(line.scanDirection);
    recipe.transition = transitionToRecipe(line.transition);
    recipe.pickMode = pickModeToRecipe(line.pickMode);
    recipes.append(recipe);
  }

  QString error;
  if (!m_recipeManager.saveGeometryLines(camera.id, recipes, &error))
  {
    appendLog(error);
  }
}

void MainWindow::activateGeometryLineDrawing(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  deactivateImageDrawingTools();
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Line;
  m_lineGeometryMouseControllers[camera.id].begin(activeGeometryLineConfig(camera.id).bandHalfWidth);
  m_largeImage->setGeometryOverlayPointEditingEnabled(false);
  m_largeImage->setGeometryPointPickingEnabled(true);
  updateGeometryLineOverlay(camera);
  appendLog(trText("log.geometryLineDrawing") + ": " + camera.id);
}

void MainWindow::handleGeometryLinePoint(const CameraConfig& camera, const QPointF& imagePoint)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  LineGeometryMouseController& controller = m_lineGeometryMouseControllers[camera.id];
  controller.setBandHalfWidth(activeGeometryLineConfig(camera.id).bandHalfWidth);
  const bool completed = controller.handleClick(imagePoint);
  updateGeometryLineOverlay(camera);

  if (!completed)
  {
    return;
  }

  const LineGeometryEditorState& state = controller.state();
  GeometryLineRuntimeConfig& config = activeGeometryLineConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageLine = true;

  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasLine = true;
  }

  m_largeImage->setGeometryPointPickingEnabled(false);
  m_largeImage->setGeometryOverlayPointEditingEnabled(true);
  updateGeometryLineOverlay(camera);
  appendLog(trText("log.geometryLineGuideSaved") + ": " + camera.id);
  saveGeometryLinesRecipe(camera);
  testGeometryLine(camera);
}

void MainWindow::handleGeometryLineHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint)
{
  if (camera.id != m_selectedCameraId || pointIndex < 0 || pointIndex > 1)
  {
    return;
  }

  LineGeometryMouseController& controller = m_lineGeometryMouseControllers[camera.id];
  controller.movePoint(pointIndex, imagePoint);

  const LineGeometryEditorState& state = controller.state();
  if (!state.hasStart || !state.hasEnd)
  {
    updateGeometryLineOverlay(camera);
    return;
  }

  GeometryLineRuntimeConfig& config = activeGeometryLineConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageLine = true;

  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasLine = true;
  }
  else
  {
    config.hasLine = false;
  }

  updateGeometryLineOverlay(camera);
  saveGeometryLinesRecipe(camera);
  testGeometryLine(camera);
}

GeometryOverlay MainWindow::configuredGeometryLinesOverlay(const CameraConfig& camera, bool includeActive) const
{
  GeometryOverlay overlay;
  const PartPose& pose = m_cameraRuntime.at(camera.id).currentPose();
  const QVector<GeometryLineRuntimeConfig> lines = m_geometryLineConfigs.value(camera.id);
  const int activeIndex = m_activeGeometryLineIndexes.value(camera.id, 0);

  for (int i = 0; i < lines.size(); ++i)
  {
    if (!includeActive && i == activeIndex)
    {
      continue;
    }

    const GeometryLineRuntimeConfig& line = lines[i];
    const bool usePartLine = pose.valid && line.hasLine;
    if (!usePartLine && !line.hasImageLine)
    {
      continue;
    }

    const cv::Point2d imageStart = usePartLine ? partToImage(pose, line.partStart) : line.imageStart;
    const cv::Point2d imageEnd = usePartLine ? partToImage(pose, line.partEnd) : line.imageEnd;
    const QPointF start(imageStart.x, imageStart.y);
    const QPointF end(imageEnd.x, imageEnd.y);
    const QPointF center = (start + end) * 0.5;
    const QPointF delta = end - start;
    const double length = std::hypot(delta.x(), delta.y());
    const double angleDegrees = std::atan2(delta.y(), delta.x()) * 180.0 / CV_PI;
    const bool highlightActive =
      m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry &&
      m_geometryDrawingTarget == GeometryDrawingTarget::Line &&
      i == activeIndex;
    const QColor lineColor = highlightActive ? QColor("#ff4fd8") : QColor(170, 205, 220, 170);
    const QColor bandColor = highlightActive ? QColor(0, 210, 255, 35) : QColor(120, 170, 190, 22);
    overlay.bands.append({center, QSizeF(length, line.bandHalfWidth * 2.0), angleDegrees, lineColor, bandColor});
    overlay.lines.append({start, end, lineColor, highlightActive ? 3 : 1});
    if (includeActive && highlightActive)
    {
      overlay.points.append({start, "1"});
      overlay.points.append({end, "2"});
    }
  }

  return overlay;
}

void MainWindow::updateGeometryLineOverlay(const CameraConfig& camera, const GeometryOverlay& extraOverlay)
{
  GeometryOverlay overlay = configuredGeometryLinesOverlay(camera, false);
  GeometryOverlay activeOverlay = m_lineGeometryMouseControllers[camera.id].overlay();
  for (const GeometryOverlayPoint& point : activeOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : activeOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : activeOverlay.bands)
  {
    overlay.bands.append(band);
  }

  for (const GeometryOverlayPoint& point : extraOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : extraOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : extraOverlay.bands)
  {
    overlay.bands.append(band);
  }
  appendCurrentPartPoseOverlay(camera, overlay);
  m_largeImage->setGeometryOverlay(overlay);
}

void MainWindow::restoreCleanGeometryImage(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  const auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt != m_cameraRuntime.end() && !runtimeIt->second.currentFrame().empty())
  {
    m_selectedPreview = matToPixmap(runtimeIt->second.currentFrame());
  }
  else
  {
    m_selectedPreview = loadCameraPreview(camera);
  }

  if (m_selectedPreview.isNull())
  {
    m_largeImage->clearImage();
  }
  else
  {
    m_largeImage->setImage(m_selectedPreview);
  }
  m_largeImage->clearRoi();
  m_largeImage->clearExclusionRects();
  m_largeImage->clearCircles();
}

void MainWindow::testGeometryLine(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  GeometryLineRuntimeConfig& lineConfig = activeGeometryLineConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid && !lineConfig.hasLine && lineConfig.hasImageLine)
  {
    lineConfig.partStart = imageToPart(pose, lineConfig.imageStart);
    lineConfig.partEnd = imageToPart(pose, lineConfig.imageEnd);
    lineConfig.hasLine = true;
  }

  const bool usePartLine = pose.valid && lineConfig.hasLine;
  if (!usePartLine && !lineConfig.hasImageLine)
  {
    appendLog(trText("log.geometryLineRoiMissing") + ": " + camera.id);
    return;
  }

  const cv::Point2d imageStart = usePartLine ? partToImage(pose, lineConfig.partStart) : lineConfig.imageStart;
  const cv::Point2d imageEnd = usePartLine ? partToImage(pose, lineConfig.partEnd) : lineConfig.imageEnd;
  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  EdgeLineDetectorConfig config;
  config.id = lineConfig.id;
  config.label = lineConfig.id;
  config.guideStart = imageStart;
  config.guideEnd = imageEnd;
  config.bandHalfWidth = lineConfig.bandHalfWidth;
  config.edgeSensitivity = lineConfig.edgeSensitivity;
  config.edgeCleanupDerivative = lineConfig.edgeCleanupDerivative;
  config.edgeStatisticalFilter = lineConfig.edgeStatisticalFilter;
  config.useSubpixel = isBwDimensionalCamera(camera) && lineConfig.useSubpixel;
  config.scanDirection = lineConfig.scanDirection;
  config.transition = lineConfig.transition;
  config.pickMode = lineConfig.pickMode;

  // Run detection in background to avoid blocking UI using helper
  auto job = [input, config]() -> EdgeLineDetectorResult {
    EdgeLineDetector detector;
    return detector.detect(input, config);
  };

  const QString __pendingCameraId_testGeometryLine = camera.id;
  incPendingJobs(__pendingCameraId_testGeometryLine);
  runAsyncTask(decltype(job)(job), this, [this, camera, imageStart, imageEnd, __pendingCameraId_testGeometryLine](const EdgeLineDetectorResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testGeometryLine](void*) { decPendingJobs(__pendingCameraId_testGeometryLine); });
    if (!result.processed || result.diagnosticImage.empty())
    {
      appendLog(result.message.isEmpty() ? trText("log.geometryLineFailed") + ": " + camera.id : result.message);
      return;
    }

    m_selectedPreview = matToPixmap(result.diagnosticImage);
    m_largeImage->setImage(m_selectedPreview);
    m_largeImage->setRoi(QRect(result.searchRoi.x, result.searchRoi.y, result.searchRoi.width, result.searchRoi.height));

    LineGeometryMouseController& controller = m_lineGeometryMouseControllers[camera.id];
    controller.setLine(QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y), activeGeometryLineConfig(camera.id).bandHalfWidth);
    m_geometryDrawingTarget = GeometryDrawingTarget::Line;
    updateGeometryLineOverlay(camera);

    if (!result.found)
    {
      appendLog(result.message.isEmpty() ? trText("log.geometryLineNotFound") + ": " + camera.id : result.message);
      return;
    }

    GeometrySet& geometries = m_cameraRuntime[camera.id].geometries();
    for (int i = geometries.lines.size() - 1; i >= 0; --i)
    {
      if (geometries.lines[i].meta.id == result.line.meta.id)
      {
        geometries.lines.removeAt(i);
      }
    }
    geometries.lines.append(result.line);
    GeometryOverlay detectedOverlay;
    detectedOverlay.lines.append({
      QPointF(result.line.start.x, result.line.start.y),
      QPointF(result.line.end.x, result.line.end.y),
      QColor("#35c46a"),
      3
    });
    updateGeometryLineOverlay(camera, detectedOverlay);

    appendLog(QString("%1: %2 x1=%3 y1=%4 x2=%5 y2=%6")
                .arg(trText("log.geometryLineFound"))
                .arg(camera.id)
                .arg(result.line.start.x, 0, 'f', 1)
                .arg(result.line.start.y, 0, 'f', 1)
                .arg(result.line.end.x, 0, 'f', 1)
                .arg(result.line.end.y, 0, 'f', 1));
  });
}

void MainWindow::testConfiguredGeometryLines(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  loadGeometryLinesRecipe(camera);
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  cv::Mat diagnostic;
  if (input.channels() == 1)
  {
    cv::cvtColor(input, diagnostic, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(diagnostic);
  }

  GeometrySet& geometries = m_cameraRuntime[camera.id].geometries();
  geometries.lines.clear();
  GeometryOverlay detectedOverlay;

  for (int i = 0; i < lines.size(); ++i)
  {
    GeometryLineRuntimeConfig& line = lines[i];
    if (!line.enabled)
    {
      continue;
    }

    if (pose.valid && !line.hasLine && line.hasImageLine)
    {
      line.partStart = imageToPart(pose, line.imageStart);
      line.partEnd = imageToPart(pose, line.imageEnd);
      line.hasLine = true;
    }

    const bool usePartLine = pose.valid && line.hasLine;
    if (!usePartLine && !line.hasImageLine)
    {
      continue;
    }

    const cv::Point2d imageStart = usePartLine ? partToImage(pose, line.partStart) : line.imageStart;
    const cv::Point2d imageEnd = usePartLine ? partToImage(pose, line.partEnd) : line.imageEnd;

    EdgeLineDetectorConfig config;
    config.id = line.id;
    config.label = line.id;
    config.guideStart = imageStart;
    config.guideEnd = imageEnd;
    config.bandHalfWidth = line.bandHalfWidth;
    config.edgeSensitivity = line.edgeSensitivity;
    config.edgeCleanupDerivative = line.edgeCleanupDerivative;
    config.edgeStatisticalFilter = line.edgeStatisticalFilter;
    config.useSubpixel = isBwDimensionalCamera(camera) && line.useSubpixel;
    config.scanDirection = line.scanDirection;
    config.transition = line.transition;
    config.pickMode = line.pickMode;

    EdgeLineDetector detector;
    const EdgeLineDetectorResult result = detector.detect(input, config);
    if (!result.processed)
    {
      continue;
    }

    if (!result.found)
    {
      continue;
    }

    geometries.lines.append(result.line);
    cv::line(
      diagnostic,
      cv::Point(static_cast<int>(std::round(result.line.start.x)), static_cast<int>(std::round(result.line.start.y))),
      cv::Point(static_cast<int>(std::round(result.line.end.x)), static_cast<int>(std::round(result.line.end.y))),
      cv::Scalar(0, 255, 0),
      2,
      cv::LINE_AA);
    detectedOverlay.lines.append({
      QPointF(result.line.start.x, result.line.start.y),
      QPointF(result.line.end.x, result.line.end.y),
      QColor("#35c46a"),
      3
    });
  }

  loadGeometryPointRecipe(camera);
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  geometries.points.clear();
  for (GeometryPointRuntimeConfig& point : points)
  {
    if (!point.enabled || !point.hasGuide || !pose.valid)
    {
      continue;
    }

    const cv::Point2d imageStart = partToImage(pose, point.partStart);
    const cv::Point2d imageEnd = partToImage(pose, point.partEnd);

    EdgePointDetectorConfig config;
    config.id = point.id;
    config.label = point.id;
    config.scanStart = imageStart;
    config.scanEnd = imageEnd;
    config.edgeSensitivity = point.edgeSensitivity;
    config.useSubpixel = isBwDimensionalCamera(camera) && point.useSubpixel;
    config.transition = point.transition;
    config.pickMode = point.pickMode;

    EdgePointDetector detector;
    const EdgePointDetectorResult result = detector.detect(input, config);
    if (result.processed && result.found)
    {
      geometries.points.append(result.point);
      GeometryDiagnosticDrawing::drawCyanPointCross(diagnostic, result.point.point);
      GeometryDiagnosticDrawing::appendCyanPointCross(detectedOverlay, result.point.point);
    }
  }

  loadGeometryCirclesRecipe(camera);
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[camera.id];
  geometries.circles.clear();
  for (GeometryCircleRuntimeConfig& circle : circles)
  {
    if (!circle.enabled)
    {
      continue;
    }

    if (pose.valid && !circle.hasCircle && circle.hasImageCircle)
    {
      circle.partCenter = imageToPart(pose, circle.imageCenter);
      circle.hasCircle = true;
    }

    const bool usePartCircle = pose.valid && circle.hasCircle;
    if (!usePartCircle && !circle.hasImageCircle)
    {
      continue;
    }

    const cv::Point2d guideCenter = usePartCircle ? partToImage(pose, circle.partCenter) : circle.imageCenter;
    const int guideRadius = static_cast<int>(std::round(circle.radius));
    if (guideRadius <= 0)
    {
      continue;
    }

    EdgeCircleDetectorConfig config;
    config.id = circle.id;
    config.label = circle.id;
    config.guideCenter = guideCenter;
    config.guideRadius = circle.radius;
    config.innerBand = circle.innerBand;
    config.outerBand = circle.outerBand;
    config.edgeSensitivity = circle.edgeSensitivity;
    config.edgeCleanupDerivative = circle.edgeCleanupDerivative;
    config.edgeStatisticalFilter = circle.edgeStatisticalFilter;
    config.useSubpixel = isBwDimensionalCamera(camera) && circle.useSubpixel;
    config.transition = circle.transition;
    config.pickMode = circle.pickMode;

    EdgeCircleDetector detector;
    const EdgeCircleDetectorResult result = detector.detect(input, config);
    if (!result.processed)
    {
      continue;
    }

    if (!result.found)
    {
      continue;
    }

    geometries.circles.append(result.circle);
    cv::circle(
      diagnostic,
      cv::Point(static_cast<int>(std::round(result.circle.center.x)), static_cast<int>(std::round(result.circle.center.y))),
      static_cast<int>(std::round(result.circle.radius)),
      cv::Scalar(0, 255, 0),
      2,
      cv::LINE_AA);
  }

  m_selectedPreview = matToPixmap(diagnostic);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->clearRoi();
  m_largeImage->clearCircles();
  GeometryOverlay setupOverlay;
  for (const GeometryOverlayLine& line : detectedOverlay.lines)
  {
    setupOverlay.lines.append(line);
  }
  for (const GeometryOverlayPoint& point : detectedOverlay.points)
  {
    setupOverlay.points.append(point);
  }
  appendCurrentPartPoseOverlay(camera, setupOverlay);
  m_largeImage->setGeometryOverlay(setupOverlay);
}

void MainWindow::activateGeometryPointDrawing(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  deactivateImageDrawingTools();
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Point;
  m_pointGeometryMouseControllers[camera.id].begin(3.0);
  m_largeImage->setGeometryOverlayPointEditingEnabled(false);
  m_largeImage->setGeometryPointPickingEnabled(true);
  updateGeometryPointOverlay(camera);
  appendLog(trText("log.geometryPointDrawing") + ": " + camera.id);
}

void MainWindow::activateGeometryCircleDrawing(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  deactivateImageDrawingTools();
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Circle;
  m_largeImage->setThreePointCircleDrawingEnabled(true);
  showConfiguredGeometryCircles(camera);
  appendLog(trText("log.geometryCircleDrawing") + ": " + camera.id);
}


void MainWindow::handleGeometryCirclePoints(const CameraConfig& camera, const QVector<QPoint>& points)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  ImageCircle imageCircle;
  if (!circleFromThreePoints(points, imageCircle))
  {
    appendLog(trText("log.surfaceEdgeCircleInvalid") + ": " + camera.id);
    return;
  }

  GeometryCircleRuntimeConfig& config = activeGeometryCircleConfig(camera.id);
  config.imageCenter = cv::Point2d(imageCircle.center.x(), imageCircle.center.y());
  config.radius = imageCircle.radius;
  config.hasImageCircle = true;

  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid)
  {
    config.partCenter = imageToPart(pose, config.imageCenter);
    config.hasCircle = true;
  }
  else
  {
    config.hasCircle = false;
  }

  m_largeImage->setThreePointCircleDrawingEnabled(false);
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Circle;
  saveGeometryCirclesRecipe(camera);
  showConfiguredGeometryCircles(camera);
  testGeometryCircle(camera);
}


void MainWindow::showConfiguredGeometryCircles(const CameraConfig& camera)
{
  GeometryOverlay overlay;
  m_largeImage->clearCircles();
  appendCurrentPartPoseOverlay(camera, overlay);
  m_largeImage->setGeometryOverlay(overlay);
}


void MainWindow::testGeometryCircle(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  GeometryCircleRuntimeConfig& circleConfig = activeGeometryCircleConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid && !circleConfig.hasCircle && circleConfig.hasImageCircle)
  {
    circleConfig.partCenter = imageToPart(pose, circleConfig.imageCenter);
    circleConfig.hasCircle = true;
    saveGeometryCirclesRecipe(camera);
  }

  const bool usePartCircle = pose.valid && circleConfig.hasCircle;
  if (!usePartCircle && !circleConfig.hasImageCircle)
  {
    showConfiguredGeometryCircles(camera);
    appendLog(trText("log.geometryCircleMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const cv::Point2d guideCenter = usePartCircle ? partToImage(pose, circleConfig.partCenter) : circleConfig.imageCenter;
  EdgeCircleDetectorConfig config;
  config.id = circleConfig.id;
  config.label = circleConfig.id;
  config.guideCenter = guideCenter;
  config.guideRadius = circleConfig.radius;
  config.innerBand = circleConfig.innerBand;
  config.outerBand = circleConfig.outerBand;
  config.edgeSensitivity = circleConfig.edgeSensitivity;
  config.edgeCleanupDerivative = circleConfig.edgeCleanupDerivative;
  config.edgeStatisticalFilter = circleConfig.edgeStatisticalFilter;
  config.useSubpixel = isBwDimensionalCamera(camera) && circleConfig.useSubpixel;
  config.transition = circleConfig.transition;
  config.pickMode = circleConfig.pickMode;

  EdgeCircleDetector detector;
  const EdgeCircleDetectorResult result = detector.detect(input, config);
  if (!result.processed || result.diagnosticImage.empty())
  {
    appendLog(result.message.isEmpty() ? trText("log.geometryCircleFailed") + ": " + camera.id : result.message);
    return;
  }

  m_selectedPreview = matToPixmap(result.diagnosticImage);
  m_largeImage->setImage(m_selectedPreview);
  showConfiguredGeometryCircles(camera);

  if (!result.found)
  {
    appendLog(result.message.isEmpty() ? trText("log.geometryCircleNotFound") + ": " + camera.id : result.message);
    return;
  }

  GeometrySet& geometries = m_cameraRuntime[camera.id].geometries();
  for (int i = geometries.circles.size() - 1; i >= 0; --i)
  {
    if (geometries.circles[i].meta.id == circleConfig.id)
    {
      geometries.circles.removeAt(i);
    }
  }
  geometries.circles.append(result.circle);
  appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
              .arg(trText("log.geometryCircleFound"))
              .arg(camera.id)
              .arg(result.circle.center.x, 0, 'f', 1)
              .arg(result.circle.center.y, 0, 'f', 1)
              .arg(result.circle.radius, 0, 'f', 1));
}


void MainWindow::handleGeometryPointGuidePoint(const CameraConfig& camera, const QPointF& imagePoint)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  LineGeometryMouseController& controller = m_pointGeometryMouseControllers[camera.id];
  const bool completed = controller.handleClick(imagePoint);
  updateGeometryPointOverlay(camera);

  if (!completed)
  {
    return;
  }

  const LineGeometryEditorState& state = controller.state();
  GeometryPointRuntimeConfig& config = activeGeometryPointConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageGuide = true;

  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasGuide = true;
  }

  m_largeImage->setGeometryPointPickingEnabled(false);
  m_largeImage->setGeometryOverlayPointEditingEnabled(true);
  updateGeometryPointOverlay(camera);
  appendLog(trText("log.geometryPointGuideSaved") + ": " + camera.id);
  saveGeometryPointRecipe(camera);
  testGeometryPoint(camera);
}


void MainWindow::handleGeometryPointHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint)
{
  if (camera.id != m_selectedCameraId || pointIndex < 0 || pointIndex > 1)
  {
    return;
  }

  LineGeometryMouseController& controller = m_pointGeometryMouseControllers[camera.id];
  controller.movePoint(pointIndex, imagePoint);

  const LineGeometryEditorState& state = controller.state();
  if (!state.hasStart || !state.hasEnd)
  {
    updateGeometryPointOverlay(camera);
    return;
  }

  GeometryPointRuntimeConfig& config = activeGeometryPointConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageGuide = true;

  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasGuide = true;
  }
  else
  {
    config.hasGuide = false;
  }

  updateGeometryPointOverlay(camera);
  saveGeometryPointRecipe(camera);
  testGeometryPoint(camera);
}


GeometryOverlay MainWindow::configuredGeometryPointsOverlay(const CameraConfig& camera, bool includeActive) const
{
  GeometryOverlay overlay;
  const PartPose& pose = m_cameraRuntime.at(camera.id).currentPose();
  const QVector<GeometryPointRuntimeConfig> points = m_geometryPointConfigs.value(camera.id);
  const int activeIndex = m_activeGeometryPointIndexes.value(camera.id, 0);

  for (int i = 0; i < points.size(); ++i)
  {
    if (!includeActive && i == activeIndex)
    {
      continue;
    }

    const GeometryPointRuntimeConfig& point = points[i];
    const bool usePartGuide = pose.valid && point.hasGuide;
    if (!usePartGuide && !point.hasImageGuide)
    {
      continue;
    }

    const cv::Point2d imageStart = usePartGuide ? partToImage(pose, point.partStart) : point.imageStart;
    const cv::Point2d imageEnd = usePartGuide ? partToImage(pose, point.partEnd) : point.imageEnd;
    const QPointF start(imageStart.x, imageStart.y);
    const QPointF end(imageEnd.x, imageEnd.y);
    const QPointF center = (start + end) * 0.5;
    const QPointF delta = end - start;
    const double length = std::hypot(delta.x(), delta.y());
    const double angleDegrees = std::atan2(delta.y(), delta.x()) * 180.0 / CV_PI;
    const bool highlightActive =
      m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry &&
      m_geometryDrawingTarget == GeometryDrawingTarget::Point &&
      i == activeIndex;
    const QColor lineColor = highlightActive ? QColor("#ff4fd8") : QColor(170, 205, 220, 170);
    const QColor bandColor = highlightActive ? QColor(255, 79, 216, 24) : QColor(120, 170, 190, 18);
    overlay.bands.append({center, QSizeF(length, 6.0), angleDegrees, lineColor, bandColor});
    overlay.lines.append({start, end, lineColor, highlightActive ? 3 : 1});
    if (includeActive && highlightActive)
    {
      overlay.points.append({start, "1"});
      overlay.points.append({end, "2"});
    }
  }

  return overlay;
}


void MainWindow::updateGeometryPointOverlay(const CameraConfig& camera, const GeometryOverlay& extraOverlay)
{
  GeometryOverlay overlay = configuredGeometryPointsOverlay(camera, false);
  GeometryOverlay activeOverlay = m_pointGeometryMouseControllers[camera.id].overlay();
  for (const GeometryOverlayPoint& point : activeOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : activeOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : activeOverlay.bands)
  {
    overlay.bands.append(band);
  }
  for (const GeometryOverlayPoint& point : extraOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : extraOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : extraOverlay.bands)
  {
    overlay.bands.append(band);
  }
  appendCurrentPartPoseOverlay(camera, overlay);
  m_largeImage->setGeometryOverlay(overlay);
}


void MainWindow::appendCurrentPartPoseOverlay(const CameraConfig& camera, GeometryOverlay& overlay) const
{
  const auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt == m_cameraRuntime.end())
  {
    return;
  }

  const PartPose& pose = runtimeIt->second.currentPose();
  if (!pose.valid)
  {
    return;
  }

  GeometryDiagnosticDrawing::appendCyanPointCross(overlay, pose.origin);
}


void MainWindow::testGeometryPoint(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  GeometryPointRuntimeConfig& pointConfig = activeGeometryPointConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid && !pointConfig.hasGuide && pointConfig.hasImageGuide)
  {
    pointConfig.partStart = imageToPart(pose, pointConfig.imageStart);
    pointConfig.partEnd = imageToPart(pose, pointConfig.imageEnd);
    pointConfig.hasGuide = true;
  }

  const bool usePartGuide = pose.valid && pointConfig.hasGuide;
  if (!usePartGuide && !pointConfig.hasImageGuide)
  {
    updateGeometryPointOverlay(camera);
    return;
  }

  const cv::Point2d imageStart = usePartGuide ? partToImage(pose, pointConfig.partStart) : pointConfig.imageStart;
  const cv::Point2d imageEnd = usePartGuide ? partToImage(pose, pointConfig.partEnd) : pointConfig.imageEnd;
  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  EdgePointDetectorConfig config;
  config.id = pointConfig.id;
  config.label = pointConfig.id;
  config.scanStart = imageStart;
  config.scanEnd = imageEnd;
  config.edgeSensitivity = pointConfig.edgeSensitivity;
  config.useSubpixel = isBwDimensionalCamera(camera) && pointConfig.useSubpixel;
  config.transition = pointConfig.transition;
  config.pickMode = pointConfig.pickMode;

  EdgePointDetector detector;
  const EdgePointDetectorResult result = detector.detect(input, config);
  if (!result.processed || result.diagnosticImage.empty())
  {
    appendLog(result.message.isEmpty() ? trText("log.geometryPointFailed") + ": " + camera.id : result.message);
    return;
  }

  m_selectedPreview = matToPixmap(result.diagnosticImage);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->clearRoi();

  LineGeometryMouseController& controller = m_pointGeometryMouseControllers[camera.id];
  controller.setLine(QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y), 3.0);
  m_geometryDrawingTarget = GeometryDrawingTarget::Point;

  GeometryOverlay detectedOverlay;
  if (result.found)
  {
    GeometryDiagnosticDrawing::appendCyanPointCross(detectedOverlay, result.point.point);
  }
  updateGeometryPointOverlay(camera, detectedOverlay);

  if (!result.found)
  {
    appendLog(result.message.isEmpty() ? trText("log.geometryPointNotFound") + ": " + camera.id : result.message);
    return;
  }

  GeometrySet& geometries = m_cameraRuntime[camera.id].geometries();
  for (int i = geometries.points.size() - 1; i >= 0; --i)
  {
    if (geometries.points[i].meta.id == pointConfig.id)
    {
      geometries.points.removeAt(i);
    }
  }
  geometries.points.append(result.point);

  appendLog(QString("%1: %2 x=%3 y=%4")
              .arg(trText("log.geometryPointFound"))
              .arg(camera.id)
              .arg(result.point.point.x, 0, 'f', 1)
              .arg(result.point.point.y, 0, 'f', 1));
}

