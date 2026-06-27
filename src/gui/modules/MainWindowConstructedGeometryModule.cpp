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

bool findCircleLikeSource(const GeometrySet& set, const QString& id, CircleGeometry& circle)
{
  return constructedGeometryCircleSourceValue(set, id, circle);
}

MainWindowConstructedGeometryModule::MainWindowConstructedGeometryModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
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
  CircleGeometry circle;
  if (!line || !findCircleLikeSource(set, circleId, circle))
  {
    log(QString("%1: %2").arg(tr("log.constructedLineCircleIntersectionMissing"), camera.id));
    return;
  }

  const QVector<PointGeometry> points = ConstructedGeometryMath::lineCircleIntersections(*line, circle);
  if (points.isEmpty())
  {
    log(QString("%1: %2").arg(tr("log.constructedLineCircleIntersectionNone"), camera.id));
    return;
  }

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(line->start), toPointF(line->end), QColor("#35c46a"), 5});
  appendCircleOverlay(overlay, circle);

  for (int i = 0; i < points.size(); ++i)
  {
    ConstructedPointGeometry constructed;
    constructed.point = points[i];
    constructed.sourceAId = line->meta.id;
    constructed.sourceBId = circle.meta.id;
    set.constructedPoints.append(constructed);
    overlay.points.append({QPointF(points[i].point.x, points[i].point.y), QString("X%1").arg(i + 1), QColor("#ff8a00")});
  }
  saveConstructedGeometryRecipeAction(camera, "line_circle_intersection", line->meta.id, circle.meta.id);

  largeImage()->setGeometryOverlay(overlay);
  log(QString("%1: %2 %3/%4 punti=%5")
        .arg(tr("log.constructedLineCircleIntersectionCreated"))
        .arg(camera.id)
        .arg(line->meta.id)
        .arg(circle.meta.id)
        .arg(points.size()));
}

void MainWindowConstructedGeometryModule::createCircleCircleIntersection(const CameraConfig& camera,
                                                                         const QString& firstCircleId,
                                                                         const QString& secondCircleId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  CircleGeometry firstCircle;
  CircleGeometry secondCircle;
  if (!findCircleLikeSource(set, firstCircleId, firstCircle) ||
      !findCircleLikeSource(set, secondCircleId, secondCircle) ||
      firstCircleId == secondCircleId)
  {
    log(QString("%1: %2").arg(tr("log.constructedCircleIntersectionMissing"), camera.id));
    return;
  }

  const QVector<PointGeometry> points = ConstructedGeometryMath::circleCircleIntersections(firstCircle, secondCircle);
  if (points.isEmpty())
  {
    log(QString("%1: %2").arg(tr("log.constructedCircleIntersectionNone"), camera.id));
    return;
  }

  GeometryOverlay overlay;
  appendCircleOverlay(overlay, firstCircle);
  appendCircleOverlay(overlay, secondCircle);

  for (int i = 0; i < points.size(); ++i)
  {
    ConstructedPointGeometry constructed;
    constructed.point = points[i];
    constructed.sourceAId = firstCircle.meta.id;
    constructed.sourceBId = secondCircle.meta.id;
    set.constructedPoints.append(constructed);
    overlay.points.append({QPointF(points[i].point.x, points[i].point.y), QString("X%1").arg(i + 1), QColor("#ff8a00")});
  }
  saveConstructedGeometryRecipeAction(camera, "circle_circle_intersection", firstCircle.meta.id, secondCircle.meta.id);

  largeImage()->setGeometryOverlay(overlay);
  log(QString("%1: %2 %3/%4 punti=%5")
        .arg(tr("log.constructedCircleIntersectionCreated"))
        .arg(camera.id)
        .arg(firstCircle.meta.id)
        .arg(secondCircle.meta.id)
        .arg(points.size()));
}

void MainWindowConstructedGeometryModule::createCircleCenter(const CameraConfig& camera, const QString& circleId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  CircleGeometry circle;
  if (!findCircleLikeSource(set, circleId, circle))
  {
    log(QString("%1: %2").arg(tr("log.constructedCircleCenterMissing"), camera.id));
    return;
  }

  const PointGeometry point = ConstructedGeometryMath::circleCenter(circle);
  ConstructedPointGeometry constructed;
  constructed.point = point;
  constructed.sourceAId = circle.meta.id;
  set.constructedPoints.append(constructed);
  saveConstructedGeometryRecipeAction(camera, "circle_center", constructed.sourceAId);

  GeometryOverlay overlay;
  appendCircleOverlay(overlay, circle);
  overlay.points.append({QPointF(point.point.x, point.point.y), "C", QColor("#ff8a00")});
  largeImage()->setGeometryOverlay(overlay);

  log(QString("%1: %2 %3 x=%4 y=%5")
        .arg(tr("log.constructedCircleCenterCreated"))
        .arg(camera.id)
        .arg(constructed.sourceAId)
        .arg(point.point.x, 0, 'f', 2)
        .arg(point.point.y, 0, 'f', 2));
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

void MainWindowConstructedGeometryModule::createMidline(const CameraConfig& camera,
                                                        const QString& firstLineId,
                                                        const QString& secondLineId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const LineGeometry* firstLine = findConstructedGeometryLineSource(set, firstLineId);
  const LineGeometry* secondLine = findConstructedGeometryLineSource(set, secondLineId);
  if (!firstLine || !secondLine || firstLine == secondLine)
  {
    log(QString("%1: %2").arg(tr("log.constructedMidlineMissing"), camera.id));
    return;
  }

  LineGeometry line;
  if (!ConstructedGeometryMath::midlineBetweenLines(*firstLine, *secondLine, line))
  {
    log(QString("%1: %2").arg(tr("log.constructedMidlineInvalid"), camera.id));
    return;
  }

  ConstructedLineGeometry constructed;
  constructed.line = line;
  constructed.sourceAId = firstLine->meta.id;
  constructed.sourceBId = secondLine->meta.id;
  set.constructedLines.append(constructed);
  saveConstructedGeometryRecipeAction(camera, "midline_between_lines", constructed.sourceAId, constructed.sourceBId);

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(firstLine->start), toPointF(firstLine->end), QColor("#35c46a"), 3});
  overlay.lines.append({toPointF(secondLine->start), toPointF(secondLine->end), QColor("#35c46a"), 3});
  overlay.lines.append({toPointF(line.start), toPointF(line.end), QColor("#ff8a00"), 5});
  largeImage()->setGeometryOverlay(overlay);

  log(QString("%1: %2 %3/%4")
        .arg(tr("log.constructedMidlineCreated"))
        .arg(camera.id)
        .arg(constructed.sourceAId)
        .arg(constructed.sourceBId));
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
  CircleGeometry circle;
  if (!point || !findCircleLikeSource(set, circleId, circle))
  {
    log(QString("%1: %2").arg(tr("log.constructedTangentMissing"), camera.id));
    return;
  }

  const QVector<LineGeometry> lines = ConstructedGeometryMath::tangentLinesFromPointToCircle(*point, circle);
  if (lines.isEmpty())
  {
    log(QString("%1: %2").arg(tr("log.constructedTangentInvalid"), camera.id));
    return;
  }

  GeometryOverlay overlay;
  appendCircleOverlay(overlay, circle);
  overlay.points.append({QPointF(point->point.x, point->point.y), "P", QColor("#35c46a")});

  for (const LineGeometry& line : lines)
  {
    ConstructedLineGeometry constructed;
    constructed.line = line;
    constructed.sourceAId = point->meta.id;
    constructed.sourceBId = circle.meta.id;
    set.constructedLines.append(constructed);
    overlay.lines.append({toPointF(line.start), toPointF(line.end), QColor("#ff8a00"), 5});
  }
  saveConstructedGeometryRecipeAction(camera, "tangent_line", point->meta.id, circle.meta.id);

  largeImage()->setGeometryOverlay(overlay);
  log(QString("%1: %2 %3/%4 linee=%5")
        .arg(tr("log.constructedTangentCreated"))
        .arg(camera.id)
        .arg(point->meta.id)
        .arg(circle.meta.id)
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
