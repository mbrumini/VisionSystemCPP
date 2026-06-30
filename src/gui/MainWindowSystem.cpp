#include "gui/MainWindow.h"

#include "calibration/CalibrationRecipe.h"
#include "config/RecipeJsonUtils.h"
#include "gui/CheckerboardCalibrationDialog.h"
#include "util/AsyncExecutor.h"

#include <QDir>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QSettings>
#include <QVBoxLayout>

#include <thread>

namespace
{
QString configPath()
{
  return RecipeJsonUtils::appPath("config/cameras.json");
}

QString appPath(const QString& relativePath)
{
  return RecipeJsonUtils::appPath(relativePath);
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

QString defaultCameraBackendForType(const QString& type)
{
  if (type == "vimba")
  {
    return "vimbax";
  }
  if (type == "usb")
  {
    return "opencv_usb";
  }
  return type;
}

QString defaultCameraProfileForBackend(const QString& backend)
{
  if (backend == "vimbax")
  {
    return "allied_vimbax_generic";
  }
  if (backend == "opencv_usb")
  {
    return "opencv_usb_generic";
  }
  if (backend == "file" || backend == "simulator")
  {
    return "file_simulated";
  }
  return {};
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
    updateMeasurementResults();
    buildMenu();
    QMessageBox::information(this, trText("access.loginTitle"), QString("%1: %2").arg(trText("access.loginOk"), roleLabel));
    return;
  }

  appendLog(trText("access.loginDenied"));
  QMessageBox::warning(this, trText("access.loginTitle"), trText("access.loginDenied"));
}

void MainWindow::showAccessManagement()
{
  QDialog dialog(this);
  dialog.setWindowTitle(trText("access.manageAccess"));
  dialog.resize(1040, 620);

  auto* layout = new QVBoxLayout(&dialog);
  auto* note = new QLabel(trText("access.managementPlaceholder"), &dialog);
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* tabs = new QTabWidget(&dialog);

  auto* usersPage = new QWidget(tabs);
  auto* usersLayout = new QVBoxLayout(usersPage);
  auto* usersTable = new QTableWidget(usersPage);
  usersTable->setColumnCount(6);
  usersTable->setHorizontalHeaderLabels({
    trText("access.alias"),
    trText("access.level"),
    trText("access.enabled"),
    trText("access.password"),
    trText("access.permissions"),
    trText("access.notes")
  });
  usersTable->verticalHeader()->setVisible(false);
  usersTable->setAlternatingRowColors(true);
  usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  usersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  usersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  usersTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  usersTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  usersTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  usersTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
  usersLayout->addWidget(usersTable);

  auto addUserRow = [this, usersTable]() {
    const int row = usersTable->rowCount();
    usersTable->insertRow(row);
    usersTable->setItem(row, 0, new QTableWidgetItem(trText("access.newAlias")));

    auto* role = new QComboBox(usersTable);
    role->addItems({
      trText("access.roleSupervisor"),
      trText("access.roleAdministrator"),
      trText("access.roleOperator"),
      trText("access.roleViewer")
    });
    role->setCurrentIndex(2);
    usersTable->setCellWidget(row, 1, role);

    auto* enabled = new QCheckBox(usersTable);
    enabled->setChecked(true);
    enabled->setText(trText("access.enabled"));
    usersTable->setCellWidget(row, 2, enabled);

    auto* password = new QPushButton(trText("access.setPassword"), usersTable);
    connect(password, &QPushButton::clicked, this, [this]() {
      QMessageBox::information(this, trText("access.manageAccess"), trText("access.logicNotConnected"));
    });
    usersTable->setCellWidget(row, 3, password);

    auto* permissions = new QPushButton(trText("access.customize"), usersTable);
    connect(permissions, &QPushButton::clicked, this, [this]() {
      QMessageBox::information(this, trText("access.manageAccess"), trText("access.logicNotConnected"));
    });
    usersTable->setCellWidget(row, 4, permissions);
    usersTable->setItem(row, 5, new QTableWidgetItem());
    usersTable->setCurrentCell(row, 0);
  };

  auto* userActions = new QWidget(usersPage);
  auto* userActionsLayout = new QHBoxLayout(userActions);
  userActionsLayout->setContentsMargins(0, 0, 0, 0);
  auto* addUser = new QPushButton(trText("access.addUser"), userActions);
  auto* removeUser = new QPushButton(trText("access.removeUser"), userActions);
  userActionsLayout->addWidget(addUser);
  userActionsLayout->addWidget(removeUser);
  userActionsLayout->addStretch(1);
  usersLayout->addWidget(userActions);
  connect(addUser, &QPushButton::clicked, &dialog, addUserRow);
  connect(removeUser, &QPushButton::clicked, &dialog, [usersTable]() {
    const int row = usersTable->currentRow();
    if (row >= 0)
    {
      usersTable->removeRow(row);
    }
  });

  auto* rolesPage = new QWidget(tabs);
  auto* rolesLayout = new QVBoxLayout(rolesPage);
  auto* rolesTable = new QTableWidget(rolesPage);
  const QVector<AccessRole> roles = {
    AccessRole::Supervisor,
    AccessRole::Administrator,
    AccessRole::Operator,
    AccessRole::Viewer
  };
  const QVector<QPair<AccessPermission, QString>> permissions = {
    {AccessPermission::ViewOverview, trText("access.permissionViewOverview")},
    {AccessPermission::ViewCamera, trText("access.permissionViewCamera")},
    {AccessPermission::StartStopMachine, trText("access.permissionStartStop")},
    {AccessPermission::EditSetup, trText("access.permissionEditSetup")},
    {AccessPermission::EditGeometries, trText("access.permissionEditGeometries")},
    {AccessPermission::EditMeasurements, trText("access.permissionEditMeasurements")},
    {AccessPermission::EditCalibration, trText("access.permissionEditCalibration")},
    {AccessPermission::EditRecipes, trText("access.permissionEditRecipes")},
    {AccessPermission::EditCameraSources, trText("access.permissionEditCameraSources")},
    {AccessPermission::EditSystemSettings, trText("access.permissionEditSystem")},
    {AccessPermission::ManageUsers, trText("access.permissionManageUsers")}
  };
  rolesTable->setColumnCount(roles.size() + 1);
  QStringList roleHeaders = {trText("access.permissions")};
  roleHeaders.append({
    trText("access.roleSupervisor"),
    trText("access.roleAdministrator"),
    trText("access.roleOperator"),
    trText("access.roleViewer")
  });
  rolesTable->setHorizontalHeaderLabels(roleHeaders);
  rolesTable->setRowCount(permissions.size());
  rolesTable->verticalHeader()->setVisible(false);
  rolesTable->setAlternatingRowColors(true);
  rolesTable->setSelectionMode(QAbstractItemView::NoSelection);
  rolesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int column = 1; column < rolesTable->columnCount(); ++column)
  {
    rolesTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
  }

  AccessPolicy currentPolicy;
  for (int row = 0; row < permissions.size(); ++row)
  {
    rolesTable->setItem(row, 0, readOnlyItem(permissions[row].second));
    for (int roleIndex = 0; roleIndex < roles.size(); ++roleIndex)
    {
      auto* allowed = new QCheckBox(rolesTable);
      allowed->setChecked(currentPolicy.allows(roles[roleIndex], permissions[row].first));
      allowed->setToolTip(trText("access.notSavedYet"));
      auto* cell = new QWidget(rolesTable);
      auto* cellLayout = new QHBoxLayout(cell);
      cellLayout->setContentsMargins(0, 0, 0, 0);
      cellLayout->setAlignment(Qt::AlignCenter);
      cellLayout->addWidget(allowed);
      rolesTable->setCellWidget(row, roleIndex + 1, cell);
    }
  }
  rolesLayout->addWidget(rolesTable);

  auto* toolsPage = new QWidget(tabs);
  auto* toolsLayout = new QVBoxLayout(toolsPage);
  auto* toolsNote = new QLabel(trText("access.optionalToolsNote"), toolsPage);
  toolsNote->setWordWrap(true);
  toolsLayout->addWidget(toolsNote);

  auto* toolsTable = new QTableWidget(toolsPage);
  toolsTable->setColumnCount(3);
  toolsTable->setHorizontalHeaderLabels({
    trText("access.optionalTool"),
    trText("access.enabledForUsers"),
    trText("access.description")
  });
  toolsTable->verticalHeader()->setVisible(false);
  toolsTable->setAlternatingRowColors(true);
  toolsTable->setSelectionMode(QAbstractItemView::NoSelection);
  toolsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  toolsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  toolsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  toolsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

  struct OptionalToolRow
  {
    QString id;
    QString label;
    QString description;
  };
  const QVector<OptionalToolRow> optionalTools = {
    {"aiLocalization", trText("tools.aiLocalization"), trText("access.aiLocalizationDescription")},
    {"aiClassification", trText("tools.aiClassification"), trText("access.aiClassificationDescription")},
    {"aiAnomaly", trText("tools.aiAnomaly"), trText("access.aiAnomalyDescription")},
    {"aiSegmentation", trText("tools.aiSegmentation"), trText("access.aiSegmentationDescription")}
  };
  toolsTable->setRowCount(optionalTools.size());
  QSettings accessSettings;
  for (int row = 0; row < optionalTools.size(); ++row)
  {
    const OptionalToolRow& tool = optionalTools[row];
    toolsTable->setItem(row, 0, readOnlyItem(tool.label));

    auto* enabled = new QCheckBox(trText("access.enabled"), toolsTable);
    enabled->setChecked(accessSettings.value(QString("access/tools/%1").arg(tool.id), true).toBool());
    connect(enabled, &QCheckBox::toggled, &dialog, [tool](bool checked) {
      QSettings settings;
      settings.setValue(QString("access/tools/%1").arg(tool.id), checked);
    });
    auto* enabledCell = new QWidget(toolsTable);
    auto* enabledLayout = new QHBoxLayout(enabledCell);
    enabledLayout->setContentsMargins(8, 0, 8, 0);
    enabledLayout->setAlignment(Qt::AlignCenter);
    enabledLayout->addWidget(enabled);
    toolsTable->setCellWidget(row, 1, enabledCell);
    toolsTable->setItem(row, 2, readOnlyItem(tool.description));
  }
  toolsLayout->addWidget(toolsTable);

  tabs->addTab(usersPage, trText("access.users"));
  tabs->addTab(rolesPage, trText("access.rolesAndPermissions"));
  tabs->addTab(toolsPage, trText("access.optionalTools"));
  layout->addWidget(tabs);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  dialog.exec();
  if (!m_selectedCameraId.isEmpty())
  {
    showCameraToolList(m_selectedCamera);
  }
}

void MainWindow::showCameraSystemSettings()
{
  QDialog dialog(this);
  dialog.setWindowTitle("Sistema | Configurazione telecamere");
  dialog.resize(1180, 620);

  auto* layout = new QVBoxLayout(&dialog);
  auto* table = new QTableWidget(&dialog);
  table->setColumnCount(12);
  table->setHorizontalHeaderLabels({
    "Slot",
    "ID",
    "Attiva",
    "Presente",
    "Nome",
    "Tipo",
    "Funzione",
    "Device / cartella",
    "Calib.",
    "Tipo calib.",
    "mm/px",
    "File calib."
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
    typeCombo->addItems({"file", "usb", "vimba", "simulator"});
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
    else if (camera.type == "simulator")
    {
      source = camera.simulatorChannel.isEmpty() ? camera.id : camera.simulatorChannel;
    }
    else
    {
      source = camera.deviceId;
    }
    auto* sourceItem = new QTableWidgetItem(source);
    if (camera.type != "simulator")
    {
      sourceItem->setFlags(sourceItem->flags() & ~Qt::ItemIsEditable);
    }
    sourceItem->setToolTip(
      camera.type == "simulator"
        ? "Canale esposto da TestVision; se vuoto viene usato l'ID camera"
        : "La sorgente si configura con il comando dedicato");
    table->setItem(row, 7, sourceItem);
    connect(typeCombo, &QComboBox::currentTextChanged, &dialog, [table, row, camera](const QString& type) {
      QTableWidgetItem* item = table->item(row, 7);
      if (!item)
      {
        return;
      }

      if (type == "simulator")
      {
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        if (item->text().trimmed().isEmpty() ||
            item->text() == camera.folder ||
            item->text() == camera.deviceId ||
            item->text().startsWith("usb:"))
        {
          item->setText(camera.simulatorChannel.isEmpty() ? camera.id : camera.simulatorChannel);
        }
        item->setToolTip("Canale esposto da TestVision; se vuoto viene usato l'ID camera");
      }
      else
      {
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setToolTip("La sorgente si configura con il comando dedicato");
        if (type == "file")
        {
          item->setText(camera.folder);
        }
        else if (type == "usb")
        {
          item->setText(camera.usbIndex >= 0 ? QString("usb:%1").arg(camera.usbIndex) : QString());
        }
        else if (type == "vimba")
        {
          item->setText(camera.deviceId);
        }
      }
    });

    auto* calibrationEnabled = new QCheckBox(table);
    calibrationEnabled->setChecked(camera.calibration.enabled);
    table->setCellWidget(row, 8, calibrationEnabled);

    auto* calibrationType = new QComboBox(table);
    calibrationType->addItems({"none", "manual", "checkerboard"});
    const int calibrationTypeIndex = calibrationType->findText(camera.calibration.type);
    calibrationType->setCurrentIndex(calibrationTypeIndex >= 0 ? calibrationTypeIndex : 0);
    table->setCellWidget(row, 9, calibrationType);

    auto* pixelSize = new QDoubleSpinBox(table);
    pixelSize->setRange(0.0, 1000.0);
    pixelSize->setDecimals(8);
    pixelSize->setSingleStep(0.001);
    pixelSize->setSuffix(" mm/px");
    pixelSize->setValue(camera.calibration.pixelSizeXMm > 0.0 ? camera.calibration.pixelSizeXMm : camera.calibration.pixelSizeYMm);
    table->setCellWidget(row, 10, pixelSize);

    table->setItem(row, 11, new QTableWidgetItem(camera.calibration.file));
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
  table->horizontalHeader()->setSectionResizeMode(10, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(11, QHeaderView::Stretch);
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
      camera.backend = defaultCameraBackendForType(camera.type);
      camera.cameraProfileId = defaultCameraProfileForBackend(camera.backend);
    }
    if (camera.type == "simulator")
    {
      const QString channel = table->item(row, 7)
        ? table->item(row, 7)->text().trimmed()
        : QString();
      camera.simulatorChannel = channel.isEmpty() ? camera.id : channel;
    }
    if (auto* profileCombo = qobject_cast<QComboBox*>(table->cellWidget(row, 6)))
    {
      camera.processingProfileId = profileCombo->currentText();
    }
    if (auto* calibrationEnabled = qobject_cast<QCheckBox*>(table->cellWidget(row, 8)))
    {
      camera.calibration.enabled = calibrationEnabled->isChecked();
    }
    if (auto* calibrationType = qobject_cast<QComboBox*>(table->cellWidget(row, 9)))
    {
      camera.calibration.type = calibrationType->currentText();
    }
    if (auto* pixelSize = qobject_cast<QDoubleSpinBox*>(table->cellWidget(row, 10)))
    {
      camera.calibration.pixelSizeXMm = pixelSize->value();
      camera.calibration.pixelSizeYMm = pixelSize->value();
    }
    if (QTableWidgetItem* calibrationFile = table->item(row, 11))
    {
      camera.calibration.file = calibrationFile->text().trimmed();
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

void MainWindow::showCheckerboardCalibrationDialog()
{
  CheckerboardCalibrationDialog dialog(
    m_config.cameras(),
    m_selectedCameraId,
    [this](const CameraConfig& camera, QString* error) {
      return m_imaging.currentInputImage(camera, error);
    },
    [this](const cv::Mat& image) {
      return m_imaging.matToPixmap(image);
    },
    [this](const CameraConfig& camera) {
      const auto runtimeIt = m_cameraRuntime.find(camera.id);
      if (runtimeIt == m_cameraRuntime.end() || !runtimeIt->second.running())
      {
        m_setup.startCameraSimulation(camera, false);
      }
    },
    [this](const CameraConfig& camera) {
      m_setup.stopCameraSimulation(camera, false);
    },
    this);

  if (dialog.exec() != QDialog::Accepted)
  {
    return;
  }

  const CheckerboardCalibrationDialog::Result result = dialog.result();
  if (result.cameraIndex < 0 || result.cameraIndex >= m_config.cameras().size() || !result.model.valid)
  {
    return;
  }

  QDir projectDir(RecipeJsonUtils::appRootPath());
  projectDir.mkpath("calibrations");
  const QString relativeFile = QString("calibrations/%1_checkerboard.json").arg(result.model.cameraId);
  const QString absoluteFile = projectDir.filePath(relativeFile);

  QString error;
  CalibrationRecipe calibrationRecipe;
  if (!calibrationRecipe.save(absoluteFile, result.model, &error))
  {
    QMessageBox::warning(this, "Calibrazione checkerboard", error);
    appendLog(error);
    return;
  }

  QVector<CameraConfig> updated = m_config.cameras();
  CameraConfig& updatedCamera = updated[result.cameraIndex];
  updatedCamera.calibration.enabled = true;
  updatedCamera.calibration.type = "checkerboard";
  updatedCamera.calibration.file = relativeFile;
  updatedCamera.calibration.pixelSizeXMm = result.model.pixelSizeXMm;
  updatedCamera.calibration.pixelSizeYMm = result.model.pixelSizeYMm;
  updatedCamera.calibration.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
  if (!m_config.saveCameraSystemSettings(configPath(), updated, &error))
  {
    QMessageBox::warning(this, "Calibrazione checkerboard", error);
    appendLog(error);
    return;
  }

  const QString message = QString("Calibrazione salvata: %1 | X=%2 mm/px Y=%3 mm/px errore=%4 px")
    .arg(relativeFile)
    .arg(result.model.pixelSizeXMm, 0, 'f', 8)
    .arg(result.model.pixelSizeYMm, 0, 'f', 8)
    .arg(result.model.rmsErrorPixels, 0, 'f', 3);
  appendLog(message);
  QMessageBox::information(this, "Calibrazione checkerboard", message);
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
    appendLog(trText("log.detailedLogDisabled"), true);
    m_detailedLogger.setEnabled(false, {});
    settings.setValue("system/detailedLogEnabled", false);
    syncAsyncMetricsLogging();
    return;
  }

  QString error;
  const QString logRoot = appPath("logs");
  if (!m_detailedLogger.setEnabled(true, logRoot, &error))
  {
    appendLog(error, true);
    settings.setValue("system/detailedLogEnabled", false);
    syncAsyncMetricsLogging();
    return;
  }

  settings.setValue("system/detailedLogEnabled", true);
  syncAsyncMetricsLogging();
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
