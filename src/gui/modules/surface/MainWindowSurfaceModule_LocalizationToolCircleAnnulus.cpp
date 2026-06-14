#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/SurfaceLocalizationPanelWidget.h"
#include "gui/ToolCatalog.h"

void MainWindowSurfaceModule::showSurfaceLocalizationPanel(const CameraConfig& camera)
{
  showCircleAnnulusLocalizationPanel(camera);
}

void MainWindowSurfaceModule::showCircleAnnulusLocalizationPanel(const CameraConfig& camera)
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
  handlers.clearLocalization = [this, camera]() { clearSurfaceLocalization(camera); };
  handlers.methodChanged = [this, camera](const QString& method) { saveSurfaceMethodAndPreview(camera, method); };
  handlers.thresholdChanged = [this, camera](int value) { saveSurfaceThresholdAndPreview(camera, value); };
  handlers.edgeSensitivityChanged = [this, camera](int value) { saveSurfaceEdgeAndPreview(camera, value); };
  handlers.edgeBandChanged = [this, camera](int innerWidth, int outerWidth) { saveSurfaceEdgeBandAndPreview(camera, innerWidth, outerWidth); };
  handlers.edgeFitMaxErrorChanged = [this, camera](int value) { saveSurfaceEdgeFitMaxErrorAndPreview(camera, value); };
  handlers.back = [this, camera]() {
    context().showLocalizationStrategyList(camera);
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
