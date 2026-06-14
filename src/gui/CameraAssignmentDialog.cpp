#include "gui/CameraAssignmentDialog.h"

#include "camera/VimbaXDiscovery.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

CameraAssignmentDialog::CameraAssignmentDialog(
  int currentSlot,
  int maxSlot,
  std::function<void(const QString&)> diagnosticLog,
  QWidget* parent)
  : QDialog(parent),
    m_diagnosticLog(std::move(diagnosticLog))
{
  setWindowTitle("VimbaX camera assignment");
  resize(720, 420);

  auto* layout = new QVBoxLayout(this);

  m_statusLabel = new QLabel(this);
  m_statusLabel->setWordWrap(true);
  layout->addWidget(m_statusLabel);

  m_table = new QTableWidget(this);
  m_table->setColumnCount(4);
  m_table->setHorizontalHeaderLabels({ "Nome", "ID", "Seriale", "Interfaccia" });
  m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(m_table, 1);

  auto* form = new QFormLayout();
  m_slotSpin = new QSpinBox(this);
  m_slotSpin->setRange(1, maxSlot);
  m_slotSpin->setValue(currentSlot);
  form->addRow("Posizione / slot", m_slotSpin);

  m_displayNameEdit = new QLineEdit(this);
  form->addRow("Nome visualizzato", m_displayNameEdit);

  m_enabledCheck = new QCheckBox("Attiva camera", this);
  m_enabledCheck->setChecked(true);
  form->addRow(QString(), m_enabledCheck);
  layout->addLayout(form);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  auto* refreshButton = buttons->addButton("Rileggi VimbaX", QDialogButtonBox::ActionRole);
  connect(refreshButton, &QPushButton::clicked, this, [this]() { refreshDevices(); });
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() { updateSelectionFields(); });

  refreshDevices();
}

CameraDeviceInfo CameraAssignmentDialog::selectedDevice() const
{
  const int row = m_table->currentRow();
  if (row < 0 || row >= m_devices.size())
  {
    return {};
  }

  return m_devices[row];
}

int CameraAssignmentDialog::selectedSlot() const
{
  return m_slotSpin->value();
}

QString CameraAssignmentDialog::displayName() const
{
  return m_displayNameEdit->text().trimmed();
}

bool CameraAssignmentDialog::cameraEnabled() const
{
  return m_enabledCheck->isChecked();
}

void CameraAssignmentDialog::refreshDevices()
{
  if (m_diagnosticLog)
  {
    m_diagnosticLog("VimbaX UI refresh requested");
  }

  QString error;
  QStringList diagnostics;
  const QVector<CameraDeviceInfo> devices = VimbaXDiscovery::discover(&error, &diagnostics);
  for (const QString& diagnostic : diagnostics)
  {
    if (m_diagnosticLog)
    {
      m_diagnosticLog(diagnostic);
    }
  }

  populateDevices(devices);

  if (!error.isEmpty())
  {
    m_statusLabel->setText(error);
    if (m_diagnosticLog)
    {
      m_diagnosticLog("VimbaX UI refresh error: " + error);
    }
    return;
  }

  m_statusLabel->setText(QString("Camere VimbaX trovate: %1").arg(devices.size()));
  if (m_diagnosticLog)
  {
    m_diagnosticLog(QString("VimbaX UI refresh completed: devices=%1").arg(devices.size()));
  }
}

void CameraAssignmentDialog::populateDevices(const QVector<CameraDeviceInfo>& devices)
{
  m_devices = devices;
  m_table->setRowCount(devices.size());

  for (int row = 0; row < devices.size(); ++row)
  {
    const CameraDeviceInfo& device = devices[row];
    m_table->setItem(row, 0, new QTableWidgetItem(device.displayName));
    m_table->setItem(row, 1, new QTableWidgetItem(device.deviceId));
    m_table->setItem(row, 2, new QTableWidgetItem(device.serial));
    m_table->setItem(row, 3, new QTableWidgetItem(device.interfaceId));
  }

  if (!devices.isEmpty())
  {
    m_table->selectRow(0);
  }

  if (m_diagnosticLog)
  {
    m_diagnosticLog(QString("VimbaX UI table populated: rows=%1").arg(devices.size()));
  }
}

void CameraAssignmentDialog::updateSelectionFields()
{
  const CameraDeviceInfo device = selectedDevice();
  if (device.deviceId.isEmpty() && device.serial.isEmpty())
  {
    return;
  }

  m_displayNameEdit->setText(device.displayName.isEmpty() ? device.modelName : device.displayName);
}
