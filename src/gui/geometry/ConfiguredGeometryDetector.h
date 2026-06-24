#pragma once

#include "gui/geometry/GeometryOverlay.h"
#include "gui/geometry/GeometryRuntimeConfig.h"
#include "geometry/ArcGeometry.h"
#include "geometry/CircleGeometry.h"
#include "geometry/LineGeometry.h"
#include "geometry/PointGeometry.h"
#include "runtime/PartPose.h"

#include <QSize>
#include <QVector>

#include <opencv2/core.hpp>

struct ConfiguredGeometryDetectInput
{
  cv::Mat image;
  PartPose pose;
  bool useSubpixelDimensional = false;
  bool buildDiagnostic = false;
  bool buildGuideOverlay = false;
  QSize guideReferenceSize;
  QVector<GeometryLineRuntimeConfig> lines;
  QVector<GeometryPointRuntimeConfig> points;
  QVector<GeometryCircleRuntimeConfig> circles;
  QVector<GeometryArcRuntimeConfig> arcs;
};

struct ConfiguredGeometryDetectOutput
{
  QVector<LineGeometry> lines;
  QVector<PointGeometry> points;
  QVector<CircleGeometry> circles;
  QVector<ArcGeometry> arcs;
  cv::Mat diagnostic;
  GeometryOverlay guideOverlay;
};

ConfiguredGeometryDetectOutput detectConfiguredGeometries(const ConfiguredGeometryDetectInput& input);
