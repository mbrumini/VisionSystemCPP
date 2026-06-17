#pragma once

#include "calibration/CalibrationTypes.h"
#include "config/AppConfig.h"

#include <QDialog>
#include <QPixmap>
#include <QVector>

#include <opencv2/core/mat.hpp>

#include <functional>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;

class CheckerboardCalibrationDialog : public QDialog
{
public:
  struct Result
  {
    int cameraIndex = -1;
    CameraCalibrationModel model;
  };

  using FrameProvider = std::function<cv::Mat(const CameraConfig&, QString*)>;
  using PixmapConverter = std::function<QPixmap(const cv::Mat&)>;
  using CameraAction = std::function<void(const CameraConfig&)>;

  CheckerboardCalibrationDialog(const QVector<CameraConfig>& cameras,
                                const QString& selectedCameraId,
                                FrameProvider frameProvider,
                                PixmapConverter pixmapConverter,
                                CameraAction startLive,
                                CameraAction stopLive,
                                QWidget* parent = nullptr);

  Result result() const;

private:
  int selectedCameraIndex() const;
  CameraConfig selectedCamera() const;
  void setPreviewImage(const cv::Mat& image);
  void ensureLiveStarted();
  void updateLiveFrame();
  void restartLive();
  void freezeLiveFrame();
  void calibrateCurrentFrame();

  QVector<CameraConfig> m_cameras;
  FrameProvider m_frameProvider;
  PixmapConverter m_pixmapConverter;
  CameraAction m_startLive;
  CameraAction m_stopLive;
  Result m_result;

  QComboBox* m_cameraCombo = nullptr;
  QSpinBox* m_columns = nullptr;
  QSpinBox* m_rows = nullptr;
  QDoubleSpinBox* m_pitch = nullptr;
  QLabel* m_preview = nullptr;
  QLabel* m_status = nullptr;
  QPushButton* m_freezeButton = nullptr;
  QPushButton* m_resumeButton = nullptr;
  QTimer* m_liveTimer = nullptr;

  cv::Mat m_liveFrame;
  cv::Mat m_frozenFrame;
  bool m_frozen = false;
};
