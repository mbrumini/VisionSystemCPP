#pragma once

#include "config/RecipeManager.h"
#include "gui/geometry/ConfiguredGeometryDetector.h"
#include "geometry/GeometrySet.h"
#include "measurement/MeasurementGeometry.h"

#include <QVector>

struct CameraMeasurementCalibration
{
  bool enabled = false;
  double pixelSizeXMm = 0.0;
  double pixelSizeYMm = 0.0;
};

struct GeometryPipelineInput
{
  ConfiguredGeometryDetectInput geometry;
  QVector<ConstructedGeometryRecipeConfig> constructedConfigs;
  QVector<MeasurementRecipeConfig> measurementConfigs;
  CameraMeasurementCalibration calibration;
};

struct GeometryPipelineOutput
{
  QVector<LineGeometry> lines;
  QVector<PointGeometry> points;
  QVector<CircleGeometry> circles;
  QVector<ArcGeometry> arcs;
  QVector<ConstructedPointGeometry> constructedPoints;
  QVector<ConstructedLineGeometry> constructedLines;
  QVector<MeasurementResult> measurements;
};

GeometryPipelineOutput runGeometryMeasurementPipeline(const GeometryPipelineInput& input);

void rebuildConstructedGeometries(GeometrySet& set, const QVector<ConstructedGeometryRecipeConfig>& configs);

void rebuildMeasurements(GeometrySet& set,
                         const QVector<MeasurementRecipeConfig>& configs,
                         const CameraMeasurementCalibration& calibration);
