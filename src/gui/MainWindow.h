#pragma once

#include "config/AppConfig.h"
#include "config/RecipeManager.h"
#include "config/TranslationManager.h"
#include "gui/geometry/LineGeometryMouseController.h"
#include "gui/CameraTileWidget.h"
#include "gui/ImageViewWidget.h"
#include "processing/LocalizationProcessor.h"
#include "processing/SurfaceDefectProcessor.h"
#include "processing/geometry/EdgeLineDetector.h"
#include "runtime/CameraRuntime.h"

#include <QGridLayout>
#include <QHash>
#include <QLabel>
#include <QMainWindow>
#include <QRect>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <opencv2/core/mat.hpp>

#include <map>

class MainWindow : public QMainWindow
{
public:
  explicit MainWindow(QWidget* parent = nullptr);

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  enum class SurfaceCircleTarget
  {
    None,
    Outer,
    Inner,
    Edge
  };

  enum class GeometryDrawingTarget
  {
    None,
    Line,
    Point
  };

  struct GeometryLineRuntimeConfig
  {
    QString id = "line_1";
    bool enabled = true;
    cv::Point2d imageStart;
    cv::Point2d imageEnd;
    cv::Point2d partStart;
    cv::Point2d partEnd;
    int bandHalfWidth = 20;
    int edgeSensitivity = 60;
    int edgeCleanupDerivative = 12;
    int edgeStatisticalFilter = 0;
    bool useSubpixel = true;
    EdgeLineScanDirection scanDirection = EdgeLineScanDirection::NormalPositive;
    EdgeLineTransition transition = EdgeLineTransition::LightToDark;
    EdgeLinePickMode pickMode = EdgeLinePickMode::First;
    bool hasImageLine = false;
    bool hasLine = false;
  };

  struct GeometryPointRuntimeConfig
  {
    QString id = "point_1";
    bool enabled = true;
    cv::Point2d imageStart;
    cv::Point2d imageEnd;
    cv::Point2d partStart;
    cv::Point2d partEnd;
    int edgeSensitivity = 60;
    bool useSubpixel = true;
    EdgeLineTransition transition = EdgeLineTransition::LightToDark;
    EdgeLinePickMode pickMode = EdgeLinePickMode::First;
    bool hasImageGuide = false;
    bool hasGuide = false;
  };

  void buildUi();
  void buildMenu();
  void rebuildUi();
  void changeLanguage(const QString& languageCode);
  void toggleFullScreen();
  void selectRecipe();
  void createRecipe();
  void duplicateRecipe();
  void importRecipe();
  void exportRecipe();
  void setActiveRecipe(const QString& recipeId);
  void configureCameraSource(const CameraConfig& camera);
  void configureCameraSampleImage(const CameraConfig& camera);
  void configureCameraTestImages(const CameraConfig& camera);
  void acquireCameraSampleImage(const CameraConfig& camera);
  void ensureRecipeCameraFolders();
  void refreshSelectedCameraRecipeData();
  void loadConfiguration();
  void showGridView();
  void selectCamera(const CameraConfig& camera);
  void updateControlPanel(const CameraConfig* camera);
  void deactivateImageDrawingTools();
  void showCameraToolList(const CameraConfig& camera);
  void showCameraSetupPanel(const CameraConfig& camera);
  void showGeometryPanel(const CameraConfig& camera);
  void showGeometryPointPanel(const CameraConfig& camera);
  void showGeometryLinePanel(const CameraConfig& camera);
  void showToolPanel(const CameraConfig& camera, const QString& toolId);
  void refreshPoseForCurrentFrame(const CameraConfig& camera);
  void loadGeometryPointsRecipe(const CameraConfig& camera);
  void saveGeometryPointsRecipe(const CameraConfig& camera);
  void addGeometryPoint(const CameraConfig& camera);
  GeometryPointRuntimeConfig& activeGeometryPointConfig(const QString& cameraId);
  const GeometryPointRuntimeConfig& activeGeometryPointConfig(const QString& cameraId) const;
  void loadGeometryLinesRecipe(const CameraConfig& camera);
  void saveGeometryLinesRecipe(const CameraConfig& camera);
  void addGeometryLine(const CameraConfig& camera);
  GeometryLineRuntimeConfig& activeGeometryLineConfig(const QString& cameraId);
  const GeometryLineRuntimeConfig& activeGeometryLineConfig(const QString& cameraId) const;
  void activateGeometryLineDrawing(const CameraConfig& camera);
  void handleGeometryLinePoint(const CameraConfig& camera, const QPointF& imagePoint);
  void handleGeometryLineHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint);
  GeometryOverlay configuredGeometryLinesOverlay(const CameraConfig& camera, bool includeActive) const;
  void updateGeometryLineOverlay(const CameraConfig& camera, const GeometryOverlay& extraOverlay = {});
  void testGeometryLine(const CameraConfig& camera);
  void testConfiguredGeometryLines(const CameraConfig& camera);
  void activateGeometryPointDrawing(const CameraConfig& camera);
  void handleGeometryPointGuidePoint(const CameraConfig& camera, const QPointF& imagePoint);
  void handleGeometryPointHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint);
  GeometryOverlay configuredGeometryPointsOverlay(const CameraConfig& camera, bool includeActive) const;
  void updateGeometryPointOverlay(const CameraConfig& camera, const GeometryOverlay& extraOverlay = {});
  void testGeometryPoint(const CameraConfig& camera);
  void startCameraSimulation(const CameraConfig& camera);
  void stopCameraSimulation(const CameraConfig& camera);
  void stepCameraSimulation(const CameraConfig& camera);
  void advanceCameraFrame(const CameraConfig& camera);
  void processCurrentCameraFrame(const CameraConfig& camera);
  QString cameraSetupDetailsText(const CameraConfig& camera) const;
  void updateCameraSetupDetails(const CameraConfig& camera);
  void showSurfaceLocalizationPanel(const CameraConfig& camera);
  void showSurfaceLocalizationStrategyPanel(const CameraConfig& camera, const QString& strategyId);
  void activateLocalizationRoiDrawing(const CameraConfig& camera);
  void activateLocalizationExclusionDrawing(const CameraConfig& camera);
  void clearLocalizationExclusions(const CameraConfig& camera);
  void testLocalization(const CameraConfig& camera);
  void activateSurfaceDefectRoiDrawing(const CameraConfig& camera);
  void activateSurfaceDefectExclusionDrawing(const CameraConfig& camera);
  void clearSurfaceDefectExclusions(const CameraConfig& camera);
  void testSurfaceLocalization(const CameraConfig& camera);
  void testSurfaceLocalizationStrategy(const CameraConfig& camera);
  void testSurfaceEdgePcaLocalization(const CameraConfig& camera);
  void previewSurfaceModel(const CameraConfig& camera);
  void acquireSurfaceModel(const CameraConfig& camera);
  void testSurfaceShapeModel(const CameraConfig& camera);
  void testSurfaceTemplateModel(const CameraConfig& camera);
  void activateSurfaceOuterCircleDrawing(const CameraConfig& camera);
  void activateSurfaceInnerCircleDrawing(const CameraConfig& camera);
  void activateSurfaceEdgeCircleCenterRadiusDrawing(const CameraConfig& camera);
  void activateSurfaceEdgeCircleDrawing(const CameraConfig& camera);
  void activateSurfaceThreePointCircleDrawing(const CameraConfig& camera, SurfaceCircleTarget target);
  void saveSurfaceThresholdAndPreview(const CameraConfig& camera, int thresholdValue);
  void saveSurfaceEdgeAndPreview(const CameraConfig& camera, int sensitivity);
  void saveSurfaceEdgeBandAndPreview(const CameraConfig& camera, int innerWidth, int outerWidth);
  void saveSurfaceEdgeFitMaxErrorAndPreview(const CameraConfig& camera, int maxError);
  void saveSurfaceMethodAndPreview(const CameraConfig& camera, const QString& method);
  void testSurfaceAnnulusLocalization(const CameraConfig& camera);
  void showStoredSurfaceLocalizationGeometry(const CameraConfig& camera, const SurfaceAnnulusLocalizationConfig& annulus);
  bool isBwDimensionalCamera(const CameraConfig& camera) const;
  bool isGrayscaleLocalizationCamera(const CameraConfig& camera) const;
  void clearToolPanel();
  void appendLog(const QString& message);
  void updateLargePreview();
  QString trText(const QString& key) const;
  QString runtimeStatusText(CameraRuntime::Status status) const;
  PartPose partPoseFromLocalizationResult(const CameraConfig& camera, const LocalizationResult& result) const;
  PartPose partPoseFromSurfaceReference(const CameraConfig& camera, const SurfaceLocalizationReference& reference) const;
  QPixmap loadCameraPreview(const CameraConfig& camera) const;
  QPixmap matToPixmap(const cv::Mat& image) const;
  cv::Mat currentInputImage(const CameraConfig& camera, QString* errorMessage = nullptr) const;
  QString cameraSampleImagePath(const CameraConfig& camera) const;
  QString cameraTestImagesFolder(const CameraConfig& camera) const;
  QString resolvedCameraFolder(const CameraConfig& camera) const;
  QString firstImageInFolder(const QString& folder) const;

  AppConfig m_config;
  RecipeManager m_recipeManager;
  TranslationManager m_translations;
  QVector<CameraTileWidget*> m_tiles;
  QStackedWidget* m_imageStack = nullptr;
  QWidget* m_gridPage = nullptr;
  QWidget* m_gridContent = nullptr;
  QGridLayout* m_gridLayout = nullptr;
  ImageViewWidget* m_largeImage = nullptr;
  QLabel* m_largeTitle = nullptr;
  QLabel* m_systemStatus = nullptr;
  QLabel* m_cameraDetails = nullptr;
  QLabel* m_setupDetails = nullptr;
  QString m_setupCameraId;
  QWidget* m_toolsContainer = nullptr;
  QVBoxLayout* m_toolsLayout = nullptr;
  QTextEdit* m_log = nullptr;
  QString m_selectedCameraId;
  CameraConfig m_selectedCamera;
  QPixmap m_selectedPreview;
  QString m_selectedImagePath;
  QTimer* m_simulationTimer = nullptr;
  std::map<QString, CameraRuntime> m_cameraRuntime;
  QHash<QString, LocalizationResult> m_lastLocalizationResults;
  QHash<QString, SurfaceLocalizationReference> m_lastSurfaceLocalizationResults;

  enum class ActiveDrawingRecipe
  {
    None,
    Localization,
    SurfaceDefects,
    Geometry
  };

  ActiveDrawingRecipe m_activeDrawingRecipe = ActiveDrawingRecipe::None;

  SurfaceCircleTarget m_surfaceCircleTarget = SurfaceCircleTarget::None;
  GeometryDrawingTarget m_geometryDrawingTarget = GeometryDrawingTarget::None;

  QHash<QString, QVector<GeometryLineRuntimeConfig>> m_geometryLineConfigs;
  QHash<QString, int> m_activeGeometryLineIndexes;
  QHash<QString, LineGeometryMouseController> m_lineGeometryMouseControllers;
  QHash<QString, QVector<GeometryPointRuntimeConfig>> m_geometryPointConfigs;
  QHash<QString, int> m_activeGeometryPointIndexes;
  QHash<QString, LineGeometryMouseController> m_pointGeometryMouseControllers;
};
