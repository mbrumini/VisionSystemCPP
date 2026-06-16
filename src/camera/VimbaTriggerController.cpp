#include "camera/VimbaTriggerController.h"

#ifdef VISION_WITH_VIMBAX
#include <VmbCPP/Feature.h>

namespace
{
QString vimbaError(const QString& action, VmbErrorType error)
{
  return QString("%1 fallito: Vimba error %2").arg(action).arg(static_cast<int>(error));
}

QString defaultExternalLine(const CameraTriggerConfig& trigger)
{
  if (!trigger.cameraLine.trimmed().isEmpty())
  {
    return trigger.cameraLine.trimmed();
  }

  if (!trigger.source.trimmed().isEmpty() && trigger.source != "ioBoard")
  {
    return trigger.source.trimmed();
  }

  return "Line1";
}
}

VimbaTriggerController::VimbaTriggerController(VmbCPP::CameraPtr camera)
  : m_camera(std::move(camera))
{
}

bool VimbaTriggerController::configureSoftware(QString* errorMessage)
{
  if (!setEnum("AcquisitionMode", "Continuous", nullptr))
  {
    setEnum("AcquisitionMode", "SingleFrame", nullptr);
  }
  if (!setEnum("TriggerSelector", "FrameStart", errorMessage))
  {
    return false;
  }
  if (!setEnum("TriggerMode", "On", errorMessage))
  {
    return false;
  }
  return setEnum("TriggerSource", "Software", errorMessage);
}

bool VimbaTriggerController::configureExternal(const CameraTriggerConfig& trigger, QString* errorMessage)
{
  if (!setEnum("AcquisitionMode", "Continuous", nullptr))
  {
    setEnum("AcquisitionMode", "SingleFrame", nullptr);
  }
  if (!setEnum("TriggerSelector", "FrameStart", errorMessage))
  {
    return false;
  }
  if (!setEnum("TriggerMode", "On", errorMessage))
  {
    return false;
  }
  return setEnum("TriggerSource", defaultExternalLine(trigger), errorMessage);
}

bool VimbaTriggerController::configureFreeRun(QString* errorMessage)
{
  if (!setEnum("AcquisitionMode", "Continuous", nullptr))
  {
    setEnum("AcquisitionMode", "SingleFrame", nullptr);
  }
  return setEnum("TriggerMode", "Off", errorMessage);
}

bool VimbaTriggerController::startAcquisition(QString* errorMessage)
{
  return runCommand("AcquisitionStart", errorMessage);
}

bool VimbaTriggerController::stopAcquisition(QString* errorMessage)
{
  return runCommand("AcquisitionStop", errorMessage);
}

bool VimbaTriggerController::sendSoftwareTrigger(QString* errorMessage)
{
  return runCommand("TriggerSoftware", errorMessage);
}

bool VimbaTriggerController::setEnum(const char* featureName, const QString& value, QString* errorMessage)
{
  if (!m_camera)
  {
    if (errorMessage)
    {
      *errorMessage = "Camera Vimba non aperta";
    }
    return false;
  }

  VmbCPP::FeaturePtr feature;
  VmbErrorType error = m_camera->GetFeatureByName(featureName, feature);
  if (error != VmbErrorSuccess || !feature)
  {
    if (errorMessage)
    {
      *errorMessage = vimbaError(QString("Feature %1").arg(featureName), error);
    }
    return false;
  }

  error = feature->SetValue(value.toStdString().c_str());
  if (error != VmbErrorSuccess)
  {
    if (errorMessage)
    {
      *errorMessage = vimbaError(QString("%1=%2").arg(featureName, value), error);
    }
    return false;
  }

  return true;
}

bool VimbaTriggerController::runCommand(const char* featureName, QString* errorMessage)
{
  if (!m_camera)
  {
    if (errorMessage)
    {
      *errorMessage = "Camera Vimba non aperta";
    }
    return false;
  }

  VmbCPP::FeaturePtr feature;
  VmbErrorType error = m_camera->GetFeatureByName(featureName, feature);
  if (error != VmbErrorSuccess || !feature)
  {
    if (errorMessage)
    {
      *errorMessage = vimbaError(QString("Feature %1").arg(featureName), error);
    }
    return false;
  }

  error = feature->RunCommand();
  if (error != VmbErrorSuccess)
  {
    if (errorMessage)
    {
      *errorMessage = vimbaError(QString("Comando %1").arg(featureName), error);
    }
    return false;
  }

  return true;
}
#else
bool VimbaTriggerController::configureSoftware(QString* errorMessage)
{
  if (errorMessage) *errorMessage = "SDK VimbaX non disponibile";
  return false;
}
bool VimbaTriggerController::configureExternal(const CameraTriggerConfig&, QString* errorMessage)
{
  if (errorMessage) *errorMessage = "SDK VimbaX non disponibile";
  return false;
}
bool VimbaTriggerController::configureFreeRun(QString* errorMessage)
{
  if (errorMessage) *errorMessage = "SDK VimbaX non disponibile";
  return false;
}
bool VimbaTriggerController::startAcquisition(QString* errorMessage)
{
  if (errorMessage) *errorMessage = "SDK VimbaX non disponibile";
  return false;
}
bool VimbaTriggerController::stopAcquisition(QString* errorMessage)
{
  if (errorMessage) *errorMessage = "SDK VimbaX non disponibile";
  return false;
}
bool VimbaTriggerController::sendSoftwareTrigger(QString* errorMessage)
{
  if (errorMessage) *errorMessage = "SDK VimbaX non disponibile";
  return false;
}
#endif
