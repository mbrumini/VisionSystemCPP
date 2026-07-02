#include "gui/modules/MainWindowSurfaceModule.h"

#include "gui/SurfaceLocalizationStrategies.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QCheckBox>

void MainWindowSurfaceModule::showEdgePcaLocalizationPanel(const CameraConfig& camera)
{
  QString error;
  if (!recipes().saveSurfaceLocalizationMethod(camera.id, "edgePca", &error))
  {
    log(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
  const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy("edgePca", translations());
  QVBoxLayout* layout = nullptr;
  auto* panel = createSurfaceLocalizationToolPanel(camera, strategy, &layout);
  addSurfaceDefectAoeButtons(camera, layout, [this, camera]() { testSurfaceEdgePcaLocalization(camera); });

  auto* sensitivityBox = new QGroupBox(tr("labels.edgeSensitivity"), panel);
  auto* sensitivityLayout = new QGridLayout(sensitivityBox);
  sensitivityLayout->setContentsMargins(8, 10, 8, 8);
  auto* sensitivityValue = new QLabel(QString::number(annulus.edgeSensitivity), sensitivityBox);
  sensitivityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  auto* sensitivitySlider = new QSlider(Qt::Horizontal, sensitivityBox);
  sensitivitySlider->setRange(1, 255);
  sensitivitySlider->setValue(annulus.edgeSensitivity);
  sensitivityLayout->addWidget(sensitivityValue, 0, 1);
  sensitivityLayout->addWidget(sensitivitySlider, 1, 0, 1, 2);
  QObject::connect(sensitivitySlider, &QSlider::valueChanged, window(), [sensitivityValue](int value) {
    sensitivityValue->setText(QString::number(value));
  });
  QObject::connect(sensitivitySlider, &QSlider::sliderReleased, window(), [this, camera, sensitivitySlider]() {
    const int value = sensitivitySlider->value();
    QString error;
    if (!recipes().saveSurfaceEdgeSensitivity(camera.id, value, &error))
    {
      log(error);
      return;
    }
    testSurfaceEdgePcaLocalization(camera);
  });
  layout->addWidget(sensitivityBox);

  auto* resolveBox = new QCheckBox(tr("labels.pcaResolveAmbiguity"), panel);
  resolveBox->setChecked(annulus.pcaResolveAmbiguity);
  QObject::connect(resolveBox, &QCheckBox::toggled, window(), [this, camera](bool checked) {
    QString error;
    if (!recipes().saveSurfacePcaResolveAmbiguity(camera.id, checked, &error))
    {
      log(error);
      return;
    }
    testSurfaceEdgePcaLocalization(camera);
  });
  layout->addWidget(resolveBox);

  addSurfaceLocalizationBackButton(camera, layout);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + strategy.label);
  showStoredSurfaceDefectAoe(camera);
  testSurfaceEdgePcaLocalization(camera);
}
