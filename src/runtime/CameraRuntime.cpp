#include "CameraRuntime.h"

#include "camera/FileCamera.h"
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

  if (m_source)
  {
    m_source->close();
    m_source.reset();
  }
  m_sourceType.clear();
  m_sourceFolder.clear();
  m_sourceUsbIndex = -1;

  m_status = Status::Stopped;
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
    m_running = false;
    m_status = Status::Stopped;
    if (errorMessage)
    {
      *errorMessage = "Nessun altro frame";
    }
    return false;
  }

  m_currentFrame = frame;
  m_frameIndex += 1;
  m_status = m_running ? Status::Running : Status::Ready;
  return true;
}

bool CameraRuntime::running() const
{
  return m_running;
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

void CameraRuntime::clearCurrentFrame()
{
  m_currentFrame.release();
  m_frameIndex = 0;
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
  if (camera.type != "file" && camera.type != "usb" && camera.type != "vimba")
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

  if (m_source && (sameUsbSource || sameFileSource))
  {
    return true;
  }

  if (m_source)
  {
    m_source->close();
    m_source.reset();
    m_sourceType.clear();
    m_sourceFolder.clear();
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

    m_source = std::make_unique<UsbCamera>(camera.usbIndex);
  }
  else if (camera.type == "vimba")
  {
    m_source = std::make_unique<VimbaCamera>(camera);
  }
  else
  {
    m_source = std::make_unique<FileCamera>(resolvedFolder.toStdString(), m_loop);
  }

  if (!m_source->open())
  {
    m_source.reset();
    m_sourceType.clear();
    m_sourceFolder.clear();
    m_sourceUsbIndex = -1;
    if (errorMessage)
    {
      *errorMessage = QString("Avvio camera fallito: %1 type=%2").arg(camera.id, camera.type);
    }
    return false;
  }

  m_sourceType = camera.type;
  m_sourceFolder = camera.type == "file" ? resolvedFolder : QString();
  m_sourceUsbIndex = camera.type == "usb" ? camera.usbIndex : -1;
  return true;
}
