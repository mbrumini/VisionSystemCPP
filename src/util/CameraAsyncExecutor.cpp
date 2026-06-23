#include "util/CameraAsyncExecutor.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QThreadPool>

namespace
{
QMutex g_poolsMutex;
QHash<QString, QThreadPool*> g_pools;
}

namespace CameraAsyncExecutor
{
QThreadPool* poolForCamera(const QString& cameraId)
{
  QMutexLocker lock(&g_poolsMutex);
  QThreadPool* pool = g_pools.value(cameraId, nullptr);
  if (!pool)
  {
    pool = new QThreadPool();
    pool->setMaxThreadCount(1);
    g_pools.insert(cameraId, pool);
  }
  return pool;
}

void ensurePools(const QStringList& cameraIds)
{
  for (const QString& cameraId : cameraIds)
  {
    if (!cameraId.isEmpty())
    {
      poolForCamera(cameraId);
    }
  }
}

void waitForAll()
{
  QMutexLocker lock(&g_poolsMutex);
  for (QThreadPool* pool : g_pools)
  {
    if (pool)
    {
      pool->waitForDone();
    }
  }
}

} // namespace CameraAsyncExecutor
