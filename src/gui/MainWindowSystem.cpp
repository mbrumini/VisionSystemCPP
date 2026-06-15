#include "gui/MainWindow.h"

#include "util/AsyncExecutor.h"

#include <QDir>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>

#include <thread>

void MainWindow::showAccessLogin()
{
  bool ok = false;
  const QString password = QInputDialog::getText(
    this,
    trText("access.loginTitle"),
    trText("access.password"),
    QLineEdit::Password,
    QString(),
    &ok);

  if (!ok)
  {
    return;
  }

  if (m_accessSession.authenticateBackdoor(password))
  {
    const QString roleLabel = accessRoleLabel(m_accessSession.role());
    if (m_logBox)
    {
      m_logBox->setVisible(true);
    }
    appendLog(QString("%1: %2").arg(trText("access.loginOk"), roleLabel));
    QMessageBox::information(this, trText("access.loginTitle"), QString("%1: %2").arg(trText("access.loginOk"), roleLabel));
    return;
  }

  appendLog(trText("access.loginDenied"));
  QMessageBox::warning(this, trText("access.loginTitle"), trText("access.loginDenied"));
}

void MainWindow::setThreadLimitPrompt()
{
  bool ok = false;
  const int current = QThreadPool::globalInstance()->maxThreadCount();
  const unsigned int hw = std::thread::hardware_concurrency();
  const int hwCount = hw == 0 ? 1 : static_cast<int>(hw);
  const int value = QInputDialog::getInt(
    this,
    trText("commands.setMaxThreads"),
    trText("labels.maxThreads"),
    current,
    0,
    1024,
    1,
    &ok);

  if (!ok)
  {
    return;
  }

  if (value <= 0)
  {
    AsyncExecutor::setDefaultMaxThreadsToHardware();
    appendLog(QString("%1: %2").arg(trText("log.threadLimitSet"), QString("auto=%1").arg(hwCount)));
    QSettings settings;
    settings.setValue("system/maxThreads", 0);
    return;
  }

  AsyncExecutor::setMaxThreads(value);
  appendLog(QString("%1: %2").arg(trText("log.threadLimitSet"), QString::number(value)));
  QSettings settings;
  settings.setValue("system/maxThreads", value);
}

void MainWindow::setDetailedLogEnabled(bool enabled)
{
  QSettings settings;
  if (!enabled)
  {
    appendLog(trText("log.detailedLogDisabled"));
    m_detailedLogger.setEnabled(false, {});
    settings.setValue("system/detailedLogEnabled", false);
    return;
  }

  QString error;
  const QString logRoot = QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("logs");
  if (!m_detailedLogger.setEnabled(true, logRoot, &error))
  {
    appendLog(error);
    settings.setValue("system/detailedLogEnabled", false);
    return;
  }

  settings.setValue("system/detailedLogEnabled", true);
  appendLog(QString("%1: %2").arg(trText("log.detailedLogEnabled"), m_detailedLogger.filePath()));
}

void MainWindow::setSetupDetailsVisible(bool visible)
{
  m_setupDetailsVisible = visible;
  QSettings settings;
  settings.setValue("ui/setupDetailsVisible", visible);
  if (m_setupPanel)
  {
    m_setupPanel->setDetailsVisible(visible);
  }
}
