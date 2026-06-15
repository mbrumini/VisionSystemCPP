#include "gui/UsbCameraAssignmentDialog.h"

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

UsbCameraAssignmentDialog::UsbCameraAssignmentDialog(
  int currentSlot,
  int currentUsbIndex,
  int maxSlot,
  std::function<void(const QString&)> diagnosticLog,
  QWidget* parent)
  : QDialog(parent),
    m_currentUsbIndex(currentUsbIndex),
    m_diagnosticLog(std::move(diagnosticLog))
{
  setWindowTitle("USB camera assignment");
  resize(650, 380);

  auto* layout = new QVBoxLayout(this);

  m_statusLabel = new QLabel(this);
  m_statusLabel->setWordWrap(true);
  layout->addWidget(m_statusLabel);

  m_table = new QTableWidget(this);
  m_table->setColumnCount(4);
  m_table->setHorizontalHeaderLabels({ "Nome", "Indice", "Risoluzione", "FPS" });
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
  auto* refreshButton = buttons->addButton("Rileggi USB", QDialogButtonBox::ActionRole);
  connect(refreshButton, &QPushButton::clicked, this, [this]() { refreshDevices(); });
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() { updateSelectionFields(); });

  refreshDevices();
}

UsbCameraDeviceInfo UsbCameraAssignmentDialog::selectedDevice() const
{
  const int row = m_table->currentRow();
  if (row < 0 || row >= m_devices.size())
  {
    return {};
  }

  return m_devices[row];
}

int UsbCameraAssignmentDialog::selectedSlot() const
{
  return m_slotSpin->value();
}

QString UsbCameraAssignmentDialog::displayName() const
{
  return m_displayNameEdit->text().trimmed();
}

bool UsbCameraAssignmentDialog::cameraEnabled() const
{
  return m_enabledCheck->isChecked();
}

void UsbCameraAssignmentDialog::refreshDevices()
{
  if (m_diagnosticLog)
  {
    m_diagnosticLog("USB camera UI refresh requested");
  }

  QStringList diagnostics;
  const QVector<UsbCameraDeviceInfo> devices = UsbCameraDiscovery::discover(10, &diagnostics);
  for (const QString& diagnostic : diagnostics)
  {
    if (m_diagnosticLog)
    {
      m_diagnosticLog(diagnostic);
    }
  }

  populateDevices(devices);
  m_statusLabel->setText(QString("Camere USB trovate: %1").arg(devices.size()));
  if (m_diagnosticLog)
  {
    m_diagnosticLog(QString("USB camera UI refresh completed: devices=%1").arg(devices.size()));
  }
}

void UsbCameraAssignmentDialog::populateDevices(const QVector<UsbCameraDeviceInfo>& devices)
{
  m_devices = devices;
  m_table->setRowCount(devices.size());

  for (int row = 0; row < devices.size(); ++row)
  {
    const UsbCameraDeviceInfo& device = devices[row];
    m_table->setItem(row, 0, new QTableWidgetItem(device.displayName));
    m_table->setItem(row, 1, new QTableWidgetItem(QString::number(device.index)));
    m_table->setItem(row, 2, new QTableWidgetItem(QString("%1x%2").arg(device.width).arg(device.height)));
    m_table->setItem(row, 3, new QTableWidgetItem(QString::number(device.fps, 'f', 2)));
  }

  int rowToSelect = devices.isEmpty() ? -1 : 0;
  for (int row = 0; row < devices.size(); ++row)
  {
    if (devices[row].index == m_currentUsbIndex)
    {
      rowToSelect = row;
      break;
    }
  }

  if (rowToSelect >= 0)
  {
    m_table->selectRow(rowToSelect);
  }

  if (m_diagnosticLog)
  {
    m_diagnosticLog(QString("USB camera UI table populated: rows=%1").arg(devices.size()));
  }
}

void UsbCameraAssignmentDialog::updateSelectionFields()
{
  const UsbCameraDeviceInfo device = selectedDevice();
  if (device.index < 0)
  {
    return;
  }

  m_currentUsbIndex = device.index;
  m_displayNameEdit->setText(device.displayName);
}
