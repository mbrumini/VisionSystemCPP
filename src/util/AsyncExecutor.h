#pragma once

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>
#include <QDateTime>

#include <functional>
#include <memory>
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
  QPointer<QObject> context(parent);

  auto finish = [context, callback = std::move(callback), taskName, start](auto&&... args) mutable {
    const qint64 end = QDateTime::currentMSecsSinceEpoch();
    if (g_metricsHandler)
    {
      g_metricsHandler(taskName, end - start);
    }
    if (context)
    {
      callback(std::forward<decltype(args)>(args)...);
    }
  };

  QThreadPool::globalInstance()->start(QRunnable::create([job = std::move(job), context, finish = std::move(finish)]() mutable {
    if constexpr (std::is_void_v<Result>)
    {
      job();
      if (context)
      {
        QMetaObject::invokeMethod(context, [finish = std::move(finish)]() mutable {
          finish();
        }, Qt::QueuedConnection);
      }
    }
    else
    {
      auto result = std::make_shared<Result>(job());
      if (context)
      {
        QMetaObject::invokeMethod(context, [finish = std::move(finish), result]() mutable {
          finish(*result);
        }, Qt::QueuedConnection);
      }
    }
  }));
}

} // namespace AsyncExecutor
