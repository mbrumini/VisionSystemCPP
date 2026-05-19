#pragma once

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QDateTime>
#include <QVariant>

#include <functional>
#include <type_traits>
#include <QString>
#include <algorithm>
#include <thread>

namespace AsyncExecutor
{
using MetricsHandler = std::function<void(const QString&, qint64)>;

static inline MetricsHandler g_metricsHandler = nullptr;

inline void setMetricsHandler(MetricsHandler handler)
{
  g_metricsHandler = std::move(handler);
}

inline void setMaxThreads(int n)
{
  QThreadPool::globalInstance()->setMaxThreadCount(std::max(1, n));
}

inline void setDefaultMaxThreadsToHardware()
{
  const unsigned int hc = std::thread::hardware_concurrency();
  setMaxThreads(hc == 0 ? 1 : static_cast<int>(hc));
}

template<typename F, typename Callback>
void runAsyncTask(F job, QObject* parent, Callback callback, const QString& taskName = QString())
{
  using Result = std::invoke_result_t<F>;

  const qint64 start = QDateTime::currentMSecsSinceEpoch();
  QFuture<Result> future = QtConcurrent::run(job);
  QFutureWatcher<Result>* watcher = new QFutureWatcher<Result>(parent);
  watcher->setProperty("startTime", QVariant::fromValue(start));
  watcher->setProperty("taskName", QVariant::fromValue(taskName));
  watcher->setFuture(future);

  QObject::connect(watcher, &QFutureWatcher<Result>::finished, parent, [watcher, callback]() mutable {
    const qint64 end = QDateTime::currentMSecsSinceEpoch();
    const qint64 start = watcher->property("startTime").toLongLong();
    const QString name = watcher->property("taskName").toString();

    if constexpr (std::is_void_v<Result>)
    {
      watcher->deleteLater();
      if (g_metricsHandler)
      {
        g_metricsHandler(name, end - start);
      }
      callback();
    }
    else
    {
      Result result = watcher->result();
      watcher->deleteLater();
      if (g_metricsHandler)
      {
        g_metricsHandler(name, end - start);
      }
      callback(result);
    }
  });
}

} // namespace AsyncExecutor
