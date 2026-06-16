#pragma once

#include "config/AppConfig.h"

#include <QString>

#ifdef VISION_WITH_VIMBAX
#include <VmbCPP/VmbCPP.h>
#endif

class VimbaTriggerController
{
public:
#ifdef VISION_WITH_VIMBAX
  explicit VimbaTriggerController(VmbCPP::CameraPtr camera);

  bool configureSoftware(QString* errorMessage = nullptr);
  bool configureExternal(const CameraTriggerConfig& trigger, QString* errorMessage = nullptr);
  bool configureFreeRun(QString* errorMessage = nullptr);
  bool startAcquisition(QString* errorMessage = nullptr);
  bool stopAcquisition(QString* errorMessage = nullptr);
  bool sendSoftwareTrigger(QString* errorMessage = nullptr);

private:
  bool setEnum(const char* featureName, const QString& value, QString* errorMessage);
  bool runCommand(const char* featureName, QString* errorMessage);

  VmbCPP::CameraPtr m_camera;
#else
  bool configureSoftware(QString* errorMessage = nullptr);
  bool configureExternal(const CameraTriggerConfig& trigger, QString* errorMessage = nullptr);
  bool configureFreeRun(QString* errorMessage = nullptr);
  bool startAcquisition(QString* errorMessage = nullptr);
  bool stopAcquisition(QString* errorMessage = nullptr);
  bool sendSoftwareTrigger(QString* errorMessage = nullptr);
#endif
};
