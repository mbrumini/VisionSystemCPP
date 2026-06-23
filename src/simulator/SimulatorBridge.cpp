#include "simulator/SimulatorBridge.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <opencv2/imgcodecs.hpp>

#include <Windows.h>

#include <thread>
#include <vector>

namespace
{
constexpr int kProtocolVersion = 1;
constexpr int kMaximumQueuedFramesPerChannel = 32;
const QString kServerName = QStringLiteral("VisionSystemSimulator");
const wchar_t* kPipePath = L"\\\\.\\pipe\\VisionSystemSimulator";
}

struct SimulatorBridge::ClientConnection
{
  HANDLE pipe = INVALID_HANDLE_VALUE;
  std::mutex writeMutex;
  std::atomic_bool connected = true;
};

SimulatorBridge& SimulatorBridge::instance()
{
  static SimulatorBridge* bridge = new SimulatorBridge();
  return *bridge;
}

bool SimulatorBridge::start(QString* errorMessage)
{
  bool expected = false;
  if (!m_running.compare_exchange_strong(expected, true))
  {
    return true;
  }

  HANDLE probe = CreateNamedPipeW(
    kPipePath,
    PIPE_ACCESS_DUPLEX,
    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
    PIPE_UNLIMITED_INSTANCES,
    1024 * 1024,
    1024 * 1024,
    0,
    nullptr);
  if (probe == INVALID_HANDLE_VALUE)
  {
    m_running = false;
    if (errorMessage)
    {
      *errorMessage = QString("Avvio named pipe simulatore fallito: errore Windows %1")
        .arg(GetLastError());
    }
    return false;
  }
  CloseHandle(probe);

  std::thread([this]() { acceptLoop(); }).detach();
  return true;
}

void SimulatorBridge::stop()
{
  m_running = false;
}

bool SimulatorBridge::running() const
{
  return m_running;
}

QString SimulatorBridge::serverName() const
{
  return kServerName;
}

bool SimulatorBridge::takeFrame(const QString& channel, SimulatorFrame& frame)
{
  std::lock_guard lock(m_mutex);
  auto it = m_framesByChannel.find(channel);
  if (it == m_framesByChannel.end() || it->isEmpty())
  {
    return false;
  }

  frame = it->dequeue();
  return !frame.image.empty();
}

void SimulatorBridge::setFrameAvailableHandler(std::function<void(const QString&)> handler)
{
  std::lock_guard lock(m_mutex);
  m_frameAvailableHandler = std::move(handler);
}

void SimulatorBridge::setSampleAvailableHandler(
  std::function<void(const SimulatorFrame&)> handler)
{
  std::lock_guard lock(m_mutex);
  m_sampleAvailableHandler = std::move(handler);
}

void SimulatorBridge::setRecipeRequestHandler(
  std::function<bool(const QString&, QString*)> handler)
{
  std::lock_guard lock(m_mutex);
  m_recipeRequestHandler = std::move(handler);
}

void SimulatorBridge::publishResult(const SimulatorFrameMetadata& metadata, const QJsonObject& result)
{
  if (!metadata.valid())
  {
    return;
  }

  std::shared_ptr<ClientConnection> client;
  {
    std::lock_guard lock(m_mutex);
    client = m_frameClients.take(frameKey(metadata));
  }
  if (!client)
  {
    return;
  }

  QJsonObject message = result;
  message["type"] = "result";
  message["protocolVersion"] = kProtocolVersion;
  message["scenarioId"] = metadata.scenarioId;
  message["cameraId"] = metadata.cameraId;
  message["channel"] = metadata.channel;
  message["slot"] = metadata.slot;
  message["frameId"] = metadata.frameId;
  std::thread([this, client, message]() {
    sendMessage(client, message);
  }).detach();
}

void SimulatorBridge::publishFrameEvent(
  const SimulatorFrameMetadata& metadata,
  const QString& type,
  const QString& message)
{
  if (!metadata.valid())
  {
    return;
  }

  std::shared_ptr<ClientConnection> client;
  {
    std::lock_guard lock(m_mutex);
    client = m_frameClients.value(frameKey(metadata));
  }
  if (!client)
  {
    return;
  }

  QJsonObject event;
  event["type"] = type;
  event["protocolVersion"] = kProtocolVersion;
  event["scenarioId"] = metadata.scenarioId;
  event["cameraId"] = metadata.cameraId;
  event["channel"] = metadata.channel;
  event["slot"] = metadata.slot;
  event["frameId"] = metadata.frameId;
  if (!message.isEmpty())
  {
    event["message"] = message;
  }
  std::thread([this, client, event]() {
    sendMessage(client, event);
  }).detach();
}

void SimulatorBridge::publishChannelEvent(
  const QString& channel,
  const QString& type,
  const QString& message)
{
  SimulatorFrameMetadata metadata;
  {
    std::lock_guard lock(m_mutex);
    const auto it = m_framesByChannel.constFind(channel);
    if (it == m_framesByChannel.constEnd() || it->isEmpty())
    {
      return;
    }
    metadata = it->head().metadata;
  }
  publishFrameEvent(metadata, type, message);
}

void SimulatorBridge::acceptLoop()
{
  while (m_running)
  {
    HANDLE pipe = CreateNamedPipeW(
      kPipePath,
      PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES,
      1024 * 1024,
      1024 * 1024,
      0,
      nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
    {
      m_running = false;
      return;
    }

    const BOOL connected = ConnectNamedPipe(pipe, nullptr)
      ? TRUE
      : (GetLastError() == ERROR_PIPE_CONNECTED);
    if (!connected)
    {
      CloseHandle(pipe);
      continue;
    }

    auto client = std::make_shared<ClientConnection>();
    client->pipe = pipe;
    std::thread([this, client]() { clientLoop(client); }).detach();
  }
}

void SimulatorBridge::clientLoop(const std::shared_ptr<ClientConnection>& client)
{
  sendMessage(client, {
    {"type", "hello"},
    {"protocolVersion", kProtocolVersion},
    {"server", kServerName}
  });

  QByteArray buffer;
  std::vector<char> chunk(64 * 1024);
  while (m_running && client->connected)
  {
    DWORD available = 0;
    if (!PeekNamedPipe(
          client->pipe,
          nullptr,
          0,
          nullptr,
          &available,
          nullptr))
    {
      break;
    }
    if (available == 0)
    {
      Sleep(5);
      continue;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(
          client->pipe,
          chunk.data(),
          std::min<DWORD>(available, static_cast<DWORD>(chunk.size())),
          &bytesRead,
          nullptr) ||
        bytesRead == 0)
    {
      break;
    }

    buffer.append(chunk.data(), static_cast<qsizetype>(bytesRead));
    while (true)
    {
      const qsizetype newline = buffer.indexOf('\n');
      if (newline < 0)
      {
        break;
      }

      const QByteArray line = buffer.left(newline).trimmed();
      buffer.remove(0, newline + 1);
      if (line.isEmpty())
      {
        continue;
      }

      QJsonParseError parseError;
      const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
      if (parseError.error != QJsonParseError::NoError || !document.isObject())
      {
        sendMessage(client, {
          {"type", "error"},
          {"code", "invalid_json"},
          {"message", parseError.errorString()}
        });
        continue;
      }

      processMessage(client, document.object());
    }
  }

  {
    std::lock_guard lock(client->writeMutex);
    client->connected = false;
    FlushFileBuffers(client->pipe);
    DisconnectNamedPipe(client->pipe);
    CloseHandle(client->pipe);
    client->pipe = INVALID_HANDLE_VALUE;
  }
}

void SimulatorBridge::processMessage(
  const std::shared_ptr<ClientConnection>& client,
  const QJsonObject& message)
{
  const QString type = message.value("type").toString();
  if (type == "ping")
  {
    sendMessage(client, {
      {"type", "pong"},
      {"protocolVersion", kProtocolVersion}
    });
    return;
  }

  if (type == "setRecipe")
  {
    const QString recipeId = message.value("recipeId").toString().trimmed();
    std::function<bool(const QString&, QString*)> handler;
    {
      std::lock_guard lock(m_mutex);
      handler = m_recipeRequestHandler;
    }
    QString error;
    const bool accepted =
      !recipeId.isEmpty() && handler && handler(recipeId, &error);
    sendMessage(client, {
      {"type", accepted ? "recipeAccepted" : "recipeRejected"},
      {"protocolVersion", kProtocolVersion},
      {"recipeId", recipeId},
      {"message", accepted ? "Ricetta caricata" : error}
    });
    return;
  }

  if (type != "frame" && type != "sample")
  {
    sendMessage(client, {
      {"type", "error"},
      {"code", "unsupported_message"},
      {"message", QString("Tipo messaggio non supportato: %1").arg(type)}
    });
    return;
  }

  const int protocolVersion = message.value("protocolVersion").toInt();
  const QString channel = message.value("channel").toString();
  const QString cameraId = message.value("cameraId").toString(channel);
  const qint64 frameId = message.value("frameId").toVariant().toLongLong();
  const QByteArray encodedImage = message.value("imageBase64").toString().toLatin1();
  if (protocolVersion != kProtocolVersion || channel.isEmpty() ||
      !message.contains("frameId") || frameId < 0 || encodedImage.isEmpty())
  {
    sendMessage(client, {
      {"type", "error"},
      {"code", "invalid_frame"},
      {"message", "Frame incompleto o versione protocollo non supportata"}
    });
    return;
  }

  const QByteArray imageBytes = QByteArray::fromBase64(encodedImage);
  const std::vector<uchar> bytes(imageBytes.begin(), imageBytes.end());
  cv::Mat image = cv::imdecode(bytes, cv::IMREAD_COLOR);
  if (image.empty())
  {
    sendMessage(client, {
      {"type", "error"},
      {"code", "invalid_image"},
      {"message", "Immagine base64 non decodificabile"}
    });
    return;
  }

  SimulatorFrame frame;
  frame.metadata.protocolVersion = protocolVersion;
  frame.metadata.scenarioId = message.value("scenarioId").toString();
  frame.metadata.cameraId = cameraId;
  frame.metadata.channel = channel;
  frame.metadata.strategyId = message.value("strategyId").toString();
  frame.metadata.recipeId = message.value("recipeId").toString();
  frame.metadata.slot = message.value("slot").toInt();
  frame.metadata.frameId = frameId;
  frame.metadata.timestamp = QDateTime::fromString(
    message.value("timestamp").toString(),
    Qt::ISODateWithMs);
  frame.image = image;

  if (type == "sample")
  {
    std::function<void(const SimulatorFrame&)> sampleAvailableHandler;
    {
      std::lock_guard lock(m_mutex);
      sampleAvailableHandler = m_sampleAvailableHandler;
    }
    sendMessage(client, {
      {"type", "sampleAccepted"},
      {"protocolVersion", kProtocolVersion},
      {"scenarioId", frame.metadata.scenarioId},
      {"cameraId", frame.metadata.cameraId},
      {"channel", frame.metadata.channel},
      {"slot", frame.metadata.slot}
    });
    if (sampleAvailableHandler)
    {
      sampleAvailableHandler(frame);
    }
    return;
  }

  int queueDepth = 0;
  std::function<void(const QString&)> frameAvailableHandler;
  {
    std::lock_guard lock(m_mutex);
    QQueue<SimulatorFrame>& queue = m_framesByChannel[channel];
    while (queue.size() >= kMaximumQueuedFramesPerChannel)
    {
      const SimulatorFrame dropped = queue.dequeue();
      const QString droppedKey = frameKey(dropped.metadata);
      std::shared_ptr<ClientConnection> droppedClient = m_frameClients.take(droppedKey);
      if (droppedClient)
      {
        QJsonObject result;
        result["type"] = "result";
        result["protocolVersion"] = kProtocolVersion;
        result["scenarioId"] = dropped.metadata.scenarioId;
        result["cameraId"] = dropped.metadata.cameraId;
        result["channel"] = dropped.metadata.channel;
        result["slot"] = dropped.metadata.slot;
        result["frameId"] = dropped.metadata.frameId;

        QJsonObject poseObject;
        poseObject["valid"] = false;
        poseObject["x"] = 0.0;
        poseObject["y"] = 0.0;
        poseObject["angleDeg"] = 0.0;
        poseObject["score"] = 0.0;
        poseObject["method"] = "queue_overflow";
        result["pose"] = poseObject;
        result["processingMs"] = 0.0;

        QJsonArray errors;
        errors.append("Coda piena: frame scartato");
        result["errors"] = errors;

        std::thread([this, droppedClient, result]() {
          sendMessage(droppedClient, result);
        }).detach();
      }
    }
    queue.enqueue(frame);
    queueDepth = queue.size();
    m_frameClients.insert(frameKey(frame.metadata), client);
    frameAvailableHandler = m_frameAvailableHandler;
  }

  sendMessage(client, {
    {"type", "frameAccepted"},
    {"protocolVersion", kProtocolVersion},
    {"scenarioId", frame.metadata.scenarioId},
    {"cameraId", frame.metadata.cameraId},
    {"channel", frame.metadata.channel},
    {"slot", frame.metadata.slot},
    {"frameId", frame.metadata.frameId},
    {"queueDepth", queueDepth}
  });
  if (frameAvailableHandler)
  {
    frameAvailableHandler(channel);
  }
}

void SimulatorBridge::sendMessage(
  const std::shared_ptr<ClientConnection>& client,
  const QJsonObject& message)
{
  if (!client)
  {
    return;
  }

  QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
  payload.append('\n');

  std::lock_guard lock(client->writeMutex);
  if (!client->connected || client->pipe == INVALID_HANDLE_VALUE)
  {
    return;
  }
  DWORD written = 0;
  if (!WriteFile(
        client->pipe,
        payload.constData(),
        static_cast<DWORD>(payload.size()),
        &written,
        nullptr))
  {
    client->connected = false;
  }
}

QString SimulatorBridge::frameKey(const SimulatorFrameMetadata& metadata) const
{
  return QString("%1|%2|%3")
    .arg(metadata.scenarioId, metadata.channel)
    .arg(metadata.frameId);
}

int SimulatorBridge::queueSize(const QString& channel) const
{
  std::lock_guard lock(m_mutex);
  auto it = m_framesByChannel.constFind(channel);
  if (it == m_framesByChannel.constEnd())
  {
    return 0;
  }
  return it->size();
}

int SimulatorBridge::discardAllQueuedFrames(const QString& reason)
{
  std::vector<std::pair<std::shared_ptr<ClientConnection>, QJsonObject>> droppedResults;

  {
    std::lock_guard lock(m_mutex);
    for (auto channelIt = m_framesByChannel.begin(); channelIt != m_framesByChannel.end(); ++channelIt)
    {
      QQueue<SimulatorFrame>& queue = channelIt.value();
      while (!queue.isEmpty())
      {
        const SimulatorFrame dropped = queue.dequeue();
        const QString droppedKey = frameKey(dropped.metadata);
        std::shared_ptr<ClientConnection> droppedClient = m_frameClients.take(droppedKey);
        if (!droppedClient)
        {
          continue;
        }

        QJsonObject result;
        result["type"] = "result";
        result["protocolVersion"] = kProtocolVersion;
        result["scenarioId"] = dropped.metadata.scenarioId;
        result["cameraId"] = dropped.metadata.cameraId;
        result["channel"] = dropped.metadata.channel;
        result["slot"] = dropped.metadata.slot;
        result["frameId"] = dropped.metadata.frameId;

        QJsonObject poseObject;
        poseObject["valid"] = false;
        poseObject["x"] = 0.0;
        poseObject["y"] = 0.0;
        poseObject["angleDeg"] = 0.0;
        poseObject["score"] = 0.0;
        poseObject["method"] = "queue_discarded";
        result["pose"] = poseObject;
        result["processingMs"] = 0.0;

        QJsonArray errors;
        errors.append(reason.isEmpty() ? QStringLiteral("Frame scartato") : reason);
        result["errors"] = errors;

        droppedResults.emplace_back(std::move(droppedClient), std::move(result));
      }
    }
    m_framesByChannel.clear();
  }

  for (const auto& dropped : droppedResults)
  {
    std::thread([this, client = dropped.first, result = dropped.second]() {
      sendMessage(client, result);
    }).detach();
  }

  return static_cast<int>(droppedResults.size());
}
