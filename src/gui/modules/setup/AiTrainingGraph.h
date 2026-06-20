#pragma once

#include <QPixmap>
#include <QString>

class RecipeManager;

QPixmap renderAiClassificationTrainingGraph(const RecipeManager& recipes, const QString& cameraId);
QPixmap renderAiLocalizationTrainingGraph(const RecipeManager& recipes, const QString& cameraId);
