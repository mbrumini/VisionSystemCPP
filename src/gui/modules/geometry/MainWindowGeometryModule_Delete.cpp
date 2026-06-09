#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowContext.h"

namespace
{
template <typename Geometry>
void removeGeometryResultById(QVector<Geometry>& geometries, const QString& id)
{
  for (int i = geometries.size() - 1; i >= 0; --i)
  {
    if (geometries[i].meta.id == id)
    {
      geometries.removeAt(i);
    }
  }
}

void removeMeasurementResultsBySource(GeometrySet& geometries, const QString& id)
{
  for (int i = geometries.measurements.size() - 1; i >= 0; --i)
  {
    const MeasurementResult& measurement = geometries.measurements[i];
    if (measurement.sourceAId == id || measurement.sourceBId == id)
    {
      geometries.measurements.removeAt(i);
    }
  }
}
}

void MainWindowGeometryModule::removeMeasurementsForDeletedGeometry(const CameraConfig& camera, const QString& geometryId)
{
  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  removeMeasurementResultsBySource(geometries, geometryId);

  QVector<MeasurementRecipeConfig> measurements = recipes().loadMeasurements(camera.id);
  for (int i = measurements.size() - 1; i >= 0; --i)
  {
    if (measurements[i].sourceAId == geometryId || measurements[i].sourceBId == geometryId)
    {
      measurements.removeAt(i);
    }
  }

  QString error;
  if (!recipes().saveMeasurements(camera.id, measurements, &error))
  {
    log(error);
  }

  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
}

void MainWindowGeometryModule::removeActiveGeometryLine(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_lineConfigs[camera.id];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
    m_activeLineIndexes[camera.id] = 0;
    showGeometryLinePanel(camera);
    return;
  }

  const int index = qBound(0, m_activeLineIndexes.value(camera.id, 0), lines.size() - 1);
  const QString removedId = lines[index].id;
  lines.removeAt(index);
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
  }

  m_activeLineIndexes[camera.id] = qBound(0, index, lines.size() - 1);
  m_lineMouseControllers[camera.id].begin(activeGeometryLineConfig(camera.id).bandHalfWidth);

  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  removeGeometryResultById(geometries.lines, removedId);
  saveGeometryLinesRecipe(camera);
  removeMeasurementsForDeletedGeometry(camera, removedId);
  updateGeometryLineOverlay(camera);
  showGeometryLinePanel(camera);
}

void MainWindowGeometryModule::removeActiveGeometryPoint(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_pointConfigs[camera.id];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
    m_activePointIndexes[camera.id] = 0;
    showGeometryPointPanel(camera);
    return;
  }

  const int index = qBound(0, m_activePointIndexes.value(camera.id, 0), points.size() - 1);
  const QString removedId = points[index].id;
  points.removeAt(index);
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }

  m_activePointIndexes[camera.id] = qBound(0, index, points.size() - 1);
  m_pointMouseControllers[camera.id].begin(3.0);

  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  removeGeometryResultById(geometries.points, removedId);
  saveGeometryPointRecipe(camera);
  removeMeasurementsForDeletedGeometry(camera, removedId);
  updateGeometryPointOverlay(camera);
  showGeometryPointPanel(camera);
}

void MainWindowGeometryModule::removeActiveGeometryCircle(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_circleConfigs[camera.id];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
    m_activeCircleIndexes[camera.id] = 0;
    showGeometryCirclePanel(camera);
    return;
  }

  const int index = qBound(0, m_activeCircleIndexes.value(camera.id, 0), circles.size() - 1);
  const QString removedId = circles[index].id;
  circles.removeAt(index);
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }

  m_activeCircleIndexes[camera.id] = qBound(0, index, circles.size() - 1);

  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  removeGeometryResultById(geometries.circles, removedId);
  saveGeometryCirclesRecipe(camera);
  removeMeasurementsForDeletedGeometry(camera, removedId);
  showGeometryCirclePanel(camera);
}

void MainWindowGeometryModule::removeActiveGeometryArc(const CameraConfig& camera)
{
  QVector<GeometryArcRuntimeConfig>& arcs = m_arcConfigs[camera.id];
  if (arcs.isEmpty())
  {
    GeometryArcRuntimeConfig arc;
    arc.id = "arc_1";
    arcs.append(arc);
    m_activeArcIndexes[camera.id] = 0;
    showGeometryArcPanel(camera);
    return;
  }

  const int index = qBound(0, m_activeArcIndexes.value(camera.id, 0), arcs.size() - 1);
  const QString removedId = arcs[index].id;
  arcs.removeAt(index);
  if (arcs.isEmpty())
  {
    GeometryArcRuntimeConfig arc;
    arc.id = "arc_1";
    arcs.append(arc);
  }

  m_activeArcIndexes[camera.id] = qBound(0, index, arcs.size() - 1);

  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  removeGeometryResultById(geometries.arcs, removedId);
  saveGeometryArcsRecipe(camera);
  removeMeasurementsForDeletedGeometry(camera, removedId);
  showGeometryArcPanel(camera);
}
