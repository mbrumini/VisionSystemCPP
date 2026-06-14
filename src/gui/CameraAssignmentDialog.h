#pragma once

#include "camera/CameraDeviceInfo.h"

#include <QDialog>
#include <QVector>

#include <functional>

class QCheckBox;
class QLineEdit;
class QLabel;
class QSpinBox;
class QTableWidget;

class CameraAssignmentDialog : public QDialog
{
public:
  CameraAssignmentDialog(
    int currentSlot,
    int maxSlot,
    std::function<void(const QString&)> diagnosticLog = {},
    QWidget* parent = nullptr);

  CameraDeviceInfo selectedDevice() const;
  int selectedSlot() const;
  QString displayName() const;
  bool cameraEnabled() const;

private:
  void refreshDevices();
  void populateDevices(const QVector<CameraDeviceInfo>& devices);
  void updateSelectionFields();

  QVector<CameraDeviceInfo> m_devices;
  std::function<void(const QString&)> m_diagnosticLog;
  QLabel* m_statusLabel = nullptr;
  QTableWidget* m_table = nullptr;
  QSpinBox* m_slotSpin = nullptr;
  QLineEdit* m_displayNameEdit = nullptr;
  QCheckBox* m_enabledCheck = nullptr;
};
