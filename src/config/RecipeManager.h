#pragma once

#include <QHash>
#include <QRect>
#include <QPoint>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

struct PartPose;
struct CameraAcquisitionConfig;

struct LocalizationSettings
{
  double thresholdFactor = 0.5;
  double thresholdOffset = 0.0;
};

struct SurfaceDefectSettings
{
  int thresholdMin = 0;
  int thresholdMax = 80;
  bool pcaResolveAmbiguity = false;
  bool hasReferenceHalfPlane = false;
  bool referencePositiveHalfPlane = false;
};

struct RecipeRotatedRoi
{
  QPointF center;
  QSizeF size;
  double angleDegrees = 0.0;
  bool valid = false;
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
  bool pcaResolveAmbiguity = false;
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
  bool hasReferenceAngle = false;
  double referenceAngleDegrees = 0.0;
  bool useConvexHull = false;
  bool modelUseEdges = true;
  QString modelType = "shape";
};

struct GeometryLineRecipeConfig
{
  bool enabled = false;
  QString id = "line_1";
  QString alias;
  QString coordinateSpace = "part";
  bool anchorInImageSpace = false;
  QPointF partStart;
  QPointF partEnd;
  QPointF imageStart;
  QPointF imageEnd;
  int bandHalfWidth = 20;
  int edgeSensitivity = 60;
  int edgeCleanupDerivative = 12;
  int edgeStatisticalFilter = 0;
  bool useSubpixel = false;
  QString scanDirection = "normal_positive";
  QString transition = "light_to_dark";
  QString pickMode = "first";
};

struct GeometryPointRecipeConfig
{
  bool enabled = false;
  QString id = "point_1";
  QString alias;
  QString coordinateSpace = "part";
  bool anchorInImageSpace = false;
  QPointF partStart;
  QPointF partEnd;
  QPointF imageStart;
  QPointF imageEnd;
  int edgeSensitivity = 60;
  bool useSubpixel = false;
  QString transition = "light_to_dark";
  QString pickMode = "first";
};

struct GeometryCircleRecipeConfig
{
  bool enabled = false;
  QString id = "circle_1";
  QString alias;
  QString coordinateSpace = "part";
  bool anchorInImageSpace = false;
  QPointF partCenter;
  QPointF imageCenter;
  double radius = 0.0;
  int innerBand = 20;
  int outerBand = 20;
  int edgeSensitivity = 60;
  int edgeCleanupDerivative = 12;
  int edgeStatisticalFilter = 0;
  bool useSubpixel = false;
  QString scanDirection = "normal_positive";
  QString transition = "light_to_dark";
  QString pickMode = "first";
};

struct GeometryArcRecipeConfig
{
  bool enabled = false;
  QString id = "arc_1";
  QString alias;
  QString coordinateSpace = "part";
  bool anchorInImageSpace = false;
  QPointF partCenter;
  QPointF partStart;
  QPointF partEnd;
  QPointF imageCenter;
  QPointF imageStart;
  QPointF imageEnd;
  QPointF imageThrough;
  QPointF partThrough;
  double radius = 0.0;
  double startAngleRadians = 0.0;
  double endAngleRadians = 0.0;
  double partStartAngleRadians = 0.0;
  double partEndAngleRadians = 0.0;
  int innerBand = 20;
  int outerBand = 20;
  int edgeSensitivity = 60;
  int edgeCleanupDerivative = 12;
  int edgeStatisticalFilter = 0;
  bool useSubpixel = false;
  QString scanDirection = "normal_positive";
  QString transition = "light_to_dark";
  QString pickMode = "first";
};

struct ConstructedGeometryRecipeConfig
{
  bool enabled = true;
  QString id;
  QString type;
  QString sourceAId;
  QString sourceBId;
  double offset = 0.0;
};

struct ThreadMeasurementLimits
{
  bool enabled = false;
  QString alias;
  double nominal = 0.0;
  double min = 0.0;
  double max = 0.0;
  bool hasNominal = false;
  bool hasMin = false;
  bool hasMax = false;
};

struct ThreadInspectionSettings
{
  bool enabled = false;
  bool hasExtractionRoi = false;
  QString coordinateSpace = "part";
  QRect partRoi;
  QRect imageRoi;
  int thresholdMin = 0;
  int thresholdMax = 80;
  int morphOpenRadius = 2;
  int minSpeckAreaPx = 6;
  int maxSpeckAreaPx = 0;
  int profileSmoothRadius = 3;
  double outlierRejectSigma = 2.5;
  ThreadMeasurementLimits majorDiameter;
  ThreadMeasurementLimits minorDiameter;
  ThreadMeasurementLimits pitchDiameter;
  ThreadMeasurementLimits pitchLength;
  ThreadMeasurementLimits phaseOffset;
};

struct MeasurementRecipeConfig
{
  bool enabled = true;
  QString id;
  QString alias;
  QString type;
  QString criterion = "average";
  QString sourceAId;
  QString sourceBId;
  QPointF labelPoint;
  bool hasLabelPoint = false;
  QString unit = "px";
  double samplePixels = 0.0;
  double sampleValue = 0.0;
  bool hasSampleScale = false;
  double nominal = 0.0;
  double min = 0.0;
  double max = 0.0;
  bool hasNominal = false;
  bool hasMin = false;
  bool hasMax = false;
};

struct AiClassificationClassConfig
{
  int id = 0;
  QString name;
};

struct AiSegmentationFeatureConfig
{
  int id = 0;
  QString name;
};

class RecipeManager
{
public:
  explicit RecipeManager(QString recipeId = "default");

  static QString recipesRootPath();
  static QString defaultRecipeTemplateId();
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
  bool loadGlobalSurfaceAoi(const QString& cameraId, QRect& roi) const;
  LocalizationSettings loadLocalizationSettings(const QString& cameraId) const;
  QVector<QRect> loadLocalizationExclusionRects(const QString& cameraId) const;
  bool saveLocalizationRoi(const QString& cameraId, const QRect& roi, QString* errorMessage = nullptr) const;
  bool saveGlobalSurfaceAoi(const QString& cameraId, const QRect& roi, QString* errorMessage = nullptr) const;
  bool addLocalizationExclusionRect(const QString& cameraId, const QRect& rect, QString* errorMessage = nullptr) const;
  bool saveLocalizationExclusionRects(const QString& cameraId, const QVector<QRect>& rects, QString* errorMessage = nullptr) const;
  bool clearLocalizationExclusionRects(const QString& cameraId, QString* errorMessage = nullptr) const;
  bool loadSurfaceDefectRoi(const QString& cameraId, QRect& roi) const;
  bool loadSurfaceDefectRotatedRoi(const QString& cameraId, RecipeRotatedRoi& roi) const;
  QRect effectiveSurfaceSearchRect(const QString& cameraId, const QSize& imageSize) const;
  QVector<QPoint> loadSurfaceDefectPolygon(const QString& cameraId) const;
  SurfaceDefectSettings loadSurfaceDefectSettings(const QString& cameraId) const;
  QVector<QRect> loadSurfaceDefectExclusionRects(const QString& cameraId) const;
  bool saveSurfaceDefectRoi(const QString& cameraId, const QRect& roi, QString* errorMessage = nullptr) const;
  bool saveSurfaceDefectRotatedRoi(const QString& cameraId, const RecipeRotatedRoi& roi, QString* errorMessage = nullptr) const;
  bool saveSurfaceDefectPolygon(const QString& cameraId, const QVector<QPoint>& polygon, QString* errorMessage = nullptr) const;
  bool clearSurfaceDefectRoi(const QString& cameraId, QString* errorMessage = nullptr) const;
  bool clearSurfaceDefectPolygon(const QString& cameraId, QString* errorMessage = nullptr) const;
  bool saveSurfaceDefectThreshold(const QString& cameraId, int minValue, int maxValue, QString* errorMessage = nullptr) const;
  bool saveSurfaceDefectPcaResolveAmbiguity(const QString& cameraId, bool resolve, QString* errorMessage = nullptr) const;
  bool saveSurfaceDefectAxisReference(const QString& cameraId, bool positiveHalfPlane, QString* errorMessage = nullptr) const;
  bool addSurfaceDefectExclusionRect(const QString& cameraId, const QRect& rect, QString* errorMessage = nullptr) const;
  bool saveSurfaceDefectExclusionRects(const QString& cameraId, const QVector<QRect>& rects, QString* errorMessage = nullptr) const;
  bool clearSurfaceDefectExclusionRects(const QString& cameraId, QString* errorMessage = nullptr) const;
  bool clearSurfaceLocalizationGeometry(const QString& cameraId, QString* errorMessage = nullptr) const;
  bool hasSurfaceLocalizationSetup(const QString& cameraId) const;
  SurfaceLocalizationStrategyConfig loadSurfaceLocalizationStrategy(const QString& cameraId) const;
  bool saveSurfaceLocalizationStrategy(const QString& cameraId, const SurfaceLocalizationStrategyConfig& config, QString* errorMessage = nullptr) const;
  bool saveSurfaceStrategyCircle(const QString& cameraId, const QString& featureId, const QPoint& center, int radius, QString* errorMessage = nullptr) const;
  SurfaceAnnulusLocalizationConfig loadSurfaceAnnulusLocalization(const QString& cameraId) const;
  bool saveSurfaceAnnulusCircle(const QString& cameraId, bool outerCircle, const QPoint& center, int radius, QString* errorMessage = nullptr) const;
  bool saveSurfaceAnnulusThreshold(const QString& cameraId, int minValue, int maxValue, QString* errorMessage = nullptr) const;
  bool saveSurfaceLocalizationMethod(const QString& cameraId, const QString& method, QString* errorMessage = nullptr) const;
  bool saveSurfaceEdgeCircle(const QString& cameraId, const QPoint& center, int radius, QString* errorMessage = nullptr) const;
  bool saveSurfaceEdgeSensitivity(const QString& cameraId, int sensitivity, QString* errorMessage = nullptr) const;
  bool saveSurfaceEdgeBand(const QString& cameraId, int innerWidth, int outerWidth, QString* errorMessage = nullptr) const;
  bool saveSurfaceEdgeFitMaxError(const QString& cameraId, int maxError, QString* errorMessage = nullptr) const;
  bool saveSurfacePcaResolveAmbiguity(const QString& cameraId, bool resolve, QString* errorMessage = nullptr) const;
  SurfaceModelConfig loadSurfaceModel(const QString& cameraId) const;
  bool saveSurfaceModel(const QString& cameraId, const QRect& searchRoi, const QVector<QPoint>& contour, const QString& templateImagePath, QString* errorMessage = nullptr, double referenceAngleDegrees = std::numeric_limits<double>::quiet_NaN()) const;
  bool saveSurfaceModelEdgeSensitivity(const QString& cameraId, int sensitivity, QString* errorMessage = nullptr) const;
  bool saveSurfaceModelMaxShapeDistance(const QString& cameraId, double distance, QString* errorMessage = nullptr) const;
  bool saveSurfaceModelMinTemplateScore(const QString& cameraId, double score, QString* errorMessage = nullptr) const;
  bool saveSurfaceModelAngleRange(const QString& cameraId, double startDegrees, double endDegrees, double stepDegrees, QString* errorMessage = nullptr) const;
  bool saveSurfaceModelUseConvexHull(const QString& cameraId, bool useConvexHull, QString* errorMessage = nullptr) const;
  bool saveSurfaceModelUseEdges(const QString& cameraId, bool useEdges, QString* errorMessage = nullptr) const;
  bool saveSurfaceModelType(const QString& cameraId, const QString& modelType, QString* errorMessage = nullptr) const;
  QString surfaceModelTemplateImagePath(const QString& cameraId) const;
  QVector<GeometryLineRecipeConfig> loadGeometryLines(const QString& cameraId) const;
  bool saveGeometryLines(const QString& cameraId, const QVector<GeometryLineRecipeConfig>& configs, QString* errorMessage = nullptr) const;
  GeometryLineRecipeConfig loadGeometryLine(const QString& cameraId, const QString& lineId = "line_1") const;
  bool saveGeometryLine(const QString& cameraId, const GeometryLineRecipeConfig& config, QString* errorMessage = nullptr) const;
  QVector<GeometryPointRecipeConfig> loadGeometryPoints(const QString& cameraId) const;
  bool saveGeometryPoints(const QString& cameraId, const QVector<GeometryPointRecipeConfig>& configs, QString* errorMessage = nullptr) const;
  GeometryPointRecipeConfig loadGeometryPoint(const QString& cameraId, const QString& pointId = "point_1") const;
  bool saveGeometryPoint(const QString& cameraId, const GeometryPointRecipeConfig& config, QString* errorMessage = nullptr) const;
  QVector<GeometryCircleRecipeConfig> loadGeometryCircles(const QString& cameraId) const;
  bool saveGeometryCircles(const QString& cameraId, const QVector<GeometryCircleRecipeConfig>& configs, QString* errorMessage = nullptr) const;
  QVector<GeometryArcRecipeConfig> loadGeometryArcs(const QString& cameraId) const;
  bool saveGeometryArcs(const QString& cameraId, const QVector<GeometryArcRecipeConfig>& configs, QString* errorMessage = nullptr) const;
  bool loadGeometryGuideReferenceSize(const QString& cameraId, QSize& size) const;
  bool saveGeometryGuideReferenceSize(const QString& cameraId, const QSize& size, QString* errorMessage = nullptr) const;
  QVector<ConstructedGeometryRecipeConfig> loadConstructedGeometries(const QString& cameraId) const;
  bool saveConstructedGeometries(const QString& cameraId, const QVector<ConstructedGeometryRecipeConfig>& configs, QString* errorMessage = nullptr) const;
  bool appendConstructedGeometry(const QString& cameraId, const ConstructedGeometryRecipeConfig& config, QString* errorMessage = nullptr) const;
  QVector<MeasurementRecipeConfig> loadMeasurements(const QString& cameraId) const;
  bool saveMeasurements(const QString& cameraId, const QVector<MeasurementRecipeConfig>& configs, QString* errorMessage = nullptr) const;
  bool appendMeasurement(const QString& cameraId, const MeasurementRecipeConfig& config, QString* errorMessage = nullptr) const;

  bool hasCameraAcquisitionSettings(const QString& cameraId) const;
  bool loadCameraAcquisitionSettings(const QString& cameraId, CameraAcquisitionConfig& acquisition) const;
  bool saveCameraAcquisitionSettings(
    const QString& cameraId,
    const CameraAcquisitionConfig& acquisition,
    QString* errorMessage = nullptr) const;

  ThreadInspectionSettings loadThreadInspectionSettings(const QString& cameraId) const;
  bool saveThreadInspectionSettings(const QString& cameraId, const ThreadInspectionSettings& settings, QString* errorMessage = nullptr) const;
  bool saveThreadInspectionExtractionRoi(
    const QString& cameraId,
    const QString& coordinateSpace,
    const QRect& partRoi,
    const QRect& imageRoi,
    QString* errorMessage = nullptr) const;
  bool clearThreadInspectionExtractionRoi(const QString& cameraId, QString* errorMessage = nullptr) const;
  bool migrateThreadInspectionRoiToPartSpace(
    const QString& cameraId,
    const PartPose& pose,
    QString* errorMessage = nullptr) const;
  QString cameraSampleImagesPath(const QString& cameraId) const;
  QString cameraTestImagesPath(const QString& cameraId) const;
  QString cameraAiRawImagesPath(const QString& cameraId) const;
  QString cameraAiClassificationRawImagesPath(const QString& cameraId) const;
  QString cameraAiLocalizationRawImagesPath(const QString& cameraId) const;
  QString cameraAiClassificationClassImagesPath(const QString& cameraId, const AiClassificationClassConfig& classConfig) const;
  QString cameraAiSegmentationRawImagesPath(const QString& cameraId) const;
  QString cameraAiSegmentationFeatureImagesPath(const QString& cameraId, const AiSegmentationFeatureConfig& featureConfig) const;
  QString cameraAiSegmentationFeatureMasksPath(const QString& cameraId, const AiSegmentationFeatureConfig& featureConfig) const;
  QString cameraAiSegmentationFeatureLabelsPath(const QString& cameraId, const AiSegmentationFeatureConfig& featureConfig) const;
  QString firstCameraSampleImagePath(const QString& cameraId) const;
  QString firstCameraTestImagePath(const QString& cameraId) const;
  bool ensureCameraImageFolders(const QString& cameraId, QString* errorMessage = nullptr) const;
  bool importCameraSampleImage(const QString& cameraId, const QString& sourceFilePath, QString* errorMessage = nullptr) const;
  bool importCameraTestImages(const QString& cameraId, const QString& sourceDirectory, QString* errorMessage = nullptr) const;
  QVector<AiClassificationClassConfig> loadAiClassificationClasses(const QString& cameraId) const;
  bool addAiClassificationClass(const QString& cameraId, const QString& className, AiClassificationClassConfig* createdClass = nullptr, QString* errorMessage = nullptr) const;
  QString loadAiClassificationActiveModelPath(const QString& cameraId) const;
  bool saveAiClassificationActiveModelPath(const QString& cameraId, const QString& modelPath, QString* errorMessage = nullptr) const;
  QVector<AiSegmentationFeatureConfig> loadAiSegmentationFeatures(const QString& cameraId) const;
  bool addAiSegmentationFeature(const QString& cameraId, const QString& featureName, AiSegmentationFeatureConfig* createdFeature = nullptr, QString* errorMessage = nullptr) const;

private:
  static bool copyDirectory(const QString& sourceDirectory, const QString& destinationDirectory, QString* errorMessage);
  static bool writeRecipeMetadata(const QString& recipeId, const QString& path, QString* errorMessage);

  QString cameraRecipePath(const QString& cameraId) const;
  QString cameraImagesPath(const QString& cameraId, const QString& kind) const;
  void invalidateCameraRecipeCache(const QString& cameraId) const;
  void clearRecipeCache() const;

  QString m_recipeId;
  mutable QHash<QString, QVector<MeasurementRecipeConfig>> m_measurementsCache;
  mutable QHash<QString, QVector<ConstructedGeometryRecipeConfig>> m_constructedGeometriesCache;
};
