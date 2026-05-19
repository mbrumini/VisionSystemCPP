#include "MainWindow.h"

#include "gui/SurfaceLocalizationPanelWidget.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "processing/SurfaceModelTrainer.h"
#include "util/AsyncExecutor.h"

#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

#include <opencv2/imgcodecs.hpp>

#include <memory>
#include <vector>

using AsyncExecutor::runAsyncTask;

namespace
{
std::vector<cv::Rect> toCvRects(const QVector<QRect>& rects)
{
  std::vector<cv::Rect> result;
  result.reserve(static_cast<size_t>(rects.size()));

  for (const QRect& rect : rects)
  {
    result.emplace_back(rect.x(), rect.y(), rect.width(), rect.height());
  }

  return result;
}

SurfaceTwoCirclesStrategyConfig toProcessorStrategy(const SurfaceLocalizationStrategyConfig& recipeConfig)
{
  SurfaceTwoCirclesStrategyConfig result;
  result.originFeatureId = recipeConfig.origin.toStdString();
  result.xAxisFromFeatureId = recipeConfig.xAxisFrom.toStdString();
  result.xAxisToFeatureId = recipeConfig.xAxisTo.toStdString();

  for (const SurfaceStrategyFeatureConfig& recipeFeature : recipeConfig.features)
  {
    SurfaceCircleFeatureConfig feature;
    feature.id = recipeFeature.id.toStdString();
    feature.polarity = recipeFeature.polarity.toStdString();
    feature.searchRoi = cv::Rect(
      recipeFeature.searchRoi.x(),
      recipeFeature.searchRoi.y(),
      recipeFeature.searchRoi.width(),
      recipeFeature.searchRoi.height());
    feature.threshold.minValue = recipeFeature.thresholdMin;
    feature.threshold.maxValue = recipeFeature.thresholdMax;
    feature.expectedRadiusMin = recipeFeature.expectedRadiusMin;
    feature.expectedRadiusMax = recipeFeature.expectedRadiusMax;
    result.features.push_back(feature);
  }

  return result;
}
}
void MainWindow::showSurfaceLocalizationStrategyPanel(const CameraConfig& camera, const QString& strategyId)
{
  deactivateImageDrawingTools();

  if (strategyId == "threshold" || strategyId == "edge")
  {
    QString error;
    if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, strategyId, &error))
    {
      appendLog(error);
      return;
    }

    showSurfaceLocalizationPanel(camera);
    return;
  }

  if (strategyId == "edgePca")
  {
    QString error;
    if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, strategyId, &error))
    {
      appendLog(error);
      return;
    }

    clearToolPanel();

    const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
    const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, m_translations);
    auto* panel = new QWidget(m_toolsContainer);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* title = new QLabel(strategy.label + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot), panel);
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

    auto* roiButton = new QPushButton(trText("actions.surfaceSearchRoi"), buttons);
    auto* maskButton = new QPushButton(trText("actions.surfaceAddExclusion"), buttons);
    auto* clearButton = new QPushButton(trText("actions.surfaceClearExclusions"), buttons);
    auto* testButton = new QPushButton(trText("actions.testStrategy"), buttons);
    for (QPushButton* button : {roiButton, maskButton, clearButton, testButton})
    {
      button->setMinimumHeight(34);
    }
    connect(roiButton, &QPushButton::clicked, this, [this, camera]() { activateSurfaceDefectRoiDrawing(camera); });
    connect(maskButton, &QPushButton::clicked, this, [this, camera]() { activateSurfaceDefectExclusionDrawing(camera); });
    connect(clearButton, &QPushButton::clicked, this, [this, camera]() { clearSurfaceDefectExclusions(camera); });
    connect(testButton, &QPushButton::clicked, this, [this, camera]() { testSurfaceEdgePcaLocalization(camera); });
    buttonsLayout->addWidget(roiButton, 0, 0);
    buttonsLayout->addWidget(maskButton, 0, 1);
    buttonsLayout->addWidget(clearButton, 1, 0);
    buttonsLayout->addWidget(testButton, 1, 1);
    layout->addWidget(buttons);

    auto* sensitivityBox = new QGroupBox(trText("labels.edgeSensitivity"), panel);
    auto* sensitivityLayout = new QGridLayout(sensitivityBox);
    sensitivityLayout->setContentsMargins(8, 10, 8, 8);
    auto* sensitivityValue = new QLabel(QString::number(annulus.edgeSensitivity), sensitivityBox);
    sensitivityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* sensitivitySlider = new QSlider(Qt::Horizontal, sensitivityBox);
    sensitivitySlider->setRange(1, 255);
    sensitivitySlider->setValue(annulus.edgeSensitivity);
    sensitivityLayout->addWidget(sensitivityValue, 0, 1);
    sensitivityLayout->addWidget(sensitivitySlider, 1, 0, 1, 2);
    connect(sensitivitySlider, &QSlider::valueChanged, this, [this, camera, sensitivityValue](int value) {
      sensitivityValue->setText(QString::number(value));
      QString error;
      if (!m_recipeManager.saveSurfaceEdgeSensitivity(camera.id, value, &error))
      {
        appendLog(error);
        return;
      }
      testSurfaceEdgePcaLocalization(camera);
    });
    layout->addWidget(sensitivityBox);

    auto* backButton = new QPushButton(trText("groups.localizationStrategies"), panel);
    connect(backButton, &QPushButton::clicked, this, [this, camera]() {
      showLocalizationStrategyList(camera);
      appendLog(trText("log.backToCameraTools") + ": " + camera.id);
    });
    layout->addWidget(backButton);
    layout->addStretch(1);

    m_toolsLayout->addWidget(panel);
    appendLog(trText("log.toolPanel") + ": " + strategy.label);
    testSurfaceEdgePcaLocalization(camera);
    return;
  }

  if (strategyId == "model")
  {
    QString error;
    if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, strategyId, &error))
    {
      appendLog(error);
      return;
    }

    clearToolPanel();

    const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, m_translations);
    auto* panel = new QWidget(m_toolsContainer);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* title = new QLabel(strategy.label + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot), panel);
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

    auto* roiButton = new QPushButton(trText("actions.surfaceSearchRoi"), buttons);
    auto* previewButton = new QPushButton(trText("actions.previewModel"), buttons);
    auto* acquireButton = new QPushButton(trText("actions.acquireModel"), buttons);
    auto* shapeButton = new QPushButton(trText("actions.testShapeModel"), buttons);
    auto* templateButton = new QPushButton(trText("actions.testTemplateModel"), buttons);
    for (QPushButton* button : {roiButton, previewButton, acquireButton, shapeButton, templateButton})
    {
      button->setMinimumHeight(34);
    }
    connect(roiButton, &QPushButton::clicked, this, [this, camera]() { activateSurfaceDefectRoiDrawing(camera); });
    connect(previewButton, &QPushButton::clicked, this, [this, camera]() { previewSurfaceModel(camera); });
    connect(acquireButton, &QPushButton::clicked, this, [this, camera]() { acquireSurfaceModel(camera); });
    connect(shapeButton, &QPushButton::clicked, this, [this, camera]() { testSurfaceShapeModel(camera); });
    connect(templateButton, &QPushButton::clicked, this, [this, camera]() { testSurfaceTemplateModel(camera); });
    buttonsLayout->addWidget(roiButton, 0, 0);
    buttonsLayout->addWidget(previewButton, 0, 1);
    buttonsLayout->addWidget(acquireButton, 1, 0);
    buttonsLayout->addWidget(shapeButton, 1, 1);
    buttonsLayout->addWidget(templateButton, 2, 0, 1, 2);
    layout->addWidget(buttons);

    SurfaceModelConfig model = m_recipeManager.loadSurfaceModel(camera.id);
    auto* modelBox = new QGroupBox(trText("groups.strategyControls"), panel);
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
      connect(slider, &QSlider::valueChanged, this, [valueLabel, handler, displayScale](int sliderValue) {
        valueLabel->setText(QString::number(sliderValue * displayScale, 'f', displayScale == 1.0 ? 0 : 2));
        handler(sliderValue);
      });
    };

    addSlider(trText("labels.edgeSensitivity"), 0, 1, 255, model.edgeSensitivity, [this, camera](int value) {
      QString error;
      if (!m_recipeManager.saveSurfaceModelEdgeSensitivity(camera.id, value, &error))
      {
        appendLog(error);
      }
      previewSurfaceModel(camera);
    });
    addSlider(trText("labels.modelMaxShapeDistance"), 2, 1, 500, static_cast<int>(model.maxShapeDistance * 100.0), [this, camera](int value) {
      QString error;
      if (!m_recipeManager.saveSurfaceModelMaxShapeDistance(camera.id, value / 100.0, &error))
      {
        appendLog(error);
      }
    }, 0.01);
    addSlider(trText("labels.modelMinTemplateScore"), 4, 0, 100, static_cast<int>(model.minTemplateScore * 100.0), [this, camera](int value) {
      QString error;
      if (!m_recipeManager.saveSurfaceModelMinTemplateScore(camera.id, value / 100.0, &error))
      {
        appendLog(error);
      }
    }, 0.01);
    addSlider(trText("labels.modelAngleStart"), 6, -180, 180, static_cast<int>(model.angleStartDegrees), [this, camera](int value) {
      const SurfaceModelConfig current = m_recipeManager.loadSurfaceModel(camera.id);
      QString error;
      if (!m_recipeManager.saveSurfaceModelAngleRange(camera.id, value, current.angleEndDegrees, current.angleStepDegrees, &error))
      {
        appendLog(error);
      }
    });
    addSlider(trText("labels.modelAngleEnd"), 8, -180, 180, static_cast<int>(model.angleEndDegrees), [this, camera](int value) {
      const SurfaceModelConfig current = m_recipeManager.loadSurfaceModel(camera.id);
      QString error;
      if (!m_recipeManager.saveSurfaceModelAngleRange(camera.id, current.angleStartDegrees, value, current.angleStepDegrees, &error))
      {
        appendLog(error);
      }
    });
    addSlider(trText("labels.modelAngleStep"), 10, 1, 45, static_cast<int>(model.angleStepDegrees), [this, camera](int value) {
      const SurfaceModelConfig current = m_recipeManager.loadSurfaceModel(camera.id);
      QString error;
      if (!m_recipeManager.saveSurfaceModelAngleRange(camera.id, current.angleStartDegrees, current.angleEndDegrees, value, &error))
      {
        appendLog(error);
      }
    });
    layout->addWidget(modelBox);

    auto* backButton = new QPushButton(trText("groups.localizationStrategies"), panel);
    connect(backButton, &QPushButton::clicked, this, [this, camera]() {
      showLocalizationStrategyList(camera);
      appendLog(trText("log.backToCameraTools") + ": " + camera.id);
    });
    layout->addWidget(backButton);
    layout->addStretch(1);

    m_toolsLayout->addWidget(panel);
    appendLog(trText("log.toolPanel") + ": " + strategy.label);
    return;
  }

  clearToolPanel();

  const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, m_translations);
  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  auto* title = new QLabel(strategy.label + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(strategy.note, panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* controlsBox = new QGroupBox(trText("groups.strategyControls"), panel);
  auto* controlsLayout = new QVBoxLayout(controlsBox);
  controlsLayout->setSpacing(8);
  controlsLayout->addWidget(new QLabel(trText("labels.strategyPlanned"), controlsBox));
  controlsLayout->addWidget(new QPushButton(trText("actions.configureStrategy"), controlsBox));
  controlsLayout->addWidget(new QPushButton(trText("actions.testStrategy"), controlsBox));
  layout->addWidget(controlsBox);

  auto* backButton = new QPushButton(trText("groups.localizationStrategies"), panel);
  connect(backButton, &QPushButton::clicked, this, [this, camera]() {
    showLocalizationStrategyList(camera);
    appendLog(trText("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + strategy.label);
}


void MainWindow::showSurfaceLocalizationPanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);

  SurfaceLocalizationPanelWidget::Handlers handlers;
  handlers.drawOuterCircle = [this, camera]() { activateSurfaceOuterCircleDrawing(camera); };
  handlers.drawInnerCircle = [this, camera]() { activateSurfaceInnerCircleDrawing(camera); };
  handlers.drawEdgeCircle = [this, camera]() { activateSurfaceEdgeCircleCenterRadiusDrawing(camera); };
  handlers.drawThreePointCircle = [this, camera]() { activateSurfaceThreePointCircleDrawing(camera, SurfaceCircleTarget::None); };
  handlers.addExclusion = [this, camera]() { activateSurfaceDefectExclusionDrawing(camera); };
  handlers.clearExclusions = [this, camera]() { clearSurfaceDefectExclusions(camera); };
  handlers.methodChanged = [this, camera](const QString& method) { saveSurfaceMethodAndPreview(camera, method); };
  handlers.thresholdChanged = [this, camera](int value) { saveSurfaceThresholdAndPreview(camera, value); };
  handlers.edgeSensitivityChanged = [this, camera](int value) { saveSurfaceEdgeAndPreview(camera, value); };
  handlers.edgeBandChanged = [this, camera](int innerWidth, int outerWidth) { saveSurfaceEdgeBandAndPreview(camera, innerWidth, outerWidth); };
  handlers.edgeFitMaxErrorChanged = [this, camera](int value) { saveSurfaceEdgeFitMaxErrorAndPreview(camera, value); };
  handlers.back = [this, camera]() {
    showCameraToolList(camera);
    appendLog(trText("log.backToCameraTools") + ": " + camera.id);
  };

  auto* panel = new SurfaceLocalizationPanelWidget(
    ToolCatalog::label("surfaceLocalization", m_translations) + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot),
    annulus,
    m_translations,
    handlers,
    m_toolsContainer);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + ToolCatalog::label("surfaceLocalization", m_translations));

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
  }
}


void MainWindow::activateLocalizationRoiDrawing(const CameraConfig& camera)
{
  if (!isBwDimensionalCamera(camera))
  {
    appendLog(trText("log.localizationNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_largeImage->setRoiDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::Localization;
  appendLog(trText("log.localizationRoiDrawing") + ": " + camera.id);
}


void MainWindow::activateLocalizationExclusionDrawing(const CameraConfig& camera)
{
  if (!isBwDimensionalCamera(camera))
  {
    appendLog(trText("log.localizationNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_largeImage->setExclusionDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::Localization;
  appendLog(trText("log.localizationExclusionDrawing") + ": " + camera.id);
}


void MainWindow::clearLocalizationExclusions(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.clearLocalizationExclusionRects(camera.id, &error))
  {
    appendLog(error);
    return;
  }

  m_largeImage->clearExclusionRects();
  appendLog(trText("log.localizationExclusionsCleared") + ": " + camera.id);
}


void MainWindow::activateSurfaceDefectRoiDrawing(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_largeImage->setRoiDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceRoiDrawing") + ": " + camera.id);
}

void MainWindow::activateSurfaceDefectExclusionDrawing(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_largeImage->setExclusionDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceExclusionDrawing") + ": " + camera.id);
}

void MainWindow::clearSurfaceDefectExclusions(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.clearSurfaceLocalizationGeometry(camera.id, &error))
  {
    appendLog(error);
    return;
  }

  m_largeImage->clearExclusionRects();
  m_largeImage->clearCircles();
  m_lastSurfaceLocalizationResults.remove(camera.id);
  m_selectedPreview = loadCameraPreview(camera);
  m_largeImage->setImage(m_selectedPreview);
  appendLog(trText("log.surfaceGeometryCleared") + ": " + camera.id);
}
void MainWindow::activateSurfaceOuterCircleDrawing(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_surfaceCircleTarget = SurfaceCircleTarget::Outer;
  m_largeImage->setOuterCircleDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceOuterCircleDrawing") + ": " + camera.id);
}
void MainWindow::activateSurfaceInnerCircleDrawing(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);

  if (!annulus.hasOuterCircle || annulus.outerRadius <= 0)
  {
    appendLog(trText("log.surfaceOuterCircleMissing") + ": " + camera.id);
    return;
  }

  m_surfaceCircleTarget = SurfaceCircleTarget::Inner;
  m_largeImage->setInnerCircleDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceInnerCircleDrawing") + ": " + camera.id);
}

void MainWindow::activateSurfaceEdgeCircleCenterRadiusDrawing(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, "edge", &error))
  {
    appendLog(error);
    return;
  }

  m_surfaceCircleTarget = SurfaceCircleTarget::Edge;
  m_largeImage->setOuterCircleDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceEdgeCircleDrawing") + ": " + camera.id);
}

void MainWindow::activateSurfaceEdgeCircleDrawing(const CameraConfig& camera)
{
  activateSurfaceThreePointCircleDrawing(camera, SurfaceCircleTarget::Edge);
}

void MainWindow::activateSurfaceThreePointCircleDrawing(const CameraConfig& camera, SurfaceCircleTarget target)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  if (target == SurfaceCircleTarget::Edge)
  {
    QString error;

    if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, "edge", &error))
    {
      appendLog(error);
      return;
    }
  }

  m_surfaceCircleTarget = target;
  m_largeImage->setThreePointCircleDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceThreePointCircleDrawing") + ": " + camera.id);
}

void MainWindow::saveSurfaceThresholdAndPreview(const CameraConfig& camera, int thresholdValue)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.saveSurfaceAnnulusThreshold(camera.id, 0, thresholdValue, &error))
  {
    appendLog(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);

  if (annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.innerRadius > 0 && annulus.outerRadius > annulus.innerRadius)
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindow::saveSurfaceEdgeAndPreview(const CameraConfig& camera, int sensitivity)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.saveSurfaceEdgeSensitivity(camera.id, sensitivity, &error))
  {
    appendLog(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  showStoredSurfaceLocalizationGeometry(camera, annulus);

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.innerRadius > 0 && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindow::saveSurfaceEdgeBandAndPreview(const CameraConfig& camera, int innerWidth, int outerWidth)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.saveSurfaceEdgeBand(camera.id, innerWidth, outerWidth, &error))
  {
    appendLog(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);

  if (annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner)
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindow::saveSurfaceEdgeFitMaxErrorAndPreview(const CameraConfig& camera, int maxError)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;
  if (!m_recipeManager.saveSurfaceEdgeFitMaxError(camera.id, maxError, &error))
  {
    appendLog(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  showStoredSurfaceLocalizationGeometry(camera, annulus);

  if (annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner)
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindow::saveSurfaceMethodAndPreview(const CameraConfig& camera, const QString& method)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, method, &error))
  {
    appendLog(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  showStoredSurfaceLocalizationGeometry(camera, annulus);

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.innerRadius > 0 && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindow::showStoredSurfaceLocalizationGeometry(const CameraConfig& camera, const SurfaceAnnulusLocalizationConfig& annulus)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_selectedPreview = loadCameraPreview(camera);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->setExclusionRects(m_recipeManager.loadSurfaceDefectExclusionRects(camera.id));

  if (annulus.method == "edge")
  {
    if (annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner)
    {
      m_largeImage->setCircles({
        {annulus.edgeCenter, annulus.edgeRadius + annulus.edgeBandOuter},
        {annulus.edgeCenter, qMax(1, annulus.edgeRadius - annulus.edgeBandInner)}
      });
      return;
    }

    m_largeImage->clearCircles();
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
  m_largeImage->setCircles(circles);
}

void MainWindow::testSurfaceAnnulusLocalization(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);

  if (annulus.method == "edge")
  {
    if (!annulus.hasEdgeCircle || annulus.edgeRadius <= annulus.edgeBandInner)
    {
      appendLog(trText("log.surfaceEdgeCircleMissing") + ": " + camera.id);
      return;
    }
  }
  else if (!annulus.hasOuterCircle || !annulus.hasInnerCircle || annulus.innerRadius <= 0 || annulus.outerRadius <= annulus.innerRadius)
  {
    appendLog(trText("log.surfaceAnnulusMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  SurfaceAnnulusThresholdConfig processorConfig;
  if (annulus.method == "edge")
  {
    processorConfig.center = cv::Point(annulus.edgeCenter.x(), annulus.edgeCenter.y());
    processorConfig.outerRadius = annulus.edgeRadius + annulus.edgeBandOuter;
    processorConfig.innerRadius = qMax(1, annulus.edgeRadius - annulus.edgeBandInner);
  }
  else
  {
    processorConfig.center = cv::Point(annulus.center.x(), annulus.center.y());
    processorConfig.outerRadius = annulus.outerRadius;
    processorConfig.innerRadius = annulus.innerRadius;
  }
  processorConfig.threshold.minValue = annulus.thresholdMin;
  processorConfig.threshold.maxValue = annulus.thresholdMax;
  processorConfig.edgeSensitivity = annulus.edgeSensitivity;
  processorConfig.edgeFitMaxError = annulus.edgeFitMaxError;

  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, processorConfig, exclusionRects, annulus]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    if (annulus.method == "edge")
    {
      return processor.locateAnnulusByEdge(input, processorConfig, toCvRects(exclusionRects));
    }
    return processor.locateAnnulusByGrayscaleThreshold(input, processorConfig, toCvRects(exclusionRects));
  };
  const QString __pendingCameraId_testSurfaceAnnulus = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceAnnulus);
  runAsyncTask(decltype(job)(job), this, [this, camera, annulus, exclusionRects, __pendingCameraId_testSurfaceAnnulus](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceAnnulus](void*) { decPendingJobs(__pendingCameraId_testSurfaceAnnulus); });
    const bool suppressViewUpdate =
      camera.id == m_selectedCameraId &&
      (m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry || m_setupCameraId == camera.id);
    if (!result.processed || result.diagnosticImage.empty())
    {
      appendLog(trText("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    if (!suppressViewUpdate)
    {
      m_selectedPreview = matToPixmap(result.diagnosticImage);
      m_largeImage->setImage(m_selectedPreview);
      m_largeImage->setExclusionRects(exclusionRects);
      m_largeImage->clearCircles();
    }

    if (result.blobs.empty())
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      if (!suppressViewUpdate)
      {
        m_largeImage->clearGeometryOverlay();
      }
      appendLog(QString("%1: %2 blobs=0 min=%3 max=%4")
                  .arg(trText("log.surfaceNotFound"))
                  .arg(camera.id)
                  .arg(annulus.thresholdMin)
                  .arg(annulus.thresholdMax));
      return;
    }

    const SurfaceBlob& mainBlob = result.blobs.front();
    if (result.localization.found)
    {
      m_lastSurfaceLocalizationResults.insert(camera.id, result.localization);
      m_cameraRuntime[camera.id].setCurrentPose(partPoseFromSurfaceReference(camera, result.localization));
      GeometryOverlay overlay;
      appendCurrentPartPoseOverlay(camera, overlay);
      m_largeImage->setGeometryOverlay(overlay);
    }
    else
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      if (!suppressViewUpdate)
      {
        m_largeImage->clearGeometryOverlay();
      }
    }

    appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5 score=%6 area=%7 blobs=%8 min=%9 max=%10")
                .arg(trText("log.surfaceFound"))
                .arg(camera.id)
                .arg(result.localization.found ? result.localization.center.x : mainBlob.center.x, 0, 'f', 1)
                .arg(result.localization.found ? result.localization.center.y : mainBlob.center.y, 0, 'f', 1)
                .arg(result.localization.radius, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2)
                .arg(mainBlob.area, 0, 'f', 1)
                .arg(result.blobs.size())
                .arg(annulus.thresholdMin)
                .arg(annulus.thresholdMax));
  });
}

void MainWindow::testSurfaceLocalization(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QRect roi;

  if (!m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    appendLog(trText("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const SurfaceDefectSettings recipeSettings = m_recipeManager.loadSurfaceDefectSettings(camera.id);
  SurfaceThresholdSettings thresholdSettings;
  thresholdSettings.minValue = recipeSettings.thresholdMin;
  thresholdSettings.maxValue = recipeSettings.thresholdMax;

  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, roi, exclusionRects, thresholdSettings]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.detectByGrayscaleThreshold(
      input,
      cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
      toCvRects(exclusionRects),
      thresholdSettings);
  };

  const QString __pendingCameraId_testSurfaceLocalization = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceLocalization);
  runAsyncTask(decltype(job)(job), this, [this, camera, roi, exclusionRects, thresholdSettings, __pendingCameraId_testSurfaceLocalization](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceLocalization](void*) { decPendingJobs(__pendingCameraId_testSurfaceLocalization); });
    const bool suppressViewUpdate =
      camera.id == m_selectedCameraId &&
      (m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry || m_setupCameraId == camera.id);
    if (!result.processed || result.diagnosticImage.empty())
    {
      appendLog(trText("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    if (!suppressViewUpdate)
    {
      m_selectedPreview = matToPixmap(result.diagnosticImage);
      m_largeImage->setImage(m_selectedPreview);
      m_largeImage->setRoi(roi);
      m_largeImage->setExclusionRects(exclusionRects);
    }

    if (result.blobs.empty())
    {
      appendLog(QString("%1: %2 blobs=0 min=%3 max=%4")
                  .arg(trText("log.surfaceNotFound"))
                  .arg(camera.id)
                  .arg(thresholdSettings.minValue)
                  .arg(thresholdSettings.maxValue));
      return;
    }

    const SurfaceBlob& mainBlob = result.blobs.front();
    appendLog(QString("%1: %2 cx=%3 cy=%4 area=%5 blobs=%6 min=%7 max=%8")
                .arg(trText("log.surfaceFound"))
                .arg(camera.id)
                .arg(mainBlob.center.x, 0, 'f', 1)
                .arg(mainBlob.center.y, 0, 'f', 1)
                .arg(mainBlob.area, 0, 'f', 1)
                .arg(result.blobs.size())
                .arg(thresholdSettings.minValue)
                .arg(thresholdSettings.maxValue));
  });
}

void MainWindow::testSurfaceLocalizationStrategy(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const SurfaceLocalizationStrategyConfig recipeStrategy = m_recipeManager.loadSurfaceLocalizationStrategy(camera.id);

  if (recipeStrategy.name != "two_circles_axis" || recipeStrategy.features.size() < 2)
  {
    appendLog(trText("log.surfaceStrategyMissing") + ": " + camera.id);
    return;
  }

  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);
  auto processorStrategy = toProcessorStrategy(recipeStrategy);

  auto job = [input, processorStrategy, exclusionRects]() -> SurfaceStrategyResult {
    SurfaceDefectProcessor processor;
    return processor.locateTwoCirclesAxis(input, processorStrategy, toCvRects(exclusionRects));
  };

  const QString __pendingCameraId_testSurfaceLocalizationStrategy = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceLocalizationStrategy);
  runAsyncTask(decltype(job)(job), this, [this, camera, exclusionRects, __pendingCameraId_testSurfaceLocalizationStrategy](const SurfaceStrategyResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceLocalizationStrategy](void*) { decPendingJobs(__pendingCameraId_testSurfaceLocalizationStrategy); });
    const bool suppressViewUpdate =
      camera.id == m_selectedCameraId &&
      (m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry || m_setupCameraId == camera.id);
    if (result.diagnosticImage.empty())
    {
      appendLog(trText("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    if (!suppressViewUpdate)
    {
      m_selectedPreview = matToPixmap(result.diagnosticImage);
      m_largeImage->setImage(m_selectedPreview);
      m_largeImage->setExclusionRects(exclusionRects);
    }

    if (!result.found)
    {
      appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    appendLog(QString("%1: %2 originX=%3 originY=%4 features=%5")
                .arg(trText("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.origin.x, 0, 'f', 1)
                .arg(result.origin.y, 0, 'f', 1)
                .arg(result.features.size()));
  });
}

void MainWindow::testSurfaceEdgePcaLocalization(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QRect roi;
  if (!m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    appendLog(trText("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, roi, exclusionRects, annulus]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.locateByEdgePca(
      input,
      cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
      toCvRects(exclusionRects),
      annulus.edgeSensitivity);
  };

  const QString __pendingCameraId_testSurfaceEdgePca = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceEdgePca);
  runAsyncTask(decltype(job)(job), this, [this, camera, roi, exclusionRects, __pendingCameraId_testSurfaceEdgePca](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceEdgePca](void*) { decPendingJobs(__pendingCameraId_testSurfaceEdgePca); });
    const bool suppressViewUpdate =
      camera.id == m_selectedCameraId &&
      (m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry || m_setupCameraId == camera.id);
    if (!result.processed || result.diagnosticImage.empty())
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      if (!suppressViewUpdate)
      {
        m_largeImage->clearGeometryOverlay();
      }
      appendLog(trText("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    if (!suppressViewUpdate)
    {
      m_selectedPreview = matToPixmap(result.diagnosticImage);
      m_largeImage->setImage(m_selectedPreview);
      m_largeImage->setRoi(roi);
      m_largeImage->setExclusionRects(exclusionRects);
      m_largeImage->clearCircles();
    }

    if (!result.localization.found)
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      if (!suppressViewUpdate)
      {
        m_largeImage->clearGeometryOverlay();
      }
      appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    m_lastSurfaceLocalizationResults.insert(camera.id, result.localization);
    m_cameraRuntime[camera.id].setCurrentPose(partPoseFromSurfaceReference(camera, result.localization));
    GeometryOverlay overlay;
    appendCurrentPartPoseOverlay(camera, overlay);
    m_largeImage->setGeometryOverlay(overlay);
    appendLog(QString("%1: %2 cx=%3 cy=%4 angle=%5 score=%6")
                .arg(trText("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.localization.center.x, 0, 'f', 1)
                .arg(result.localization.center.y, 0, 'f', 1)
                .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2));
  });
}

void MainWindow::acquireSurfaceModel(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera) || camera.id != m_selectedCameraId)
  {
    return;
  }

  QRect roi;
  if (!m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    appendLog(trText("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const SurfaceModelConfig current = m_recipeManager.loadSurfaceModel(camera.id);
  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);
  SurfaceModelTrainer trainer;
  const SurfaceModelTrainingResult training = trainer.trainFromRoi(
    input,
    cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
    toCvRects(exclusionRects),
    current.edgeSensitivity);

  if (!training.trained || training.templateImage.empty() || training.contour.empty())
  {
    appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
    return;
  }

  const QString templatePath = m_recipeManager.surfaceModelTemplateImagePath(camera.id);
  QDir().mkpath(QFileInfo(templatePath).dir().absolutePath());
  if (!cv::imwrite(templatePath.toStdString(), training.templateImage))
  {
    appendLog("Impossibile salvare template modello: " + templatePath);
    return;
  }

  QVector<QPoint> contour;
  contour.reserve(static_cast<int>(training.contour.size()));
  for (const cv::Point& point : training.contour)
  {
    contour.append(QPoint(point.x, point.y));
  }

  QString error;
  if (!m_recipeManager.saveSurfaceModel(camera.id, roi, contour, templatePath, &error))
  {
    appendLog(error);
    return;
  }

  m_selectedPreview = matToPixmap(training.diagnosticImage);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->setRoi(roi);
  m_largeImage->setExclusionRects(exclusionRects);
  appendLog(QString("%1: %2 points=%3").arg(trText("actions.acquireModel")).arg(camera.id).arg(contour.size()));
}

void MainWindow::previewSurfaceModel(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera) || camera.id != m_selectedCameraId)
  {
    return;
  }

  QRect roi;
  if (!m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    appendLog(trText("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const SurfaceModelConfig current = m_recipeManager.loadSurfaceModel(camera.id);
  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);
  SurfaceModelTrainer trainer;
  const SurfaceModelTrainingResult training = trainer.trainFromRoi(
    input,
    cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
    toCvRects(exclusionRects),
    current.edgeSensitivity);

  if (training.diagnosticImage.empty())
  {
    appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
    return;
  }

  m_selectedPreview = matToPixmap(training.diagnosticImage);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->setRoi(roi);
  m_largeImage->setExclusionRects(exclusionRects);
  appendLog(QString("%1: %2 points=%3")
              .arg(trText("actions.previewModel"))
              .arg(camera.id)
              .arg(training.contour.size()));
}

void MainWindow::testSurfaceShapeModel(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera) || camera.id != m_selectedCameraId)
  {
    return;
  }

  const SurfaceModelConfig model = m_recipeManager.loadSurfaceModel(camera.id);
  if (!model.hasModel)
  {
    appendLog(trText("log.surfaceStrategyMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty() || model.contour.isEmpty())
  {
    appendLog(input.empty() ? imageError : trText("surfaceModel.missing"));
    return;
  }

  SurfaceShapeMatchConfig config;
  config.searchRoi = cv::Rect(model.searchRoi.x(), model.searchRoi.y(), model.searchRoi.width(), model.searchRoi.height());
  config.modelContour.reserve(static_cast<size_t>(model.contour.size()));
  for (const QPoint& point : model.contour)
  {
    config.modelContour.emplace_back(point.x(), point.y());
  }
  config.edgeSensitivity = model.edgeSensitivity;
  config.maxShapeDistance = model.maxShapeDistance;

  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, config, exclusionRects]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.locateByShapeMatching(input, config, toCvRects(exclusionRects));
  };

  const QString __pendingCameraId_testSurfaceShapeModel = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceShapeModel);
  runAsyncTask(decltype(job)(job), this, [this, camera, model, exclusionRects, __pendingCameraId_testSurfaceShapeModel](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceShapeModel](void*) { decPendingJobs(__pendingCameraId_testSurfaceShapeModel); });
    const bool suppressViewUpdate =
      camera.id == m_selectedCameraId &&
      (m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry || m_setupCameraId == camera.id);
    if (!result.processed || result.diagnosticImage.empty() || !result.localization.found)
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      if (!suppressViewUpdate)
      {
        m_largeImage->clearGeometryOverlay();
      }
      appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    m_lastSurfaceLocalizationResults.insert(camera.id, result.localization);
    m_cameraRuntime[camera.id].setCurrentPose(partPoseFromSurfaceReference(camera, result.localization));
    if (!suppressViewUpdate)
    {
      m_selectedPreview = matToPixmap(result.diagnosticImage);
      m_largeImage->setImage(m_selectedPreview);
      m_largeImage->setRoi(model.searchRoi);
      m_largeImage->setExclusionRects(exclusionRects);
      m_largeImage->clearCircles();
    }
    GeometryOverlay overlay;
    appendCurrentPartPoseOverlay(camera, overlay);
    m_largeImage->setGeometryOverlay(overlay);
    appendLog(QString("%1: %2 shape cx=%3 cy=%4 angle=%5 score=%6")
                .arg(trText("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.localization.center.x, 0, 'f', 1)
                .arg(result.localization.center.y, 0, 'f', 1)
                .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2));
  });
}

void MainWindow::testSurfaceTemplateModel(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera) || camera.id != m_selectedCameraId)
  {
    return;
  }

  const SurfaceModelConfig model = m_recipeManager.loadSurfaceModel(camera.id);
  if (!model.hasModel)
  {
    appendLog(trText("log.surfaceStrategyMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  const cv::Mat modelImage = cv::imread(model.templateImagePath.toStdString(), cv::IMREAD_COLOR);
  if (input.empty() || modelImage.empty())
  {
    appendLog(input.empty() ? imageError : trText("log.imageMissing") + ": " + model.templateImagePath);
    return;
  }

  SurfaceTemplateMatchConfig config;
  config.searchRoi = cv::Rect(model.searchRoi.x(), model.searchRoi.y(), model.searchRoi.width(), model.searchRoi.height());
  config.modelImage = modelImage;
  config.edgeSensitivity = model.edgeSensitivity;
  config.minScore = model.minTemplateScore;
  config.angleStartDegrees = model.angleStartDegrees;
  config.angleEndDegrees = model.angleEndDegrees;
  config.angleStepDegrees = model.angleStepDegrees;

  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, config, exclusionRects]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.locateByTemplateMatching(input, config, toCvRects(exclusionRects));
  };

  const QString __pendingCameraId_testSurfaceTemplateModel = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceTemplateModel);
  runAsyncTask(decltype(job)(job), this, [this, camera, model, exclusionRects, __pendingCameraId_testSurfaceTemplateModel](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceTemplateModel](void*) { decPendingJobs(__pendingCameraId_testSurfaceTemplateModel); });
    const bool suppressViewUpdate =
      camera.id == m_selectedCameraId &&
      (m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry || m_setupCameraId == camera.id);
    if (!result.processed || result.diagnosticImage.empty() || !result.localization.found)
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      if (!suppressViewUpdate)
      {
        m_largeImage->clearGeometryOverlay();
      }
      appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    m_lastSurfaceLocalizationResults.insert(camera.id, result.localization);
    m_cameraRuntime[camera.id].setCurrentPose(partPoseFromSurfaceReference(camera, result.localization));
    if (!suppressViewUpdate)
    {
      m_selectedPreview = matToPixmap(result.diagnosticImage);
      m_largeImage->setImage(m_selectedPreview);
      m_largeImage->setRoi(model.searchRoi);
      m_largeImage->setExclusionRects(exclusionRects);
      m_largeImage->clearCircles();
    }
    GeometryOverlay overlay;
    appendCurrentPartPoseOverlay(camera, overlay);
    m_largeImage->setGeometryOverlay(overlay);
    appendLog(QString("%1: %2 template cx=%3 cy=%4 angle=%5 score=%6")
                .arg(trText("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.localization.center.x, 0, 'f', 1)
                .arg(result.localization.center.y, 0, 'f', 1)
                .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2));
  });
}

void MainWindow::testLocalization(const CameraConfig& camera)
{
  if (!isBwDimensionalCamera(camera))
  {
    appendLog(trText("log.localizationNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QRect roi;

  if (!m_recipeManager.loadLocalizationRoi(camera.id, roi))
  {
    appendLog(trText("log.localizationRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const LocalizationSettings settings = m_recipeManager.loadLocalizationSettings(camera.id);
  const QVector<QRect> exclusionRects = m_recipeManager.loadLocalizationExclusionRects(camera.id);

  auto job = [input, roi, exclusionRects, settings]() -> LocalizationResult {
    LocalizationProcessor processor;
    return processor.locateDarkObjectOnLightBackground(
      input,
      cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
      toCvRects(exclusionRects),
      settings.thresholdFactor,
      settings.thresholdOffset);
  };
  const QString __pendingCameraId_testLocalization = camera.id;
  incPendingJobs(__pendingCameraId_testLocalization);
  runAsyncTask(decltype(job)(job), this, [this, camera, roi, settings, __pendingCameraId_testLocalization](const LocalizationResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testLocalization](void*) { decPendingJobs(__pendingCameraId_testLocalization); });
    const bool suppressViewUpdate =
      camera.id == m_selectedCameraId &&
      (m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry || m_setupCameraId == camera.id);
    if (result.diagnosticImage.empty())
    {
      m_lastLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      if (!suppressViewUpdate)
      {
        m_largeImage->clearGeometryOverlay();
      }
      appendLog(trText("log.localizationFailed") + ": " + camera.id);
      return;
    }

    if (!suppressViewUpdate)
    {
      m_selectedPreview = matToPixmap(result.diagnosticImage);
      m_largeImage->setImage(m_selectedPreview);
      m_largeImage->setRoi(roi);
    }
    m_lastLocalizationResults.insert(camera.id, result);

    if (!result.found)
    {
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      if (!suppressViewUpdate)
      {
        m_largeImage->clearGeometryOverlay();
      }
      appendLog(trText("log.localizationNotFound") + ": " + camera.id);
      return;
    }

    m_cameraRuntime[camera.id].setCurrentPose(partPoseFromLocalizationResult(camera, result));
    GeometryOverlay overlay;
    appendCurrentPartPoseOverlay(camera, overlay);
    m_largeImage->setGeometryOverlay(overlay);
    appendLog(QString("%1: %2 cx=%3 cy=%4 angle=%5 area=%6 thr=%7 factor=%8 offset=%9")
                .arg(trText("log.localizationFound"))
                .arg(camera.id)
                .arg(result.center.x, 0, 'f', 1)
                .arg(result.center.y, 0, 'f', 1)
                .arg(result.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.area, 0, 'f', 1)
                .arg(result.thresholdValue, 0, 'f', 1)
                .arg(settings.thresholdFactor, 0, 'f', 3)
                .arg(settings.thresholdOffset, 0, 'f', 1));
  });
}

