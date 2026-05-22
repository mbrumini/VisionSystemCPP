#include "gui/modules/MainWindowConstructedGeometryModule.h"

#include "geometry/ConstructedGeometryMath.h"
#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/ConstructedGeometryCircleSource.h"
#include "gui/modules/ConstructedGeometryLineSource.h"
#include "runtime/CameraRuntime.h"

#include <cmath>

#include <QColor>
#include <QComboBox>
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
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(geometrySet.lines);
  const QVector<ConstructedGeometryCircleSource> circleSources = constructedGeometryCircleSources(geometrySet.circles);

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

  const QVector<QPair<QString, QString>> constructedActions = {
    {"midpoint", "actions.midpoint"},
    {"offsetLine", "actions.offsetLine"},
    {"angleBisector", "actions.angleBisector"},
    {"tangentLine", "actions.tangentLine"},
    {"projectPoint", "actions.projectPoint"}
  };

  for (int i = 0; i < constructedActions.size(); ++i)
  {
    const QString actionId = constructedActions[i].first;
    const QString label = tr(constructedActions[i].second);
    auto* button = new QPushButton(label, actions);
    button->setMinimumHeight(38);
    actionsLayout->addWidget(button, 9 + (i / 2), i % 2);
    QObject::connect(button, &QPushButton::clicked, window(), [this, camera, actionId, label]() {
      log(QString("%1: %2 -> %3").arg(tr("log.constructedGeometryAction"), camera.id, label));
    });
  }

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
  const LineGeometry* firstLine = findConstructedGeometryLineSource(set.lines, firstLineId);
  const LineGeometry* secondLine = findConstructedGeometryLineSource(set.lines, secondLineId);
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
  const LineGeometry* line = findConstructedGeometryLineSource(set.lines, lineId);
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

  largeImage()->setGeometryOverlay(overlay);
  log(QString("%1: %2 %3/%4 punti=%5")
        .arg(tr("log.constructedCircleIntersectionCreated"))
        .arg(camera.id)
        .arg(firstCircle->meta.id)
        .arg(secondCircle->meta.id)
        .arg(points.size()));
}
