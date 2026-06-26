#include "gui/modules/MainWindowRecipeModule.h"

#include "config/RecipeDeletion.h"

#include <QInputDialog>
#include <QMessageBox>

namespace
{
QString fallbackRecipeAfterDelete(const QString& deletedRecipeId)
{
  const QStringList recipes = RecipeManager::availableRecipes();
  for (const QString& recipeId : recipes)
  {
    if (recipeId != deletedRecipeId)
    {
      return recipeId;
    }
  }

  return {};
}
}

void MainWindowRecipeModule::deleteRecipe()
{
  const QStringList available = RecipeManager::availableRecipes();
  if (available.isEmpty())
  {
    QMessageBox::information(window(), tr("menu.recipes"), tr("messages.noRecipes"));
    return;
  }

  bool ok = false;
  const QString recipeId = QInputDialog::getItem(
    window(),
    tr("menu.deleteRecipe"),
    tr("labels.recipe"),
    available,
    qMax(0, available.indexOf(recipes().recipeId())),
    false,
    &ok);

  if (!ok || recipeId.isEmpty())
  {
    return;
  }

  if (recipeId == RecipeManager::defaultRecipeTemplateId())
  {
    QMessageBox::warning(window(), tr("menu.deleteRecipe"), tr("messages.cannotDeleteTemplateRecipe"));
    return;
  }

  if (available.size() <= 1)
  {
    QMessageBox::warning(window(), tr("menu.deleteRecipe"), tr("messages.cannotDeleteLastRecipe"));
    return;
  }

  const QMessageBox::StandardButton answer = QMessageBox::question(
    window(),
    tr("menu.deleteRecipe"),
    tr("messages.confirmDeleteRecipe").arg(recipeId),
    QMessageBox::Yes | QMessageBox::No,
    QMessageBox::No);
  if (answer != QMessageBox::Yes)
  {
    return;
  }

  QString error;
  if (!RecipeDeletion::deleteRecipeDirectory(recipeId, &error))
  {
    QMessageBox::warning(window(), tr("menu.deleteRecipe"), error);
    return;
  }

  log(tr("log.recipeDeleted") + ": " + recipeId);

  if (recipes().recipeId() == recipeId)
  {
    const QString fallbackRecipeId = fallbackRecipeAfterDelete(recipeId);
    if (!fallbackRecipeId.isEmpty())
    {
      setActiveRecipe(fallbackRecipeId);
    }
  }
}
