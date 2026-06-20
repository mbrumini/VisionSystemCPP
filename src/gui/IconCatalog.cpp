#include "gui/IconCatalog.h"

#include <QColor>
#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QApplication>

namespace
{
QColor iconColor(const QString& id)
{
  static const QColor acquisition("#4d9cff");
  static const QColor geometry("#00d2ff");
  static const QColor constructed("#ff9f3f");
  static const QColor measurement("#f5d547");
  static const QColor tolerance("#35c46a");
  static const QColor danger("#ff4f5e");
  static const QColor diagnostic("#b58cff");
  static const QColor neutral("#eef2f6");

  if (id == "delete" || id == "clear" || id == "clearRoi" ||
      id == "surfaceAddExclusion" || id == "surfaceClearExclusions")
  {
    return danger;
  }

  if (id == "setup" || id == "camera" || id == "acquireSample" ||
      id == "aiSample" || id == "aiTrain" ||
      id == "assignSampleImage" || id == "assignTestImages" ||
      id == "assignImageFolder" || id == "nextFrame" ||
      id == "start" || id == "stop" || id == "reload" ||
      id == "grid" || id == "fullscreen" || id == "back" ||
      id == "new" || id == "testGeometry" || id == "testLocalization" ||
      id == "testStrategy")
  {
    return acquisition;
  }

  if (id == "pointGeometry" || id == "lineGeometry" ||
      id == "circleGeometry" || id == "arcGeometry" ||
      id == "edgeGeometry" || id == "contourGeometry" ||
      id == "geometries" || id == "surfaceLocalization" ||
      id == "surfaceThreshold" || id == "surfaceEdge" ||
      id == "surfaceCircleThreshold" || id == "surfaceCircleEdge" ||
      id == "surfacePca" || id == "surfaceSearchRoi" ||
      id == "surfaceOuterCircle" || id == "surfaceInnerCircle" ||
      id == "surfaceEdgeCircle")
  {
    return geometry;
  }

  if (id == "constructedGeometries" || id == "lineLineIntersection" ||
      id == "lineCircleIntersection" || id == "circleCircleIntersection" ||
      id == "circleCenter" || id == "midpoint" || id == "offsetLine" ||
      id == "angleBisector" || id == "tangentLine" ||
      id == "projectPoint")
  {
    return constructed;
  }

  if (id == "measurements" || id == "dimensions" ||
      id == "diameter" || id == "length" || id == "radius" ||
      id == "distance" || id == "angle" ||
      id == "pointPointDistance" || id == "pointLineDistance" ||
      id == "lineLineDistance" || id == "circleDiameterMeasure" ||
      id == "lineLineAngle")
  {
    return measurement;
  }

  if (id == "tolerances" || id == "nominal" ||
      id == "minTolerance" || id == "maxTolerance" ||
      id == "okNokRule")
  {
    return tolerance;
  }

  if (id == "help" ||
      id == "configureStrategy" || id == "previewModel" ||
      id == "acquireModel" || id == "testShapeModel" ||
      id == "testTemplateModel" || id == "surfaceModel" ||
      id == "ai" || id == "aiModel" || id == "aiLocalization" || id == "aiClassification" ||
      id == "aiAnomaly" || id == "aiSegmentation" ||
      id == "confidence" ||
      id == "classes" || id == "datasetCapture" ||
      id == "defectMap" || id == "lighting" ||
      id == "contrast" || id == "calibration" ||
      id == "roi" || id == "threshold" ||
      id == "surfaceDefects")
  {
    return diagnostic;
  }

  return neutral;
}

QIcon tintedIcon(const QString& path, const QColor& color)
{
  const QIcon source(path);
  if (!color.isValid())
  {
    return source;
  }

  QIcon icon;
  const QList<QSize> sizes = {QSize(24, 24), QSize(32, 32), QSize(48, 48), QSize(64, 64)};
  for (const QSize& size : sizes)
  {
    QPixmap pixmap = source.pixmap(size);
    if (pixmap.isNull())
    {
      continue;
    }

    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();
    icon.addPixmap(pixmap);
  }

  return icon.isNull() ? source : icon;
}
}

QIcon IconCatalog::icon(const QString& id)
{
  static const QHash<QString, QString> paths = {
    {"start", ":/icons/play.svg"},
    {"stop", ":/icons/stop.svg"},
    {"grid", ":/icons/grid.svg"},
    {"reload", ":/icons/refresh.svg"},
    {"fullscreen", ":/icons/maximize.svg"},
    {"help", ":/icons/help.svg"},
    {"setup", ":/icons/tool-setup.svg"},
    {"localization", ":/icons/crosshair.svg"},
    {"geometries", ":/icons/shapes.svg"},
    {"constructedGeometries", ":/icons/combine.svg"},
    {"measurements", ":/icons/ruler.svg"},
    {"dimensions", ":/icons/ruler.svg"},
    {"camera", ":/icons/tool-camera.svg"},
    {"threshold", ":/icons/contrast.svg"},
    {"calibration", ":/icons/scale.svg"},
    {"roi", ":/icons/scan.svg"},
    {"polygon", ":/icons/shapes.svg"},
    {"saveSample", ":/icons/save.svg"},
    {"sampleImage", ":/icons/image.svg"},
    {"aiSample", ":/icons/database.svg"},
    {"tolerances", ":/icons/gauge.svg"},
    {"surfaceLocalization", ":/icons/scan-search.svg"},
    {"surfaceDefects", ":/icons/sparkles.svg"},
    {"surfaceThreshold", ":/icons/surface-threshold.svg"},
    {"surfaceEdge", ":/icons/surface-edge.svg"},
    {"surfaceCircleThreshold", ":/icons/surface-circle-threshold.svg"},
    {"surfaceCircleEdge", ":/icons/surface-circle-edge.svg"},
    {"surfacePca", ":/icons/surface-pca.svg"},
    {"surfaceModel", ":/icons/surface-model.svg"},
    {"surfaceSearchRoi", ":/icons/surface-roi.svg"},
    {"surfaceOuterCircle", ":/icons/surface-threshold.svg"},
    {"surfaceInnerCircle", ":/icons/surface-edge.svg"},
    {"surfaceEdgeCircle", ":/icons/surface-pca.svg"},
    {"surfaceAddExclusion", ":/icons/surface-exclusion.svg"},
    {"surfaceClearExclusions", ":/icons/surface-clear-exclusions.svg"},
    {"lighting", ":/icons/lightbulb.svg"},
    {"contrast", ":/icons/contrast.svg"},
    {"defectMap", ":/icons/map.svg"},
    {"aiModel", ":/icons/tool-ai.svg"},
    {"ai", ":/icons/tool-ai.svg"},
    {"aiClassification", ":/icons/tags.svg"},
    {"aiLocalization", ":/icons/localization.svg"},
    {"aiAnomaly", ":/icons/sparkles.svg"},
    {"aiSegmentation", ":/icons/shapes.svg"},
    {"aiTrain", ":/icons/play.svg"},
    {"confidence", ":/icons/shield.svg"},
    {"classes", ":/icons/tags.svg"},
    {"datasetCapture", ":/icons/database.svg"},
    {"back", ":/icons/action-back.svg"}
    ,{"acquireSample", ":/icons/tool-camera.svg"}
    ,{"assignSampleImage", ":/icons/action-import.svg"}
    ,{"assignTestImages", ":/icons/database.svg"}
    ,{"assignImageFolder", ":/icons/action-folder.svg"}
    ,{"nextFrame", ":/icons/action-next-frame.svg"}
    ,{"pointGeometry", ":/icons/geometry-point.svg"}
    ,{"lineGeometry", ":/icons/geometry-line.svg"}
    ,{"circleGeometry", ":/icons/geometry-circle.svg"}
    ,{"arcGeometry", ":/icons/geometry-arc.svg"}
    ,{"edgeGeometry", ":/icons/scan-search.svg"}
    ,{"contourGeometry", ":/icons/shapes.svg"}
    ,{"diameter", ":/icons/measure-diameter.svg"}
    ,{"length", ":/icons/measure-distance.svg"}
    ,{"radius", ":/icons/measure-diameter.svg"}
    ,{"distance", ":/icons/measure-distance.svg"}
    ,{"angle", ":/icons/measure-angle.svg"}
    ,{"new", ":/icons/action-new.svg"}
    ,{"delete", ":/icons/action-delete.svg"}
    ,{"area", ":/icons/shapes.svg"}
    ,{"clear", ":/icons/refresh.svg"}
    ,{"clearRoi", ":/icons/refresh.svg"}
    ,{"clearPolygon", ":/icons/refresh.svg"}
    ,{"testGeometry", ":/icons/play.svg"}
    ,{"testLocalization", ":/icons/play.svg"}
    ,{"testStrategy", ":/icons/play.svg"}
    ,{"configureStrategy", ":/icons/configure.svg"}
    ,{"previewModel", ":/icons/model-preview.svg"}
    ,{"acquireModel", ":/icons/model-acquire.svg"}
    ,{"testShapeModel", ":/icons/model-shape-test.svg"}
    ,{"testTemplateModel", ":/icons/model-template-test.svg"}
    ,{"testStrategy", ":/icons/play.svg"}
    ,{"lineLineIntersection", ":/icons/constructed-line-line.svg"}
    ,{"lineCircleIntersection", ":/icons/constructed-line-circle.svg"}
    ,{"circleCircleIntersection", ":/icons/constructed-circle-circle.svg"}
    ,{"circleCenter", ":/icons/geometry-point.svg"}
    ,{"midpoint", ":/icons/geometry-point.svg"}
    ,{"offsetLine", ":/icons/constructed-offset.svg"}
    ,{"angleBisector", ":/icons/constructed-bisector.svg"}
    ,{"tangentLine", ":/icons/constructed-tangent.svg"}
    ,{"projectPoint", ":/icons/constructed-projection.svg"}
    ,{"pointPointDistance", ":/icons/measure-point-point.svg"}
    ,{"pointLineDistance", ":/icons/measure-point-line.svg"}
    ,{"lineLineDistance", ":/icons/measure-line-line.svg"}
    ,{"circleDiameterMeasure", ":/icons/measure-diameter.svg"}
    ,{"lineLineAngle", ":/icons/measure-angle.svg"}
    ,{"nominal", ":/icons/tolerance-nominal.svg"}
    ,{"minTolerance", ":/icons/tolerance-min.svg"}
    ,{"maxTolerance", ":/icons/tolerance-max.svg"}
    ,{"okNokRule", ":/icons/tolerance-oknok.svg"}
  };

  const QString path = paths.value(id);
  if (!path.isEmpty())
  {
    return tintedIcon(path, iconColor(id));
  }

  return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
}
