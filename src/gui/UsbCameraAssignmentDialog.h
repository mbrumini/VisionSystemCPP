#pragma once

#include "camera/UsbCameraDiscovery.h"

#include <QDialog>
#include <QVector>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;

class UsbCameraAssignmentDialog : public QDialog
{
public:
  UsbCameraAssignmentDialog(
    int currentSlot,
    int currentUsbIndex,
    int maxSlot,
    std::function<void(const QString&)> diagnosticLog = {},
    QWidget* parent = nullptr);

  UsbCameraDeviceInfo selectedDevice() const;
  int selectedSlot() const;
  QString displayName() const;
  bool cameraEnabled() const;

private:
  void refreshDevices();
  void populateDevices(const QVector<UsbCameraDeviceInfo>& devices);
  void updateSelectionFields();

  QVector<UsbCameraDeviceInfo> m_devices;
  int m_currentUsbIndex = -1;
  std::function<void(const QString&)> m_diagnosticLog;
  QLabel* m_statusLabel = nullptr;
  QTableWidget* m_table = nullptr;
  QSpinBox* m_slotSpin = nullptr;
  QLineEdit* m_displayNameEdit = nullptr;
  QCheckBox* m_enabledCheck = nullptr;
};
