#pragma once

#include "config/AppConfig.h"
#include "config/RecipeManager.h"
#include "config/TranslationManager.h"
#include "gui/CameraTileWidget.h"
#include "gui/ImageViewWidget.h"

#include <QGridLayout>
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
  void activateLocalizationRoiDrawing(const CameraConfig& camera);
  void testLocalization(const CameraConfig& camera);
  bool isBwDimensionalCamera(const CameraConfig& camera) const;
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
};
