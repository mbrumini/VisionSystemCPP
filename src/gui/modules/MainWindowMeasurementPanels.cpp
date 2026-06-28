#include "gui/modules/MainWindowMeasurementModule.h"

#include "gui/modules/ConstructedGeometryCircleSource.h"
#include "gui/modules/ConstructedGeometryLineSource.h"
#include "gui/modules/ConstructedGeometryPointSource.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/geometry/GeometryOverlay.h"
#include "gui/TouchIconButton.h"
#include "runtime/CameraRuntime.h"

#include <QComboBox>
#include <QColor>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
QPointF toPointF(const cv::Point2d& point)
{
  return QPointF(point.x, point.y);
}

void addSources(QComboBox* combo, const QVector<ConstructedGeometryPointSource>& sources)
{
  for (const ConstructedGeometryPointSource& source : sources)
  {
    combo->addItem(source.label, source.id);
  }
}

void addSources(QComboBox* combo, const QVector<ConstructedGeometryLineSource>& sources)
{
  for (const ConstructedGeometryLineSource& source : sources)
  {
    combo->addItem(source.label, source.id);
  }
}

void addSources(QComboBox* combo, const QVector<ConstructedGeometryCircleSource>& sources)
{
  for (const ConstructedGeometryCircleSource& source : sources)
  {
    combo->addItem(source.label, source.id);
  }
}

void applySecondarySourceDefault(QComboBox* secondaryCombo, int sourceCount)
{
  if (sourceCount >= 2)
  {
    secondaryCombo->setCurrentIndex(1);
  }
}

bool setComboCurrentData(QComboBox* combo, const QString& id)
{
  if (!combo)
  {
    return false;
  }
  const int index = combo->findData(id);
  if (index < 0)
  {
    return false;
  }
  combo->setCurrentIndex(index);
  return true;
}

double pointDistance(const cv::Point2d& a, const cv::Point2d& b)
{
  const cv::Point2d delta = a - b;
  return std::sqrt(delta.dot(delta));
}

double lineSegmentDistance(const LineGeometry& line, const cv::Point2d& point)
{
  const cv::Point2d segment = line.end - line.start;
  const double lengthSquared = segment.dot(segment);
  if (lengthSquared <= 0.000001)
  {
    return pointDistance(line.start, point);
  }
  const cv::Point2d delta = point - line.start;
  const double t = std::clamp(delta.dot(segment) / lengthSquared, 0.0, 1.0);
  return pointDistance(line.start + segment * t, point);
}

QString nearestPointSourceId(const QVector<ConstructedGeometryPointSource>& sources,
                             const QPointF& imagePoint,
                             double maxDistance = 28.0,
                             double* bestDistanceOut = nullptr)
{
  const cv::Point2d target(imagePoint.x(), imagePoint.y());
  QString bestId;
  double bestDistance = maxDistance;
  for (const ConstructedGeometryPointSource& source : sources)
  {
    if (!source.point)
    {
      continue;
    }
    const double distance = pointDistance(source.point->point, target);
    if (distance <= bestDistance)
    {
      bestDistance = distance;
      bestId = source.id;
    }
  }
  if (bestDistanceOut)
  {
    *bestDistanceOut = bestId.isEmpty() ? std::numeric_limits<double>::infinity() : bestDistance;
  }
  return bestId;
}

QString nearestLineSourceId(const QVector<ConstructedGeometryLineSource>& sources,
                            const QPointF& imagePoint,
                            double maxDistance = 28.0,
                            double* bestDistanceOut = nullptr)
{
  const cv::Point2d target(imagePoint.x(), imagePoint.y());
  QString bestId;
  double bestDistance = maxDistance;
  for (const ConstructedGeometryLineSource& source : sources)
  {
    if (!source.line)
    {
      continue;
    }
    const double distance = lineSegmentDistance(*source.line, target);
    if (distance <= bestDistance)
    {
      bestDistance = distance;
      bestId = source.id;
    }
  }
  if (bestDistanceOut)
  {
    *bestDistanceOut = bestId.isEmpty() ? std::numeric_limits<double>::infinity() : bestDistance;
  }
  return bestId;
}

QString nearestCircleSourceId(const GeometrySet& set,
                              const QVector<ConstructedGeometryCircleSource>& sources,
                              const QPointF& imagePoint,
                              double maxDistance = 32.0,
                              double* bestDistanceOut = nullptr)
{
  const cv::Point2d target(imagePoint.x(), imagePoint.y());
  QString bestId;
  double bestDistance = maxDistance;
  for (const ConstructedGeometryCircleSource& source : sources)
  {
    CircleGeometry circle;
    if (!constructedGeometryCircleSourceValue(set, source.id, circle) || circle.radius <= 0.0)
    {
      continue;
    }
    const double centerDistance = pointDistance(circle.center, target);
    const double distance = std::min(std::abs(centerDistance - circle.radius), centerDistance);
    if (distance <= bestDistance)
    {
      bestDistance = distance;
      bestId = source.id;
    }
  }
  if (bestDistanceOut)
  {
    *bestDistanceOut = bestId.isEmpty() ? std::numeric_limits<double>::infinity() : bestDistance;
  }
  return bestId;
}

QVBoxLayout* createMeasurementPage(QWidget* panel, const QString& titleText)
{
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(titleText, panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  return layout;
}

void appendMeasurementHint(QWidget* panel, QVBoxLayout* layout, const QString& text)
{
  auto* hint = new QLabel(text, panel);
  hint->setWordWrap(true);
  hint->setObjectName("toolPanelHint");
  layout->addWidget(hint);
}

QPushButton* createSaveMeasurementButton(QWidget* panel, const QString& label)
{
  auto* saveButton = new QPushButton(label, panel);
  saveButton->setObjectName("primaryActionButton");
  saveButton->setMinimumHeight(44);
  return saveButton;
}

const ConstructedGeometryPointSource* findPointSource(
  const QVector<ConstructedGeometryPointSource>& sources,
  const QString& id)
{
  for (const ConstructedGeometryPointSource& source : sources)
  {
    if (source.id == id)
    {
      return &source;
    }
  }
  return nullptr;
}

const ConstructedGeometryLineSource* findLineSource(
  const QVector<ConstructedGeometryLineSource>& sources,
  const QString& id)
{
  for (const ConstructedGeometryLineSource& source : sources)
  {
    if (source.id == id)
    {
      return &source;
    }
  }
  return nullptr;
}

const ConstructedGeometryCircleSource* findCircleSource(
  const QVector<ConstructedGeometryCircleSource>& sources,
  const QString& id)
{
  for (const ConstructedGeometryCircleSource& source : sources)
  {
    if (source.id == id)
    {
      return &source;
    }
  }
  return nullptr;
}

void appendSelectionPoint(GeometryOverlay& overlay,
                          const PointGeometry& point,
                          const QString& label,
                          const QColor& color)
{
  overlay.points.append({toPointF(point.point), label, color, 9.0});
}

void appendSelectionLine(GeometryOverlay& overlay,
                         const LineGeometry& line,
                         const QString& label,
                         const QColor& color)
{
  const QPointF start = toPointF(line.start);
  const QPointF end = toPointF(line.end);
  overlay.lines.append({start, end, color, 9});
  overlay.points.append({(start + end) * 0.5, label, color, 8.0});
}

void appendSelectionCircle(GeometryOverlay& overlay,
                           const CircleGeometry& circle,
                           const QString& label,
                           const QColor& color)
{
  constexpr int kSegments = 96;
  constexpr double kPi = 3.14159265358979323846;
  QPointF previous;
  for (int index = 0; index <= kSegments; ++index)
  {
    const double angle = 2.0 * kPi * static_cast<double>(index) / static_cast<double>(kSegments);
    const QPointF current(
      circle.center.x + std::cos(angle) * circle.radius,
      circle.center.y + std::sin(angle) * circle.radius);
    if (index > 0)
    {
      overlay.lines.append({previous, current, color, 7});
    }
    previous = current;
  }
  overlay.points.append({toPointF(circle.center), label, color, 8.0});
}
}

void MainWindowMeasurementModule::handleImagePick(const CameraConfig& camera, const QPointF& imagePoint)
{
  if (m_imagePickMode == ImagePickMode::None || !m_pickPrimaryCombo)
  {
    return;
  }

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  if (m_imagePickMode == ImagePickMode::PointPoint)
  {
    const QVector<ConstructedGeometryPointSource> sources =
      constructedGeometryPointSources(set, geometryAliasesForCamera(camera));
    const QString id = nearestPointSourceId(sources, imagePoint);
    QComboBox* target = (m_nextPickTarget % 2 == 0) ? m_pickPrimaryCombo.data() : m_pickSecondaryCombo.data();
    if (!id.isEmpty() && setComboCurrentData(target, id))
    {
      ++m_nextPickTarget;
    }
    return;
  }

  if (m_imagePickMode == ImagePickMode::PointLine)
  {
    const QVector<ConstructedGeometryPointSource> pointSources =
      constructedGeometryPointSources(set, geometryAliasesForCamera(camera));
    const QVector<ConstructedGeometryLineSource> lineSources =
      constructedGeometryLineSources(set, geometryAliasesForCamera(camera));
    double pointDistance = 0.0;
    double lineDistance = 0.0;
    const QString pointId = nearestPointSourceId(pointSources, imagePoint, 28.0, &pointDistance);
    const QString lineId = nearestLineSourceId(lineSources, imagePoint, 28.0, &lineDistance);
    if (!pointId.isEmpty() && pointDistance <= lineDistance)
    {
      setComboCurrentData(m_pickPrimaryCombo.data(), pointId);
      return;
    }
    if (!lineId.isEmpty())
    {
      setComboCurrentData(m_pickSecondaryCombo.data(), lineId);
    }
    return;
  }

  if (m_imagePickMode == ImagePickMode::LineLineDistance || m_imagePickMode == ImagePickMode::LineLineAngle)
  {
    const QVector<ConstructedGeometryLineSource> sources =
      constructedGeometryLineSources(set, geometryAliasesForCamera(camera));
    const QString id = nearestLineSourceId(sources, imagePoint);
    QComboBox* target = (m_nextPickTarget % 2 == 0) ? m_pickPrimaryCombo.data() : m_pickSecondaryCombo.data();
    if (!id.isEmpty() && setComboCurrentData(target, id))
    {
      ++m_nextPickTarget;
    }
    return;
  }

  if (m_imagePickMode == ImagePickMode::CircleDiameter)
  {
    const QVector<ConstructedGeometryCircleSource> sources =
      constructedGeometryCircleSources(set, geometryAliasesForCamera(camera));
    const QString id = nearestCircleSourceId(set, sources, imagePoint);
    if (!id.isEmpty())
    {
      setComboCurrentData(m_pickPrimaryCombo.data(), id);
    }
  }
}

void MainWindowMeasurementModule::showPointPointDistancePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  m_imagePickMode = ImagePickMode::None;
  refreshMeasurementSources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryPointSource> pointSources =
    constructedGeometryPointSources(set, geometryAliasesForCamera(camera));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = createMeasurementPage(panel, QString("%1 | %2").arg(tr("actions.pointPointDistance"), camera.id));

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);

  auto* pointACombo = new QComboBox(form);
  auto* pointBCombo = new QComboBox(form);
  addSources(pointACombo, pointSources);
  addSources(pointBCombo, pointSources);
  applySecondarySourceDefault(pointBCombo, pointSources.size());

  const auto updateSourceOverlay = [this, camera, pointACombo, pointBCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString pointAId = pointACombo->currentData().toString();
    if (const PointGeometry* point = findConstructedGeometryPointSource(refreshedSet, pointAId))
    {
      appendSelectionPoint(overlay, *point, QStringLiteral("A %1").arg(pointAId), QColor("#ffcc00"));
    }
    const QString pointBId = pointBCombo->currentData().toString();
    if (const PointGeometry* point = findConstructedGeometryPointSource(refreshedSet, pointBId))
    {
      appendSelectionPoint(overlay, *point, QStringLiteral("B %1").arg(pointBId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(pointACombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(pointBCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  m_imagePickMode = ImagePickMode::PointPoint;
  m_pickPrimaryCombo = pointACombo;
  m_pickSecondaryCombo = pointBCombo;
  m_nextPickTarget = 0;

  formLayout->addWidget(new QLabel(tr("labels.sourcePointA"), form), 0, 0);
  formLayout->addWidget(pointACombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourcePointB"), form), 1, 0);
  formLayout->addWidget(pointBCombo, 1, 1);
  layout->addWidget(form);

  if (pointSources.isEmpty())
  {
    const GeometrySet& geometrySet = cameraRuntime()[camera.id].geometries();
    const bool hasLines = !geometrySet.lines.isEmpty() || !geometrySet.constructedLines.isEmpty();
    appendMeasurementHint(
      panel,
      layout,
      hasLines ? tr("hints.measurementNoPointSourcesUseLines") : tr("hints.measurementNoPointSources"));
  }

  const auto refreshPanel = [this, camera]() { showPointPointDistancePanel(camera); };
  auto* saveButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement"));
  saveButton->setEnabled(pointSources.size() >= 2);
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, pointACombo, pointBCombo, refreshPanel]() {
    createPointPointDistance(camera, pointACombo->currentData().toString(), pointBCombo->currentData().toString());
    refreshPanel();
  });
  appendMeasurementListControls(panel, layout, camera, refreshPanel);
  layout->addWidget(saveButton);

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowMeasurementModule::showPointLineDistancePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  m_imagePickMode = ImagePickMode::None;
  refreshMeasurementSources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryPointSource> pointSources =
    constructedGeometryPointSources(set, geometryAliasesForCamera(camera));
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(set, geometryAliasesForCamera(camera));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = createMeasurementPage(panel, QString("%1 | %2").arg(tr("actions.pointLineDistance"), camera.id));

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);

  auto* pointCombo = new QComboBox(form);
  auto* lineCombo = new QComboBox(form);
  addSources(pointCombo, pointSources);
  addSources(lineCombo, lineSources);

  const auto updateSourceOverlay = [this, camera, pointCombo, lineCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString pointId = pointCombo->currentData().toString();
    if (const PointGeometry* point = findConstructedGeometryPointSource(refreshedSet, pointId))
    {
      appendSelectionPoint(overlay, *point, QStringLiteral("P %1").arg(pointId), QColor("#ffcc00"));
    }
    const QString lineId = lineCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, lineId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("L %1").arg(lineId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(pointCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(lineCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  m_imagePickMode = ImagePickMode::PointLine;
  m_pickPrimaryCombo = pointCombo;
  m_pickSecondaryCombo = lineCombo;
  m_nextPickTarget = 0;

  formLayout->addWidget(new QLabel(tr("labels.sourcePoint"), form), 0, 0);
  formLayout->addWidget(pointCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceLine"), form), 1, 0);
  formLayout->addWidget(lineCombo, 1, 1);
  layout->addWidget(form);

  const auto refreshPanel = [this, camera]() { showPointLineDistancePanel(camera); };
  auto* saveButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement"));
  saveButton->setEnabled(!pointSources.isEmpty() && !lineSources.isEmpty());
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, pointCombo, lineCombo, refreshPanel]() {
    createPointLineDistance(camera, pointCombo->currentData().toString(), lineCombo->currentData().toString());
    refreshPanel();
  });
  appendMeasurementListControls(panel, layout, camera, refreshPanel);
  layout->addWidget(saveButton);

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowMeasurementModule::showLineLineDistancePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  m_imagePickMode = ImagePickMode::None;
  refreshMeasurementSources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(set, geometryAliasesForCamera(camera));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = createMeasurementPage(panel, QString("%1 | %2").arg(tr("actions.lineLineDistance"), camera.id));

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);

  auto* lineACombo = new QComboBox(form);
  auto* lineBCombo = new QComboBox(form);
  addSources(lineACombo, lineSources);
  addSources(lineBCombo, lineSources);
  applySecondarySourceDefault(lineBCombo, lineSources.size());

  const auto updateSourceOverlay = [this, camera, lineACombo, lineBCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString lineAId = lineACombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, lineAId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("A %1").arg(lineAId), QColor("#ffcc00"));
    }
    const QString lineBId = lineBCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, lineBId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("B %1").arg(lineBId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(lineACombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(lineBCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  m_imagePickMode = ImagePickMode::LineLineDistance;
  m_pickPrimaryCombo = lineACombo;
  m_pickSecondaryCombo = lineBCombo;
  m_nextPickTarget = 0;

  formLayout->addWidget(new QLabel(tr("labels.sourceLineA"), form), 0, 0);
  formLayout->addWidget(lineACombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceLineB"), form), 1, 0);
  formLayout->addWidget(lineBCombo, 1, 1);
  layout->addWidget(form);

  if (lineSources.size() < 2)
  {
    appendMeasurementHint(panel, layout, tr("hints.measurementNoLineSources"));
  }

  const auto refreshPanel = [this, camera]() { showLineLineDistancePanel(camera); };
  auto* saveButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement"));
  auto* saveMinButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement") + " MIN");
  auto* saveMaxButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement") + " MAX");
  saveButton->setEnabled(lineSources.size() >= 2);
  saveMinButton->setEnabled(lineSources.size() >= 2);
  saveMaxButton->setEnabled(lineSources.size() >= 2);
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, lineACombo, lineBCombo, refreshPanel]() {
    createLineLineDistance(camera, lineACombo->currentData().toString(), lineBCombo->currentData().toString());
    refreshPanel();
  });
  QObject::connect(saveMinButton, &QPushButton::clicked, window(), [this, camera, lineACombo, lineBCombo, refreshPanel]() {
    createLineLineDistance(
      camera,
      lineACombo->currentData().toString(),
      lineBCombo->currentData().toString(),
      "line_line_distance_min");
    refreshPanel();
  });
  QObject::connect(saveMaxButton, &QPushButton::clicked, window(), [this, camera, lineACombo, lineBCombo, refreshPanel]() {
    createLineLineDistance(
      camera,
      lineACombo->currentData().toString(),
      lineBCombo->currentData().toString(),
      "line_line_distance_max");
    refreshPanel();
  });
  appendMeasurementListControls(panel, layout, camera, refreshPanel);
  layout->addWidget(saveButton);
  layout->addWidget(saveMinButton);
  layout->addWidget(saveMaxButton);

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowMeasurementModule::showCircleDiameterPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  m_imagePickMode = ImagePickMode::None;
  refreshMeasurementSources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryCircleSource> circleSources =
    constructedGeometryCircleSources(set, geometryAliasesForCamera(camera));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = createMeasurementPage(panel, QString("%1 | %2").arg(tr("actions.circleDiameterMeasure"), camera.id));

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);

  auto* circleCombo = new QComboBox(form);
  addSources(circleCombo, circleSources);

  const auto updateSourceOverlay = [this, camera, circleCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString circleId = circleCombo->currentData().toString();
    CircleGeometry circle;
    if (constructedGeometryCircleSourceValue(refreshedSet, circleId, circle))
    {
      appendSelectionCircle(overlay, circle, QStringLiteral("C %1").arg(circleId), QColor("#ffcc00"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(circleCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  m_imagePickMode = ImagePickMode::CircleDiameter;
  m_pickPrimaryCombo = circleCombo;
  m_pickSecondaryCombo = nullptr;
  m_nextPickTarget = 0;

  formLayout->addWidget(new QLabel(tr("labels.sourceCircle"), form), 0, 0);
  formLayout->addWidget(circleCombo, 0, 1);
  layout->addWidget(form);

  const auto refreshPanel = [this, camera]() { showCircleDiameterPanel(camera); };
  auto* saveButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement"));
  saveButton->setEnabled(!circleSources.isEmpty());
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, circleCombo, refreshPanel]() {
    createCircleDiameter(camera, circleCombo->currentData().toString());
    refreshPanel();
  });
  appendMeasurementListControls(panel, layout, camera, refreshPanel);
  layout->addWidget(saveButton);

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowMeasurementModule::showLineLineAnglePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  m_imagePickMode = ImagePickMode::None;
  refreshMeasurementSources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(set, geometryAliasesForCamera(camera));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = createMeasurementPage(panel, QString("%1 | %2").arg(tr("actions.lineLineAngle"), camera.id));

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);

  auto* lineACombo = new QComboBox(form);
  auto* lineBCombo = new QComboBox(form);
  addSources(lineACombo, lineSources);
  addSources(lineBCombo, lineSources);
  applySecondarySourceDefault(lineBCombo, lineSources.size());

  const auto updateSourceOverlay = [this, camera, lineACombo, lineBCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString lineAId = lineACombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, lineAId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("A %1").arg(lineAId), QColor("#ffcc00"));
    }
    const QString lineBId = lineBCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, lineBId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("B %1").arg(lineBId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(lineACombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(lineBCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  m_imagePickMode = ImagePickMode::LineLineAngle;
  m_pickPrimaryCombo = lineACombo;
  m_pickSecondaryCombo = lineBCombo;
  m_nextPickTarget = 0;

  formLayout->addWidget(new QLabel(tr("labels.sourceLineA"), form), 0, 0);
  formLayout->addWidget(lineACombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceLineB"), form), 1, 0);
  formLayout->addWidget(lineBCombo, 1, 1);
  layout->addWidget(form);

  const auto refreshPanel = [this, camera]() { showLineLineAnglePanel(camera); };
  auto* saveButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement"));
  saveButton->setEnabled(lineSources.size() >= 2);
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, lineACombo, lineBCombo, refreshPanel]() {
    createLineLineAngle(camera, lineACombo->currentData().toString(), lineBCombo->currentData().toString());
    refreshPanel();
  });
  appendMeasurementListControls(panel, layout, camera, refreshPanel);
  layout->addWidget(saveButton);

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}
