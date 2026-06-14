#include "camera/VimbaXDiscovery.h"

#include <QByteArray>

#ifdef VISION_WITH_VIMBAX
#include <VmbCPP/VmbCPP.h>
#endif

namespace
{
void addDiagnostic(QStringList* diagnostics, const QString& message)
{
  if (diagnostics)
  {
    diagnostics->append(message);
  }
}

QString envValue(const char* name)
{
  const QByteArray value = qgetenv(name);
  return value.isEmpty() ? QString("<empty>") : QString::fromLocal8Bit(value);
}

#ifdef VISION_WITH_VIMBAX
QString toQString(const std::string& value)
{
  return QString::fromStdString(value);
}

QString cameraString(const VmbCPP::CameraPtr& camera, VmbErrorType (VmbCPP::Camera::*getter)(std::string&) const)
{
  std::string value;
  if (camera && VmbErrorSuccess == ((*camera).*getter)(value))
  {
    return toQString(value);
  }

  return {};
}
#endif
}

bool VimbaXDiscovery::isSdkAvailable()
{
#ifdef VISION_WITH_VIMBAX
  return true;
#else
  return false;
#endif
}

QVector<CameraDeviceInfo> VimbaXDiscovery::discover(QString* errorMessage, QStringList* diagnostics)
{
  QVector<CameraDeviceInfo> devices;
  addDiagnostic(diagnostics, QString("VimbaX discovery: sdkCompiled=%1").arg(isSdkAvailable() ? "true" : "false"));
  addDiagnostic(diagnostics, "VimbaX env VIMBA_X_HOME=" + envValue("VIMBA_X_HOME"));
  addDiagnostic(diagnostics, "VimbaX env VIMBAX_HOME=" + envValue("VIMBAX_HOME"));
  addDiagnostic(diagnostics, "VimbaX env GENICAM_GENTL64_PATH=" + envValue("GENICAM_GENTL64_PATH"));

#ifdef VISION_WITH_VIMBAX
  VmbCPP::VmbSystem& system = VmbCPP::VmbSystem::GetInstance();
  addDiagnostic(diagnostics, "VimbaX Startup begin");
  VmbErrorType error = system.Startup();
  if (error != VmbErrorSuccess)
  {
    addDiagnostic(diagnostics, QString("VimbaX Startup failed code=%1").arg(static_cast<int>(error)));
    if (errorMessage)
    {
      *errorMessage = QString("VimbaX Startup fallito: %1").arg(static_cast<int>(error));
    }
    return devices;
  }
  addDiagnostic(diagnostics, "VimbaX Startup ok");

  VmbCPP::CameraPtrVector cameras;
  addDiagnostic(diagnostics, "VimbaX GetCameras begin");
  error = system.GetCameras(cameras);
  if (error != VmbErrorSuccess)
  {
    addDiagnostic(diagnostics, QString("VimbaX GetCameras failed code=%1").arg(static_cast<int>(error)));
    system.Shutdown();
    addDiagnostic(diagnostics, "VimbaX Shutdown after GetCameras failure");
    if (errorMessage)
    {
      *errorMessage = QString("Lettura camere VimbaX fallita: %1").arg(static_cast<int>(error));
    }
    return devices;
  }
  addDiagnostic(diagnostics, QString("VimbaX GetCameras ok count=%1").arg(static_cast<int>(cameras.size())));

  for (const VmbCPP::CameraPtr& camera : cameras)
  {
    CameraDeviceInfo info;
    info.deviceId = cameraString(camera, &VmbCPP::Camera::GetID);
    info.displayName = cameraString(camera, &VmbCPP::Camera::GetName);
    info.modelName = cameraString(camera, &VmbCPP::Camera::GetModel);
    info.serial = cameraString(camera, &VmbCPP::Camera::GetSerialNumber);
    info.interfaceId = cameraString(camera, &VmbCPP::Camera::GetInterfaceID);

    if (info.displayName.isEmpty())
    {
      info.displayName = info.modelName.isEmpty() ? info.deviceId : info.modelName;
    }

    devices.append(info);
    addDiagnostic(
      diagnostics,
      QString("VimbaX camera[%1]: name='%2' model='%3' serial='%4' id='%5' interface='%6'")
        .arg(devices.size() - 1)
        .arg(info.displayName)
        .arg(info.modelName)
        .arg(info.serial)
        .arg(info.deviceId)
        .arg(info.interfaceId));
  }

  system.Shutdown();
  addDiagnostic(diagnostics, "VimbaX Shutdown ok");
  return devices;
#else
  addDiagnostic(diagnostics, "VimbaX discovery unavailable: build without VISION_WITH_VIMBAX");
  if (errorMessage)
  {
    *errorMessage = "SDK VimbaX non trovato in fase di compilazione.";
  }
  return devices;
#endif
}
