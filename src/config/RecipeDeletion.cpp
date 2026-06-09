#include "config/RecipeDeletion.h"

#include "config/RecipeManager.h"

#include <QDir>
#include <QFileInfo>

namespace RecipeDeletion
{
bool deleteRecipeDirectory(const QString& recipeId, QString* errorMessage)
{
  const QString normalizedId = RecipeManager::normalizeRecipeId(recipeId);
  if (normalizedId.isEmpty())
  {
    if (errorMessage)
    {
      *errorMessage = "ID ricetta non valido.";
    }
    return false;
  }

  const QString recipePath = QDir(RecipeManager::recipesRootPath()).filePath(normalizedId);
  if (!QFileInfo(recipePath).isDir())
  {
    if (errorMessage)
    {
      *errorMessage = "Ricetta non trovata: " + normalizedId;
    }
    return false;
  }

  if (!QFileInfo::exists(QDir(recipePath).filePath("recipe.json")))
  {
    if (errorMessage)
    {
      *errorMessage = "Directory ricetta non valida: " + recipePath;
    }
    return false;
  }

  if (!QDir(recipePath).removeRecursively())
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile eliminare ricetta: " + recipePath;
    }
    return false;
  }

  return true;
}
}
