#pragma once

#include "gui/modules/MainWindowModuleBase.h"

#include <QHash>
#include <QPointF>
#include <QString>

#include <functional>

class CameraConfig;
class GeometryOverlay;
struct MeasurementRecipeConfig;

class MainWindowMeasurementModule : public MainWindowModuleBase
{
public:
  explicit MainWindowMeasurementModule(MainWindowContext& context);

  void showMeasurementPanel(const CameraConfig& camera);
  void showPointPointDistancePanel(const CameraConfig& camera);
  void showPointLineDistancePanel(const CameraConfig& camera);
  void showLineLineDistancePanel(const CameraConfig& camera);
  void showCircleDiameterPanel(const CameraConfig& camera);
  void showLineLineAnglePanel(const CameraConfig& camera);
  void showMeasurementRealValuesPanel(const CameraConfig& camera);
  void rebuildMeasurementRecipe(const CameraConfig& camera);
  void removeMeasurement(const CameraConfig& camera, const QString& measurementId);
  void appendMeasurementOverlay(const CameraConfig& camera, GeometryOverlay& overlay, bool compact = false) const;
  void setMeasurementLabelPosition(const CameraConfig& camera, const QString& measurementKey, const QPointF& imagePoint);

private:
  void refreshMeasurementSources(const CameraConfig& camera);
  void createPointPointDistance(const CameraConfig& camera, const QString& pointAId, const QString& pointBId);
  void createPointLineDistance(const CameraConfig& camera, const QString& pointId, const QString& lineId);
  void createLineLineDistance(const CameraConfig& camera, const QString& lineAId, const QString& lineBId);
  void createCircleDiameter(const CameraConfig& camera, const QString& circleId);
  void createLineLineAngle(const CameraConfig& camera, const QString& lineAId, const QString& lineBId);
  void saveMeasurementRealSettings(const CameraConfig& camera, const MeasurementRecipeConfig& config);
  bool saveMeasurementRecipeAction(const CameraConfig& camera,
                                   const QString& type,
                                   const QString& sourceAId,
                                   const QString& sourceBId = {},
                                   double samplePixels = 0.0);
  void finalizeMeasurementCreate(const CameraConfig& camera, const QString& successLog);
  void appendMeasurementListControls(QWidget* panel,
                                     QVBoxLayout* layout,
                                     const CameraConfig& camera,
                                     const std::function<void()>& refreshPanel);
  QHash<QString, QString> geometryAliasesForCamera(const CameraConfig& camera) const;
  void showMeasurementListDialog(const CameraConfig& camera);
  void showMeasurementToleranceDialog(const CameraConfig& camera, const QString& measurementId);
  QString selectedMeasurementKey(const QString& cameraId) const;

  QHash<QString, QString> m_selectedMeasurementKeys;
};
