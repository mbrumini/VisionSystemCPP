#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/SurfaceLocalizationPanelWidget.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

#include <functional>

MainWindowSurfaceModule::MainWindowSurfaceModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

void MainWindowSurfaceModule::showSurfaceLocalizationStrategyPanel(const CameraConfig& camera, const QString& strategyId)
{
  context().deactivateImageDrawingTools();

  if (strategyId == "threshold" || strategyId == "edge")
  {
    QString error;
    if (!recipes().saveSurfaceLocalizationMethod(camera.id, strategyId, &error))
    {
      log(error);
      return;
    }

    showSurfaceLocalizationPanel(camera);
    return;
  }

  if (strategyId == "edgePca")
  {
    QString error;
    if (!recipes().saveSurfaceLocalizationMethod(camera.id, strategyId, &error))
    {
      log(error);
      return;
    }

    context().clearToolPanel();

    const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
    const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, translations());
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

    auto* roiButton = new QPushButton(tr("actions.surfaceSearchRoi"), buttons);
    auto* maskButton = new QPushButton(tr("actions.surfaceAddExclusion"), buttons);
    auto* clearButton = new QPushButton(tr("actions.surfaceClearExclusions"), buttons);
    auto* testButton = new QPushButton(tr("actions.testStrategy"), buttons);
    for (QPushButton* button : {roiButton, maskButton, clearButton, testButton})
    {
      button->setMinimumHeight(34);
    }
    QObject::connect(roiButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectRoiDrawing(camera); });
    QObject::connect(maskButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectExclusionDrawing(camera); });
    QObject::connect(clearButton, &QPushButton::clicked, window(), [this, camera]() { clearSurfaceDefectExclusions(camera); });
    QObject::connect(testButton, &QPushButton::clicked, window(), [this, camera]() { testSurfaceEdgePcaLocalization(camera); });
    buttonsLayout->addWidget(roiButton, 0, 0);
    buttonsLayout->addWidget(maskButton, 0, 1);
    buttonsLayout->addWidget(clearButton, 1, 0);
    buttonsLayout->addWidget(testButton, 1, 1);
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

    auto* backButton = new QPushButton(tr("groups.localizationStrategies"), panel);
    QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
      context().showLocalizationStrategyList(camera);
      log(tr("log.backToCameraTools") + ": " + camera.id);
    });
    layout->addWidget(backButton);
    layout->addStretch(1);

    toolsLayout()->addWidget(panel);
    log(tr("log.toolPanel") + ": " + strategy.label);
    testSurfaceEdgePcaLocalization(camera);
    return;
  }

  if (strategyId == "model")
  {
    QString error;
    if (!recipes().saveSurfaceLocalizationMethod(camera.id, strategyId, &error))
    {
      log(error);
      return;
    }

    context().clearToolPanel();

    const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, translations());
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

    auto* roiButton = new QPushButton(tr("actions.surfaceSearchRoi"), buttons);
    auto* previewButton = new QPushButton(tr("actions.previewModel"), buttons);
    auto* acquireButton = new QPushButton(tr("actions.acquireModel"), buttons);
    auto* shapeButton = new QPushButton(tr("actions.testShapeModel"), buttons);
    auto* templateButton = new QPushButton(tr("actions.testTemplateModel"), buttons);
    for (QPushButton* button : {roiButton, previewButton, acquireButton, shapeButton, templateButton})
    {
      button->setMinimumHeight(34);
    }
    QObject::connect(roiButton, &QPushButton::clicked, window(), [this, camera]() { activateSurfaceDefectRoiDrawing(camera); });
    QObject::connect(previewButton, &QPushButton::clicked, window(), [this, camera]() { previewSurfaceModel(camera); });
    QObject::connect(acquireButton, &QPushButton::clicked, window(), [this, camera]() { acquireSurfaceModel(camera); });
    QObject::connect(shapeButton, &QPushButton::clicked, window(), [this, camera]() { testSurfaceShapeModel(camera); });
    QObject::connect(templateButton, &QPushButton::clicked, window(), [this, camera]() { testSurfaceTemplateModel(camera); });
    buttonsLayout->addWidget(roiButton, 0, 0);
    buttonsLayout->addWidget(previewButton, 0, 1);
    buttonsLayout->addWidget(acquireButton, 1, 0);
    buttonsLayout->addWidget(shapeButton, 1, 1);
    buttonsLayout->addWidget(templateButton, 2, 0, 1, 2);
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
      }
      previewSurfaceModel(camera);
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

    auto* backButton = new QPushButton(tr("groups.localizationStrategies"), panel);
    QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
      context().showLocalizationStrategyList(camera);
      log(tr("log.backToCameraTools") + ": " + camera.id);
    });
    layout->addWidget(backButton);
    layout->addStretch(1);

    toolsLayout()->addWidget(panel);
    log(tr("log.toolPanel") + ": " + strategy.label);
    return;
  }

  context().clearToolPanel();

  const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, translations());
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

  auto* controlsBox = new QGroupBox(tr("groups.strategyControls"), panel);
  auto* controlsLayout = new QVBoxLayout(controlsBox);
  controlsLayout->setSpacing(8);
  controlsLayout->addWidget(new QLabel(tr("labels.strategyPlanned"), controlsBox));
  controlsLayout->addWidget(new QPushButton(tr("actions.configureStrategy"), controlsBox));
  controlsLayout->addWidget(new QPushButton(tr("actions.testStrategy"), controlsBox));
  layout->addWidget(controlsBox);

  auto* backButton = new QPushButton(tr("groups.localizationStrategies"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    context().showLocalizationStrategyList(camera);
    log(tr("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + strategy.label);
}


void MainWindowSurfaceModule::showSurfaceLocalizationPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);

  SurfaceLocalizationPanelWidget::Handlers handlers;
  handlers.drawOuterCircle = [this, camera]() { activateSurfaceOuterCircleDrawing(camera); };
  handlers.drawInnerCircle = [this, camera]() { activateSurfaceInnerCircleDrawing(camera); };
  handlers.drawEdgeCircle = [this, camera]() { activateSurfaceEdgeCircleCenterRadiusDrawing(camera); };
  handlers.drawThreePointCircle = [this, camera]() { activateSurfaceThreePointCircleDrawing(camera, CircleTarget::None); };
  handlers.addExclusion = [this, camera]() { activateSurfaceDefectExclusionDrawing(camera); };
  handlers.clearExclusions = [this, camera]() { clearSurfaceDefectExclusions(camera); };
  handlers.methodChanged = [this, camera](const QString& method) { saveSurfaceMethodAndPreview(camera, method); };
  handlers.thresholdChanged = [this, camera](int value) { saveSurfaceThresholdAndPreview(camera, value); };
  handlers.edgeSensitivityChanged = [this, camera](int value) { saveSurfaceEdgeAndPreview(camera, value); };
  handlers.edgeBandChanged = [this, camera](int innerWidth, int outerWidth) { saveSurfaceEdgeBandAndPreview(camera, innerWidth, outerWidth); };
  handlers.edgeFitMaxErrorChanged = [this, camera](int value) { saveSurfaceEdgeFitMaxErrorAndPreview(camera, value); };
  handlers.back = [this, camera]() {
    context().showCameraToolList(camera);
    log(tr("log.backToCameraTools") + ": " + camera.id);
  };

  auto* panel = new SurfaceLocalizationPanelWidget(
    ToolCatalog::label("surfaceLocalization", translations()) + " | " + QString("%1 %2").arg(tr("labels.camera")).arg(camera.slot),
    annulus,
    translations(),
    handlers,
    toolsContainer());

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + ToolCatalog::label("surfaceLocalization", translations()));

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
  }
}


void MainWindowSurfaceModule::activateLocalizationRoiDrawing(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    log(tr("log.localizationNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  largeImage()->setRoiDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Localization;
  log(tr("log.localizationRoiDrawing") + ": " + camera.id);
}


void MainWindowSurfaceModule::activateLocalizationExclusionDrawing(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    log(tr("log.localizationNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  largeImage()->setExclusionDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Localization;
  log(tr("log.localizationExclusionDrawing") + ": " + camera.id);
}


void MainWindowSurfaceModule::clearLocalizationExclusions(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  QString error;

  if (!recipes().clearLocalizationExclusionRects(camera.id, &error))
  {
    log(error);
    return;
  }

  largeImage()->clearExclusionRects();
  log(tr("log.localizationExclusionsCleared") + ": " + camera.id);
}


void MainWindowSurfaceModule::activateSurfaceDefectRoiDrawing(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  largeImage()->setRoiDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::SurfaceDefects;
  log(tr("log.surfaceRoiDrawing") + ": " + camera.id);
}

void MainWindowSurfaceModule::activateSurfaceDefectExclusionDrawing(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  largeImage()->setExclusionDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::SurfaceDefects;
  log(tr("log.surfaceExclusionDrawing") + ": " + camera.id);
}

void MainWindowSurfaceModule::clearSurfaceDefectExclusions(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  QString error;

  if (!recipes().clearSurfaceLocalizationGeometry(camera.id, &error))
  {
    log(error);
    return;
  }

  largeImage()->clearExclusionRects();
  largeImage()->clearCircles();
  context().lastSurfaceLocalizationResults->remove(camera.id);
  selectedPreview() = context().imaging->loadCameraPreview(camera);
  largeImage()->setImage(selectedPreview());
  log(tr("log.surfaceGeometryCleared") + ": " + camera.id);
}
void MainWindowSurfaceModule::activateSurfaceOuterCircleDrawing(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  m_circleTarget = CircleTarget::Outer;
  largeImage()->setOuterCircleDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::SurfaceDefects;
  log(tr("log.surfaceOuterCircleDrawing") + ": " + camera.id);
}
void MainWindowSurfaceModule::activateSurfaceInnerCircleDrawing(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);

  if (!annulus.hasOuterCircle || annulus.outerRadius <= 0)
  {
    log(tr("log.surfaceOuterCircleMissing") + ": " + camera.id);
    return;
  }

  m_circleTarget = CircleTarget::Inner;
  largeImage()->setInnerCircleDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::SurfaceDefects;
  log(tr("log.surfaceInnerCircleDrawing") + ": " + camera.id);
}

void MainWindowSurfaceModule::activateSurfaceEdgeCircleCenterRadiusDrawing(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  QString error;

  if (!recipes().saveSurfaceLocalizationMethod(camera.id, "edge", &error))
  {
    log(error);
    return;
  }

  m_circleTarget = CircleTarget::Edge;
  largeImage()->setOuterCircleDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::SurfaceDefects;
  log(tr("log.surfaceEdgeCircleDrawing") + ": " + camera.id);
}

void MainWindowSurfaceModule::activateSurfaceEdgeCircleDrawing(const CameraConfig& camera)
{
  activateSurfaceThreePointCircleDrawing(camera, CircleTarget::Edge);
}

void MainWindowSurfaceModule::activateSurfaceThreePointCircleDrawing(const CameraConfig& camera, CircleTarget target)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  if (target == CircleTarget::Edge)
  {
    QString error;

    if (!recipes().saveSurfaceLocalizationMethod(camera.id, "edge", &error))
    {
      log(error);
      return;
    }
  }

  m_circleTarget = target;
  largeImage()->setThreePointCircleDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::SurfaceDefects;
  log(tr("log.surfaceThreePointCircleDrawing") + ": " + camera.id);
}

void MainWindowSurfaceModule::saveSurfaceThresholdAndPreview(const CameraConfig& camera, int thresholdValue)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  QString error;

  if (!recipes().saveSurfaceAnnulusThreshold(camera.id, 0, thresholdValue, &error))
  {
    log(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);

  if (annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.innerRadius > 0 && annulus.outerRadius > annulus.innerRadius)
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindowSurfaceModule::saveSurfaceEdgeAndPreview(const CameraConfig& camera, int sensitivity)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  QString error;

  if (!recipes().saveSurfaceEdgeSensitivity(camera.id, sensitivity, &error))
  {
    log(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
  showStoredSurfaceLocalizationGeometry(camera, annulus);

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.innerRadius > 0 && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindowSurfaceModule::saveSurfaceEdgeBandAndPreview(const CameraConfig& camera, int innerWidth, int outerWidth)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  QString error;

  if (!recipes().saveSurfaceEdgeBand(camera.id, innerWidth, outerWidth, &error))
  {
    log(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);

  if (annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner)
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindowSurfaceModule::saveSurfaceEdgeFitMaxErrorAndPreview(const CameraConfig& camera, int maxError)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  QString error;
  if (!recipes().saveSurfaceEdgeFitMaxError(camera.id, maxError, &error))
  {
    log(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
  showStoredSurfaceLocalizationGeometry(camera, annulus);

  if (annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner)
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindowSurfaceModule::saveSurfaceMethodAndPreview(const CameraConfig& camera, const QString& method)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != selectedCameraId())
  {
    return;
  }

  QString error;

  if (!recipes().saveSurfaceLocalizationMethod(camera.id, method, &error))
  {
    log(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
  showStoredSurfaceLocalizationGeometry(camera, annulus);

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.innerRadius > 0 && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindowSurfaceModule::showStoredSurfaceLocalizationGeometry(const CameraConfig& camera, const SurfaceAnnulusLocalizationConfig& annulus)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  selectedPreview() = context().imaging->loadCameraPreview(camera);
  largeImage()->setImage(selectedPreview());
  largeImage()->setExclusionRects(recipes().loadSurfaceDefectExclusionRects(camera.id));

  if (annulus.method == "edge")
  {
    if (annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner)
    {
      largeImage()->setCircles({
        {annulus.edgeCenter, annulus.edgeRadius + annulus.edgeBandOuter},
        {annulus.edgeCenter, qMax(1, annulus.edgeRadius - annulus.edgeBandInner)}
      });
      return;
    }

    largeImage()->clearCircles();
    return;
  }

  QVector<ImageCircle> circles;
  if (annulus.hasOuterCircle)
  {
    circles.append({annulus.center, annulus.outerRadius});
  }
  if (annulus.hasInnerCircle)
  {
    circles.append({annulus.center, annulus.innerRadius});
  }
  largeImage()->setCircles(circles);
}

