#include "gui/modules/MainWindowRecipeModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/MainWindowThreadModule.h"
#include "runtime/CameraRuntime.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>

namespace
{
QStringList operatorVisibleRecipes(const QStringList& recipes, bool isGuru)
{
  if (isGuru)
  {
    return recipes;
  }

  QStringList visible = recipes;
  visible.removeAll(RecipeManager::defaultRecipeTemplateId());
  return visible;
}
}

MainWindowRecipeModule::MainWindowRecipeModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

void MainWindowRecipeModule::selectRecipe()
{
  const QStringList available = operatorVisibleRecipes(
    RecipeManager::availableRecipes(),
    context().isGuru && context().isGuru());
  if (available.isEmpty())
  {
    QMessageBox::information(window(), tr("menu.recipes"), tr("messages.noRecipes"));
    return;
  }

  bool ok = false;
  const QString selectedRecipe = QInputDialog::getItem(
    window(),
    tr("menu.selectRecipe"),
    tr("labels.recipe"),
    available,
    qMax(0, available.indexOf(recipes().recipeId())),
    false,
    &ok);

  if (!ok || selectedRecipe.isEmpty())
  {
    return;
  }

  setActiveRecipe(selectedRecipe);
}

void MainWindowRecipeModule::createRecipe()
{
  bool ok = false;
  const QString recipeName = QInputDialog::getText(
    window(),
    tr("menu.newRecipe"),
    tr("labels.recipeName"),
    QLineEdit::Normal,
    "",
    &ok);

  if (!ok)
  {
    return;
  }

  const QString recipeId = RecipeManager::normalizeRecipeId(recipeName);
  QString error;
  if (!RecipeManager::createRecipe(recipeId, &error))
  {
    QMessageBox::warning(window(), tr("menu.newRecipe"), error);
    return;
  }

  setActiveRecipe(recipeId);
}

void MainWindowRecipeModule::duplicateRecipe()
{
  QStringList resolutionLines;
  for (const CameraConfig& camera : config().activeCameras())
  {
    QSize referenceSize;
    if (recipes().loadGeometryGuideReferenceSize(camera.id, referenceSize))
    {
      resolutionLines.append(
        QString("%1: %2 x %3").arg(camera.id).arg(referenceSize.width()).arg(referenceSize.height()));
    }
  }

  QString message = tr("messages.confirmDuplicateRecipeDetail");
  if (!resolutionLines.isEmpty())
  {
    message += "\n\n" + tr("messages.duplicateRecipeReferenceSizes") + "\n" + resolutionLines.join("\n");
  }

  const auto answer = QMessageBox::question(
    window(),
    tr("menu.duplicateRecipe"),
    message,
    QMessageBox::Yes | QMessageBox::No,
    QMessageBox::No);
  if (answer != QMessageBox::Yes)
  {
    return;
  }

  bool ok = false;
  const QString recipeName = QInputDialog::getText(
    window(),
    tr("menu.duplicateRecipe"),
    tr("labels.recipeName"),
    QLineEdit::Normal,
    recipes().recipeId() + "_copy",
    &ok);

  if (!ok)
  {
    return;
  }

  const QString recipeId = RecipeManager::normalizeRecipeId(recipeName);
  QString error;
  if (!RecipeManager::duplicateRecipe(recipes().recipeId(), recipeId, &error))
  {
    QMessageBox::warning(window(), tr("menu.duplicateRecipe"), error);
    return;
  }

  setActiveRecipe(recipeId);
}

void MainWindowRecipeModule::importRecipe()
{
  const QString sourceDirectory = QFileDialog::getExistingDirectory(
    window(),
    tr("menu.importRecipe"),
    RecipeManager::recipesRootPath());

  if (sourceDirectory.isEmpty())
  {
    return;
  }

  QString importedRecipeId;
  QString error;
  if (!RecipeManager::importRecipeDirectory(sourceDirectory, &importedRecipeId, &error))
  {
    QMessageBox::warning(window(), tr("menu.importRecipe"), error);
    return;
  }

  setActiveRecipe(importedRecipeId);
}

void MainWindowRecipeModule::exportRecipe()
{
  const QString destinationDirectory = QFileDialog::getExistingDirectory(
    window(),
    tr("menu.exportRecipe"),
    RecipeManager::recipesRootPath());

  if (destinationDirectory.isEmpty())
  {
    return;
  }

  QString error;
  if (!RecipeManager::exportRecipeDirectory(recipes().recipeId(), destinationDirectory, &error))
  {
    QMessageBox::warning(window(), tr("menu.exportRecipe"), error);
    return;
  }

  log(tr("log.recipeExported") + ": " + recipes().recipeId());
}

void MainWindowRecipeModule::ensureRecipeCameraFolders()
{
  for (const CameraConfig& camera : config().activeCameras())
  {
    QString error;
    if (!recipes().ensureCameraImageFolders(camera.id, &error))
    {
      log(error);
    }
  }
}

void MainWindowRecipeModule::setActiveRecipe(const QString& recipeId)
{
  if (context().resetMeasurementStatistics)
  {
    context().resetMeasurementStatistics();
  }

  recipes().setRecipeId(recipeId);
  for (const CameraConfig& camera : config().activeCameras())
  {
    cameraRuntime()[camera.id].clearCurrentPose(camera.id);
    cameraRuntime()[camera.id].clearGeometries();
  }
  if (context().thread)
  {
    context().thread->clearThreadOverlays();
  }
  if (!selectedCameraId().isEmpty() && context().imaging)
  {
    context().imaging->restoreSampleWorkspace(selectedCamera());
  }
  if (context().geometry)
  {
    context().geometry->reloadRecipeState();
  }

  QString error;
  if (!RecipeManager::saveActiveRecipeId(recipes().recipeId(), &error))
  {
    log(error);
  }

  if (context().updateRecipeText)
  {
    context().updateRecipeText(recipes().recipeId());
  }
  log(tr("log.activeRecipe") + ": " + recipes().recipeId());
  ensureRecipeCameraFolders();
  applyRecipeAcquisitionSettings();
  refreshSelectedCameraRecipeData();
  if (!selectedCameraId().isEmpty() && context().setup)
  {
    context().setup->refreshSetupGeometryResults(selectedCamera());
  }
}

void MainWindowRecipeModule::refreshSelectedCameraRecipeData()
{
  if (selectedCameraId().isEmpty() || !largeImage())
  {
    return;
  }

  QRect roi;
  if (MainWindowCameraProfile::isGrayscaleLocalization(selectedCamera(), config()))
  {
    if (recipes().loadSurfaceDefectRoi(selectedCameraId(), roi))
    {
      largeImage()->setRoi(roi);
    }
    else
    {
      largeImage()->clearRoi();
    }

    largeImage()->setExclusionRects(recipes().loadSurfaceDefectExclusionRects(selectedCameraId()));
    largeImage()->setSearchPolygon(recipes().loadSurfaceDefectPolygon(selectedCameraId()));
    largeImage()->clearCircles();
    GeometryOverlay overlay;
    if (context().geometry && recipes().hasSurfaceLocalizationSetup(selectedCameraId()))
    {
      context().geometry->appendCurrentPartPoseOverlay(selectedCamera(), overlay);
    }
    largeImage()->setGeometryOverlay(overlay);
    syncThreadExtractionRoiOverlay(selectedCamera());
    return;
  }

  if (recipes().loadLocalizationRoi(selectedCameraId(), roi))
  {
    largeImage()->setRoi(roi);
  }
  else
  {
    largeImage()->clearRoi();
  }

  largeImage()->setExclusionRects(recipes().loadLocalizationExclusionRects(selectedCameraId()));
  largeImage()->clearSearchPolygon();
}

void MainWindowRecipeModule::applyRecipeAcquisitionSettings()
{
  const QString camerasConfigPath = RecipeJsonUtils::appPath("config/cameras.json");

  for (const CameraConfig& camera : config().activeCameras())
  {
    CameraAcquisitionConfig acquisition = camera.acquisition;
    if (!recipes().loadCameraAcquisitionSettings(camera.id, acquisition))
    {
      continue;
    }

    config().updateCameraAcquisitionSettings(camera.id, acquisition);

    QString error;
    if (!config().saveCameraAcquisitionSettings(camerasConfigPath, camera.id, acquisition, &error))
    {
      log(error);
      continue;
    }

    CameraRuntime& runtime = cameraRuntime()[camera.id];
    runtime.setIntervalMs(acquisition.frameIntervalMs);
    if (!runtime.applyAcquisitionSettings(acquisition, &error) && !error.isEmpty())
    {
      log(error);
    }

    if (selectedCameraId() == camera.id)
    {
      selectedCamera().acquisition = acquisition;
      if (runtime.running() && context().simulationTimer)
      {
        context().simulationTimer->start(runtime.intervalMs());
      }
    }
  }
}
