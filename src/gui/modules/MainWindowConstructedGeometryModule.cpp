#include "gui/modules/MainWindowConstructedGeometryModule.h"

#include "geometry/ConstructedGeometryMath.h"
#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/ConstructedGeometryCircleSource.h"
#include "gui/modules/ConstructedGeometryLineSource.h"
#include "gui/modules/ConstructedGeometryPointSource.h"
#include "runtime/CameraRuntime.h"

#include <cmath>

#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPointF>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QPointF toPointF(const cv::Point2d& point)
{
  return QPointF(point.x, point.y);
}

void appendCircleOverlay(GeometryOverlay& overlay, const CircleGeometry& circle)
{
  constexpr int segments = 96;
  QPointF previous;
  for (int i = 0; i <= segments; ++i)
  {
    const double angle = (2.0 * 3.14159265358979323846 * i) / segments;
    const QPointF current(circle.center.x + std::cos(angle) * circle.radius,
                          circle.center.y + std::sin(angle) * circle.radius);
    if (i > 0)
    {
      overlay.lines.append({previous, current, QColor("#00d2ff"), 3});
    }
    previous = current;
  }
}
}

MainWindowConstructedGeometryModule::MainWindowConstructedGeometryModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

void MainWindowConstructedGeometryModule::showConstructedGeometryPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();
  rebuildConstructedGeometryRecipe(camera);

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.constructedGeometries"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(tr("labels.constructedGeometryNote"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* actions = new QWidget(panel);
  auto* actionsLayout = new QGridLayout(actions);
  actionsLayout->setContentsMargins(0, 0, 0, 0);
  actionsLayout->setHorizontalSpacing(8);
  actionsLayout->setVerticalSpacing(8);

  const GeometrySet& geometrySet = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(geometrySet);
  const QVector<ConstructedGeometryCircleSource> circleSources = constructedGeometryCircleSources(geometrySet.circles);
  const QVector<ConstructedGeometryPointSource> pointSources = constructedGeometryPointSources(geometrySet);

  auto* firstLineCombo = new QComboBox(actions);
  auto* secondLineCombo = new QComboBox(actions);
  for (const ConstructedGeometryLineSource& source : lineSources)
  {
    firstLineCombo->addItem(source.label, source.id);
    secondLineCombo->addItem(source.label, source.id);
  }
  if (secondLineCombo->count() > 1)
  {
    secondLineCombo->setCurrentIndex(1);
  }

  auto* firstLineLabel = new QLabel(tr("labels.sourceLineA"), actions);
  auto* secondLineLabel = new QLabel(tr("labels.sourceLineB"), actions);
  auto* lineIntersectionButton = new QPushButton(tr("actions.lineLineIntersection"), actions);
  lineIntersectionButton->setMinimumHeight(38);
  lineIntersectionButton->setEnabled(lineSources.size() >= 2);

  actionsLayout->addWidget(firstLineLabel, 0, 0);
  actionsLayout->addWidget(firstLineCombo, 0, 1);
  actionsLayout->addWidget(secondLineLabel, 1, 0);
  actionsLayout->addWidget(secondLineCombo, 1, 1);
  actionsLayout->addWidget(lineIntersectionButton, 2, 0, 1, 2);

  QObject::connect(lineIntersectionButton, &QPushButton::clicked, window(), [this, camera, firstLineCombo, secondLineCombo]() {
    createLineLineIntersection(camera, firstLineCombo->currentData().toString(), secondLineCombo->currentData().toString());
  });

  auto* lineCircleLineCombo = new QComboBox(actions);
  auto* lineCircleCircleCombo = new QComboBox(actions);
  for (const ConstructedGeometryLineSource& source : lineSources)
  {
    lineCircleLineCombo->addItem(source.label, source.id);
  }
  for (const ConstructedGeometryCircleSource& source : circleSources)
  {
    lineCircleCircleCombo->addItem(source.label, source.id);
  }

  auto* lineCircleLineLabel = new QLabel(tr("labels.sourceLine"), actions);
  auto* lineCircleCircleLabel = new QLabel(tr("labels.sourceCircle"), actions);
  auto* lineCircleButton = new QPushButton(tr("actions.lineCircleIntersection"), actions);
  lineCircleButton->setMinimumHeight(38);
  lineCircleButton->setEnabled(!lineSources.isEmpty() && !circleSources.isEmpty());

  actionsLayout->addWidget(lineCircleLineLabel, 3, 0);
  actionsLayout->addWidget(lineCircleLineCombo, 3, 1);
  actionsLayout->addWidget(lineCircleCircleLabel, 4, 0);
  actionsLayout->addWidget(lineCircleCircleCombo, 4, 1);
  actionsLayout->addWidget(lineCircleButton, 5, 0, 1, 2);

  QObject::connect(lineCircleButton, &QPushButton::clicked, window(), [this, camera, lineCircleLineCombo, lineCircleCircleCombo]() {
    createLineCircleIntersection(camera, lineCircleLineCombo->currentData().toString(), lineCircleCircleCombo->currentData().toString());
  });

  auto* firstCircleCombo = new QComboBox(actions);
  auto* secondCircleCombo = new QComboBox(actions);
  for (const ConstructedGeometryCircleSource& source : circleSources)
  {
    firstCircleCombo->addItem(source.label, source.id);
    secondCircleCombo->addItem(source.label, source.id);
  }
  if (secondCircleCombo->count() > 1)
  {
    secondCircleCombo->setCurrentIndex(1);
  }

  auto* firstCircleLabel = new QLabel(tr("labels.sourceCircleA"), actions);
  auto* secondCircleLabel = new QLabel(tr("labels.sourceCircleB"), actions);
  auto* circleIntersectionButton = new QPushButton(tr("actions.circleCircleIntersection"), actions);
  circleIntersectionButton->setMinimumHeight(38);
  circleIntersectionButton->setEnabled(circleSources.size() >= 2);

  actionsLayout->addWidget(firstCircleLabel, 6, 0);
  actionsLayout->addWidget(firstCircleCombo, 6, 1);
  actionsLayout->addWidget(secondCircleLabel, 7, 0);
  actionsLayout->addWidget(secondCircleCombo, 7, 1);
  actionsLayout->addWidget(circleIntersectionButton, 8, 0, 1, 2);

  QObject::connect(circleIntersectionButton, &QPushButton::clicked, window(), [this, camera, firstCircleCombo, secondCircleCombo]() {
    createCircleCircleIntersection(camera, firstCircleCombo->currentData().toString(), secondCircleCombo->currentData().toString());
  });

  auto* firstPointCombo = new QComboBox(actions);
  auto* secondPointCombo = new QComboBox(actions);
  for (const ConstructedGeometryPointSource& source : pointSources)
  {
    firstPointCombo->addItem(source.label, source.id);
    secondPointCombo->addItem(source.label, source.id);
  }
  if (secondPointCombo->count() > 1)
  {
    secondPointCombo->setCurrentIndex(1);
  }

  auto* firstPointLabel = new QLabel(tr("labels.sourcePointA"), actions);
  auto* secondPointLabel = new QLabel(tr("labels.sourcePointB"), actions);
  auto* midpointButton = new QPushButton(tr("actions.midpoint"), actions);
  midpointButton->setMinimumHeight(38);
  midpointButton->setEnabled(pointSources.size() >= 2);

  actionsLayout->addWidget(firstPointLabel, 9, 0);
  actionsLayout->addWidget(firstPointCombo, 9, 1);
  actionsLayout->addWidget(secondPointLabel, 10, 0);
  actionsLayout->addWidget(secondPointCombo, 10, 1);
  actionsLayout->addWidget(midpointButton, 11, 0, 1, 2);

  QObject::connect(midpointButton, &QPushButton::clicked, window(), [this, camera, firstPointCombo, secondPointCombo]() {
    createMidpoint(camera, firstPointCombo->currentData().toString(), secondPointCombo->currentData().toString());
  });

  auto* offsetLineCombo = new QComboBox(actions);
  for (const ConstructedGeometryLineSource& source : lineSources)
  {
    offsetLineCombo->addItem(source.label, source.id);
  }

  auto* offsetLineLabel = new QLabel(tr("labels.sourceLine"), actions);
  auto* offsetValueLabel = new QLabel(tr("labels.offsetPixels"), actions);
  auto* offsetSpin = new QDoubleSpinBox(actions);
  offsetSpin->setRange(-10000.0, 10000.0);
  offsetSpin->setDecimals(2);
  offsetSpin->setSingleStep(1.0);
  offsetSpin->setValue(10.0);

  auto* offsetButton = new QPushButton(tr("actions.offsetLine"), actions);
  offsetButton->setMinimumHeight(38);
  offsetButton->setEnabled(!lineSources.isEmpty());

  actionsLayout->addWidget(offsetLineLabel, 12, 0);
  actionsLayout->addWidget(offsetLineCombo, 12, 1);
  actionsLayout->addWidget(offsetValueLabel, 13, 0);
  actionsLayout->addWidget(offsetSpin, 13, 1);
  actionsLayout->addWidget(offsetButton, 14, 0, 1, 2);

  QObject::connect(offsetButton, &QPushButton::clicked, window(), [this, camera, offsetLineCombo, offsetSpin]() {
    createOffsetLine(camera, offsetLineCombo->currentData().toString(), offsetSpin->value());
  });

  auto* firstBisectorLineCombo = new QComboBox(actions);
  auto* secondBisectorLineCombo = new QComboBox(actions);
  for (const ConstructedGeometryLineSource& source : lineSources)
  {
    firstBisectorLineCombo->addItem(source.label, source.id);
    secondBisectorLineCombo->addItem(source.label, source.id);
  }
  if (secondBisectorLineCombo->count() > 1)
  {
    secondBisectorLineCombo->setCurrentIndex(1);
  }

  auto* firstBisectorLineLabel = new QLabel(tr("labels.sourceLineA"), actions);
  auto* secondBisectorLineLabel = new QLabel(tr("labels.sourceLineB"), actions);
  auto* bisectorButton = new QPushButton(tr("actions.angleBisector"), actions);
  bisectorButton->setMinimumHeight(38);
  bisectorButton->setEnabled(lineSources.size() >= 2);

  actionsLayout->addWidget(firstBisectorLineLabel, 15, 0);
  actionsLayout->addWidget(firstBisectorLineCombo, 15, 1);
  actionsLayout->addWidget(secondBisectorLineLabel, 16, 0);
  actionsLayout->addWidget(secondBisectorLineCombo, 16, 1);
  actionsLayout->addWidget(bisectorButton, 17, 0, 1, 2);

  QObject::connect(bisectorButton, &QPushButton::clicked, window(), [this, camera, firstBisectorLineCombo, secondBisectorLineCombo]() {
    createAngleBisectors(camera, firstBisectorLineCombo->currentData().toString(), secondBisectorLineCombo->currentData().toString());
  });

  auto* tangentPointCombo = new QComboBox(actions);
  auto* tangentCircleCombo = new QComboBox(actions);
  for (const ConstructedGeometryPointSource& source : pointSources)
  {
    tangentPointCombo->addItem(source.label, source.id);
  }
  for (const ConstructedGeometryCircleSource& source : circleSources)
  {
    tangentCircleCombo->addItem(source.label, source.id);
  }

  auto* tangentPointLabel = new QLabel(tr("labels.sourcePoint"), actions);
  auto* tangentCircleLabel = new QLabel(tr("labels.sourceCircle"), actions);
  auto* tangentButton = new QPushButton(tr("actions.tangentLine"), actions);
  tangentButton->setMinimumHeight(38);
  tangentButton->setEnabled(!pointSources.isEmpty() && !circleSources.isEmpty());

  actionsLayout->addWidget(tangentPointLabel, 18, 0);
  actionsLayout->addWidget(tangentPointCombo, 18, 1);
  actionsLayout->addWidget(tangentCircleLabel, 19, 0);
  actionsLayout->addWidget(tangentCircleCombo, 19, 1);
  actionsLayout->addWidget(tangentButton, 20, 0, 1, 2);

  QObject::connect(tangentButton, &QPushButton::clicked, window(), [this, camera, tangentPointCombo, tangentCircleCombo]() {
    createTangentLines(camera, tangentPointCombo->currentData().toString(), tangentCircleCombo->currentData().toString());
  });

  auto* projectPointCombo = new QComboBox(actions);
  auto* projectLineCombo = new QComboBox(actions);
  for (const ConstructedGeometryPointSource& source : pointSources)
  {
    projectPointCombo->addItem(source.label, source.id);
  }
  for (const ConstructedGeometryLineSource& source : lineSources)
  {
    projectLineCombo->addItem(source.label, source.id);
  }

  auto* projectPointLabel = new QLabel(tr("labels.sourcePoint"), actions);
  auto* projectLineLabel = new QLabel(tr("labels.sourceLine"), actions);
  auto* projectButton = new QPushButton(tr("actions.projectPoint"), actions);
  projectButton->setMinimumHeight(38);
  projectButton->setEnabled(!pointSources.isEmpty() && !lineSources.isEmpty());

  actionsLayout->addWidget(projectPointLabel, 21, 0);
  actionsLayout->addWidget(projectPointCombo, 21, 1);
  actionsLayout->addWidget(projectLineLabel, 22, 0);
  actionsLayout->addWidget(projectLineCombo, 22, 1);
  actionsLayout->addWidget(projectButton, 23, 0, 1, 2);

  QObject::connect(projectButton, &QPushButton::clicked, window(), [this, camera, projectPointCombo, projectLineCombo]() {
    createProjectedPoint(camera, projectPointCombo->currentData().toString(), projectLineCombo->currentData().toString());
  });

  layout->addWidget(actions);

  auto* backButton = new QPushButton(tr("commands.backToCameraTools"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    context().showCameraToolList(camera);
    log(tr("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.constructedGeometryPanel") + ": " + camera.id);
}

void MainWindowConstructedGeometryModule::createLineLineIntersection(const CameraConfig& camera,
                                                                     const QString& firstLineId,
                                                                     const QString& secondLineId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const LineGeometry* firstLine = findConstructedGeometryLineSource(set, firstLineId);
  const LineGeometry* secondLine = findConstructedGeometryLineSource(set, secondLineId);
  if (!firstLine || !secondLine || firstLineId == secondLineId)
  {
    log(QString("%1: %2").arg(tr("log.constructedLineIntersectionMissing"), camera.id));
    return;
  }

  PointGeometry point;
  if (!ConstructedGeometryMath::lineLineIntersection(*firstLine, *secondLine, point))
  {
    log(QString("%1: %2").arg(tr("log.constructedLineIntersectionParallel"), camera.id));
    return;
  }

  ConstructedPointGeometry constructed;
  constructed.point = point;
  constructed.sourceAId = firstLine->meta.id;
  constructed.sourceBId = secondLine->meta.id;
  set.constructedPoints.append(constructed);
  saveConstructedGeometryRecipeAction(camera, "line_line_intersection", constructed.sourceAId, constructed.sourceBId);

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(firstLine->start), toPointF(firstLine->end), QColor("#35c46a"), 5});
  overlay.lines.append({toPointF(secondLine->start), toPointF(secondLine->end), QColor("#35c46a"), 5});
  overlay.points.append({QPointF(point.point.x, point.point.y), "X", QColor("#ff8a00")});
  largeImage()->setGeometryOverlay(overlay);

  log(QString("%1: %2 %3/%4 x=%5 y=%6")
        .arg(tr("log.constructedLineIntersectionCreated"))
        .arg(camera.id)
        .arg(constructed.sourceAId)
        .arg(constructed.sourceBId)
        .arg(point.point.x, 0, 'f', 2)
        .arg(point.point.y, 0, 'f', 2));
}

void MainWindowConstructedGeometryModule::createLineCircleIntersection(const CameraConfig& camera,
                                                                       const QString& lineId,
                                                                       const QString& circleId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const LineGeometry* line = findConstructedGeometryLineSource(set, lineId);
  const CircleGeometry* circle = findConstructedGeometryCircleSource(set.circles, circleId);
  if (!line || !circle)
  {
    log(QString("%1: %2").arg(tr("log.constructedLineCircleIntersectionMissing"), camera.id));
    return;
  }

  const QVector<PointGeometry> points = ConstructedGeometryMath::lineCircleIntersections(*line, *circle);
  if (points.isEmpty())
  {
    log(QString("%1: %2").arg(tr("log.constructedLineCircleIntersectionNone"), camera.id));
    return;
  }

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(line->start), toPointF(line->end), QColor("#35c46a"), 5});
  appendCircleOverlay(overlay, *circle);

  for (int i = 0; i < points.size(); ++i)
  {
    ConstructedPointGeometry constructed;
    constructed.point = points[i];
    constructed.sourceAId = line->meta.id;
    constructed.sourceBId = circle->meta.id;
    set.constructedPoints.append(constructed);
    overlay.points.append({QPointF(points[i].point.x, points[i].point.y), QString("X%1").arg(i + 1), QColor("#ff8a00")});
  }
  saveConstructedGeometryRecipeAction(camera, "line_circle_intersection", line->meta.id, circle->meta.id);

  largeImage()->setGeometryOverlay(overlay);
  log(QString("%1: %2 %3/%4 punti=%5")
        .arg(tr("log.constructedLineCircleIntersectionCreated"))
        .arg(camera.id)
        .arg(line->meta.id)
        .arg(circle->meta.id)
        .arg(points.size()));
}

void MainWindowConstructedGeometryModule::createCircleCircleIntersection(const CameraConfig& camera,
                                                                         const QString& firstCircleId,
                                                                         const QString& secondCircleId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const CircleGeometry* firstCircle = findConstructedGeometryCircleSource(set.circles, firstCircleId);
  const CircleGeometry* secondCircle = findConstructedGeometryCircleSource(set.circles, secondCircleId);
  if (!firstCircle || !secondCircle || firstCircleId == secondCircleId)
  {
    log(QString("%1: %2").arg(tr("log.constructedCircleIntersectionMissing"), camera.id));
    return;
  }

  const QVector<PointGeometry> points = ConstructedGeometryMath::circleCircleIntersections(*firstCircle, *secondCircle);
  if (points.isEmpty())
  {
    log(QString("%1: %2").arg(tr("log.constructedCircleIntersectionNone"), camera.id));
    return;
  }

  GeometryOverlay overlay;
  appendCircleOverlay(overlay, *firstCircle);
  appendCircleOverlay(overlay, *secondCircle);

  for (int i = 0; i < points.size(); ++i)
  {
    ConstructedPointGeometry constructed;
    constructed.point = points[i];
    constructed.sourceAId = firstCircle->meta.id;
    constructed.sourceBId = secondCircle->meta.id;
    set.constructedPoints.append(constructed);
    overlay.points.append({QPointF(points[i].point.x, points[i].point.y), QString("X%1").arg(i + 1), QColor("#ff8a00")});
  }
  saveConstructedGeometryRecipeAction(camera, "circle_circle_intersection", firstCircle->meta.id, secondCircle->meta.id);

  largeImage()->setGeometryOverlay(overlay);
  log(QString("%1: %2 %3/%4 punti=%5")
        .arg(tr("log.constructedCircleIntersectionCreated"))
        .arg(camera.id)
        .arg(firstCircle->meta.id)
        .arg(secondCircle->meta.id)
        .arg(points.size()));
}

void MainWindowConstructedGeometryModule::createMidpoint(const CameraConfig& camera,
                                                         const QString& firstPointId,
                                                         const QString& secondPointId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const PointGeometry* firstPoint = findConstructedGeometryPointSource(set, firstPointId);
  const PointGeometry* secondPoint = findConstructedGeometryPointSource(set, secondPointId);
  if (!firstPoint || !secondPoint || firstPointId == secondPointId)
  {
    log(QString("%1: %2").arg(tr("log.constructedMidpointMissing"), camera.id));
    return;
  }

  const PointGeometry point = ConstructedGeometryMath::midpoint(*firstPoint, *secondPoint);
  ConstructedPointGeometry constructed;
  constructed.point = point;
  constructed.sourceAId = firstPoint->meta.id;
  constructed.sourceBId = secondPoint->meta.id;
  set.constructedPoints.append(constructed);
  saveConstructedGeometryRecipeAction(camera, "midpoint", constructed.sourceAId, constructed.sourceBId);

  GeometryOverlay overlay;
  overlay.points.append({QPointF(firstPoint->point.x, firstPoint->point.y), "A", QColor("#35c46a")});
  overlay.points.append({QPointF(secondPoint->point.x, secondPoint->point.y), "B", QColor("#35c46a")});
  overlay.lines.append({toPointF(firstPoint->point), toPointF(secondPoint->point), QColor("#35c46a"), 2});
  overlay.points.append({QPointF(point.point.x, point.point.y), "M", QColor("#ff8a00")});
  largeImage()->setGeometryOverlay(overlay);

  log(QString("%1: %2 %3/%4 x=%5 y=%6")
        .arg(tr("log.constructedMidpointCreated"))
        .arg(camera.id)
        .arg(constructed.sourceAId)
        .arg(constructed.sourceBId)
        .arg(point.point.x, 0, 'f', 2)
        .arg(point.point.y, 0, 'f', 2));
}

void MainWindowConstructedGeometryModule::createOffsetLine(const CameraConfig& camera,
                                                           const QString& lineId,
                                                           double offset)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const LineGeometry* source = findConstructedGeometryLineSource(set, lineId);
  if (!source)
  {
    log(QString("%1: %2").arg(tr("log.constructedOffsetLineMissing"), camera.id));
    return;
  }

  LineGeometry line;
  if (!ConstructedGeometryMath::offsetLine(*source, offset, line))
  {
    log(QString("%1: %2").arg(tr("log.constructedOffsetLineInvalid"), camera.id));
    return;
  }

  ConstructedLineGeometry constructed;
  constructed.line = line;
  constructed.sourceAId = source->meta.id;
  constructed.offset = offset;
  set.constructedLines.append(constructed);
  saveConstructedGeometryRecipeAction(camera, "offset_line", constructed.sourceAId, {}, offset);

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(source->start), toPointF(source->end), QColor("#35c46a"), 3});
  overlay.lines.append({toPointF(line.start), toPointF(line.end), QColor("#ff8a00"), 5});
  largeImage()->setGeometryOverlay(overlay);

  log(QString("%1: %2 %3 offset=%4")
        .arg(tr("log.constructedOffsetLineCreated"))
        .arg(camera.id)
        .arg(constructed.sourceAId)
        .arg(offset, 0, 'f', 2));
}

void MainWindowConstructedGeometryModule::createAngleBisectors(const CameraConfig& camera,
                                                               const QString& firstLineId,
                                                               const QString& secondLineId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const LineGeometry* firstLine = findConstructedGeometryLineSource(set, firstLineId);
  const LineGeometry* secondLine = findConstructedGeometryLineSource(set, secondLineId);
  if (!firstLine || !secondLine || firstLineId == secondLineId)
  {
    log(QString("%1: %2").arg(tr("log.constructedBisectorMissing"), camera.id));
    return;
  }

  const QVector<LineGeometry> lines = ConstructedGeometryMath::angleBisectors(*firstLine, *secondLine);
  if (lines.isEmpty())
  {
    log(QString("%1: %2").arg(tr("log.constructedBisectorInvalid"), camera.id));
    return;
  }

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(firstLine->start), toPointF(firstLine->end), QColor("#35c46a"), 3});
  overlay.lines.append({toPointF(secondLine->start), toPointF(secondLine->end), QColor("#35c46a"), 3});

  for (const LineGeometry& line : lines)
  {
    ConstructedLineGeometry constructed;
    constructed.line = line;
    constructed.sourceAId = firstLine->meta.id;
    constructed.sourceBId = secondLine->meta.id;
    set.constructedLines.append(constructed);
    overlay.lines.append({toPointF(line.start), toPointF(line.end), QColor("#ff8a00"), 5});
  }
  saveConstructedGeometryRecipeAction(camera, "angle_bisector", firstLine->meta.id, secondLine->meta.id);

  largeImage()->setGeometryOverlay(overlay);
  log(QString("%1: %2 %3/%4 linee=%5")
        .arg(tr("log.constructedBisectorCreated"))
        .arg(camera.id)
        .arg(firstLine->meta.id)
        .arg(secondLine->meta.id)
        .arg(lines.size()));
}

void MainWindowConstructedGeometryModule::createTangentLines(const CameraConfig& camera,
                                                             const QString& pointId,
                                                             const QString& circleId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const PointGeometry* point = findConstructedGeometryPointSource(set, pointId);
  const CircleGeometry* circle = findConstructedGeometryCircleSource(set.circles, circleId);
  if (!point || !circle)
  {
    log(QString("%1: %2").arg(tr("log.constructedTangentMissing"), camera.id));
    return;
  }

  const QVector<LineGeometry> lines = ConstructedGeometryMath::tangentLinesFromPointToCircle(*point, *circle);
  if (lines.isEmpty())
  {
    log(QString("%1: %2").arg(tr("log.constructedTangentInvalid"), camera.id));
    return;
  }

  GeometryOverlay overlay;
  appendCircleOverlay(overlay, *circle);
  overlay.points.append({QPointF(point->point.x, point->point.y), "P", QColor("#35c46a")});

  for (const LineGeometry& line : lines)
  {
    ConstructedLineGeometry constructed;
    constructed.line = line;
    constructed.sourceAId = point->meta.id;
    constructed.sourceBId = circle->meta.id;
    set.constructedLines.append(constructed);
    overlay.lines.append({toPointF(line.start), toPointF(line.end), QColor("#ff8a00"), 5});
  }
  saveConstructedGeometryRecipeAction(camera, "tangent_line", point->meta.id, circle->meta.id);

  largeImage()->setGeometryOverlay(overlay);
  log(QString("%1: %2 %3/%4 linee=%5")
        .arg(tr("log.constructedTangentCreated"))
        .arg(camera.id)
        .arg(point->meta.id)
        .arg(circle->meta.id)
        .arg(lines.size()));
}

void MainWindowConstructedGeometryModule::createProjectedPoint(const CameraConfig& camera,
                                                               const QString& pointId,
                                                               const QString& lineId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const PointGeometry* sourcePoint = findConstructedGeometryPointSource(set, pointId);
  const LineGeometry* sourceLine = findConstructedGeometryLineSource(set, lineId);
  if (!sourcePoint || !sourceLine)
  {
    log(QString("%1: %2").arg(tr("log.constructedProjectionMissing"), camera.id));
    return;
  }

  PointGeometry point;
  if (!ConstructedGeometryMath::projectPointOntoLine(*sourcePoint, *sourceLine, point))
  {
    log(QString("%1: %2").arg(tr("log.constructedProjectionInvalid"), camera.id));
    return;
  }

  ConstructedPointGeometry constructed;
  constructed.point = point;
  constructed.sourceAId = sourcePoint->meta.id;
  constructed.sourceBId = sourceLine->meta.id;
  set.constructedPoints.append(constructed);
  saveConstructedGeometryRecipeAction(camera, "project_point_line", constructed.sourceAId, constructed.sourceBId);

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(sourceLine->start), toPointF(sourceLine->end), QColor("#35c46a"), 3});
  overlay.lines.append({toPointF(sourcePoint->point), toPointF(point.point), QColor("#ff8a00"), 2});
  overlay.points.append({QPointF(sourcePoint->point.x, sourcePoint->point.y), "P", QColor("#35c46a")});
  overlay.points.append({QPointF(point.point.x, point.point.y), "H", QColor("#ff8a00")});
  largeImage()->setGeometryOverlay(overlay);

  log(QString("%1: %2 %3/%4 x=%5 y=%6")
        .arg(tr("log.constructedProjectionCreated"))
        .arg(camera.id)
        .arg(sourcePoint->meta.id)
        .arg(sourceLine->meta.id)
        .arg(point.point.x, 0, 'f', 2)
        .arg(point.point.y, 0, 'f', 2));
}
