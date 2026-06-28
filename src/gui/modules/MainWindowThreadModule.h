#pragma once

#include "gui/modules/MainWindowModuleBase.h"
#include "processing/thread/ThreadProfileMeasurer.h"

#include <QHash>

class CameraConfig;
struct ThreadProfileResult;

class MainWindowThreadModule : public MainWindowModuleBase
{
public:
  explicit MainWindowThreadModule(MainWindowContext& context);

  void showThreadInspectionPanel(const CameraConfig& camera);
  void activateThreadExtractionRoiDrawing(const CameraConfig& camera);
  void clearThreadExtractionRoi(const CameraConfig& camera);
  void syncExtractionRoiOverlay(const CameraConfig& camera);
  void refreshThreadProfileOverlay(const CameraConfig& camera);
  ThreadDiameterValues applyThreadMeasurements(const CameraConfig& camera, const ThreadProfileResult& result);
  void testThreadExtraction(const CameraConfig& camera);
  bool isThreadPanelActive() const { return m_threadPanelActive; }

private:
  void ensureThreadInspectionPose(const CameraConfig& camera);
  ThreadDiameterValues smoothThreadOverlay(const QString& cameraId, const ThreadDiameterValues& incoming);

  QHash<QString, ThreadDiameterValues> m_stableThreadOverlay;
  bool m_refreshingThreadOverlay = false;
  bool m_threadPanelActive = false;
};