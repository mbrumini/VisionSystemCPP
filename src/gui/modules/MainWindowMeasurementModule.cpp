#include "gui/modules/MainWindowMeasurementModule.h"

#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/geometry/GeometryPanelNavigation.h"
#include "gui/TouchIconButton.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
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
  for (int i = configs.size() - 1; i >= 0; --i)
  {
    if (configs[i].id == measurementId)
    {
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

  auto* listRow = new QWidget(panel);
  auto* listLayout = new QHBoxLayout(listRow);
  listLayout->setContentsMargins(0, 0, 0, 0);
  listLayout->setSpacing(6);

  auto* measureCombo = new QComboBox(listRow);
  for (const MeasurementRecipeConfig& config : measurements)
  {
    measureCombo->addItem(measurementItemLabel(config), config.id);
  }

  auto* deleteButton = createTouchIconButton("delete", tr("actions.deleteMeasurement"), listRow);
  QObject::connect(deleteButton, &QPushButton::clicked, window(), [this, camera, measureCombo, refreshPanel]() {
    removeMeasurement(camera, measureCombo->currentData().toString());
    refreshPanel();
  });

  listLayout->addWidget(measureCombo, 1);
  listLayout->addWidget(deleteButton);
  layout->addWidget(listRow);
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
  if (!measurements.isEmpty())
  {
    appendMeasurementListControls(panel, layout, camera, [this, camera]() { showMeasurementPanel(camera); });
  }

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

  auto* realValuesButton = createTouchIconButton("tolerances", tr("tools.tolerances"), panel);
  QObject::connect(realValuesButton, &QPushButton::clicked, window(), [this, camera]() {
    showMeasurementRealValuesPanel(camera);
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
