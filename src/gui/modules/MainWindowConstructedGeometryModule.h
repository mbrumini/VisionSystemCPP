#pragma once

#include "gui/modules/MainWindowModuleBase.h"

#include <QHash>

class CameraConfig;

class MainWindowConstructedGeometryModule : public MainWindowModuleBase
{
public:
  explicit MainWindowConstructedGeometryModule(MainWindowContext& context);

  void showConstructedGeometryPanel(const CameraConfig& camera);
  void rebuildConstructedGeometryRecipe(const CameraConfig& camera);

private:
  void showLineLineIntersectionPanel(const CameraConfig& camera);
  void showLineCircleIntersectionPanel(const CameraConfig& camera);
  void showCircleCircleIntersectionPanel(const CameraConfig& camera);
  void showCircleCenterPanel(const CameraConfig& camera);
  void showMidpointPanel(const CameraConfig& camera);
  void showOffsetLinePanel(const CameraConfig& camera);
  void showAngleBisectorPanel(const CameraConfig& camera);
  void showTangentLinePanel(const CameraConfig& camera);
  void showProjectPointPanel(const CameraConfig& camera);
  void refreshConstructedGeometrySources(const CameraConfig& camera);
  QHash<QString, QString> geometryAliasesForCamera(const CameraConfig& camera) const;

  void saveConstructedGeometryRecipeAction(const CameraConfig& camera,
                                           const QString& type,
                                           const QString& sourceAId,
                                           const QString& sourceBId = {},
                                           double offset = 0.0);

  void createLineLineIntersection(const CameraConfig& camera, const QString& firstLineId, const QString& secondLineId);
  void createLineCircleIntersection(const CameraConfig& camera, const QString& lineId, const QString& circleId);
  void createCircleCircleIntersection(const CameraConfig& camera, const QString& firstCircleId, const QString& secondCircleId);
  void createCircleCenter(const CameraConfig& camera, const QString& circleId);
  void createMidpoint(const CameraConfig& camera, const QString& firstPointId, const QString& secondPointId);
  void createOffsetLine(const CameraConfig& camera, const QString& lineId, double offset);
  void createAngleBisectors(const CameraConfig& camera, const QString& firstLineId, const QString& secondLineId);
  void createTangentLines(const CameraConfig& camera, const QString& pointId, const QString& circleId);
  void createProjectedPoint(const CameraConfig& camera, const QString& pointId, const QString& lineId);
};
