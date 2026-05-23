#pragma once

#include <QObject>
#include <QString>

#include <functional>

class QProcess;

class HelpModelBootstrapper : public QObject
{
public:
  using ReadyHandler = std::function<void()>;
  using ErrorHandler = std::function<void(const QString&)>;

  explicit HelpModelBootstrapper(QObject* parent = nullptr);

  bool isBusy() const;
  void ensureReady(ReadyHandler onReady, ErrorHandler onError);

private:
  void checkModels();
  void createModel();
  void tryStartServer();
  void finishReady();
  void fail(const QString& message);

  QString ollamaProgram() const;
  QString modelDirectory() const;
  QString modelfilePath() const;
  bool outputContainsModel(const QString& output) const;
  QString cleanOutput(const QString& text) const;

  QProcess* m_process = nullptr;
  ReadyHandler m_onReady;
  ErrorHandler m_onError;
  bool m_ready = false;
  int m_startAttempts = 0;
};
