#pragma once

#include <QRect>
#include <QString>

class RecipeManager
{
public:
  explicit RecipeManager(QString recipeId = "default");

  QString recipeId() const;
  bool loadLocalizationRoi(const QString& cameraId, QRect& roi) const;
  bool saveLocalizationRoi(const QString& cameraId, const QRect& roi, QString* errorMessage = nullptr) const;

private:
  QString cameraRecipePath(const QString& cameraId) const;

  QString m_recipeId;
};
