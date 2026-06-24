#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QStringList>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QSpinBox;

class CampaignEditorDialog : public QDialog
{
public:
  CampaignEditorDialog(
    const QJsonObject& campaign,
    const QStringList& recipes,
    QWidget* parent = nullptr);

  QJsonObject campaign() const;

private:
  void addItem(const QJsonObject& item = {});
  void removeItem();
  void duplicateItem();
  void loadSelectedItem();
  void storeSelectedItem();
  void updateItemLabel(int row);
  QJsonObject formItem() const;
  void setFormItem(const QJsonObject& item);
  void syncResolutionCombo();

  QLineEdit* m_name = nullptr;
  QSpinBox* m_cycles = nullptr;
  QCheckBox* m_parallel = nullptr;
  QListWidget* m_items = nullptr;
  QComboBox* m_camera = nullptr;
  QComboBox* m_recipe = nullptr;
  QComboBox* m_strategy = nullptr;
  QComboBox* m_shape = nullptr;
  QComboBox* m_resolution = nullptr;
  QSpinBox* m_canvasWidth = nullptr;
  QSpinBox* m_canvasHeight = nullptr;
  QSpinBox* m_passes = nullptr;
  QSpinBox* m_intervalMs = nullptr;
  QDoubleSpinBox* m_xMin = nullptr;
  QDoubleSpinBox* m_xMax = nullptr;
  QDoubleSpinBox* m_xStep = nullptr;
  QDoubleSpinBox* m_yMin = nullptr;
  QDoubleSpinBox* m_yMax = nullptr;
  QDoubleSpinBox* m_yStep = nullptr;
  QDoubleSpinBox* m_angleMin = nullptr;
  QDoubleSpinBox* m_angleMax = nullptr;
  QDoubleSpinBox* m_angleStep = nullptr;
  QDoubleSpinBox* m_pixelSize = nullptr;
  QCheckBox* m_sendOnly = nullptr;
  QDoubleSpinBox* m_centerMean = nullptr;
  QDoubleSpinBox* m_centerMax = nullptr;
  QDoubleSpinBox* m_angleMean = nullptr;
  QDoubleSpinBox* m_angleErrorMax = nullptr;
  QDoubleSpinBox* m_timeMean = nullptr;
  QDoubleSpinBox* m_timeMax = nullptr;
  QJsonObject m_campaign;
  int m_loadedRow = -1;
};
