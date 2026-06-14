#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/TouchIconButton.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
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

  context().clearToolPanel();

  const SurfaceDefectSettings settings = recipes().loadSurfaceDefectSettings(camera.id);
  const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy("massPca", translations());
  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  auto* title = new QLabel(strategy.label + " | " + QString("%1 %2").arg(tr("labels.camera")).arg(camera.slot), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(strategy.note, panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setSpacing(6);

  auto* roiButton = createTouchIconButton("surfaceSearchRoi", tr("actions.surfaceSearchRoi"), buttons);
  auto* polygonButton = createTouchIconButton("polygon", tr("actions.polygon"), buttons);
  auto* maskButton = createTouchIconButton("surfaceAddExclusion", tr("actions.surfaceAddExclusion"), buttons);
  auto* clearRoiButton = createTouchIconButton("clearRoi", tr("actions.clearRoi"), buttons);
  auto* clearPolygonButton = createTouchIconButton("clearPolygon", tr("actions.clearPolygon"), buttons);
  auto* clearButton = createTouchIconButton("surfaceClearExclusions", tr("actions.surfaceClearExclusions"), buttons);
  auto* clearLocalizationButton = createTouchIconButton("clear", tr("actions.clearLocalization"), buttons);
  auto* testButton = createTouchIconButton("testStrategy", tr("actions.testStrategy"), buttons);
  QObject::connect(roiButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectRoiDrawing(camera); });
  QObject::connect(polygonButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectPolygonDrawing(camera); });
  QObject::connect(maskButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectExclusionDrawing(camera); });
  QObject::connect(clearRoiButton, &QPushButton::clicked, window(), [this, camera]() { clearSurfaceDefectRoi(camera); });
  QObject::connect(clearPolygonButton, &QPushButton::clicked, window(), [this, camera]() { clearSurfaceDefectPolygon(camera); });
  QObject::connect(clearButton, &QPushButton::clicked, window(), [this, camera]() { clearSurfaceDefectExclusions(camera); });
  QObject::connect(clearLocalizationButton, &QPushButton::clicked, window(), [this, camera]() { clearSurfaceLocalization(camera); });
  QObject::connect(testButton, &QPushButton::clicked, window(), [this, camera]() { testSurfaceLocalization(camera); });
  buttonsLayout->addWidget(roiButton, 0, 0);
  buttonsLayout->addWidget(polygonButton, 0, 1);
  buttonsLayout->addWidget(maskButton, 1, 0);
  buttonsLayout->addWidget(clearButton, 1, 1);
  buttonsLayout->addWidget(clearRoiButton, 2, 0);
  buttonsLayout->addWidget(clearPolygonButton, 2, 1);
  buttonsLayout->addWidget(clearLocalizationButton, 3, 0, 1, 2);
  buttonsLayout->addWidget(testButton, 4, 0, 1, 2);
  layout->addWidget(buttons);

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

  auto* backButton = createTouchIconButton("back", tr("groups.localizationStrategies"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    context().showLocalizationStrategyList(camera);
    log(tr("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + strategy.label);
  showStoredSurfaceDefectAoe(camera);
  testSurfaceLocalization(camera);
}
