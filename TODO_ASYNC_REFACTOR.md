Async refactor changes applied

Files added/modified:
- Added `src/util/AsyncExecutor.h`: generic header-only helper to run tasks via `QtConcurrent::run` + `QFutureWatcher`, measure execution time, and report metrics via a configurable handler.
- Added `src/gui/MetricsPanelWidget.h` and `src/gui/MetricsPanelWidget.cpp`: small UI widget that displays recent metrics (timestamped) in a list.
- Modified `src/gui/MainWindow.cpp`:
  - Included `AsyncExecutor` and wired default thread count to hardware concurrency.
  - Replaced inline helper with central `AsyncExecutor::runAsyncTask` usage.
  - Converted the following heavy `test*` functions to run asynchronously and update UI on completion:
    - `testGeometryLine`
    - `testGeometryCircle`
    - `testGeometryPoint`
    - `testSurfaceAnnulusLocalization`
    - `testSurfaceLocalization`
    - `testSurfaceLocalizationStrategy`
    - `testSurfaceEdgePcaLocalization`
    - `testSurfaceShapeModel`
    - `testSurfaceTemplateModel`
    - `testLocalization`
  - Added a `MetricsPanelWidget` to the right-hand tools panel and set `AsyncExecutor::setMetricsHandler` to forward metrics to it (fallback to appendLog if missing).
- Modified `src/gui/MainWindow.h` to add `MetricsPanelWidget* m_metricsPanel` member.

Design notes & rationale:
- All heavy image-processing calls are executed in background threads to keep the UI responsive.
- Metrics for each async task are measured (ms) and sent to the MetricsPanel (or log) to allow performance tuning.
- `AsyncExecutor` is header-only for minimal intrusion; it centralizes the pattern and allows changing thread limits in one place.

TODO (next steps / improvements):
- Centralize task naming: include a `taskName` parameter in `runAsyncTask` calls for clearer metrics.
- Implement a bounded queue or cancellation logic for frame-processing jobs to avoid queuing stale frames when camera FPS exceeds processing throughput.
- Move QImage/QPixmap conversions to UI thread but ensure diagnostics images are created in worker threads to reduce copying.
- Add persistent logging for metrics (file or telemetry endpoint).
- Add UI controls to configure `QThreadPool::globalInstance()->setMaxThreadCount` at runtime.
  - Added menu action in System menu to set max threads (`Set max threads...`). Choosing 0 sets auto (hardware concurrency).
- Add unit/integration tests for `AsyncExecutor` behavior (ensuring callback on main thread and metrics emitted).

If you want, I can now:
- Implement bounded queue/cancellation for camera frames.
- Add runtime UI controls for thread pool size.
- Persist metrics to a file.

