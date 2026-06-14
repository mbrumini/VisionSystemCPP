#pragma once

#include "gui/modules/MainWindowModuleBase.h"
#include "runtime/PartPose.h"

#include <QPixmap>
#include <QString>

#include <opencv2/core/mat.hpp>

class CameraConfig;
struct LocalizationResult;
struct SurfaceLocalizationReference;

class MainWindowImagingModule : public MainWindowModuleBase
{
public:
  explicit MainWindowImagingModule(MainWindowContext& context);

  PartPose partPoseFromLocalizationResult(const CameraConfig& camera, const LocalizationResult& result) const;
  PartPose partPoseFromSurfaceReference(const CameraConfig& camera, const SurfaceLocalizationReference& reference) const;

  QPixmap loadCameraPreview(const CameraConfig& camera) const;
  QPixmap loadCameraSamplePreview(const CameraConfig& camera) const;
  void reloadCameraReferenceImage(const CameraConfig& camera);
  QPixmap matToPixmap(const cv::Mat& image) const;
  cv::Mat currentInputImage(const CameraConfig& camera, QString* errorMessage = nullptr) const;

  QString cameraSampleImagePath(const CameraConfig& camera) const;
  QString cameraTestImagesFolder(const CameraConfig& camera) const;
  QString resolvedCameraFolder(const CameraConfig& camera) const;
  QString firstImageInFolder(const QString& folder) const;
};
