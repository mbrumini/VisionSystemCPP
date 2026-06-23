#pragma once

#include "util/AsyncExecutor.h"

#include <QString>
#include <QStringList>

class QThreadPool;

namespace CameraAsyncExecutor
{
// One serial queue per camera (maxThreadCount=1). Up to 16 cameras => 16 independent pipelines.
QThreadPool* poolForCamera(const QString& cameraId);
void ensurePools(const QStringList& cameraIds);
void waitForAll();

template<typename F, typename Callback>
void runAsyncTask(const QString& cameraId,
                  F job,
                  QObject* parent,
                  Callback callback,
                  const QString& taskName = QString())
{
  AsyncExecutor::runAsyncTaskOnPool(
    poolForCamera(cameraId),
    std::move(job),
    parent,
    std::move(callback),
    taskName);
}

} // namespace CameraAsyncExecutor
