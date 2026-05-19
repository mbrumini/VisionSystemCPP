#include "MainWindow.h"

#include "util/AsyncExecutor.h"

#include <QSettings>
#include <QVariant>

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
{
  m_recipeManager.setRecipeId(RecipeManager::loadActiveRecipeId());

  QString error;
  if (!m_translations.loadLanguage("it", &error))
  {
    m_translations.loadLanguage("en");
  }

  buildUi();
  // Initialize async executor: set thread count based on hardware and enable metrics logging
  AsyncExecutor::setDefaultMaxThreadsToHardware();
  // Load persisted max threads if present
  {
    QSettings settings;
    const QVariant v = settings.value("system/maxThreads", QVariant());
    if (v.isValid())
    {
      const int saved = v.toInt();
      if (saved <= 0)
      {
        AsyncExecutor::setDefaultMaxThreadsToHardware();
      }
      else
      {
        AsyncExecutor::setMaxThreads(saved);
      }
    }
  }
  AsyncExecutor::setMetricsHandler([this](const QString& name, qint64 ms) {
    if (m_metricsPanel)
    {
      m_metricsPanel->addMetric(name, ms);
      return;
    }

    if (name.isEmpty())
    {
      appendLog(QString("metric: %1 ms").arg(ms));
    }
    else
    {
      appendLog(QString("metric %1: %2 ms").arg(name).arg(ms));
    }
  });
  loadConfiguration();
}

