#pragma once

#include "gui/modules/MainWindowModuleBase.h"
#include "processing/SurfaceDefectProcessor.h"

#include <QString>

class CameraConfig;
struct SurfaceAnnulusLocalizationConfig;

class MainWindowSurfaceModule : public MainWindowModuleBase
{
public:
  enum class CircleTarget
  {
    None,
    Outer,
    Inner,
    Edge
  };

  explicit MainWindowSurfaceModule(MainWindowContext& context);

  CircleTarget circleTarget() const { return m_circleTarget; }
  void setCircleTarget(CircleTarget target) { m_circleTarget = target; }

  void showSurfaceLocalizationPanel(const CameraConfig& camera);
  void showSurfaceLocalizationStrategyPanel(const CameraConfig& camera, const QString& strategyId);

  void activateLocalizationRoiDrawing(const CameraConfig& camera);
  void activateLocalizationExclusionDrawing(const CameraConfig& camera);
  void clearLocalizationExclusions(const CameraConfig& camera);

  void activateSurfaceDefectRoiDrawing(const CameraConfig& camera);
  void activateSurfaceDefectPolygonDrawing(const CameraConfig& camera);
  void activateSurfaceDefectExclusionDrawing(const CameraConfig& camera);
  void clearSurfaceDefectExclusions(const CameraConfig& camera);

  void activateSurfaceOuterCircleDrawing(const CameraConfig& camera);
  void activateSurfaceInnerCircleDrawing(const CameraConfig& camera);
  void activateSurfaceEdgeCircleCenterRadiusDrawing(const CameraConfig& camera);
  void activateSurfaceEdgeCircleDrawing(const CameraConfig& camera);
  void activateSurfaceThreePointCircleDrawing(const CameraConfig& camera, CircleTarget target);

  void saveSurfaceThresholdAndPreview(const CameraConfig& camera, int thresholdValue);
  void saveSurfaceEdgeAndPreview(const CameraConfig& camera, int sensitivity);
  void saveSurfaceEdgeBandAndPreview(const CameraConfig& camera, int innerWidth, int outerWidth);
  void saveSurfaceEdgeFitMaxErrorAndPreview(const CameraConfig& camera, int maxError);
  void saveSurfaceMethodAndPreview(const CameraConfig& camera, const QString& method);
  void showStoredSurfaceLocalizationGeometry(const CameraConfig& camera, const SurfaceAnnulusLocalizationConfig& annulus);

  void testSurfaceAnnulusLocalization(const CameraConfig& camera);
  void testSurfaceLocalization(const CameraConfig& camera);
  void testSurfaceLocalizationStrategy(const CameraConfig& camera);
  void testSurfaceEdgePcaLocalization(const CameraConfig& camera);
  void previewSurfaceModel(const CameraConfig& camera);
  void acquireSurfaceModel(const CameraConfig& camera);
  void testSurfaceShapeModel(const CameraConfig& camera);
  void testSurfaceTemplateModel(const CameraConfig& camera);

private:
  void showCircleAnnulusLocalizationPanel(const CameraConfig& camera);
  void showMassPcaLocalizationPanel(const CameraConfig& camera);
  void showEdgePcaLocalizationPanel(const CameraConfig& camera);
  void showModelLocalizationPanel(const CameraConfig& camera);
  void showPlannedLocalizationPanel(const CameraConfig& camera, const QString& strategyId);

  CircleTarget m_circleTarget = CircleTarget::None;
};
