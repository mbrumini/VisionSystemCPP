#pragma once

#include "config/AppConfig.h"
#include "config/TranslationManager.h"
#include "gui/CameraTileWidget.h"

#include <QGridLayout>
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QVector>

class MainWindow : public QMainWindow
{
public:
  explicit MainWindow(QWidget* parent = nullptr);

private:
  void buildUi();
  void buildMenu();
  void rebuildUi();
  void changeLanguage(const QString& languageCode);
  void loadConfiguration();
  void showGridView();
  void selectCamera(const CameraConfig& camera);
  void updateControlPanel(const CameraConfig* camera);
  void showCameraToolList(const CameraConfig& camera);
  void showToolPanel(const CameraConfig& camera, const QString& toolId);
  void clearToolPanel();
  void appendLog(const QString& message);
  QString trText(const QString& key) const;
  QPixmap loadCameraPreview(const CameraConfig& camera) const;
  QString firstImageInFolder(const QString& folder) const;

  AppConfig m_config;
  TranslationManager m_translations;
  QVector<CameraTileWidget*> m_tiles;
  QStackedWidget* m_imageStack = nullptr;
  QWidget* m_gridPage = nullptr;
  QWidget* m_gridContent = nullptr;
  QGridLayout* m_gridLayout = nullptr;
  QLabel* m_largeImage = nullptr;
  QLabel* m_largeTitle = nullptr;
  QLabel* m_systemStatus = nullptr;
  QLabel* m_cameraDetails = nullptr;
  QWidget* m_toolsContainer = nullptr;
  QVBoxLayout* m_toolsLayout = nullptr;
  QTextEdit* m_log = nullptr;
  QString m_selectedCameraId;
};
