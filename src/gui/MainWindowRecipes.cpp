#include "MainWindow.h"

#include "gui/SurfaceLocalizationPanelWidget.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"
#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "processing/SurfaceModelTrainer.h"
#include "processing/geometry/EdgeLineDetector.h"
#include "processing/geometry/EdgePointDetector.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QSettings>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>
#include <type_traits>
#include "util/AsyncExecutor.h"
#include <thread>
#include <memory>

using AsyncExecutor::runAsyncTask;

void MainWindow::selectRecipe()
{
  const QStringList recipes = RecipeManager::availableRecipes();

  if (recipes.isEmpty())
  {
    QMessageBox::information(this, trText("menu.recipes"), trText("messages.noRecipes"));
    return;
  }

  bool ok = false;
  const QString selectedRecipe = QInputDialog::getItem(
    this,
    trText("menu.selectRecipe"),
    trText("labels.recipe"),
    recipes,
    qMax(0, recipes.indexOf(m_recipeManager.recipeId())),
    false,
    &ok);

  if (!ok || selectedRecipe.isEmpty())
  {
    return;
  }

  setActiveRecipe(selectedRecipe);
}

void MainWindow::createRecipe()
{
  bool ok = false;
  const QString recipeName = QInputDialog::getText(
    this,
    trText("menu.newRecipe"),
    trText("labels.recipeName"),
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
    QMessageBox::warning(this, trText("menu.newRecipe"), error);
    return;
  }

  setActiveRecipe(recipeId);
}

void MainWindow::duplicateRecipe()
{
  bool ok = false;
  const QString recipeName = QInputDialog::getText(
    this,
    trText("menu.duplicateRecipe"),
    trText("labels.recipeName"),
    QLineEdit::Normal,
    m_recipeManager.recipeId() + "_copy",
    &ok);

  if (!ok)
  {
    return;
  }

  const QString recipeId = RecipeManager::normalizeRecipeId(recipeName);
  QString error;

  if (!RecipeManager::duplicateRecipe(m_recipeManager.recipeId(), recipeId, &error))
  {
    QMessageBox::warning(this, trText("menu.duplicateRecipe"), error);
    return;
  }

  setActiveRecipe(recipeId);
}

void MainWindow::importRecipe()
{
  const QString sourceDirectory = QFileDialog::getExistingDirectory(
    this,
    trText("menu.importRecipe"),
    RecipeManager::recipesRootPath());

  if (sourceDirectory.isEmpty())
  {
    return;
  }

  QString importedRecipeId;
  QString error;

  if (!RecipeManager::importRecipeDirectory(sourceDirectory, &importedRecipeId, &error))
  {
    QMessageBox::warning(this, trText("menu.importRecipe"), error);
    return;
  }

  setActiveRecipe(importedRecipeId);
}

void MainWindow::exportRecipe()
{
  const QString destinationDirectory = QFileDialog::getExistingDirectory(
    this,
    trText("menu.exportRecipe"),
    RecipeManager::recipesRootPath());

  if (destinationDirectory.isEmpty())
  {
    return;
  }

  QString error;

  if (!RecipeManager::exportRecipeDirectory(m_recipeManager.recipeId(), destinationDirectory, &error))
  {
    QMessageBox::warning(this, trText("menu.exportRecipe"), error);
    return;
  }

  appendLog(trText("log.recipeExported") + ": " + m_recipeManager.recipeId());
}

void MainWindow::ensureRecipeCameraFolders()
{
  for (const CameraConfig& camera : m_config.activeCameras())
  {
    QString error;
    if (!m_recipeManager.ensureCameraImageFolders(camera.id, &error))
    {
      appendLog(error);
    }
  }
}

void MainWindow::setActiveRecipe(const QString& recipeId)
{
  m_recipeManager.setRecipeId(recipeId);
  m_geometryLineConfigs.clear();
  m_activeGeometryLineIndexes.clear();
  m_geometryPointConfigs.clear();
  m_activeGeometryPointIndexes.clear();
  m_geometryCircleConfigs.clear();
  m_activeGeometryCircleIndexes.clear();
  QString error;

  if (!RecipeManager::saveActiveRecipeId(m_recipeManager.recipeId(), &error))
  {
    appendLog(error);
  }

  appendLog(trText("log.activeRecipe") + ": " + m_recipeManager.recipeId());
  ensureRecipeCameraFolders();
  refreshSelectedCameraRecipeData();
}

void MainWindow::refreshSelectedCameraRecipeData()
{
  if (m_selectedCameraId.isEmpty() || !m_largeImage)
  {
    return;
  }

  QRect roi;

  if (isGrayscaleLocalizationCamera(m_selectedCamera))
  {
    if (m_recipeManager.loadSurfaceDefectRoi(m_selectedCameraId, roi))
    {
      m_largeImage->setRoi(roi);
    }
    else
    {
      m_largeImage->clearRoi();
    }

    m_largeImage->setExclusionRects(m_recipeManager.loadSurfaceDefectExclusionRects(m_selectedCameraId));
    m_largeImage->clearCircles();
    GeometryOverlay overlay;
    appendCurrentPartPoseOverlay(m_selectedCamera, overlay);
    m_largeImage->setGeometryOverlay(overlay);
    return;
  }
  if (m_recipeManager.loadLocalizationRoi(m_selectedCameraId, roi))
  {
    m_largeImage->setRoi(roi);
  }
  else
  {
    m_largeImage->clearRoi();
  }

  m_largeImage->setExclusionRects(m_recipeManager.loadLocalizationExclusionRects(m_selectedCameraId));
}


