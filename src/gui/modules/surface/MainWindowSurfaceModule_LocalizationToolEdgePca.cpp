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

void MainWindowSurfaceModule::showEdgePcaLocalizationPanel(const CameraConfig& camera)
{
  QString error;
  if (!recipes().saveSurfaceLocalizationMethod(camera.id, "edgePca", &error))
  {
    log(error);
    return;
  }

  context().clearToolPanel();

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
  const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy("edgePca", translations());
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
  QObject::connect(testButton, &QPushButton::clicked, window(), [this, camera]() { testSurfaceEdgePcaLocalization(camera); });
  buttonsLayout->addWidget(roiButton, 0, 0);
  buttonsLayout->addWidget(polygonButton, 0, 1);
  buttonsLayout->addWidget(maskButton, 1, 0);
  buttonsLayout->addWidget(clearButton, 1, 1);
  buttonsLayout->addWidget(clearRoiButton, 2, 0);
  buttonsLayout->addWidget(clearPolygonButton, 2, 1);
  buttonsLayout->addWidget(clearLocalizationButton, 3, 0, 1, 2);
  buttonsLayout->addWidget(testButton, 4, 0, 1, 2);
  layout->addWidget(buttons);

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
  QObject::connect(sensitivitySlider, &QSlider::valueChanged, window(), [this, camera, sensitivityValue](int value) {
    sensitivityValue->setText(QString::number(value));
    QString error;
    if (!recipes().saveSurfaceEdgeSensitivity(camera.id, value, &error))
    {
      log(error);
      return;
    }
    testSurfaceEdgePcaLocalization(camera);
  });
  layout->addWidget(sensitivityBox);

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
  testSurfaceEdgePcaLocalization(camera);
}
