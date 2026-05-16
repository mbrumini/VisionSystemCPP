#pragma once

#include <QRect>
#include <QString>
#include <QStringList>

class RecipeManager
{
public:
  explicit RecipeManager(QString recipeId = "default");

  static QString recipesRootPath();
  static QString normalizeRecipeId(const QString& text);
  static QStringList availableRecipes();
  static bool createRecipe(const QString& recipeId, QString* errorMessage = nullptr);
  static bool duplicateRecipe(const QString& sourceRecipeId, const QString& newRecipeId, QString* errorMessage = nullptr);
  static bool importRecipeDirectory(const QString& sourceDirectory, QString* importedRecipeId, QString* errorMessage = nullptr);
  static bool exportRecipeDirectory(const QString& recipeId, const QString& destinationDirectory, QString* errorMessage = nullptr);
  static QString loadActiveRecipeId();
  static bool saveActiveRecipeId(const QString& recipeId, QString* errorMessage = nullptr);

  QString recipeId() const;
  void setRecipeId(const QString& recipeId);
  bool loadLocalizationRoi(const QString& cameraId, QRect& roi) const;
  bool saveLocalizationRoi(const QString& cameraId, const QRect& roi, QString* errorMessage = nullptr) const;

private:
  static bool copyDirectory(const QString& sourceDirectory, const QString& destinationDirectory, QString* errorMessage);
  static bool writeRecipeMetadata(const QString& recipeId, const QString& path, QString* errorMessage);

  QString cameraRecipePath(const QString& cameraId) const;

  QString m_recipeId;
};
