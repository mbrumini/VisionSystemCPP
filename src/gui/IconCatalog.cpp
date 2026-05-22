#include "gui/IconCatalog.h"

#include <QHash>
#include <QStyle>
#include <QApplication>

QIcon IconCatalog::icon(const QString& id)
{
  static const QHash<QString, QString> paths = {
    {"start", ":/icons/play.svg"},
    {"stop", ":/icons/stop.svg"},
    {"grid", ":/icons/grid.svg"},
    {"reload", ":/icons/refresh.svg"},
    {"fullscreen", ":/icons/maximize.svg"},
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
    {"saveSample", ":/icons/save.svg"},
    {"tolerances", ":/icons/gauge.svg"},
    {"surfaceLocalization", ":/icons/scan-search.svg"},
    {"surfaceDefects", ":/icons/sparkles.svg"},
    {"surfaceThreshold", ":/icons/surface-threshold.svg"},
    {"surfaceEdge", ":/icons/surface-edge.svg"},
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
    return QIcon(path);
  }

  return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
}
