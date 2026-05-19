#pragma once

#include "gui/modules/MainWindowModuleBase.h"

#include <QString>

class CameraConfig;

class MainWindowRecipeModule : public MainWindowModuleBase
{
public:
  explicit MainWindowRecipeModule(MainWindowContext& context);

  void selectRecipe();
  void createRecipe();
  void duplicateRecipe();
  void importRecipe();
  void exportRecipe();
  void ensureRecipeCameraFolders();
  void setActiveRecipe(const QString& recipeId);
  void refreshSelectedCameraRecipeData();
};
