#pragma once

#include <QString>

class RecipeManager;

QString aiClassificationSourcePath(const RecipeManager& recipes, const QString& cameraId);
QString aiClassificationDatasetPath(const RecipeManager& recipes, const QString& cameraId);
QString aiClassificationModelRunsPath(const RecipeManager& recipes, const QString& cameraId);
QString aiNewestClassificationResultsPath(const RecipeManager& recipes, const QString& cameraId);
QString aiClassificationValidationPath(const RecipeManager& recipes, const QString& cameraId);
QString aiNewestClassificationModelFilePath(const RecipeManager& recipes, const QString& cameraId);
QString aiClassificationModelFilePath(const RecipeManager& recipes, const QString& cameraId);
