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

namespace
{
QString measurementItemLabel(const MeasurementRecipeConfig& config)
{
  const QString name = config.alias.trimmed().isEmpty() ? config.id
                                                        : QString("%1 (%2)").arg(config.alias.trimmed(), config.id);
  return QString("%1 | %2").arg(name, config.type);
}

QString measurementKey(const MeasurementRecipeConfig& config)
{
  return QString("%1|%2|%3").arg(config.type, config.sourceAId, config.sourceBId);
}

QString measurementValueText(const MeasurementResult& measurement)
{
  if (!measurement.valid)
  {
    return "N/D";
  }
  if (measurement.hasRealValue)
  {
    return QString("%1 %2").arg(measurement.valueReal, 0, 'f', 3).arg(measurement.unit);
  }
  return QString("%1 px").arg(measurement.valuePixels, 0, 'f', 3);
}

const MeasurementResult* findMeasurementResult(const QVector<MeasurementResult>& measurements, const MeasurementRecipeConfig& config)
{
  const QString key = measurementKey(config);
  for (const MeasurementResult& measurement : measurements)
  {
    if (QString("%1|%2|%3").arg(measurement.type, measurement.sourceAId, measurement.sourceBId) == key)
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
        config.type,
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

void MainWindowMeasurementModule::showMeasurementToleranceDialog(const CameraConfig& camera, const QString& measurementId)
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

  QDialog dialog(window());
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
  refreshMeasurementSources(camera);

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

  auto* circleDiameterButton = createTouchIconButton("circleDiameterMeasure", tr("actions.circleDiameterMeasure"), panel);
  QObject::connect(circleDiameterButton, &QPushButton::clicked, window(), [this, camera]() {
    showCircleDiameterPanel(camera);
  });
  buttonLayout->addWidget(circleDiameterButton, 0, 3);

  auto* lineAngleButton = createTouchIconButton("lineLineAngle", tr("actions.lineLineAngle"), panel);
  QObject::connect(lineAngleButton, &QPushButton::clicked, window(), [this, camera]() {
    showLineLineAnglePanel(camera);
  });
  buttonLayout->addWidget(lineAngleButton, 1, 0);

  auto* realValuesButton = createTouchIconButton("tolerances", measurements.isEmpty() ? tr("tools.tolerances") : "Misure...", panel);
  QObject::connect(realValuesButton, &QPushButton::clicked, window(), [this, camera]() {
    showMeasurementListDialog(camera);
  });
  buttonLayout->addWidget(realValuesButton, 1, 1);

  auto* backButton = createTouchIconButton("back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (!GeometryPanelNavigation::returnToSetup(context(), camera))
    {
      context().showCameraToolList(camera);
    }
  });
  buttonLayout->addWidget(backButton, 1, 2);
  layout->addWidget(buttonGrid);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}

void MainWindowMeasurementModule::showMeasurementRealValuesPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshMeasurementSources(camera);

  QVector<MeasurementRecipeConfig> configs = recipes().loadMeasurements(camera.id);

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.tolerances"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  if (configs.isEmpty())
  {
    auto* note = new QLabel("Nessuna misura configurata.", panel);
    note->setObjectName("toolPanelNote");
    note->setWordWrap(true);
    layout->addWidget(note);
  }
  else
  {
    auto* form = new QWidget(panel);
    auto* formLayout = new QFormLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(8);

    auto* measureCombo = new QComboBox(form);
    for (int i = 0; i < configs.size(); ++i)
    {
      measureCombo->addItem(measurementItemLabel(configs[i]), i);
    }

    auto* aliasEdit = new QLineEdit(form);
    aliasEdit->setPlaceholderText("Alias operatore");
    auto* realValuesEnabled = new QCheckBox("Abilita tolleranza OK/NOK", form);
    auto* sampleValue = new QDoubleSpinBox(form);
    sampleValue->setRange(-1000000.0, 1000000.0);
    sampleValue->setDecimals(4);
    sampleValue->setSuffix(" mm");
    auto* nominal = new QDoubleSpinBox(form);
    nominal->setRange(-1000000.0, 1000000.0);
    nominal->setDecimals(4);
    nominal->setSuffix(" mm");
    auto* min = new QDoubleSpinBox(form);
    min->setRange(-1000000.0, 1000000.0);
    min->setDecimals(4);
    min->setSuffix(" mm");
    auto* max = new QDoubleSpinBox(form);
    max->setRange(-1000000.0, 1000000.0);
    max->setDecimals(4);
    max->setSuffix(" mm");

    auto loadConfig = [=]() {
      const int index = measureCombo->currentData().toInt();
      if (index < 0 || index >= configs.size())
      {
        return;
      }
      const MeasurementRecipeConfig& config = configs[index];
      aliasEdit->setText(config.alias);
      const bool angle = config.type == "line_line_angle";
      const QString suffix = angle ? " deg" : " mm";
      sampleValue->setSuffix(suffix);
      nominal->setSuffix(suffix);
      min->setSuffix(suffix);
      max->setSuffix(suffix);
      realValuesEnabled->setChecked(config.hasSampleScale || config.hasNominal || config.hasMin || config.hasMax);
      sampleValue->setValue(config.sampleValue);
      nominal->setValue(config.nominal);
      min->setValue(config.min);
      max->setValue(config.max);
    };

    QObject::connect(measureCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), loadConfig);
    const auto enableTolerance = [realValuesEnabled]() {
      realValuesEnabled->setChecked(true);
    };
    QObject::connect(nominal, &QDoubleSpinBox::editingFinished, window(), enableTolerance);
    QObject::connect(min, &QDoubleSpinBox::editingFinished, window(), enableTolerance);
    QObject::connect(max, &QDoubleSpinBox::editingFinished, window(), enableTolerance);
    loadConfig();

    formLayout->addRow("Misura", measureCombo);
    formLayout->addRow("Alias operatore", aliasEdit);
    formLayout->addRow(realValuesEnabled);
    formLayout->addRow("Valore campione", sampleValue);
    formLayout->addRow("Nominale", nominal);
    formLayout->addRow("Limite minimo", min);
    formLayout->addRow("Limite massimo", max);
    layout->addWidget(form);

    auto* actionRow = new QWidget(panel);
    auto* actionLayout = new QHBoxLayout(actionRow);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(6);

    auto* saveButton = createTouchIconButton("save", "Salva valori", actionRow);
    QObject::connect(saveButton, &QPushButton::clicked, window(), [=]() mutable {
      const int index = measureCombo->currentData().toInt();
      if (index < 0 || index >= configs.size())
      {
        return;
      }
      MeasurementRecipeConfig config = configs[index];
      config.alias = aliasEdit->text().trimmed();
      const bool angle = config.type == "line_line_angle";
      config.unit = angle ? "deg" : "mm";
      const bool enabled = realValuesEnabled->isChecked();
      if (config.samplePixels <= 0.000001)
      {
        const GeometrySet& set = cameraRuntime()[camera.id].geometries();
        for (const MeasurementResult& measurement : set.measurements)
        {
          if (measurement.type == config.type &&
              measurement.sourceAId == config.sourceAId &&
              measurement.sourceBId == config.sourceBId)
          {
            config.samplePixels = measurement.valuePixels;
            break;
          }
        }
      }
      config.hasSampleScale = enabled && config.samplePixels > 0.000001;
      config.sampleValue = sampleValue->value();
      config.hasNominal = enabled;
      config.nominal = nominal->value();
      config.hasMin = enabled;
      config.min = min->value();
      config.hasMax = enabled;
      config.max = max->value();
      saveMeasurementRealSettings(camera, config);
      configs[index] = config;
      rebuildMeasurementRecipe(camera);
      if (context().geometry)
      {
        context().geometry->refreshMeasurementOverlay(camera);
      }
      if (context().updateMeasurementResults)
      {
        context().updateMeasurementResults();
      }
    });
    actionLayout->addWidget(saveButton);

    auto* deleteButton = createTouchIconButton("delete", tr("actions.deleteMeasurement"), actionRow);
    QObject::connect(deleteButton, &QPushButton::clicked, window(), [=]() {
      const int index = measureCombo->currentData().toInt();
      if (index < 0 || index >= configs.size())
      {
        return;
      }
      removeMeasurement(camera, configs[index].id);
      showMeasurementRealValuesPanel(camera);
    });
    actionLayout->addWidget(deleteButton);
    layout->addWidget(actionRow);
  }

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}
