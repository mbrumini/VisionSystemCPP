#pragma once

#include <QPixmap>
#include <QString>

class RecipeManager;

QPixmap renderAiClassificationTrainingGraph(const RecipeManager& recipes, const QString& cameraId);
