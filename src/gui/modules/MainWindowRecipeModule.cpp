#include "gui/modules/MainWindowRecipeModule.h"

#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "runtime/CameraRuntime.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>

MainWindowRecipeModule::MainWindowRecipeModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

void MainWindowRecipeModule::selectRecipe()
{
  const QStringList available = RecipeManager::availableRecipes();
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
    if (context().geometry)
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
