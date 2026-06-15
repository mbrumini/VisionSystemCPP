#include "gui/modules/setup/AiClassificationPaths.h"

#include "config/RecipeManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

QString aiClassificationSourcePath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(recipes.cameraAiClassificationRawImagesPath(cameraId)).absoluteFilePath("../");
}

QString aiClassificationDatasetPath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(RecipeManager::recipesRootPath()).filePath(
    QString("%1/datasets/%2/classification_yolo").arg(recipes.recipeId(), cameraId));
}

QString aiClassificationModelRunsPath(const RecipeManager& recipes, const QString& cameraId)
{
  Q_UNUSED(cameraId);
  return QDir(RecipeManager::recipesRootPath()).filePath(
    QString("%1/models/classification/runs").arg(recipes.recipeId()));
}

QString aiNewestClassificationResultsPath(const RecipeManager& recipes, const QString& cameraId)
{
  const QDir recipeRoot(QDir(RecipeManager::recipesRootPath()).filePath(recipes.recipeId()));
  const QString modelRoot = recipeRoot.filePath("models/classification/runs");
  QFileInfo newestResults;

  QDirIterator it(modelRoot, {"results.csv"}, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext())
  {
    const QFileInfo candidate(it.next());
    const QString normalizedPath = QDir::fromNativeSeparators(candidate.absoluteFilePath());
    if (!normalizedPath.contains(QString("/%1_yolo11s_cls").arg(cameraId)))
    {
      continue;
    }

    if (!newestResults.exists() || candidate.lastModified() > newestResults.lastModified())
    {
      newestResults = candidate;
    }
  }

  return newestResults.exists() ? newestResults.absoluteFilePath() : QString();
}

QString aiClassificationValidationPath(const RecipeManager& recipes, const QString& cameraId)
{
  return QDir(RecipeManager::recipesRootPath()).filePath(
    QString("%1/datasets/%2/classification_yolo/val").arg(recipes.recipeId(), cameraId));
}

QString aiNewestClassificationModelFilePath(const RecipeManager& recipes, const QString& cameraId)
{
  const QDir recipeRoot(QDir(RecipeManager::recipesRootPath()).filePath(recipes.recipeId()));
  const QString modelRoot = recipeRoot.filePath("models/classification");
  QFileInfo newestModel;

  QDirIterator it(modelRoot, {"best.pt"}, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext())
  {
    const QFileInfo candidate(it.next());
    const QString normalizedPath = QDir::fromNativeSeparators(candidate.absoluteFilePath());
    if (!normalizedPath.contains(QString("/%1_yolo11s_cls").arg(cameraId)))
    {
      continue;
    }

    if (!newestModel.exists() || candidate.lastModified() > newestModel.lastModified())
    {
      newestModel = candidate;
    }
  }

  if (newestModel.exists())
  {
    return newestModel.absoluteFilePath();
  }

  return recipeRoot.filePath(QString("models/classification/runs/%1_yolo11s_cls/weights/best.pt").arg(cameraId));
}

QString aiClassificationModelFilePath(const RecipeManager& recipes, const QString& cameraId)
{
  const QString activeModelPath = recipes.loadAiClassificationActiveModelPath(cameraId);
  if (!activeModelPath.isEmpty() && QFileInfo::exists(activeModelPath))
  {
    return activeModelPath;
  }

  return aiNewestClassificationModelFilePath(recipes, cameraId);
}
