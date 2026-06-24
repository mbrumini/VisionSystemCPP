#pragma once

#include "gui/geometry/GeometryRuntimeConfig.h"
#include "gui/geometry/LineGeometryMouseController.h"
#include "gui/modules/MainWindowModuleBase.h"

#include <QHash>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QVector>

#include <functional>

class CameraConfig;
class GeometryOverlay;
struct ConfiguredGeometryDetectOutput;
struct GeometryPipelineOutput;

class MainWindowGeometryModule : public MainWindowModuleBase
{
public:
  enum class DrawingTarget
  {
    None,
    Line,
    Point,
    Circle,
    Arc
  };

  explicit MainWindowGeometryModule(MainWindowContext& context);

  void reloadRecipeState();
  void resetRuntimeGeometryForProduction(const CameraConfig& camera);
  QSize guideReferenceSize(const QString& cameraId) const;
  void updateGuideReferenceSize(const CameraConfig& camera);
  DrawingTarget drawingTarget() const { return m_drawingTarget; }
  void setDrawingTarget(DrawingTarget target) { m_drawingTarget = target; }

  void showGeometryPanel(const CameraConfig& camera);
  void showGeometryPointPanel(const CameraConfig& camera);
  void showGeometryLinePanel(const CameraConfig& camera);
  void showGeometryCirclePanel(const CameraConfig& camera);
  void showGeometryArcPanel(const CameraConfig& camera);

  void loadGeometryPointRecipe(const CameraConfig& camera);
  void saveGeometryPointRecipe(const CameraConfig& camera);
  void addGeometryPoint(const CameraConfig& camera);
  void removeActiveGeometryPoint(const CameraConfig& camera);
  GeometryPointRuntimeConfig& activeGeometryPointConfig(const QString& cameraId);
  const GeometryPointRuntimeConfig& activeGeometryPointConfig(const QString& cameraId) const;

  void loadGeometryLinesRecipe(const CameraConfig& camera);
  void saveGeometryLinesRecipe(const CameraConfig& camera);
  void addGeometryLine(const CameraConfig& camera);
  void removeActiveGeometryLine(const CameraConfig& camera);
  GeometryLineRuntimeConfig& activeGeometryLineConfig(const QString& cameraId);
  const GeometryLineRuntimeConfig& activeGeometryLineConfig(const QString& cameraId) const;
  void activateGeometryLineDrawing(const CameraConfig& camera);
  void handleGeometryLinePoint(const CameraConfig& camera, const QPointF& imagePoint);
  void handleGeometryLineHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint);
  GeometryOverlay configuredGeometryLinesOverlay(const CameraConfig& camera, bool includeActive) const;
  void updateGeometryLineOverlay(const CameraConfig& camera, const GeometryOverlay& extraOverlay = {});
  void restoreCleanGeometryImage(const CameraConfig& camera);
  void showRuntimeGeometryOverlay(const CameraConfig& camera);
  void refreshMeasurementOverlay(const CameraConfig& camera);
  void testGeometryLine(const CameraConfig& camera);
  void testConfiguredGeometryLines(const CameraConfig& camera);
  void testConfiguredGeometryLinesAsync(const CameraConfig& camera, std::function<void()> onComplete = nullptr);

  QHash<QString, QString> geometryAliasMap(const QString& cameraId) const;
  QString geometryDisplayLabel(const QString& cameraId, const QString& geometryId) const;
  void syncRuntimeGeometryLabels(const CameraConfig& camera);

  void activateGeometryPointDrawing(const CameraConfig& camera);
  void handleGeometryPointGuidePoint(const CameraConfig& camera, const QPointF& imagePoint);
  void handleGeometryPointHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint);
  GeometryOverlay configuredGeometryPointsOverlay(const CameraConfig& camera, bool includeActive) const;
  void updateGeometryPointOverlay(const CameraConfig& camera, const GeometryOverlay& extraOverlay = {});
  void appendCurrentPartPoseOverlay(const CameraConfig& camera, GeometryOverlay& overlay) const;
  void testGeometryPoint(const CameraConfig& camera);

  void loadGeometryCirclesRecipe(const CameraConfig& camera);
  void saveGeometryCirclesRecipe(const CameraConfig& camera);
  void addGeometryCircle(const CameraConfig& camera);
  void removeActiveGeometryCircle(const CameraConfig& camera);
  GeometryCircleRuntimeConfig& activeGeometryCircleConfig(const QString& cameraId);
  const GeometryCircleRuntimeConfig& activeGeometryCircleConfig(const QString& cameraId) const;
  void activateGeometryCircleDrawing(const CameraConfig& camera);
  void handleGeometryCirclePoints(const CameraConfig& camera, const QVector<QPoint>& points);
  void showConfiguredGeometryCircles(const CameraConfig& camera);
  void testGeometryCircle(const CameraConfig& camera);

  void loadGeometryArcsRecipe(const CameraConfig& camera);
  void saveGeometryArcsRecipe(const CameraConfig& camera);
  void addGeometryArc(const CameraConfig& camera);
  void removeActiveGeometryArc(const CameraConfig& camera);
  GeometryArcRuntimeConfig& activeGeometryArcConfig(const QString& cameraId);
  const GeometryArcRuntimeConfig& activeGeometryArcConfig(const QString& cameraId) const;
  void activateGeometryArcDrawing(const CameraConfig& camera);
  void handleGeometryArcPoints(const CameraConfig& camera, const QVector<QPoint>& points);
  void handleGeometryArcHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint);
  void showConfiguredGeometryArcs(const CameraConfig& camera);
  void testGeometryArc(const CameraConfig& camera);

  LineGeometryMouseController& lineMouseController(const QString& cameraId);
  LineGeometryMouseController& pointMouseController(const QString& cameraId);

private:
  void removeMeasurementsForDeletedGeometry(const CameraConfig& camera, const QString& geometryId);
  void applyConfiguredGeometryDetectionResult(const CameraConfig& camera, const ConfiguredGeometryDetectOutput& result);
  void applyGeometryPipelineResult(const CameraConfig& camera, const GeometryPipelineOutput& result);

  DrawingTarget m_drawingTarget = DrawingTarget::None;

  QHash<QString, QVector<GeometryLineRuntimeConfig>> m_lineConfigs;
  QHash<QString, int> m_activeLineIndexes;
  QHash<QString, LineGeometryMouseController> m_lineMouseControllers;
  QHash<QString, QVector<GeometryPointRuntimeConfig>> m_pointConfigs;
  QHash<QString, int> m_activePointIndexes;
  QHash<QString, LineGeometryMouseController> m_pointMouseControllers;
  QHash<QString, QVector<GeometryCircleRuntimeConfig>> m_circleConfigs;
  QHash<QString, int> m_activeCircleIndexes;
  QHash<QString, QVector<GeometryArcRuntimeConfig>> m_arcConfigs;
  QHash<QString, int> m_activeArcIndexes;
  QHash<QString, QSize> m_guideReferenceSizes;
  QHash<QString, quint64> m_geometryJobGeneration;
};
