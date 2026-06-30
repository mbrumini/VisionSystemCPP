#include "camera/VimbaCamera.h"

#include "camera/VimbaTriggerController.h"

#ifdef VISION_WITH_VIMBAX
#include <VmbC/VmbCTypeDefinitions.h>
#include <VmbCPP/Feature.h>
#include <VmbCPP/Frame.h>
#include <VmbCPP/IFrameObserver.h>

#include <algorithm>
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

bool setFeatureEnumValue(const VmbCPP::CameraPtr& camera, const char* name, const char* value)
{
  VmbCPP::FeaturePtr feature;
  return camera &&
    camera->GetFeatureByName(name, feature) == VmbErrorSuccess &&
    feature &&
    feature->SetValue(value) == VmbErrorSuccess;
}

bool setFeatureDoubleValue(const VmbCPP::CameraPtr& camera, const char* name, double value)
{
  VmbCPP::FeaturePtr feature;
  return camera &&
    camera->GetFeatureByName(name, feature) == VmbErrorSuccess &&
    feature &&
    feature->SetValue(value) == VmbErrorSuccess;
}

bool setRequiredEnumFeature(
  const VmbCPP::CameraPtr& camera,
  const char* name,
  const char* value,
  QString& errorMessage)
{
  VmbCPP::FeaturePtr feature;
  VmbErrorType error = camera ? camera->GetFeatureByName(name, feature) : VmbErrorBadHandle;
  if (error != VmbErrorSuccess || !feature)
  {
    errorMessage = vimbaErrorText(QString("Feature %1 non disponibile").arg(name), error);
    return false;
  }

  error = feature->SetValue(value);
  if (error != VmbErrorSuccess)
  {
    errorMessage = vimbaErrorText(QString("Impostazione %1=%2 fallita").arg(name, value), error);
    return false;
  }
  return true;
}

void setOptionalEnumFeature(const VmbCPP::CameraPtr& camera, const char* name, const char* value)
{
  VmbCPP::FeaturePtr feature;
  if (camera &&
      camera->GetFeatureByName(name, feature) == VmbErrorSuccess &&
      feature)
  {
    feature->SetValue(value);
  }
}

bool setRequiredDoubleFeature(
  const VmbCPP::CameraPtr& camera,
  const char* name,
  bool hasRequestedValue,
  double requestedValue,
  QString& errorMessage)
{
  VmbCPP::FeaturePtr feature;
  VmbErrorType error = camera ? camera->GetFeatureByName(name, feature) : VmbErrorBadHandle;
  if (error != VmbErrorSuccess || !feature)
  {
    errorMessage = vimbaErrorText(QString("Feature %1 non disponibile").arg(name), error);
    return false;
  }

  double minimum = requestedValue;
  double maximum = requestedValue;
  if (feature->GetRange(minimum, maximum) != VmbErrorSuccess)
  {
    errorMessage = QString("Lettura range Vimba %1 fallita").arg(name);
    return false;
  }

  const double appliedValue = hasRequestedValue
    ? std::clamp(requestedValue, minimum, maximum)
    : minimum;
  error = feature->SetValue(appliedValue);
  if (error != VmbErrorSuccess)
  {
    errorMessage = vimbaErrorText(
      QString("Impostazione %1=%2 fallita (range %3..%4)")
        .arg(name)
        .arg(appliedValue, 0, 'f', 3)
        .arg(minimum, 0, 'f', 3)
        .arg(maximum, 0, 'f', 3),
      error);
    return false;
  }

  double readBack = 0.0;
  error = feature->GetValue(readBack);
  if (error != VmbErrorSuccess)
  {
    errorMessage = vimbaErrorText(QString("Verifica %1 fallita").arg(name), error);
    return false;
  }
  return true;
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
  if (!applyAcquisitionSettings())
  {
    close();
    return false;
  }

  VimbaTriggerController trigger(m_camera);
  QString triggerError;
  m_softwareTrigger = isSoftwareTriggerMode(m_config.trigger);
  const bool triggerConfigured = m_softwareTrigger
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
  if (m_softwareTrigger && !trigger.sendSoftwareTrigger(&triggerError))
  {
    m_lastError = triggerError.isEmpty() ? "TriggerSoftware fallito" : triggerError;
    return false;
  }

  const VmbCPP::FramePtr receivedFrame = observer->wait(kFrameTimeoutMs);
  if (!receivedFrame)
  {
    m_camera->FlushQueue();
    m_camera->QueueFrame(m_frame);
    m_lastError = m_softwareTrigger
      ? QString("Timeout frame Vimba dopo %1 ms dal TriggerSoftware").arg(kFrameTimeoutMs)
      : QString("Timeout frame Vimba dopo %1 ms in attesa di trigger esterno").arg(kFrameTimeoutMs);
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

bool VimbaCamera::setAcquisitionSettings(const CameraAcquisitionConfig& acquisition)
{
#ifdef VISION_WITH_VIMBAX
  m_config.acquisition = acquisition;
  m_lastError.clear();
  return applyAcquisitionSettings();
#else
  Q_UNUSED(acquisition);
  m_lastError = "SDK VimbaX non disponibile";
  return false;
#endif
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

bool VimbaCamera::applyAcquisitionSettings()
{
  if (!m_camera)
  {
    m_lastError = "Camera Vimba non disponibile per configurare l'acquisizione";
    return false;
  }

  const CameraAcquisitionConfig& acquisition = m_config.acquisition;

  setOptionalEnumFeature(m_camera, "ExposureMode", "Timed");
  if (!setRequiredEnumFeature(
        m_camera,
        "ExposureAuto",
        acquisition.autoExposure ? "Continuous" : "Off",
        m_lastError))
  {
    return false;
  }
  if (!acquisition.autoExposure)
  {
    if (!setRequiredDoubleFeature(
          m_camera,
          "ExposureTime",
          acquisition.hasExposure,
          acquisition.exposure,
          m_lastError))
    {
      return false;
    }
  }

  setOptionalEnumFeature(m_camera, "GainSelector", "All");
  if (!setRequiredEnumFeature(
        m_camera,
        "GainAuto",
        acquisition.autoGain ? "Continuous" : "Off",
        m_lastError))
  {
    return false;
  }
  if (!acquisition.autoGain)
  {
    if (!setRequiredDoubleFeature(
          m_camera,
          "Gain",
          acquisition.hasGain,
          acquisition.gain,
          m_lastError))
    {
      return false;
    }
  }

  setFeatureEnumValue(m_camera, "BalanceWhiteAuto", acquisition.autoWhiteBalance ? "Continuous" : "Off");
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
