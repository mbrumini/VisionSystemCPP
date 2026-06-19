#include "camera/SimulatedCamera.h"

#include "simulator/SimulatorBridge.h"

SimulatedCamera::SimulatedCamera(QString channel)
  : m_channel(std::move(channel))
{
}

bool SimulatedCamera::open()
{
  m_open = SimulatorBridge::instance().running() && !m_channel.isEmpty();
  return m_open;
}

bool SimulatedCamera::getFrame(cv::Mat& frame)
{
  if (!m_open)
  {
    return false;
  }

  SimulatorFrame simulatorFrame;
  if (!SimulatorBridge::instance().takeFrame(m_channel, simulatorFrame))
  {
    return false;
  }

  frame = simulatorFrame.image;
  m_lastFrameMetadata = simulatorFrame.metadata;
  return !frame.empty();
}

void SimulatedCamera::close()
{
  m_open = false;
  m_lastFrameMetadata = {};
}

const SimulatorFrameMetadata& SimulatedCamera::lastFrameMetadata() const
{
  return m_lastFrameMetadata;
}
