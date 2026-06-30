#include "camera/CameraFactory.h"

#include "camera/FileCamera.h"
#include "camera/SimulatedCamera.h"
#include "camera/UsbCamera.h"
#include "camera/VimbaCamera.h"

namespace
{
QString normalizedBackend(const CameraConfig& camera)
{
  const QString backend = camera.backend.trimmed().toLower();
  if (!backend.isEmpty())
  {
    return backend;
  }

  const QString type = camera.type.trimmed().toLower();
  if (type == "vimba")
  {
    return "vimbax";
  }
  if (type == "usb")
  {
    return "opencv_usb";
  }
  return type;
}
}

QString CameraFactory::sourceKind(const CameraConfig& camera)
{
  const QString backend = normalizedBackend(camera);
  if (backend == "vimbax" || backend == "vimba")
  {
    return "vimba";
  }
  if (backend == "opencv_usb" || backend == "usb")
  {
    return "usb";
  }
  if (backend == "file")
  {
    return "file";
  }
  if (backend == "simulator")
  {
    return "simulator";
  }
  if (backend == "svs" || backend == "svs_vistek")
  {
    return "svs";
  }
  return backend;
}

QString CameraFactory::sourceIdentity(const CameraConfig& camera, const QString& resolvedFolder)
{
  const QString kind = sourceKind(camera);
  if (kind == "usb")
  {
    return QString("usb:%1").arg(camera.usbIndex);
  }
  if (kind == "file")
  {
    return resolvedFolder;
  }
  if (kind == "simulator")
  {
    return camera.simulatorChannel.isEmpty() ? camera.id : camera.simulatorChannel;
  }
  if (kind == "vimba" || kind == "svs")
  {
    return camera.deviceId.isEmpty() ? camera.serial : camera.deviceId;
  }
  return {};
}

std::shared_ptr<ICamera> CameraFactory::create(
  const CameraConfig& camera,
  const QString& resolvedFolder,
  bool loop,
  QString* errorMessage)
{
  const QString kind = sourceKind(camera);
  if (kind == "usb")
  {
    if (camera.usbIndex < 0)
    {
      if (errorMessage)
      {
        *errorMessage = "Indice camera USB non valido: " + camera.id;
      }
      return {};
    }
    return std::make_shared<UsbCamera>(camera);
  }
  if (kind == "vimba")
  {
    return std::make_shared<VimbaCamera>(camera);
  }
  if (kind == "simulator")
  {
    return std::make_shared<SimulatedCamera>(
      camera.simulatorChannel.isEmpty() ? camera.id : camera.simulatorChannel);
  }
  if (kind == "file")
  {
    return std::make_shared<FileCamera>(resolvedFolder.toStdString(), loop);
  }
  if (kind == "svs")
  {
    if (errorMessage)
    {
      *errorMessage = "Backend SVS configurato ma driver SVS non ancora disponibile";
    }
    return {};
  }

  if (errorMessage)
  {
    *errorMessage = "Sorgente camera non supportata: " + kind;
  }
  return {};
}
