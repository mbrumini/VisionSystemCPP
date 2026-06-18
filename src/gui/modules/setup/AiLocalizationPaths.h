#pragma once

#include <QString>

class RecipeManager;

QString aiLocalizationRootPath(const RecipeManager& recipes, const QString& cameraId);
QString aiLocalizationRawPath(const RecipeManager& recipes, const QString& cameraId);
QString aiLocalizationMasksPath(const RecipeManager& recipes, const QString& cameraId);
QString aiLocalizationLabelsPath(const RecipeManager& recipes, const QString& cameraId);
QString aiLocalizationDatasetPath(const RecipeManager& recipes, const QString& cameraId);
QString aiLocalizationModelsPath(const RecipeManager& recipes, const QString& cameraId);
