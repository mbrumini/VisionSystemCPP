#pragma once

#include <QString>

QString compareAiClassificationModels(
  const QString& oldModelPath,
  const QString& newModelPath,
  const QString& validationPath,
  QString* recommendedModelPath,
  QString* errorMessage);
