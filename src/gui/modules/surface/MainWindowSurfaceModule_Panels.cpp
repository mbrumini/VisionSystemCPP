#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/SurfaceLocalizationPanelWidget.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/TouchIconButton.h"
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

    showCircleAnnulusLocalizationPanel(camera);
    return;
  }

  if (strategyId == "massPca")
  {
    showMassPcaLocalizationPanel(camera);
    return;
  }

  if (strategyId == "edgePca")
  {
    showEdgePcaLocalizationPanel(camera);
    return;
  }

  if (strategyId == "model")
  {
    showModelLocalizationPanel(camera);
    return;
  }

  showPlannedLocalizationPanel(camera, strategyId);
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

void MainWindowSurfaceModule::activateSurfaceDefectPolygonDrawing(const CameraConfig& camera)
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

  largeImage()->setPolygonDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::SurfaceDefects;
  log(tr("actions.polygon") + ": " + camera.id);
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
  largeImage()->setSearchPolygon(recipes().loadSurfaceDefectPolygon(camera.id));

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

