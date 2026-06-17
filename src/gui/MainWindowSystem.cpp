#include "gui/MainWindow.h"

#include "util/AsyncExecutor.h"

#include <QDir>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QSettings>
#include <QVBoxLayout>

#include <thread>

namespace
{
QString configPath()
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("config/cameras.json");
}

QString profileLabel(const CameraConfig& camera)
{
  if (!camera.profile.guiTools.isEmpty())
  {
    return camera.profile.guiTools.join(", ");
  }
  if (!camera.profile.inspectionTypes.isEmpty())
  {
    return camera.profile.inspectionTypes.join(", ");
  }
  return camera.processingProfileId;
}

QTableWidgetItem* readOnlyItem(const QString& text)
{
  auto* item = new QTableWidgetItem(text);
  item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  return item;
}
}

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

void MainWindow::showCameraSystemSettings()
{
  QDialog dialog(this);
  dialog.setWindowTitle("Sistema | Configurazione telecamere");
  dialog.resize(1180, 620);

  auto* layout = new QVBoxLayout(&dialog);
  auto* table = new QTableWidget(&dialog);
  table->setColumnCount(10);
  table->setHorizontalHeaderLabels({
    "Slot",
    "ID",
    "Attiva",
    "Presente",
    "Nome",
    "Tipo",
    "Funzione",
    "Device / cartella",
    "Futuro 1",
    "Futuro 2"
  });
  table->verticalHeader()->setVisible(false);
  table->setAlternatingRowColors(true);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setRowCount(m_config.cameras().size());

  QStringList profileIds;
  profileIds << "default"
             << "bw_dimensional"
             << "grayscale_measurement"
             << "grayscale_surface"
             << "grayscale_ai"
             << "grayscale_surface_measurement_ai";

  for (int row = 0; row < m_config.cameras().size(); ++row)
  {
    const CameraConfig& camera = m_config.cameras()[row];
    table->setItem(row, 0, readOnlyItem(QString::number(camera.slot)));
    table->setItem(row, 1, readOnlyItem(camera.id));

    auto* enabled = new QCheckBox(table);
    enabled->setChecked(camera.enabled);
    table->setCellWidget(row, 2, enabled);

    auto* exists = new QCheckBox(table);
    exists->setChecked(camera.exists);
    table->setCellWidget(row, 3, exists);

    table->setItem(row, 4, new QTableWidgetItem(camera.displayName));

    auto* typeCombo = new QComboBox(table);
    typeCombo->addItems({"file", "usb", "vimba"});
    const int typeIndex = typeCombo->findText(camera.type);
    typeCombo->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
    table->setCellWidget(row, 5, typeCombo);

    auto* profileCombo = new QComboBox(table);
    profileCombo->addItems(profileIds);
    if (!profileIds.contains(camera.processingProfileId) && !camera.processingProfileId.isEmpty())
    {
      profileCombo->addItem(camera.processingProfileId);
    }
    const int profileIndex = profileCombo->findText(camera.processingProfileId);
    profileCombo->setCurrentIndex(profileIndex >= 0 ? profileIndex : 0);
    profileCombo->setToolTip(profileLabel(camera));
    table->setCellWidget(row, 6, profileCombo);

    QString source;
    if (camera.type == "file")
    {
      source = camera.folder;
    }
    else if (camera.type == "usb")
    {
      source = QString("usb:%1").arg(camera.usbIndex);
    }
    else
    {
      source = camera.deviceId;
    }
    table->setItem(row, 7, readOnlyItem(source));
    table->setItem(row, 8, new QTableWidgetItem(""));
    table->setItem(row, 9, new QTableWidgetItem(""));
  }

  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(9, QHeaderView::ResizeToContents);
  layout->addWidget(table);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted)
  {
    return;
  }

  QVector<CameraConfig> updated = m_config.cameras();
  for (int row = 0; row < table->rowCount() && row < updated.size(); ++row)
  {
    CameraConfig& camera = updated[row];
    if (auto* enabled = qobject_cast<QCheckBox*>(table->cellWidget(row, 2)))
    {
      camera.enabled = enabled->isChecked();
    }
    if (auto* exists = qobject_cast<QCheckBox*>(table->cellWidget(row, 3)))
    {
      camera.exists = exists->isChecked();
    }
    if (QTableWidgetItem* nameItem = table->item(row, 4))
    {
      camera.displayName = nameItem->text().trimmed();
    }
    if (auto* typeCombo = qobject_cast<QComboBox*>(table->cellWidget(row, 5)))
    {
      camera.type = typeCombo->currentText();
    }
    if (auto* profileCombo = qobject_cast<QComboBox*>(table->cellWidget(row, 6)))
    {
      camera.processingProfileId = profileCombo->currentText();
    }
  }

  QString error;
  if (!m_config.saveCameraSystemSettings(configPath(), updated, &error))
  {
    QMessageBox::warning(this, "Configura telecamere", error);
    appendLog(error);
    return;
  }

  appendLog("Configurazione telecamere salvata: " + configPath());
  loadConfiguration();
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
