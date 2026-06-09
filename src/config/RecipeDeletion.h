#pragma once

#include <QString>

namespace RecipeDeletion
{
bool deleteRecipeDirectory(const QString& recipeId, QString* errorMessage = nullptr);
}
