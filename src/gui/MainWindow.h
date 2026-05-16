#pragma once

#include "config/AppConfig.h"
#include "config/RecipeManager.h"
#include "config/TranslationManager.h"
#include "gui/CameraTileWidget.h"
#include "gui/ImageViewWidget.h"
#include "processing/LocalizationProcessor.h"
#include "processing/SurfaceDefectProcessor.h"

#include <QGridLayout>
#include <QHash>
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QVector>

#include <opencv2/core/mat.hpp>

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
  void refreshSelectedCameraRecipeData();
  void loadConfiguration();
  void showGridView();
  void selectCamera(const CameraConfig& camera);
  void updateControlPanel(const CameraConfig* camera);
  void showCameraToolList(const CameraConfig& camera);
  void showToolPanel(const CameraConfig& camera, const QString& toolId);
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
  void activateSurfaceOuterCircleDrawing(const CameraConfig& camera);
  void activateSurfaceInnerCircleDrawing(const CameraConfig& camera);
  void activateSurfaceEdgeCircleCenterRadiusDrawing(const CameraConfig& camera);
  void activateSurfaceEdgeCircleDrawing(const CameraConfig& camera);
  void activateSurfaceThreePointCircleDrawing(const CameraConfig& camera, SurfaceCircleTarget target);
  void saveSurfaceThresholdAndPreview(const CameraConfig& camera, int thresholdValue);
  void saveSurfaceEdgeAndPreview(const CameraConfig& camera, int sensitivity);
  void saveSurfaceEdgeBandAndPreview(const CameraConfig& camera, int innerWidth, int outerWidth);
  void saveSurfaceMethodAndPreview(const CameraConfig& camera, const QString& method);
  void testSurfaceAnnulusLocalization(const CameraConfig& camera);
  void showStoredSurfaceLocalizationGeometry(const CameraConfig& camera, const SurfaceAnnulusLocalizationConfig& annulus);
  bool isBwDimensionalCamera(const CameraConfig& camera) const;
  bool isGrayscaleLocalizationCamera(const CameraConfig& camera) const;
  void clearToolPanel();
  void appendLog(const QString& message);
  void updateLargePreview();
  QString trText(const QString& key) const;
  QPixmap loadCameraPreview(const CameraConfig& camera) const;
  QPixmap matToPixmap(const cv::Mat& image) const;
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
  QWidget* m_toolsContainer = nullptr;
  QVBoxLayout* m_toolsLayout = nullptr;
  QTextEdit* m_log = nullptr;
  QString m_selectedCameraId;
  CameraConfig m_selectedCamera;
  QPixmap m_selectedPreview;
  QString m_selectedImagePath;
  QHash<QString, LocalizationResult> m_lastLocalizationResults;

  enum class ActiveDrawingRecipe
  {
    None,
    Localization,
    SurfaceDefects
  };

  ActiveDrawingRecipe m_activeDrawingRecipe = ActiveDrawingRecipe::None;

  SurfaceCircleTarget m_surfaceCircleTarget = SurfaceCircleTarget::None;
};
