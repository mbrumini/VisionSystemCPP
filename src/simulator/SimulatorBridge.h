#pragma once

#include "simulator/SimulatorProtocol.h"

#include <QHash>
#include <QJsonObject>
#include <QQueue>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

class SimulatorBridge
{
public:
  static SimulatorBridge& instance();

  bool start(QString* errorMessage = nullptr);
  void stop();
  bool running() const;
  QString serverName() const;

  bool takeFrame(const QString& channel, SimulatorFrame& frame);
  int queueSize(const QString& channel) const;
  int discardAllQueuedFrames(const QString& reason = QStringLiteral("Vision in STOP"));
  void publishResult(const SimulatorFrameMetadata& metadata, const QJsonObject& result);
  void publishFrameEvent(
    const SimulatorFrameMetadata& metadata,
    const QString& type,
    const QString& message = {});
  void publishChannelEvent(
    const QString& channel,
    const QString& type,
    const QString& message = {});
  void setFrameAvailableHandler(std::function<void(const QString&)> handler);
  void setSampleAvailableHandler(std::function<void(const SimulatorFrame&)> handler);
  void setRecipeRequestHandler(
    std::function<bool(const QString&, QString*)> handler);

private:
  struct ClientConnection;

  SimulatorBridge() = default;

  void acceptLoop();
  void clientLoop(const std::shared_ptr<ClientConnection>& client);
  void processMessage(const std::shared_ptr<ClientConnection>& client, const QJsonObject& message);
  void sendMessage(const std::shared_ptr<ClientConnection>& client, const QJsonObject& message);
  QString frameKey(const SimulatorFrameMetadata& metadata) const;

  std::atomic_bool m_running = false;
  mutable std::mutex m_mutex;
  QHash<QString, QQueue<SimulatorFrame>> m_framesByChannel;
  QHash<QString, std::shared_ptr<ClientConnection>> m_frameClients;
  std::function<void(const QString&)> m_frameAvailableHandler;
  std::function<void(const SimulatorFrame&)> m_sampleAvailableHandler;
  std::function<bool(const QString&, QString*)> m_recipeRequestHandler;
};
