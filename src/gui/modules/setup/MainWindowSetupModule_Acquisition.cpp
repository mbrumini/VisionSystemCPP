#include "gui/modules/MainWindowSetupModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/TouchIconButton.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QString camerasConfigPath()
{
  return RecipeJsonUtils::appPath("config/cameras.json");
}

QDoubleSpinBox* createNumericControl(double value, double minValue, double maxValue, const QString& suffix, QWidget* parent)
{
  auto* spin = new QDoubleSpinBox(parent);
  spin->setRange(minValue, maxValue);
  spin->setDecimals(3);
  spin->setSingleStep(1.0);
  spin->setValue(value);
  spin->setSuffix(suffix);
  return spin;
}
}

void MainWindowSetupModule::showCameraAcquisitionPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  CameraAcquisitionConfig acquisition = camera.acquisition;

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("actions.cameraAcquisitionSettings"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(
    camera.type == "vimba"
      ? "VimbaX: esposizione in microsecondi, gain in unita' camera."
      : "USB: i valori dipendono dal driver; con DirectShow l'esposizione spesso usa numeri negativi.",
    panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* exposureBox = new QGroupBox(tr("actions.exposure"), panel);
  auto* exposureLayout = new QFormLayout(exposureBox);
  auto* autoExposure = new QCheckBox(tr("labels.automatic"), exposureBox);
  autoExposure->setChecked(acquisition.autoExposure);
  auto* exposure = createNumericControl(
    acquisition.hasExposure ? acquisition.exposure : (camera.type == "vimba" ? 8000.0 : -6.0),
    camera.type == "vimba" ? 1.0 : -20.0,
    camera.type == "vimba" ? 1000000.0 : 20.0,
    camera.type == "vimba" ? " us" : "",
    exposureBox);
  exposure->setEnabled(!autoExposure->isChecked());
  exposureLayout->addRow(autoExposure);
  exposureLayout->addRow(tr("actions.exposure"), exposure);
  layout->addWidget(exposureBox);

  auto* gainBox = new QGroupBox(tr("actions.gain"), panel);
  auto* gainLayout = new QFormLayout(gainBox);
  auto* autoGain = new QCheckBox(tr("labels.automatic"), gainBox);
  autoGain->setChecked(acquisition.autoGain);
  auto* gain = createNumericControl(acquisition.hasGain ? acquisition.gain : 0.0, 0.0, 48.0, "", gainBox);
  gain->setEnabled(!autoGain->isChecked());
  gainLayout->addRow(autoGain);
  gainLayout->addRow(tr("actions.gain"), gain);
  layout->addWidget(gainBox);

  auto* whiteBox = new QGroupBox("White balance", panel);
  auto* whiteLayout = new QFormLayout(whiteBox);
  auto* autoWhite = new QCheckBox(tr("labels.automatic"), whiteBox);
  autoWhite->setChecked(acquisition.autoWhiteBalance);
  auto* whiteBalance = createNumericControl(acquisition.hasWhiteBalance ? acquisition.whiteBalance : 4500.0, 2000.0, 9000.0, " K", whiteBox);
  whiteBalance->setEnabled(!autoWhite->isChecked());
  whiteLayout->addRow(autoWhite);
  whiteLayout->addRow("Temperatura", whiteBalance);
  layout->addWidget(whiteBox);

  QObject::connect(autoExposure, &QCheckBox::toggled, exposure, &QDoubleSpinBox::setDisabled);
  QObject::connect(autoGain, &QCheckBox::toggled, gain, &QDoubleSpinBox::setDisabled);
  QObject::connect(autoWhite, &QCheckBox::toggled, whiteBalance, &QDoubleSpinBox::setDisabled);

  auto* saveButton = createTouchIconButton("saveSample", tr("actions.saveOk"), panel);
  auto* backButton = createTouchIconButton("back", tr("commands.backToSetup"), panel);
  layout->addWidget(saveButton);
  layout->addWidget(backButton);
  layout->addStretch(1);

  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, autoExposure, exposure, autoGain, gain, autoWhite, whiteBalance]() {
    CameraAcquisitionConfig updated;
    updated.autoExposure = autoExposure->isChecked();
    updated.hasExposure = !updated.autoExposure;
    updated.exposure = exposure->value();
    updated.autoGain = autoGain->isChecked();
    updated.hasGain = !updated.autoGain;
    updated.gain = gain->value();
    updated.autoWhiteBalance = autoWhite->isChecked();
    updated.hasWhiteBalance = !updated.autoWhiteBalance;
    updated.whiteBalance = whiteBalance->value();

    QString error;
    if (!config().saveCameraAcquisitionSettings(camerasConfigPath(), camera.id, updated, &error))
    {
      log(error);
      return;
    }

    CameraConfig updatedCamera = camera;
    updatedCamera.acquisition = updated;
    if (selectedCameraId() == camera.id)
    {
      selectedCamera() = updatedCamera;
    }

    const bool wasRunning = cameraRuntime()[camera.id].running();
    if (wasRunning)
    {
      stopCameraSimulation(updatedCamera, false);
      startCameraSimulation(updatedCamera, false);
    }

    log(QString("%1: %2").arg(tr("log.cameraAcquisitionSaved"), camera.id));
    showCameraAcquisitionPanel(updatedCamera);
  });

  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    showCameraSetupPanel(selectedCameraId() == camera.id ? selectedCamera() : camera);
  });

  toolsLayout()->addWidget(panel);
}
