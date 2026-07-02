#include "gui/modules/MainWindowSurfaceModule.h"

#include "gui/SurfaceLocalizationStrategies.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

void MainWindowSurfaceModule::showMassPcaLocalizationPanel(const CameraConfig& camera)
{
  QString error;
  if (!recipes().saveSurfaceLocalizationMethod(camera.id, "massPca", &error))
  {
    log(error);
    return;
  }

  const SurfaceDefectSettings settings = recipes().loadSurfaceDefectSettings(camera.id);
  const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy("massPca", translations());
  QVBoxLayout* layout = nullptr;
  auto* panel = createSurfaceLocalizationToolPanel(camera, strategy, &layout);
  addSurfaceDefectAoeButtons(camera, layout, [this, camera]() { testSurfaceLocalization(camera); });

  auto* thresholdBox = new QGroupBox(tr("labels.threshold"), panel);
  auto* thresholdLayout = new QGridLayout(thresholdBox);
  thresholdLayout->setContentsMargins(8, 10, 8, 8);

  auto* minLabel = new QLabel(tr("labels.thresholdMin"), thresholdBox);
  auto* minValue = new QLabel(QString::number(settings.thresholdMin), thresholdBox);
  minValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  auto* minSlider = new QSlider(Qt::Horizontal, thresholdBox);
  minSlider->setRange(0, 255);
  minSlider->setValue(settings.thresholdMin);

  auto* maxLabel = new QLabel(tr("labels.thresholdMax"), thresholdBox);
  auto* maxValue = new QLabel(QString::number(settings.thresholdMax), thresholdBox);
  maxValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  auto* maxSlider = new QSlider(Qt::Horizontal, thresholdBox);
  maxSlider->setRange(0, 255);
  maxSlider->setValue(settings.thresholdMax);

  thresholdLayout->addWidget(minLabel, 0, 0);
  thresholdLayout->addWidget(minValue, 0, 1);
  thresholdLayout->addWidget(minSlider, 1, 0, 1, 2);
  thresholdLayout->addWidget(maxLabel, 2, 0);
  thresholdLayout->addWidget(maxValue, 2, 1);
  thresholdLayout->addWidget(maxSlider, 3, 0, 1, 2);

  auto saveThreshold = [this, camera, minSlider, maxSlider]() {
    QString error;
    if (!recipes().saveSurfaceDefectThreshold(camera.id, minSlider->value(), maxSlider->value(), &error))
    {
      log(error);
      return;
    }
    testSurfaceLocalization(camera);
  };
  QObject::connect(minSlider, &QSlider::valueChanged, window(), [minValue, maxSlider, saveThreshold](int value) {
    minValue->setText(QString::number(value));
    if (maxSlider->value() < value)
    {
      maxSlider->setValue(value);
    }
    saveThreshold();
  });
  QObject::connect(maxSlider, &QSlider::valueChanged, window(), [maxValue, saveThreshold](int value) {
    maxValue->setText(QString::number(value));
    saveThreshold();
  });
  layout->addWidget(thresholdBox);

  auto* resolveBox = new QCheckBox(tr("labels.pcaResolveAmbiguity"), panel);
  resolveBox->setChecked(settings.pcaResolveAmbiguity);
  QObject::connect(resolveBox, &QCheckBox::toggled, window(), [this, camera](bool checked) {
    QString error;
    if (!recipes().saveSurfaceDefectPcaResolveAmbiguity(camera.id, checked, &error))
    {
      log(error);
      return;
    }
    testSurfaceLocalization(camera);
  });
  layout->addWidget(resolveBox);

  addSurfaceLocalizationBackButton(camera, layout);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + strategy.label);
  showStoredSurfaceDefectAoe(camera);
  testSurfaceLocalization(camera);
}
