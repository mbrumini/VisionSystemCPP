#include "gui/modules/setup/AiLocalizationPaths.h"

#include "config/RecipeManager.h"

#include <QDir>

QString aiLocalizationRootPath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(recipes.cameraAiLocalizationRawImagesPath(cameraId)).absoluteFilePath("..");
}

QString aiLocalizationRawPath(const RecipeManager& recipes, const QString& cameraId)
{
  return recipes.cameraAiLocalizationRawImagesPath(cameraId);
}

QString aiLocalizationMasksPath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(aiLocalizationRootPath(recipes, cameraId)).filePath("masks");
}

QString aiLocalizationLabelsPath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(aiLocalizationRootPath(recipes, cameraId)).filePath("labels");
}

QString aiLocalizationDatasetPath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(RecipeManager::recipesRootPath()).filePath(
    QString("%1/datasets/%2/localization_segmentation").arg(recipes.recipeId(), cameraId));
}

QString aiLocalizationModelsPath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(RecipeManager::recipesRootPath()).filePath(
    QString("%1/models/%2/localization_segmentation").arg(recipes.recipeId(), cameraId));
}
