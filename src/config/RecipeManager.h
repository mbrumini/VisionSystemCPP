#pragma once

#include <QRect>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>

struct LocalizationSettings
{
  double thresholdFactor = 0.5;
  double thresholdOffset = 0.0;
};

struct SurfaceDefectSettings
{
  int thresholdMin = 0;
  int thresholdMax = 80;
};

struct SurfaceStrategyFeatureConfig
{
  QString id;
  QString polarity = "NB";
  QRect searchRoi;
  int thresholdMin = 0;
  int thresholdMax = 80;
  double expectedRadiusMin = 0.0;
  double expectedRadiusMax = 0.0;
};

struct SurfaceLocalizationStrategyConfig
{
  QString name = "two_circles_axis";
  QString origin = "midpoint";
  QString xAxisFrom;
  QString xAxisTo;
  QVector<SurfaceStrategyFeatureConfig> features;
};

struct SurfaceAnnulusLocalizationConfig
{
  bool hasOuterCircle = false;
  bool hasInnerCircle = false;
  QString method = "threshold";
  QPoint center;
  int outerRadius = 0;
  int innerRadius = 0;
  int thresholdMin = 0;
  int thresholdMax = 80;
  bool hasEdgeCircle = false;
  QPoint edgeCenter;
  int edgeRadius = 0;
  int edgeSensitivity = 60;
  int edgeBandInner = 20;
  int edgeBandOuter = 20;
  int edgeFitMaxError = 8;
};

struct SurfaceModelConfig
{
  bool hasModel = false;
  QRect searchRoi;
  QVector<QPoint> contour;
  QString templateImagePath;
  int edgeSensitivity = 60;
  double maxShapeDistance = 0.25;
  double minTemplateScore = 0.45;
  double angleStartDegrees = -180.0;
  double angleEndDegrees = 180.0;
  double angleStepDegrees = 5.0;
};

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
  LocalizationSettings loadLocalizationSettings(const QString& cameraId) const;
  QVector<QRect> loadLocalizationExclusionRects(const QString& cameraId) const;
  bool saveLocalizationRoi(const QString& cameraId, const QRect& roi, QString* errorMessage = nullptr) const;
  bool addLocalizationExclusionRect(const QString& cameraId, const QRect& rect, QString* errorMessage = nullptr) const;
  bool saveLocalizationExclusionRects(const QString& cameraId, const QVector<QRect>& rects, QString* errorMessage = nullptr) const;
  bool clearLocalizationExclusionRects(const QString& cameraId, QString* errorMessage = nullptr) const;
  bool loadSurfaceDefectRoi(const QString& cameraId, QRect& roi) const;
  SurfaceDefectSettings loadSurfaceDefectSettings(const QString& cameraId) const;
  QVector<QRect> loadSurfaceDefectExclusionRects(const QString& cameraId) const;
  bool saveSurfaceDefectRoi(const QString& cameraId, const QRect& roi, QString* errorMessage = nullptr) const;
  bool addSurfaceDefectExclusionRect(const QString& cameraId, const QRect& rect, QString* errorMessage = nullptr) const;
  bool saveSurfaceDefectExclusionRects(const QString& cameraId, const QVector<QRect>& rects, QString* errorMessage = nullptr) const;
  bool clearSurfaceDefectExclusionRects(const QString& cameraId, QString* errorMessage = nullptr) const;
  bool clearSurfaceLocalizationGeometry(const QString& cameraId, QString* errorMessage = nullptr) const;
  SurfaceLocalizationStrategyConfig loadSurfaceLocalizationStrategy(const QString& cameraId) const;
  SurfaceAnnulusLocalizationConfig loadSurfaceAnnulusLocalization(const QString& cameraId) const;
  bool saveSurfaceAnnulusCircle(const QString& cameraId, bool outerCircle, const QPoint& center, int radius, QString* errorMessage = nullptr) const;
  bool saveSurfaceAnnulusThreshold(const QString& cameraId, int minValue, int maxValue, QString* errorMessage = nullptr) const;
  bool saveSurfaceLocalizationMethod(const QString& cameraId, const QString& method, QString* errorMessage = nullptr) const;
  bool saveSurfaceEdgeCircle(const QString& cameraId, const QPoint& center, int radius, QString* errorMessage = nullptr) const;
  bool saveSurfaceEdgeSensitivity(const QString& cameraId, int sensitivity, QString* errorMessage = nullptr) const;
  bool saveSurfaceEdgeBand(const QString& cameraId, int innerWidth, int outerWidth, QString* errorMessage = nullptr) const;
  bool saveSurfaceEdgeFitMaxError(const QString& cameraId, int maxError, QString* errorMessage = nullptr) const;
  SurfaceModelConfig loadSurfaceModel(const QString& cameraId) const;
  bool saveSurfaceModel(const QString& cameraId, const QRect& searchRoi, const QVector<QPoint>& contour, const QString& templateImagePath, QString* errorMessage = nullptr) const;
  bool saveSurfaceModelEdgeSensitivity(const QString& cameraId, int sensitivity, QString* errorMessage = nullptr) const;
  bool saveSurfaceModelMaxShapeDistance(const QString& cameraId, double distance, QString* errorMessage = nullptr) const;
  bool saveSurfaceModelMinTemplateScore(const QString& cameraId, double score, QString* errorMessage = nullptr) const;
  bool saveSurfaceModelAngleRange(const QString& cameraId, double startDegrees, double endDegrees, double stepDegrees, QString* errorMessage = nullptr) const;
  QString surfaceModelTemplateImagePath(const QString& cameraId) const;

private:
  static bool copyDirectory(const QString& sourceDirectory, const QString& destinationDirectory, QString* errorMessage);
  static bool writeRecipeMetadata(const QString& recipeId, const QString& path, QString* errorMessage);

  QString cameraRecipePath(const QString& cameraId) const;

  QString m_recipeId;
};
