#pragma once

#include "gui/modules/MainWindowContext.h"

#include <QMainWindow>
#include <QString>

class CameraConfig;

class MainWindowModuleBase
{
public:
  explicit MainWindowModuleBase(MainWindowContext& context)
    : m_context(context)
  {
  }

protected:
  MainWindowContext& context() { return m_context; }
  const MainWindowContext& context() const { return m_context; }

  QMainWindow* window() const { return m_context.window; }
  QWidget* windowWidget() const { return m_context.window; }

  AppConfig& config() const { return *m_context.config; }
  RecipeManager& recipes() const { return *m_context.recipes; }
  TranslationManager& translations() const { return *m_context.translations; }

  ImageViewWidget* largeImage() const { return m_context.largeImage; }
  QVBoxLayout* toolsLayout() const { return m_context.toolsLayout; }
  QWidget* toolsContainer() const { return m_context.toolsContainer; }
  QTextEdit* logWidget() const { return m_context.log; }

  QString& selectedCameraId() const { return *m_context.selectedCameraId; }
  CameraConfig& selectedCamera() const { return *m_context.selectedCamera; }
  QPixmap& selectedPreview() const { return *m_context.selectedPreview; }
  QString& selectedImagePath() const { return *m_context.selectedImagePath; }

  std::map<QString, CameraRuntime>& cameraRuntime() const { return *m_context.cameraRuntime; }

  QString tr(const QString& key) const { return m_context.trText(key); }
  void log(const QString& message) const
  {
    if (m_context.isDetailedLogEnabled && !m_context.isDetailedLogEnabled())
    {
      return;
    }
    if (m_context.appendLog)
    {
      m_context.appendLog(message);
    }
  }
  template<typename Producer>
  void logLazy(Producer&& produce) const
  {
    if (m_context.isDetailedLogEnabled && !m_context.isDetailedLogEnabled())
    {
      return;
    }
    if (m_context.appendLog)
    {
      m_context.appendLog(produce());
    }
  }

  void syncThreadExtractionRoiOverlay(const CameraConfig& camera) const
  {
    if (m_context.syncThreadExtractionRoiOverlay)
    {
      m_context.syncThreadExtractionRoiOverlay(camera);
    }
  }

  bool isSetupCameraActive(const QString& cameraId) const
  {
    return m_context.setupCameraId &&
           !m_context.setupCameraId->isEmpty() &&
           *m_context.setupCameraId == cameraId;
  }

private:
  MainWindowContext& m_context;
};
