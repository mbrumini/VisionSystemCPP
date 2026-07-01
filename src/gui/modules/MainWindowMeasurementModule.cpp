#include "gui/modules/MainWindowMeasurementModule.h"

#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/geometry/GeometryPanelNavigation.h"
#include "gui/TouchIconButton.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <functional>
#include <memory>

namespace
{
QString measurementItemLabel(const MeasurementRecipeConfig& config)
{
  const QString name = config.alias.trimmed().isEmpty() ? config.id
                                                        : QString("%1 (%2)").arg(config.alias.trimmed(), config.id);
  QString type = config.type;
  QString criterion = config.criterion;
  if (type == "line_line_distance_min")
  {
    type = "line_line_distance";
    criterion = "min";
  }
  else if (type == "line_line_distance_max")
  {
    type = "line_line_distance";
    criterion = "max";
  }
  else if (type == "arc_arc_distance_min")
  {
    type = "arc_arc_distance";
    criterion = "min";
  }
  const QString criterionLabel = criterion == "min" ? "MIN" : (criterion == "max" ? "MAX" : "MEDIA");
  return QString("%1 | %2 | %3").arg(name, type, criterionLabel);
}

QString measurementKey(const MeasurementRecipeConfig& config)
{
  return QString("%1|%2|%3|%4").arg(config.type, config.criterion, config.sourceAId, config.sourceBId);
}

QString measurementValueText(const MeasurementResult& measurement)
{
  if (!measurement.valid)
  {
    return "N/D";
  }
  if (measurement.hasRealValue)
  {
    if (measurement.unit != "deg")
    {
      return QString("%1 %2 (%3 px)")
        .arg(measurement.valueReal, 0, 'f', 3)
        .arg(measurement.unit)
        .arg(measurement.valuePixels, 0, 'f', 3);
    }
    return QString("%1 %2").arg(measurement.valueReal, 0, 'f', 3).arg(measurement.unit);
  }
  return QString("%1 px").arg(measurement.valuePixels, 0, 'f', 3);
}

const MeasurementResult* findMeasurementResult(const QVector<MeasurementResult>& measurements, const MeasurementRecipeConfig& config)
{
  const QString key = measurementKey(config);
  for (const MeasurementResult& measurement : measurements)
  {
    if (QString("%1|%2|%3|%4").arg(measurement.type, measurement.criterion, measurement.sourceAId, measurement.sourceBId) == key)
    {
      return &measurement;
    }
  }
  return nullptr;
}

double currentMeasurementPixels(const GeometrySet& set, const MeasurementRecipeConfig& config)
{
  if (const MeasurementResult* result = findMeasurementResult(set.measurements, config))
  {
    return result->valid ? result->valuePixels : 0.0;
  }
  return 0.0;
}

QString threadMeasurementLabel(const QString& threadKey, const std::function<QString(const QString&)>& trText = {})
{
  if (threadKey == QStringLiteral("major"))
  {
    return trText ? trText(QStringLiteral("labels.threadMajorDiameter")) : QStringLiteral("Diametro esterno");
  }
  if (threadKey == QStringLiteral("pitch"))
  {
    return trText ? trText(QStringLiteral("labels.threadPitch")) : QStringLiteral("Passo filetto");
  }
  if (threadKey == QStringLiteral("pitch_diameter"))
  {
    return trText ? trText(QStringLiteral("labels.threadPitchDiameter")) : QStringLiteral("Diametro medio");
  }
  if (threadKey == QStringLiteral("phase"))
  {
    return trText ? trText(QStringLiteral("labels.threadPhase")) : QStringLiteral("Fase filetto");
  }
  if (threadKey == QStringLiteral("minor"))
  {
    return trText ? trText(QStringLiteral("labels.threadMinorDiameter")) : QStringLiteral("Diametro interno");
  }
  return threadKey;
}

QString threadMeasurementType(const QString& threadKey)
{
  if (threadKey == QStringLiteral("major"))
  {
    return QStringLiteral("thread_major_diameter");
  }
  if (threadKey == QStringLiteral("minor"))
  {
    return QStringLiteral("thread_minor_diameter");
  }
  if (threadKey == QStringLiteral("pitch"))
  {
    return QStringLiteral("thread_pitch");
  }
  if (threadKey == QStringLiteral("pitch_diameter"))
  {
    return QStringLiteral("thread_pitch_diameter");
  }
  if (threadKey == QStringLiteral("phase"))
  {
    return QStringLiteral("thread_phase");
  }
  return QString("thread_%1").arg(threadKey);
}

const MeasurementResult* findThreadMeasurementResult(const QVector<MeasurementResult>& measurements,
                                                     const QString& threadKey)
{
  const QString type = threadMeasurementType(threadKey);
  const QString id = QString("thread_%1").arg(threadKey);
  const MeasurementResult* fallback = nullptr;
  for (const MeasurementResult& measurement : measurements)
  {
    if (measurement.type != type && measurement.id != id)
    {
      continue;
    }

    if (measurement.valid)
    {
      return &measurement;
    }

    if (!fallback)
    {
      fallback = &measurement;
    }
  }
  return fallback;
}

ThreadMeasurementLimits* threadLimitsForKey(ThreadInspectionSettings& settings, const QString& threadKey)
{
  if (threadKey == QStringLiteral("major"))
  {
    return &settings.majorDiameter;
  }
  if (threadKey == QStringLiteral("pitch"))
  {
    return &settings.pitchLength;
  }
  if (threadKey == QStringLiteral("pitch_diameter"))
  {
    return &settings.pitchDiameter;
  }
  if (threadKey == QStringLiteral("phase"))
  {
    return &settings.phaseOffset;
  }
  if (threadKey == QStringLiteral("minor"))
  {
    return &settings.minorDiameter;
  }
  return nullptr;
}

const ThreadMeasurementLimits* threadLimitsForKey(
  const ThreadInspectionSettings& settings,
  const QString& threadKey)
{
  return threadLimitsForKey(const_cast<ThreadInspectionSettings&>(settings), threadKey);
}

struct ToleranceEntry
{
  enum class Source
  {
    Geometry,
    Thread
  };

  Source source = Source::Geometry;
  int geometryIndex = -1;
  QString threadKey;
};

QVector<ToleranceEntry> buildToleranceEntries(
  const QVector<MeasurementRecipeConfig>& configs,
  const ThreadInspectionSettings& threadSettings)
{
  QVector<ToleranceEntry> entries;
  for (int index = 0; index < configs.size(); ++index)
  {
    entries.append({ToleranceEntry::Source::Geometry, index, {}});
  }

  if (threadSettings.enabled && threadSettings.hasExtractionRoi)
  {
    entries.append({ToleranceEntry::Source::Thread, -1, QStringLiteral("major")});
    entries.append({ToleranceEntry::Source::Thread, -1, QStringLiteral("minor")});
    entries.append({ToleranceEntry::Source::Thread, -1, QStringLiteral("pitch_diameter")});
    entries.append({ToleranceEntry::Source::Thread, -1, QStringLiteral("pitch")});
    entries.append({ToleranceEntry::Source::Thread, -1, QStringLiteral("phase")});
  }

  return entries;
}

QString toleranceEntryLabel(const ToleranceEntry& entry, const QVector<MeasurementRecipeConfig>& configs)
{
  if (entry.source == ToleranceEntry::Source::Geometry)
  {
    return measurementItemLabel(configs[entry.geometryIndex]);
  }

  return threadMeasurementLabel(entry.threadKey);
}
}

MainWindowMeasurementModule::MainWindowMeasurementModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

void MainWindowMeasurementModule::removeMeasurement(const CameraConfig& camera, const QString& measurementId)
{
  if (measurementId.isEmpty())
  {
    return;
  }

  QVector<MeasurementRecipeConfig> configs = recipes().loadMeasurements(camera.id);
  bool removed = false;
  QString removedKey;
  for (int i = configs.size() - 1; i >= 0; --i)
  {
    if (configs[i].id == measurementId)
    {
      removedKey = measurementKey(configs[i]);
      configs.removeAt(i);
      removed = true;
      break;
    }
  }

  if (!removed)
  {
    return;
  }

  QString error;
  if (!recipes().saveMeasurements(camera.id, configs, &error))
  {
    log(QString("%1: %2").arg(tr("log.measurementRecipeSaveFailed"), error));
    return;
  }

  if (m_selectedMeasurementKeys.value(camera.id) == removedKey)
  {
    m_selectedMeasurementKeys.remove(camera.id);
  }

  rebuildMeasurementRecipe(camera);
  if (context().geometry && camera.id == selectedCameraId())
  {
    context().geometry->showRuntimeGeometryOverlay(camera);
  }
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
  log(tr("log.measurementDeleted") + ": " + measurementId);
}

void MainWindowMeasurementModule::refreshMeasurementSources(const CameraConfig& camera)
{
  if (context().setup)
  {
    context().setup->refreshPoseForCurrentFrame(camera);
  }
  if (context().surface)
  {
    const PartPose& pose = cameraRuntime()[camera.id].currentPose();
    if (!pose.valid)
    {
      context().surface->localizePoseOnSample(camera);
    }
  }
  if (context().geometry)
  {
    context().geometry->testConfiguredGeometryLines(camera);
  }
  if (context().constructedGeometry)
  {
    context().constructedGeometry->rebuildConstructedGeometryRecipe(camera);
  }
  if (context().geometry)
  {
    context().geometry->syncRuntimeGeometryLabels(camera);
  }
  rebuildMeasurementRecipe(camera);
  if (context().geometry && camera.id == selectedCameraId())
  {
    context().geometry->showRuntimeGeometryOverlay(camera);
  }
}

QHash<QString, QString> MainWindowMeasurementModule::geometryAliasesForCamera(const CameraConfig& camera) const
{
  if (!context().geometry)
  {
    return {};
  }

  return context().geometry->geometryAliasMap(camera.id);
}

void MainWindowMeasurementModule::appendMeasurementListControls(QWidget* panel,
                                                                  QVBoxLayout* layout,
                                                                  const CameraConfig& camera,
                                                                  const std::function<void()>& refreshPanel)
{
  const QVector<MeasurementRecipeConfig> measurements = recipes().loadMeasurements(camera.id);
  if (measurements.isEmpty())
  {
    return;
  }

  auto* listButton = createTouchIconButton("ruler", "Misure...", panel);
  QObject::connect(listButton, &QPushButton::clicked, window(), [this, camera, refreshPanel]() {
    showMeasurementListDialog(camera);
    refreshPanel();
  });
  layout->addWidget(listButton);
}

QString MainWindowMeasurementModule::selectedMeasurementKey(const QString& cameraId) const
{
  return m_selectedMeasurementKeys.value(cameraId);
}

void MainWindowMeasurementModule::showMeasurementListDialog(const CameraConfig& camera)
{
  refreshMeasurementSources(camera);

  QDialog dialog(window());
  dialog.setWindowTitle(QString("Misure | %1").arg(camera.id));
  dialog.resize(820, 420);

  auto* layout = new QVBoxLayout(&dialog);
  auto* table = new QTableWidget(0, 6, &dialog);
  table->setHorizontalHeaderLabels({"Nome", "Alias", "Tipo", "Valore", "Stato", "Sorgenti"});
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setStretchLastSection(true);

  const auto reloadTable = [this, &camera, table]() {
    rebuildMeasurementRecipe(camera);
    const QVector<MeasurementRecipeConfig> measurements = recipes().loadMeasurements(camera.id);
    const GeometrySet& set = cameraRuntime()[camera.id].geometries();
    const QString activeKey = selectedMeasurementKey(camera.id);
    int activeRow = -1;

    table->setRowCount(0);
    table->setRowCount(measurements.size());
    for (int row = 0; row < measurements.size(); ++row)
    {
      const MeasurementRecipeConfig& config = measurements[row];
      const QString key = measurementKey(config);
      const MeasurementResult* result = findMeasurementResult(set.measurements, config);
      const QString state = result && result->valid
        ? (result->judgement.isEmpty() ? "OK" : result->judgement)
        : "N/D";
      const QString value = result ? measurementValueText(*result) : "N/D";

      const QStringList values = {
        config.id,
        config.alias,
        measurementItemLabel(config),
        value,
        state,
        config.sourceBId.isEmpty() ? config.sourceAId : QString("%1 / %2").arg(config.sourceAId, config.sourceBId)
      };
      for (int column = 0; column < values.size(); ++column)
      {
        auto* item = new QTableWidgetItem(values[column]);
        item->setData(Qt::UserRole, key);
        item->setData(Qt::UserRole + 1, config.id);
        table->setItem(row, column, item);
      }
      if (key == activeKey)
      {
        activeRow = row;
      }
    }

    if (activeRow >= 0)
    {
      table->selectRow(activeRow);
    }
    table->resizeColumnsToContents();
  };
  reloadTable();
  layout->addWidget(table);

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* toleranceButton = buttons->addButton("Tolleranze...", QDialogButtonBox::ActionRole);
  auto* deleteButton = buttons->addButton(tr("actions.deleteMeasurement"), QDialogButtonBox::DestructiveRole);
  buttons->addButton(QDialogButtonBox::Close);
  layout->addWidget(buttons);

  QObject::connect(table, &QTableWidget::cellClicked, &dialog, [this, camera, table](int row, int) {
    if (auto* item = table->item(row, 0))
    {
      m_selectedMeasurementKeys[camera.id] = item->data(Qt::UserRole).toString();
      if (context().geometry)
      {
        context().geometry->refreshMeasurementOverlay(camera);
      }
    }
  });
  QObject::connect(toleranceButton, &QPushButton::clicked, &dialog, [this, camera, table, reloadTable]() {
    const int row = table->currentRow();
    if (row < 0)
    {
      return;
    }
    const QString measurementId = table->item(row, 0)->data(Qt::UserRole + 1).toString();
    showMeasurementToleranceDialog(camera, measurementId);
    reloadTable();
  });
  QObject::connect(deleteButton, &QPushButton::clicked, &dialog, [this, camera, table, reloadTable]() {
    const int row = table->currentRow();
    if (row < 0)
    {
      return;
    }
    const QString measurementId = table->item(row, 0)->data(Qt::UserRole + 1).toString();
    removeMeasurement(camera, measurementId);
    reloadTable();
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  dialog.exec();
}

void MainWindowMeasurementModule::showMeasurementToleranceDialog(const CameraConfig& camera,
                                                              const QString& measurementId,
                                                              QWidget* parent)
{
  QVector<MeasurementRecipeConfig> configs = recipes().loadMeasurements(camera.id);
  int configIndex = -1;
  for (int i = 0; i < configs.size(); ++i)
  {
    if (configs[i].id == measurementId)
    {
      configIndex = i;
      break;
    }
  }
  if (configIndex < 0)
  {
    return;
  }

  MeasurementRecipeConfig config = configs[configIndex];
  const bool angle = config.type == "line_line_angle";
  const QString unit = angle ? " deg" : " mm";

  QDialog dialog(parent ? parent : window());
  dialog.setWindowTitle(QString("Tolleranze | %1").arg(config.alias.trimmed().isEmpty() ? config.id : config.alias));
  auto* layout = new QVBoxLayout(&dialog);

  auto* aliasEdit = new QLineEdit(config.alias, &dialog);
  aliasEdit->setPlaceholderText("Alias");
  layout->addWidget(aliasEdit);

  auto* conversionBox = new QGroupBox("Valore reale", &dialog);
  auto* conversionLayout = new QVBoxLayout(conversionBox);
  auto* calibrationRadio = new QRadioButton("Usa calibrazione scacchi camera", conversionBox);
  auto* sampleRadio = new QRadioButton("Usa valore reale misurato sul pezzo", conversionBox);
  auto* sampleValue = new QDoubleSpinBox(conversionBox);
  sampleValue->setRange(-1000000.0, 1000000.0);
  sampleValue->setDecimals(4);
  sampleValue->setSuffix(unit);
  sampleValue->setValue(config.sampleValue);
  conversionLayout->addWidget(calibrationRadio);
  conversionLayout->addWidget(sampleRadio);
  conversionLayout->addWidget(sampleValue);
  layout->addWidget(conversionBox);

  if (angle)
  {
    conversionBox->setEnabled(false);
  }
  else if (config.hasSampleScale)
  {
    sampleRadio->setChecked(true);
  }
  else
  {
    calibrationRadio->setChecked(true);
  }
  sampleValue->setEnabled(sampleRadio->isChecked() && !angle);
  QObject::connect(sampleRadio, &QRadioButton::toggled, &dialog, [sampleValue, angle](bool checked) {
    sampleValue->setEnabled(checked && !angle);
  });

  auto* toleranceBox = new QGroupBox("Tolleranza", &dialog);
  auto* toleranceLayout = new QVBoxLayout(toleranceBox);
  auto* toleranceEnabled = new QCheckBox("Abilita OK/NOK", toleranceBox);
  toleranceEnabled->setChecked(config.hasMin || config.hasMax);
  auto* minMaxRadio = new QRadioButton("Min / Max", toleranceBox);
  auto* nominalRadio = new QRadioButton("Nominale + / -", toleranceBox);
  auto* toleranceForm = new QWidget(toleranceBox);
  auto* form = new QFormLayout(toleranceForm);
  form->setContentsMargins(0, 0, 0, 0);

  auto* nominal = new QDoubleSpinBox(toleranceForm);
  auto* tolMinus = new QDoubleSpinBox(toleranceForm);
  auto* tolPlus = new QDoubleSpinBox(toleranceForm);
  auto* min = new QDoubleSpinBox(toleranceForm);
  auto* max = new QDoubleSpinBox(toleranceForm);
  for (QDoubleSpinBox* spin : {nominal, tolMinus, tolPlus, min, max})
  {
    spin->setRange(-1000000.0, 1000000.0);
    spin->setDecimals(4);
    spin->setSuffix(unit);
  }

  nominal->setValue(config.nominal);
  min->setValue(config.min);
  max->setValue(config.max);
  tolMinus->setValue(config.hasNominal && config.hasMin ? config.min - config.nominal : 0.0);
  tolPlus->setValue(config.hasNominal && config.hasMax ? config.max - config.nominal : 0.0);
  if (config.hasNominal && config.hasMin && config.hasMax)
  {
    nominalRadio->setChecked(true);
  }
  else
  {
    minMaxRadio->setChecked(true);
  }

  form->addRow("Nominale", nominal);
  form->addRow("Offset limite basso", tolMinus);
  form->addRow("Offset limite alto", tolPlus);
  form->addRow("Min", min);
  form->addRow("Max", max);
  toleranceLayout->addWidget(toleranceEnabled);
  toleranceLayout->addWidget(minMaxRadio);
  toleranceLayout->addWidget(nominalRadio);
  toleranceLayout->addWidget(toleranceForm);
  layout->addWidget(toleranceBox);

  const auto syncToleranceMode = [=]() {
    const bool enabled = toleranceEnabled->isChecked();
    nominalRadio->setEnabled(enabled);
    minMaxRadio->setEnabled(enabled);
    nominal->setEnabled(enabled && nominalRadio->isChecked());
    tolMinus->setEnabled(enabled && nominalRadio->isChecked());
    tolPlus->setEnabled(enabled && nominalRadio->isChecked());
    min->setEnabled(enabled && minMaxRadio->isChecked());
    max->setEnabled(enabled && minMaxRadio->isChecked());
  };
  QObject::connect(toleranceEnabled, &QCheckBox::toggled, &dialog, syncToleranceMode);
  QObject::connect(minMaxRadio, &QRadioButton::toggled, &dialog, syncToleranceMode);
  QObject::connect(nominalRadio, &QRadioButton::toggled, &dialog, syncToleranceMode);
  syncToleranceMode();

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* saveButton = buttons->addButton("Salva misura", QDialogButtonBox::AcceptRole);
  buttons->addButton(QDialogButtonBox::Cancel);
  layout->addWidget(buttons);
  QObject::connect(saveButton, &QPushButton::clicked, &dialog, [&]() {
    config.alias = aliasEdit->text().trimmed();
    config.unit = angle ? "deg" : "mm";
    if (angle)
    {
      config.hasSampleScale = false;
      config.samplePixels = currentMeasurementPixels(cameraRuntime()[camera.id].geometries(), config);
      config.unit = "deg";
    }
    else
    {
      config.hasSampleScale = sampleRadio->isChecked();
      config.sampleValue = sampleValue->value();
      if (config.hasSampleScale && config.samplePixels <= 0.000001)
      {
        config.samplePixels = currentMeasurementPixels(cameraRuntime()[camera.id].geometries(), config);
      }
    }

    if (!toleranceEnabled->isChecked())
    {
      config.hasNominal = false;
      config.hasMin = false;
      config.hasMax = false;
    }
    else if (nominalRadio->isChecked())
    {
      config.nominal = nominal->value();
      config.min = config.nominal + tolMinus->value();
      config.max = config.nominal + tolPlus->value();
      if (config.max < config.min)
      {
        std::swap(config.min, config.max);
      }
      config.hasNominal = true;
      config.hasMin = true;
      config.hasMax = true;
    }
    else
    {
      config.min = min->value();
      config.max = max->value();
      config.nominal = (config.min + config.max) * 0.5;
      config.hasNominal = false;
      config.hasMin = true;
      config.hasMax = true;
    }

    saveMeasurementRealSettings(camera, config);
    rebuildMeasurementRecipe(camera);
    if (context().geometry)
    {
      context().geometry->refreshMeasurementOverlay(camera);
    }
    if (context().updateMeasurementResults)
    {
      context().updateMeasurementResults();
    }
    dialog.accept();
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  dialog.exec();
}

void MainWindowMeasurementModule::showMeasurementPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();
  m_imagePickMode = ImagePickMode::None;
  m_pickPrimaryCombo = nullptr;
  m_pickSecondaryCombo = nullptr;
  m_nextPickTarget = 0;
  refreshMeasurementSources(camera);
  if (context().geometry)
  {
    context().geometry->showRuntimeGeometryOverlay(camera);
  }

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.measurements"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  const QVector<MeasurementRecipeConfig> measurements = recipes().loadMeasurements(camera.id);

  auto* buttonGrid = new QWidget(panel);
  auto* buttonLayout = new QGridLayout(buttonGrid);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(6);
  buttonLayout->setColumnStretch(0, 1);
  buttonLayout->setColumnStretch(1, 1);
  buttonLayout->setColumnStretch(2, 1);

  auto* pointPointButton = createTouchIconButton("pointPointDistance", tr("actions.pointPointDistance"), panel);
  QObject::connect(pointPointButton, &QPushButton::clicked, window(), [this, camera]() {
    showPointPointDistancePanel(camera);
  });
  buttonLayout->addWidget(pointPointButton, 0, 0);

  auto* pointLineButton = createTouchIconButton("pointLineDistance", tr("actions.pointLineDistance"), panel);
  QObject::connect(pointLineButton, &QPushButton::clicked, window(), [this, camera]() {
    showPointLineDistancePanel(camera);
  });
  buttonLayout->addWidget(pointLineButton, 0, 1);

  auto* lineLineButton = createTouchIconButton("lineLineDistance", tr("actions.lineLineDistance"), panel);
  QObject::connect(lineLineButton, &QPushButton::clicked, window(), [this, camera]() {
    showLineLineDistancePanel(camera);
  });
  buttonLayout->addWidget(lineLineButton, 0, 2);

  auto* lineAngleButton = createTouchIconButton("lineLineAngle", tr("actions.lineLineAngle"), panel);
  QObject::connect(lineAngleButton, &QPushButton::clicked, window(), [this, camera]() {
    showLineLineAnglePanel(camera);
  });
  buttonLayout->addWidget(lineAngleButton, 1, 0);

  auto* arcArcButton = createTouchIconButton("arcArcDistance", tr("actions.arcArcDistance"), panel);
  QObject::connect(arcArcButton, &QPushButton::clicked, window(), [this, camera]() {
    showArcArcDistancePanel(camera);
  });
  buttonLayout->addWidget(arcArcButton, 1, 1);

  auto* circleDiameterButton = createTouchIconButton("circleDiameterMeasure", tr("actions.circleDiameterMeasure"), panel);
  QObject::connect(circleDiameterButton, &QPushButton::clicked, window(), [this, camera]() {
    showCircleDiameterPanel(camera);
  });
  buttonLayout->addWidget(circleDiameterButton, 1, 2);

  auto* realValuesButton = createTouchIconButton("tolerances", tr("tools.tolerances"), panel);
  QObject::connect(realValuesButton, &QPushButton::clicked, window(), [this, camera]() {
    showTolerancesDialog(camera);
  });
  buttonLayout->addWidget(realValuesButton, 2, 0);

  auto* backButton = createTouchIconButton("back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (!GeometryPanelNavigation::returnToSetup(context(), camera))
    {
      context().showCameraToolList(camera);
    }
  });
  buttonLayout->addWidget(backButton, 2, 1);
  layout->addWidget(buttonGrid);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}

void MainWindowMeasurementModule::showThreadToleranceDialog(const CameraConfig& camera,
                                                            const QString& threadKey,
                                                            QWidget* parent)
{
  ThreadInspectionSettings settings = recipes().loadThreadInspectionSettings(camera.id);
  ThreadMeasurementLimits* limits = threadLimitsForKey(settings, threadKey);
  if (!limits)
  {
    return;
  }

  QDialog dialog(parent ? parent : window());
  dialog.setWindowTitle(QString("%1 | %2").arg(
    tr("tools.tolerances"),
    threadMeasurementLabel(threadKey, [this](const QString& key) { return tr(key); })));
  auto* layout = new QVBoxLayout(&dialog);

  auto* aliasEdit = new QLineEdit(limits->alias, &dialog);
  aliasEdit->setPlaceholderText(tr("labels.operatorAlias"));
  layout->addWidget(aliasEdit);

  auto* measureEnabled = new QCheckBox(tr("labels.enableMeasurement"), &dialog);
  measureEnabled->setChecked(limits->enabled);
  layout->addWidget(measureEnabled);

  auto* toleranceBox = new QGroupBox("Tolleranza", &dialog);
  auto* toleranceLayout = new QVBoxLayout(toleranceBox);
  auto* toleranceEnabled = new QCheckBox(tr("labels.enableToleranceOkNok"), toleranceBox);
  toleranceEnabled->setChecked(limits->hasNominal || limits->hasMin || limits->hasMax);
  auto* minMaxRadio = new QRadioButton("Min / Max", toleranceBox);
  auto* nominalRadio = new QRadioButton("Nominale + / -", toleranceBox);
  auto* toleranceForm = new QWidget(toleranceBox);
  auto* form = new QFormLayout(toleranceForm);
  form->setContentsMargins(0, 0, 0, 0);

  auto* nominal = new QDoubleSpinBox(toleranceForm);
  auto* tolMinus = new QDoubleSpinBox(toleranceForm);
  auto* tolPlus = new QDoubleSpinBox(toleranceForm);
  auto* min = new QDoubleSpinBox(toleranceForm);
  auto* max = new QDoubleSpinBox(toleranceForm);
  for (QDoubleSpinBox* spin : {nominal, tolMinus, tolPlus, min, max})
  {
    spin->setRange(-1000000.0, 1000000.0);
    spin->setDecimals(4);
    spin->setSuffix(threadKey == QStringLiteral("phase") ? QStringLiteral(" deg") : QStringLiteral(" mm"));
  }

  nominal->setValue(limits->nominal);
  min->setValue(limits->min);
  max->setValue(limits->max);
  tolMinus->setValue(limits->hasNominal && limits->hasMin ? limits->min - limits->nominal : 0.0);
  tolPlus->setValue(limits->hasNominal && limits->hasMax ? limits->max - limits->nominal : 0.0);
  if (limits->hasNominal && limits->hasMin && limits->hasMax)
  {
    nominalRadio->setChecked(true);
  }
  else
  {
    minMaxRadio->setChecked(true);
  }

  form->addRow(tr("labels.nominal"), nominal);
  form->addRow("Offset limite basso", tolMinus);
  form->addRow("Offset limite alto", tolPlus);
  form->addRow(tr("labels.minLimit"), min);
  form->addRow(tr("labels.maxLimit"), max);
  toleranceLayout->addWidget(toleranceEnabled);
  toleranceLayout->addWidget(minMaxRadio);
  toleranceLayout->addWidget(nominalRadio);
  toleranceLayout->addWidget(toleranceForm);
  layout->addWidget(toleranceBox);

  const auto syncToleranceMode = [=]() {
    const bool enabled = toleranceEnabled->isChecked();
    nominalRadio->setEnabled(enabled);
    minMaxRadio->setEnabled(enabled);
    nominal->setEnabled(enabled && nominalRadio->isChecked());
    tolMinus->setEnabled(enabled && nominalRadio->isChecked());
    tolPlus->setEnabled(enabled && nominalRadio->isChecked());
    min->setEnabled(enabled && minMaxRadio->isChecked());
    max->setEnabled(enabled && minMaxRadio->isChecked());
  };
  QObject::connect(toleranceEnabled, &QCheckBox::toggled, &dialog, syncToleranceMode);
  QObject::connect(minMaxRadio, &QRadioButton::toggled, &dialog, syncToleranceMode);
  QObject::connect(nominalRadio, &QRadioButton::toggled, &dialog, syncToleranceMode);
  syncToleranceMode();

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* saveButton = buttons->addButton(tr("actions.saveMeasurement"), QDialogButtonBox::AcceptRole);
  buttons->addButton(QDialogButtonBox::Cancel);
  layout->addWidget(buttons);
  QObject::connect(saveButton, &QPushButton::clicked, &dialog, [&]() {
    limits->enabled = measureEnabled->isChecked();
    limits->alias = aliasEdit->text().trimmed();
    if (!toleranceEnabled->isChecked())
    {
      limits->hasNominal = false;
      limits->hasMin = false;
      limits->hasMax = false;
    }
    else if (nominalRadio->isChecked())
    {
      limits->nominal = nominal->value();
      limits->min = limits->nominal + tolMinus->value();
      limits->max = limits->nominal + tolPlus->value();
      if (limits->max < limits->min)
      {
        std::swap(limits->min, limits->max);
      }
      limits->hasNominal = true;
      limits->hasMin = true;
      limits->hasMax = true;
    }
    else
    {
      limits->min = min->value();
      limits->max = max->value();
      limits->nominal = (limits->min + limits->max) * 0.5;
      limits->hasNominal = false;
      limits->hasMin = true;
      limits->hasMax = true;
    }

    QString error;
    if (!recipes().saveThreadInspectionSettings(camera.id, settings, &error))
    {
      log(error);
      return;
    }

    rebuildMeasurementRecipe(camera);
    if (context().refreshThreadProfileOverlay)
    {
      context().refreshThreadProfileOverlay(camera);
    }
    if (context().geometry)
    {
      context().geometry->refreshMeasurementOverlay(camera);
    }
    if (context().updateMeasurementResults)
    {
      context().updateMeasurementResults();
    }
    dialog.accept();
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  dialog.exec();
}

void MainWindowMeasurementModule::showTolerancesDialog(const CameraConfig& camera)
{
  refreshMeasurementSources(camera);

  QVector<MeasurementRecipeConfig> configs = recipes().loadMeasurements(camera.id);
  const ThreadInspectionSettings threadSettings = recipes().loadThreadInspectionSettings(camera.id);
  const QVector<ToleranceEntry> entries = buildToleranceEntries(configs, threadSettings);

  QDialog dialog(window());
  dialog.setWindowTitle(QString("%1 | %2").arg(tr("tools.tolerances"), camera.id));
  dialog.resize(820, 420);

  auto* layout = new QVBoxLayout(&dialog);
  if (entries.isEmpty())
  {
    auto* note = new QLabel(tr("labels.noMeasurementsConfigured"), &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
    return;
  }

  auto* table = new QTableWidget(0, 5, &dialog);
  table->setHorizontalHeaderLabels({"Nome", "Tipo", "Valore", "Stato", "Origine"});
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setStretchLastSection(true);

  const auto reloadTable = [this, &camera, &configs, table, entries]() {
    rebuildMeasurementRecipe(camera);
    const GeometrySet& set = cameraRuntime()[camera.id].geometries();
    table->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row)
    {
      const ToleranceEntry& entry = entries[row];
      QString name;
      QString type;
      QString value = "N/D";
      QString state = "N/D";
      QString origin;

      if (entry.source == ToleranceEntry::Source::Geometry)
      {
        const MeasurementRecipeConfig& config = configs[entry.geometryIndex];
        name = config.alias.trimmed().isEmpty() ? config.id : config.alias;
        type = config.type;
        origin = "Geometria";
        if (const MeasurementResult* result = findMeasurementResult(set.measurements, config))
        {
          value = measurementValueText(*result);
          state = result->valid
            ? (result->judgement.isEmpty() ? "OK" : result->judgement)
            : "N/D";
        }
      }
      else
      {
        name = threadMeasurementLabel(entry.threadKey, [this](const QString& key) { return tr(key); });
        type = threadMeasurementType(entry.threadKey);
        origin = "Filetto";
        if (const MeasurementResult* result = findThreadMeasurementResult(set.measurements, entry.threadKey))
        {
          value = measurementValueText(*result);
          state = result->valid
            ? (result->judgement.isEmpty() ? "OK" : result->judgement)
            : "N/D";
        }
        const ThreadInspectionSettings threadLimitsSource =
          recipes().loadThreadInspectionSettings(camera.id);
        const ThreadMeasurementLimits* limits =
          threadLimitsForKey(threadLimitsSource, entry.threadKey);
        if (state == QStringLiteral("N/D") && limits && limits->enabled)
        {
          state = limits->hasMin || limits->hasMax ? "Configurato" : "N/D";
        }
      }

      const QStringList values = {name, type, value, state, origin};
      for (int column = 0; column < values.size(); ++column)
      {
        auto* item = new QTableWidgetItem(values[column]);
        item->setData(Qt::UserRole, row);
        table->setItem(row, column, item);
      }
    }
    table->resizeColumnsToContents();
  };
  reloadTable();
  layout->addWidget(table);
  if (table->rowCount() > 0)
  {
    table->selectRow(0);
  }

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* editButton = buttons->addButton("Tolleranze...", QDialogButtonBox::ActionRole);
  buttons->addButton(QDialogButtonBox::Close);
  layout->addWidget(buttons);

  const auto editSelected = [this, &camera, &configs, &dialog, table, entries, reloadTable]() {
    const int row = table->currentRow();
    if (row < 0 || row >= entries.size())
    {
      return;
    }
    const ToleranceEntry& entry = entries[row];
    if (entry.source == ToleranceEntry::Source::Geometry)
    {
      showMeasurementToleranceDialog(camera, configs[entry.geometryIndex].id, &dialog);
    }
    else
    {
      showThreadToleranceDialog(camera, entry.threadKey, &dialog);
    }
    configs = recipes().loadMeasurements(camera.id);
    reloadTable();
  };

  QObject::connect(table, &QTableWidget::cellDoubleClicked, &dialog, editSelected);
  QObject::connect(editButton, &QPushButton::clicked, &dialog, editSelected);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  dialog.exec();
}

void MainWindowMeasurementModule::showMeasurementRealValuesPanel(const CameraConfig& camera)
{
  showTolerancesDialog(camera);
}
