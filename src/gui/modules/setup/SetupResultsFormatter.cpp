#include "gui/modules/setup/SetupResultsFormatter.h"

#include "gui/modules/MainWindowContext.h"
#include "geometry/GeometrySet.h"
#include "measurement/MeasurementGeometry.h"

#include <QSet>
#include <QStringList>
#include <QVector>

namespace
{
template <typename T>
bool containsGeometryId(const QVector<T>& geometries, const QString& id)
{
  for (const T& geometry : geometries)
  {
    if (geometry.meta.id == id)
    {
      return true;
    }
  }
  return false;
}

QString yesNoStatus(bool found, const MainWindowContext& context)
{
  return found ? context.trText("status.found") : context.trText("status.notFound");
}

QString measurementTypeLabel(const QString& type, const MainWindowContext& context)
{
  if (type == "point_point_distance")
  {
    return context.trText("actions.pointPointDistance");
  }
  if (type == "point_line_distance")
  {
    return context.trText("actions.pointLineDistance");
  }
  if (type == "line_line_distance")
  {
    return context.trText("actions.lineLineDistance");
  }
  if (type == "line_line_distance_min")
  {
    return context.trText("actions.lineLineDistance") + " MIN";
  }
  if (type == "line_line_distance_max")
  {
    return context.trText("actions.lineLineDistance") + " MAX";
  }
  if (type == "circle_diameter")
  {
    return context.trText("actions.circleDiameterMeasure");
  }
  if (type == "line_line_angle")
  {
    return context.trText("actions.lineLineAngle");
  }
  return type;
}

QString measurementKey(const MeasurementRecipeConfig& config)
{
  return QString("%1|%2|%3").arg(config.type, config.sourceAId, config.sourceBId);
}
}

QString setupResultsText(const CameraConfig& camera,
                         const CameraRuntime* runtime,
                         const RecipeManager& recipes,
                         const MainWindowContext& context)
{
  if (!runtime)
  {
    return context.trText("status.invalid");
  }

  const GeometrySet& set = runtime->geometries();
  QStringList lines;
  lines << QString("%1: %2").arg(context.trText("labels.camera")).arg(camera.id);
  lines << QString("%1: %2").arg(context.trText("labels.frame")).arg(runtime->frameIndex());
  lines << QString();
  lines << context.trText("tools.geometries");

  bool hasConfiguredGeometry = false;
  for (const GeometryPointRecipeConfig& point : recipes.loadGeometryPoints(camera.id))
  {
    if (!point.enabled)
    {
      continue;
    }
    hasConfiguredGeometry = true;
    lines << QString("  %1  %2").arg(point.id, yesNoStatus(containsGeometryId(set.points, point.id), context));
  }
  for (const GeometryLineRecipeConfig& line : recipes.loadGeometryLines(camera.id))
  {
    if (!line.enabled)
    {
      continue;
    }
    hasConfiguredGeometry = true;
    lines << QString("  %1  %2").arg(line.id, yesNoStatus(containsGeometryId(set.lines, line.id), context));
  }
  for (const GeometryCircleRecipeConfig& circle : recipes.loadGeometryCircles(camera.id))
  {
    if (!circle.enabled)
    {
      continue;
    }
    hasConfiguredGeometry = true;
    lines << QString("  %1  %2").arg(circle.id, yesNoStatus(containsGeometryId(set.circles, circle.id), context));
  }
  for (const GeometryArcRecipeConfig& arc : recipes.loadGeometryArcs(camera.id))
  {
    if (!arc.enabled)
    {
      continue;
    }
    hasConfiguredGeometry = true;
    lines << QString("  %1  %2").arg(arc.id, yesNoStatus(containsGeometryId(set.arcs, arc.id), context));
  }
  if (!hasConfiguredGeometry)
  {
    lines << QString("  %1").arg(context.trText("status.invalid"));
  }

  lines << QString();
  lines << context.trText("tools.measurements");
  const QVector<MeasurementRecipeConfig> measurementConfigs = recipes.loadMeasurements(camera.id);
  if (measurementConfigs.isEmpty())
  {
    lines << QString("  %1").arg(context.trText("status.invalid"));
  }
  QSet<QString> shownMeasurementKeys;
  for (const MeasurementRecipeConfig& measurementConfig : measurementConfigs)
  {
    if (!measurementConfig.enabled)
    {
      continue;
    }
    const QString key = measurementKey(measurementConfig);
    if (shownMeasurementKeys.contains(key))
    {
      continue;
    }
    shownMeasurementKeys.insert(key);

    const MeasurementResult* result = nullptr;
    for (const MeasurementResult& measurement : set.measurements)
    {
      if (measurement.id == measurementConfig.id)
      {
        result = &measurement;
        break;
      }
    }

    const QString label = measurementConfig.id.isEmpty()
      ? measurementTypeLabel(measurementConfig.type, context)
      : measurementConfig.id;
    if (!result || !result->valid)
    {
      lines << QString("  %1  %2").arg(label, context.trText("status.notAvailable"));
      continue;
    }

    if (result->hasRealValue && result->unit != "deg")
    {
      lines << QString("  %1  %2 %3 (%4 px)")
                 .arg(label)
                 .arg(result->valueReal, 0, 'f', 3)
                 .arg(result->unit)
                 .arg(result->valuePixels, 0, 'f', 3);
    }
    else
    {
      const QString unit =
        (result->type == "line_line_angle" || result->type == "thread_phase") ? "deg" : "px";
      lines << QString("  %1  %2 %3").arg(label).arg(result->valuePixels, 0, 'f', 3).arg(unit);
    }
  }

  return lines.join('\n');
}
