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

#include <functional>

void MainWindowSurfaceModule::showModelLocalizationPanel(const CameraConfig& camera)
{
  QString error;
  if (!recipes().saveSurfaceLocalizationMethod(camera.id, "model", &error))
  {
    log(error);
    return;
  }

  context().clearToolPanel();

  const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy("model", translations());
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
  auto* clearLocalizationButton = createTouchIconButton("clear", tr("actions.clearLocalization"), buttons);
  auto* previewButton = createTouchIconButton("previewModel", tr("actions.previewModel"), buttons);
  auto* acquireButton = createTouchIconButton("acquireModel", tr("actions.acquireModel"), buttons);
  auto* shapeButton = createTouchIconButton("testShapeModel", tr("actions.testShapeModel"), buttons);
  auto* templateButton = createTouchIconButton("testTemplateModel", tr("actions.testTemplateModel"), buttons);
  QObject::connect(roiButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectRoiDrawing(camera); });
  QObject::connect(polygonButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectPolygonDrawing(camera); });
  QObject::connect(clearLocalizationButton, &QPushButton::clicked, window(), [this, camera]() { clearSurfaceLocalization(camera); });
  QObject::connect(previewButton, &QPushButton::clicked, window(), [this, camera]() { previewSurfaceModel(camera); });
  QObject::connect(acquireButton, &QPushButton::clicked, window(), [this, camera]() { acquireSurfaceModel(camera); });
  QObject::connect(shapeButton, &QPushButton::clicked, window(), [this, camera]() { testSurfaceShapeModel(camera); });
  QObject::connect(templateButton, &QPushButton::clicked, window(), [this, camera]() { testSurfaceTemplateModel(camera); });
  buttonsLayout->addWidget(roiButton, 0, 0);
  buttonsLayout->addWidget(polygonButton, 0, 1);
  buttonsLayout->addWidget(previewButton, 0, 2);
  buttonsLayout->addWidget(acquireButton, 0, 3);
  buttonsLayout->addWidget(shapeButton, 1, 0);
  buttonsLayout->addWidget(templateButton, 1, 1);
  buttonsLayout->addWidget(clearLocalizationButton, 1, 2, 1, 2);
  layout->addWidget(buttons);

  SurfaceModelConfig model = recipes().loadSurfaceModel(camera.id);
  auto* modelBox = new QGroupBox(tr("groups.strategyControls"), panel);
  auto* modelLayout = new QGridLayout(modelBox);
  modelLayout->setContentsMargins(8, 10, 8, 8);
  modelLayout->setHorizontalSpacing(8);
  modelLayout->setVerticalSpacing(4);

  auto addSlider = [this, camera, modelLayout](const QString& labelText, int row, int minValue, int maxValue, int value, std::function<void(int)> handler, double displayScale = 1.0) {
    auto* label = new QLabel(labelText, modelLayout->parentWidget());
    auto* valueLabel = new QLabel(QString::number(value * displayScale, 'f', displayScale == 1.0 ? 0 : 2), modelLayout->parentWidget());
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* slider = new QSlider(Qt::Horizontal, modelLayout->parentWidget());
    slider->setRange(minValue, maxValue);
    slider->setValue(value);
    modelLayout->addWidget(label, row, 0);
    modelLayout->addWidget(valueLabel, row, 1);
    modelLayout->addWidget(slider, row + 1, 0, 1, 2);
    QObject::connect(slider, &QSlider::valueChanged, window(), [valueLabel, handler, displayScale](int sliderValue) {
      valueLabel->setText(QString::number(sliderValue * displayScale, 'f', displayScale == 1.0 ? 0 : 2));
      handler(sliderValue);
    });
  };

  addSlider(tr("labels.edgeSensitivity"), 0, 1, 255, model.edgeSensitivity, [this, camera](int value) {
    QString error;
    if (!recipes().saveSurfaceModelEdgeSensitivity(camera.id, value, &error))
    {
      log(error);
      return;
    }
    acquireSurfaceModel(camera);
  });
  addSlider(tr("labels.modelMaxShapeDistance"), 2, 1, 500, static_cast<int>(model.maxShapeDistance * 100.0), [this, camera](int value) {
    QString error;
    if (!recipes().saveSurfaceModelMaxShapeDistance(camera.id, value / 100.0, &error))
    {
      log(error);
    }
  }, 0.01);
  addSlider(tr("labels.modelMinTemplateScore"), 4, 0, 100, static_cast<int>(model.minTemplateScore * 100.0), [this, camera](int value) {
    QString error;
    if (!recipes().saveSurfaceModelMinTemplateScore(camera.id, value / 100.0, &error))
    {
      log(error);
    }
  }, 0.01);
  addSlider(tr("labels.modelAngleStart"), 6, -180, 180, static_cast<int>(model.angleStartDegrees), [this, camera](int value) {
    const SurfaceModelConfig current = recipes().loadSurfaceModel(camera.id);
    QString error;
    if (!recipes().saveSurfaceModelAngleRange(camera.id, value, current.angleEndDegrees, current.angleStepDegrees, &error))
    {
      log(error);
    }
  });
  addSlider(tr("labels.modelAngleEnd"), 8, -180, 180, static_cast<int>(model.angleEndDegrees), [this, camera](int value) {
    const SurfaceModelConfig current = recipes().loadSurfaceModel(camera.id);
    QString error;
    if (!recipes().saveSurfaceModelAngleRange(camera.id, current.angleStartDegrees, value, current.angleStepDegrees, &error))
    {
      log(error);
    }
  });
  addSlider(tr("labels.modelAngleStep"), 10, 1, 45, static_cast<int>(model.angleStepDegrees), [this, camera](int value) {
    const SurfaceModelConfig current = recipes().loadSurfaceModel(camera.id);
    QString error;
    if (!recipes().saveSurfaceModelAngleRange(camera.id, current.angleStartDegrees, current.angleEndDegrees, value, &error))
    {
      log(error);
    }
  });
  layout->addWidget(modelBox);

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
}
