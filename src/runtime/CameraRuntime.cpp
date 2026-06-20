#include "CameraRuntime.h"

#include "camera/FileCamera.h"
#include "camera/SimulatedCamera.h"
#include "camera/UsbCamera.h"
#include "camera/VimbaCamera.h"

bool CameraRuntime::start(const CameraConfig& camera, const QString& resolvedFolder, QString* errorMessage)
{
  stop();

  if (!ensureSource(camera, resolvedFolder, errorMessage))
  {
    m_status = Status::Error;
    return false;
  }

  m_running = true;
  m_frameIndex = 0;
  m_status = Status::Running;
  return true;
}

void CameraRuntime::stop()
{
  m_running = false;

  std::shared_ptr<ICamera> sourceToClose;
  {
    std::lock_guard<std::mutex> lock(m_sourceMutex);
    sourceToClose = m_source;
    m_source.reset();
  }

  if (sourceToClose)
  {
    sourceToClose->close();
  }

  m_sourceType.clear();
  m_sourceFolder.clear();
  m_sourceDeviceId.clear();
  m_sourceUsbIndex = -1;
  m_currentSimulatorFrame = {};

  m_status = Status::Stopped;
}

bool CameraRuntime::applyAcquisitionSettings(
  const CameraAcquisitionConfig& acquisition,
  QString* errorMessage)
{
  std::shared_ptr<ICamera> source;
  {
    std::lock_guard<std::mutex> lock(m_sourceMutex);
    source = m_source;
  }

  if (!source)
  {
    if (errorMessage)
    {
      *errorMessage = "La sorgente camera non e' attiva o aperta";
    }
    return false;
  }

  if (auto* vimbaCamera = dynamic_cast<VimbaCamera*>(source.get()))
  {
    if (vimbaCamera->setAcquisitionSettings(acquisition))
    {
      return true;
    }
    if (errorMessage)
    {
      *errorMessage = vimbaCamera->lastError();
    }
    return false;
  }

  if (auto* usbCamera = dynamic_cast<UsbCamera*>(source.get()))
  {
    if (usbCamera->setAcquisitionSettings(acquisition))
    {
      return true;
    }
    if (errorMessage)
    {
      *errorMessage = "Applicazione parametri USB fallita";
    }
    return false;
  }

  if (errorMessage)
  {
    *errorMessage = "La sorgente camera non supporta parametri acquisizione live";
  }
  return false;
}

bool CameraRuntime::step(const CameraConfig& camera, const QString& resolvedFolder, QString* errorMessage)
{
  if (!ensureSource(camera, resolvedFolder, errorMessage))
  {
    m_running = false;
    m_status = Status::Error;
    return false;
  }

  cv::Mat frame;
  if (!m_source->getFrame(frame))
  {
    const bool transientLiveGrabFailure =
      (camera.type == "vimba" || camera.type == "simulator") && m_running;
    if (!transientLiveGrabFailure)
    {
      m_running = false;
    }
    m_status = transientLiveGrabFailure ? Status::Running : Status::Stopped;
    if (errorMessage)
    {
      if (const auto* vimbaCamera = dynamic_cast<const VimbaCamera*>(m_source.get());
          vimbaCamera && !vimbaCamera->lastError().isEmpty())
      {
        *errorMessage = "Grab Vimba fallito: " + vimbaCamera->lastError();
      }
      else
      {
        *errorMessage = "Nessun altro frame";
      }
    }
    return false;
  }

  m_currentFrame = frame;
  if (const auto* simulatedCamera = dynamic_cast<const SimulatedCamera*>(m_source.get()))
  {
    m_currentSimulatorFrame = simulatedCamera->lastFrameMetadata();
  }
  else
  {
    m_currentSimulatorFrame = {};
  }
  m_frameIndex += 1;
  m_status = m_running ? Status::Running : Status::Ready;
  return true;
}

bool CameraRuntime::running() const
{
  return m_running;
}

std::shared_ptr<ICamera> CameraRuntime::source() const
{
  return m_source;
}

bool CameraRuntime::loop() const
{
  return m_loop;
}

void CameraRuntime::setLoop(bool loop)
{
  m_loop = loop;
}

int CameraRuntime::intervalMs() const
{
  return m_intervalMs;
}

void CameraRuntime::setIntervalMs(int intervalMs)
{
  m_intervalMs = intervalMs;
}

int CameraRuntime::frameIndex() const
{
  return m_frameIndex;
}

CameraRuntime::Status CameraRuntime::status() const
{
  return m_status;
}

const cv::Mat& CameraRuntime::currentFrame() const
{
  return m_currentFrame;
}

void CameraRuntime::setCurrentFrame(const cv::Mat& frame)
{
  m_currentFrame = frame.clone();
  m_currentSimulatorFrame = {};
}

const SimulatorFrameMetadata& CameraRuntime::currentSimulatorFrame() const
{
  return m_currentSimulatorFrame;
}

void CameraRuntime::clearCurrentFrame()
{
  m_currentFrame.release();
  m_frameIndex = 0;
  m_currentSimulatorFrame = {};
}

const PartPose& CameraRuntime::currentPose() const
{
  return m_currentPose;
}

void CameraRuntime::setCurrentPose(const PartPose& pose)
{
  m_currentPose = pose;
}

void CameraRuntime::clearCurrentPose(const QString& cameraId)
{
  m_currentPose = makeInvalidPartPose(cameraId);
}

const GeometrySet& CameraRuntime::geometries() const
{
  return m_geometries;
}

GeometrySet& CameraRuntime::geometries()
{
  return m_geometries;
}

void CameraRuntime::clearGeometries()
{
  m_geometries.clear();
}

bool CameraRuntime::ensureSource(const CameraConfig& camera, const QString& resolvedFolder, QString* errorMessage)
{
  std::lock_guard<std::mutex> lock(m_sourceMutex);
  if (camera.type != "file" && camera.type != "usb" &&
      camera.type != "vimba" && camera.type != "simulator")
  {
    if (errorMessage)
    {
      *errorMessage = "Sorgente camera non supportata: " + camera.type;
    }
    return false;
  }

  const bool sameUsbSource =
    camera.type == "usb" &&
    m_sourceType == camera.type &&
    m_sourceUsbIndex == camera.usbIndex;
  const bool sameFileSource =
    camera.type == "file" &&
    m_sourceType == camera.type &&
    m_sourceFolder == resolvedFolder;
  const bool sameVimbaSource =
    camera.type == "vimba" &&
    m_sourceType == camera.type &&
    m_sourceDeviceId == camera.deviceId;
  const bool sameSimulatorSource =
    camera.type == "simulator" &&
    m_sourceType == camera.type &&
    m_sourceDeviceId == (camera.simulatorChannel.isEmpty() ? camera.id : camera.simulatorChannel);

  if (m_source && (sameUsbSource || sameFileSource || sameVimbaSource || sameSimulatorSource))
  {
    return true;
  }

  if (m_source)
  {
    m_source->close();
    m_source.reset();
    m_sourceType.clear();
    m_sourceFolder.clear();
    m_sourceDeviceId.clear();
    m_sourceUsbIndex = -1;
  }

  if (camera.type == "usb")
  {
    if (camera.usbIndex < 0)
    {
      if (errorMessage)
      {
        *errorMessage = "Indice camera USB non valido: " + camera.id;
      }
      return false;
    }

    m_source = std::make_shared<UsbCamera>(camera);
  }
  else if (camera.type == "vimba")
  {
    m_source = std::make_shared<VimbaCamera>(camera);
  }
  else if (camera.type == "simulator")
  {
    m_source = std::make_shared<SimulatedCamera>(
      camera.simulatorChannel.isEmpty() ? camera.id : camera.simulatorChannel);
  }
  else
  {
    m_source = std::make_shared<FileCamera>(resolvedFolder.toStdString(), m_loop);
  }

  if (!m_source->open())
  {
    const QString sourceError =
      camera.type == "vimba"
        ? static_cast<VimbaCamera*>(m_source.get())->lastError()
        : QString();
    m_source.reset();
    m_sourceType.clear();
    m_sourceFolder.clear();
    m_sourceDeviceId.clear();
    m_sourceUsbIndex = -1;
    if (errorMessage)
    {
      *errorMessage = sourceError.isEmpty()
        ? QString("Avvio camera fallito: %1 type=%2").arg(camera.id, camera.type)
        : QString("Avvio camera fallito: %1 type=%2 - %3").arg(camera.id, camera.type, sourceError);
    }
    return false;
  }

  m_sourceType = camera.type;
  m_sourceFolder = camera.type == "file" ? resolvedFolder : QString();
  m_sourceDeviceId =
    camera.type == "vimba"
      ? camera.deviceId
      : (camera.type == "simulator"
          ? (camera.simulatorChannel.isEmpty() ? camera.id : camera.simulatorChannel)
          : QString());
  m_sourceUsbIndex = camera.type == "usb" ? camera.usbIndex : -1;
  return true;
}

bool CameraRuntime::grabFrame(const CameraConfig& camera, const QString& resolvedFolder, cv::Mat& frame, SimulatorFrameMetadata& metadata, QString& error)
{
  if (!ensureSource(camera, resolvedFolder, &error))
  {
    return false;
  }

  std::shared_ptr<ICamera> source;
  {
    std::lock_guard<std::mutex> lock(m_sourceMutex);
    source = m_source;
  }

  if (!source)
  {
    error = "Sorgente camera non valida";
    return false;
  }

  if (!source->getFrame(frame))
  {
    if (const auto* vimbaCamera = dynamic_cast<const VimbaCamera*>(source.get());
        vimbaCamera && !vimbaCamera->lastError().isEmpty())
    {
      error = "Grab Vimba fallito: " + vimbaCamera->lastError();
    }
    else
    {
      error = "Nessun altro frame";
    }
    return false;
  }

  if (const auto* simulatedCamera = dynamic_cast<const SimulatedCamera*>(source.get()))
  {
    metadata = simulatedCamera->lastFrameMetadata();
  }
  else
  {
    metadata = {};
  }

  return true;
}

void CameraRuntime::updateStateAfterGrab(const cv::Mat& frame, const SimulatorFrameMetadata& metadata)
{
  m_currentFrame = frame;
  m_currentSimulatorFrame = metadata;
  m_frameIndex += 1;
  m_status = m_running ? Status::Running : Status::Ready;
}
