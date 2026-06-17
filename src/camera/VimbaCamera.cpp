#include "camera/VimbaCamera.h"

#include "camera/VimbaTriggerController.h"

#ifdef VISION_WITH_VIMBAX
#include <VmbC/VmbCTypeDefinitions.h>
#include <VmbCPP/Feature.h>
#include <VmbCPP/Frame.h>
#include <VmbCPP/IFrameObserver.h>

#include <chrono>

class VimbaCamera::FrameObserver : public VmbCPP::IFrameObserver
{
public:
  explicit FrameObserver(const VmbCPP::CameraPtr& camera)
    : VmbCPP::IFrameObserver(camera)
  {
  }

  void FrameReceived(const VmbCPP::FramePtr frame) override
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_frame = frame;
      m_ready = true;
    }
    m_condition.notify_one();
  }

  VmbCPP::FramePtr wait(int timeoutMs)
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    const bool ready = m_condition.wait_for(
      lock,
      std::chrono::milliseconds(timeoutMs),
      [this]() { return m_ready; });
    if (!ready)
    {
      return {};
    }

    m_ready = false;
    VmbCPP::FramePtr frame = m_frame;
    m_frame.reset();
    return frame;
  }

  void reset()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ready = false;
    m_frame.reset();
  }

private:
  std::mutex m_mutex;
  std::condition_variable m_condition;
  VmbCPP::FramePtr m_frame;
  bool m_ready = false;
};

namespace
{
constexpr int kFrameTimeoutMs = 1500;

QString vimbaErrorText(const QString& action, VmbErrorType error)
{
  return QString("%1: Vimba error %2").arg(action).arg(static_cast<int>(error));
}

bool featureIntValue(const VmbCPP::CameraPtr& camera, const char* name, VmbInt64_t& value)
{
  VmbCPP::FeaturePtr feature;
  return camera &&
    camera->GetFeatureByName(name, feature) == VmbErrorSuccess &&
    feature &&
    feature->GetValue(value) == VmbErrorSuccess;
}

bool isSoftwareTriggerMode(const CameraTriggerConfig& trigger)
{
  const QString mode = trigger.mode.trimmed().toLower();
  const QString line = trigger.cameraLine.trimmed();
  return mode.isEmpty() ||
    mode == "software" ||
    mode == "timed" ||
    mode == "timedsoftware" ||
    mode == "softwaretimed" ||
    line.isEmpty();
}
}
#endif

VimbaCamera::VimbaCamera(CameraConfig config)
  : m_config(std::move(config))
{
}

VimbaCamera::~VimbaCamera()
{
  close();
}

bool VimbaCamera::open()
{
#ifdef VISION_WITH_VIMBAX
  m_lastError.clear();
  VmbCPP::VmbSystem& system = VmbCPP::VmbSystem::GetInstance();
  VmbErrorType error = system.Startup();
  if (error != VmbErrorSuccess)
  {
    m_lastError = vimbaErrorText("VimbaX Startup fallito", error);
    return false;
  }
  m_systemStarted = true;

  const std::string id = m_config.deviceId.toStdString();
  if (id.empty())
  {
    m_lastError = "DeviceId Vimba vuoto";
    close();
    return false;
  }

  error = system.OpenCameraByID(id.c_str(), VmbAccessModeFull, m_camera);
  if (error != VmbErrorSuccess || !m_camera)
  {
    m_lastError = vimbaErrorText(QString("OpenCameraByID fallito: %1").arg(m_config.deviceId), error);
    close();
    return false;
  }

  setPreferredPixelFormat();

  VimbaTriggerController trigger(m_camera);
  QString triggerError;
  const bool triggerConfigured = isSoftwareTriggerMode(m_config.trigger)
    ? trigger.configureSoftware(&triggerError)
    : trigger.configureExternal(m_config.trigger, &triggerError);
  if (!triggerConfigured)
  {
    m_lastError = triggerError.isEmpty() ? "Configurazione trigger Vimba fallita" : triggerError;
    close();
    return false;
  }

  if (!prepareCapture())
  {
    if (m_lastError.isEmpty())
    {
      m_lastError = "Preparazione capture Vimba fallita";
    }
    close();
    return false;
  }

  error = m_camera->QueueFrame(m_frame);
  if (error != VmbErrorSuccess)
  {
    m_lastError = vimbaErrorText("QueueFrame iniziale fallito", error);
    close();
    return false;
  }

  if (!trigger.startAcquisition(&triggerError))
  {
    m_lastError = triggerError.isEmpty() ? "AcquisitionStart fallito" : triggerError;
    close();
    return false;
  }
  m_acquisitionStarted = true;
  return true;
#else
  m_lastError = "SDK VimbaX non disponibile";
  return false;
#endif
}

bool VimbaCamera::getFrame(cv::Mat& frame)
{
#ifdef VISION_WITH_VIMBAX
  m_lastError.clear();
  if (!m_camera || !m_frame || !m_observer)
  {
    m_lastError = "Camera Vimba non pronta per acquisire";
    return false;
  }

  auto observer = std::static_pointer_cast<FrameObserver>(m_observer);
  observer->reset();

  VimbaTriggerController trigger(m_camera);
  QString triggerError;
  if (!trigger.sendSoftwareTrigger(&triggerError))
  {
    m_lastError = triggerError.isEmpty() ? "TriggerSoftware fallito" : triggerError;
    return false;
  }

  const VmbCPP::FramePtr receivedFrame = observer->wait(kFrameTimeoutMs);
  if (!receivedFrame)
  {
    m_camera->FlushQueue();
    m_camera->QueueFrame(m_frame);
    m_lastError = QString("Timeout frame Vimba dopo %1 ms dal TriggerSoftware").arg(kFrameTimeoutMs);
    return false;
  }

  if (!copyFrameToMat(receivedFrame, frame))
  {
    if (m_lastError.isEmpty())
    {
      m_lastError = "Conversione frame Vimba fallita";
    }
    return false;
  }

  VmbErrorType error = m_camera->QueueFrame(m_frame);
  if (error != VmbErrorSuccess)
  {
    m_lastError = vimbaErrorText("QueueFrame successivo fallito", error);
    return false;
  }

  return !frame.empty();
#else
  m_lastError = "SDK VimbaX non disponibile";
  return false;
#endif
}

QString VimbaCamera::lastError() const
{
  return m_lastError;
}

void VimbaCamera::close()
{
#ifdef VISION_WITH_VIMBAX
  if (m_camera)
  {
    VimbaTriggerController trigger(m_camera);
    if (m_acquisitionStarted)
    {
      trigger.stopAcquisition();
      m_acquisitionStarted = false;
    }
    if (m_captureStarted)
    {
      m_camera->EndCapture();
      m_camera->FlushQueue();
      m_camera->RevokeAllFrames();
      m_captureStarted = false;
    }
    m_camera->Close();
    m_camera.reset();
  }

  m_frame.reset();
  m_observer.reset();

  if (m_systemStarted)
  {
    VmbCPP::VmbSystem::GetInstance().Shutdown();
    m_systemStarted = false;
  }
#endif
}

#ifdef VISION_WITH_VIMBAX
bool VimbaCamera::setPreferredPixelFormat()
{
  VmbCPP::FeaturePtr pixelFormat;
  if (!m_camera ||
      m_camera->GetFeatureByName("PixelFormat", pixelFormat) != VmbErrorSuccess ||
      !pixelFormat)
  {
    return false;
  }

  if (pixelFormat->SetValue("Bgr8") == VmbErrorSuccess)
  {
    return true;
  }

  if (pixelFormat->SetValue("Mono8") == VmbErrorSuccess)
  {
    return true;
  }

  return false;
}

bool VimbaCamera::prepareCapture()
{
  VmbInt64_t payloadSize = 0;
  if (!featureIntValue(m_camera, "PayloadSize", payloadSize) || payloadSize <= 0)
  {
    m_lastError = "PayloadSize Vimba non valido";
    return false;
  }

  m_frame = std::make_shared<VmbCPP::Frame>(payloadSize);
  m_observer = std::make_shared<FrameObserver>(m_camera);
  VmbErrorType error = m_frame->RegisterObserver(m_observer);
  if (error != VmbErrorSuccess)
  {
    m_lastError = vimbaErrorText("RegisterObserver fallito", error);
    return false;
  }
  error = m_camera->AnnounceFrame(m_frame);
  if (error != VmbErrorSuccess)
  {
    m_lastError = vimbaErrorText("AnnounceFrame fallito", error);
    return false;
  }
  error = m_camera->StartCapture();
  if (error != VmbErrorSuccess)
  {
    m_lastError = vimbaErrorText("StartCapture fallito", error);
    return false;
  }

  m_captureStarted = true;
  return true;
}

bool VimbaCamera::copyFrameToMat(const VmbCPP::FramePtr& vimbaFrame, cv::Mat& frame)
{
  VmbFrameStatusType status = VmbFrameStatusIncomplete;
  if (vimbaFrame->GetReceiveStatus(status) != VmbErrorSuccess || status != VmbFrameStatusComplete)
  {
    m_lastError = QString("Frame Vimba incompleto: status=%1").arg(static_cast<int>(status));
    return false;
  }

  const VmbUchar_t* buffer = nullptr;
  VmbUint32_t width = 0;
  VmbUint32_t height = 0;
  VmbPixelFormatType pixelFormat = VmbPixelFormatMono8;
  if (vimbaFrame->GetImage(buffer) != VmbErrorSuccess ||
      vimbaFrame->GetWidth(width) != VmbErrorSuccess ||
      vimbaFrame->GetHeight(height) != VmbErrorSuccess ||
      vimbaFrame->GetPixelFormat(pixelFormat) != VmbErrorSuccess ||
      !buffer ||
      width == 0 ||
      height == 0)
  {
    m_lastError = "Lettura buffer/dimensioni/pixel format Vimba fallita";
    return false;
  }

  int cvType = CV_8UC1;
  if (pixelFormat == VmbPixelFormatBgr8 || pixelFormat == VmbPixelFormatRgb8)
  {
    cvType = CV_8UC3;
  }
  else if (pixelFormat != VmbPixelFormatMono8)
  {
    m_lastError = QString("PixelFormat Vimba non supportato: %1").arg(static_cast<qulonglong>(pixelFormat));
    return false;
  }

  cv::Mat wrapped(static_cast<int>(height), static_cast<int>(width), cvType, const_cast<VmbUchar_t*>(buffer));
  wrapped.copyTo(frame);
  return true;
}
#endif
