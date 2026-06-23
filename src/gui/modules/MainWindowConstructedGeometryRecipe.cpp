#include "gui/modules/MainWindowConstructedGeometryModule.h"

#include "processing/GeometryMeasurementPipeline.h"

#include "runtime/CameraRuntime.h"

void MainWindowConstructedGeometryModule::saveConstructedGeometryRecipeAction(const CameraConfig& camera,
                                                                              const QString& type,
                                                                              const QString& sourceAId,
                                                                              const QString& sourceBId,
                                                                              double offset)
{
  ConstructedGeometryRecipeConfig config;
  config.enabled = true;
  config.type = type;
  config.sourceAId = sourceAId;
  config.sourceBId = sourceBId;
  config.offset = offset;

  QString error;
  if (!recipes().appendConstructedGeometry(camera.id, config, &error))
  {
    log(QString("%1: %2").arg(tr("log.constructedRecipeSaveFailed"), error));
  }
}

void MainWindowConstructedGeometryModule::rebuildConstructedGeometryRecipe(const CameraConfig& camera)
{
  rebuildConstructedGeometries(
    cameraRuntime()[camera.id].geometries(),
    recipes().loadConstructedGeometries(camera.id));
}
