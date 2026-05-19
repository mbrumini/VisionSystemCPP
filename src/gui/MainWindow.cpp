#include "MainWindow.h"

#include "gui/SurfaceLocalizationPanelWidget.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"
#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "processing/SurfaceModelTrainer.h"
#include "processing/geometry/EdgeLineDetector.h"
#include "processing/geometry/EdgePointDetector.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QSettings>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>
#include <type_traits>
#include "util/AsyncExecutor.h"
#include <thread>
#include <memory>

using AsyncExecutor::runAsyncTask;

namespace
{
QString projectPath(const QString& relativePath)
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(relativePath);
}



QString resolveProjectPath(const QString& path)
{
  if (QFileInfo(path).isAbsolute())
  {
    return path;
  }

  return projectPath(path);
}

std::vector<cv::Rect> toCvRects(const QVector<QRect>& rects)
{
  std::vector<cv::Rect> result;
  result.reserve(static_cast<size_t>(rects.size()));

  for (const QRect& rect : rects)
  {
    result.emplace_back(rect.x(), rect.y(), rect.width(), rect.height());
  }

  return result;
}

bool circleFromThreePoints(const QVector<QPoint>& points, ImageCircle& circle)
{
  if (points.size() < 3)
  {
    return false;
  }

  const double x1 = points[0].x();
  const double y1 = points[0].y();
  const double x2 = points[1].x();
  const double y2 = points[1].y();
  const double x3 = points[2].x();
  const double y3 = points[2].y();
  const double d = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));

  if (std::abs(d) < 0.001)
  {
    return false;
  }

  const double ux = ((x1 * x1 + y1 * y1) * (y2 - y3) +
                     (x2 * x2 + y2 * y2) * (y3 - y1) +
                     (x3 * x3 + y3 * y3) * (y1 - y2)) / d;
  const double uy = ((x1 * x1 + y1 * y1) * (x3 - x2) +
                     (x2 * x2 + y2 * y2) * (x1 - x3) +
                     (x3 * x3 + y3 * y3) * (x2 - x1)) / d;
  const QPoint center(qRound(ux), qRound(uy));
  const int radius = qRound(std::hypot(x1 - ux, y1 - uy));

  if (radius <= 2)
  {
    return false;
  }

  circle = {center, radius};
  return true;
}

QString scanDirectionToRecipe(EdgeLineScanDirection direction)
{
  return direction == EdgeLineScanDirection::NormalNegative ? "normal_negative" : "normal_positive";
}

EdgeLineScanDirection scanDirectionFromRecipe(const QString& value)
{
  return value == "normal_negative" ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
}

QString transitionToRecipe(EdgeLineTransition transition)
{
  return transition == EdgeLineTransition::DarkToLight ? "dark_to_light" : "light_to_dark";
}

EdgeLineTransition transitionFromRecipe(const QString& value)
{
  return value == "dark_to_light" ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
}

QString pickModeToRecipe(EdgeLinePickMode mode)
{
  if (mode == EdgeLinePickMode::Last)
  {
    return "last";
  }

  if (mode == EdgeLinePickMode::Best)
  {
    return "best";
  }

  return "first";
}

EdgeLinePickMode pickModeFromRecipe(const QString& value)
{
  if (value == "last")
  {
    return EdgeLinePickMode::Last;
  }

  if (value == "best")
  {
    return EdgeLinePickMode::Best;
  }

  return EdgeLinePickMode::First;
}

SurfaceTwoCirclesStrategyConfig toProcessorStrategy(const SurfaceLocalizationStrategyConfig& recipeConfig)
{
  SurfaceTwoCirclesStrategyConfig result;
  result.originFeatureId = recipeConfig.origin.toStdString();
  result.xAxisFromFeatureId = recipeConfig.xAxisFrom.toStdString();
  result.xAxisToFeatureId = recipeConfig.xAxisTo.toStdString();

  for (const SurfaceStrategyFeatureConfig& recipeFeature : recipeConfig.features)
  {
    SurfaceCircleFeatureConfig feature;
    feature.id = recipeFeature.id.toStdString();
    feature.polarity = recipeFeature.polarity.toStdString();
    feature.searchRoi = cv::Rect(
      recipeFeature.searchRoi.x(),
      recipeFeature.searchRoi.y(),
      recipeFeature.searchRoi.width(),
      recipeFeature.searchRoi.height());
    feature.threshold.minValue = recipeFeature.thresholdMin;
    feature.threshold.maxValue = recipeFeature.thresholdMax;
    feature.expectedRadiusMin = recipeFeature.expectedRadiusMin;
    feature.expectedRadiusMax = recipeFeature.expectedRadiusMax;
    result.features.push_back(feature);
  }

  return result;
}

}

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
{
  m_recipeManager.setRecipeId(RecipeManager::loadActiveRecipeId());

  QString error;
  if (!m_translations.loadLanguage("it", &error))
  {
    m_translations.loadLanguage("en");
  }

  buildUi();
  // Initialize async executor: set thread count based on hardware and enable metrics logging
  AsyncExecutor::setDefaultMaxThreadsToHardware();
  // Load persisted max threads if present
  {
    QSettings settings;
    const QVariant v = settings.value("system/maxThreads", QVariant());
    if (v.isValid())
    {
      const int saved = v.toInt();
      if (saved <= 0)
      {
        AsyncExecutor::setDefaultMaxThreadsToHardware();
      }
      else
      {
        AsyncExecutor::setMaxThreads(saved);
      }
    }
  }
  AsyncExecutor::setMetricsHandler([this](const QString& name, qint64 ms) {
    if (m_metricsPanel)
    {
      m_metricsPanel->addMetric(name, ms);
      return;
    }

    if (name.isEmpty())
    {
      appendLog(QString("metric: %1 ms").arg(ms));
    }
    else
    {
      appendLog(QString("metric %1: %2 ms").arg(name).arg(ms));
    }
  });
  // Initialize async executor: set thread count based on hardware and enable metrics logging
  AsyncExecutor::setDefaultMaxThreadsToHardware();
  AsyncExecutor::setMetricsHandler([this](const QString& name, qint64 ms) {
    if (name.isEmpty())
    {
      appendLog(QString("metric: %1 ms").arg(ms));
    }
    else
    {
      appendLog(QString("metric %1: %2 ms").arg(name).arg(ms));
    }
  });
  loadConfiguration();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
  QMainWindow::resizeEvent(event);
  updateLargePreview();
}

void MainWindow::buildUi()
{
  setWindowTitle("VisionSystemCPP");
  resize(1600, 900);
  buildMenu();
  if (!m_simulationTimer)
  {
    m_simulationTimer = new QTimer(this);
    connect(m_simulationTimer, &QTimer::timeout, this, [this]() {
      if (!m_selectedCameraId.isEmpty())
      {
        advanceCameraFrame(m_selectedCamera);
      }
    });
  }

  auto* root = new QWidget(this);
  auto* rootLayout = new QHBoxLayout(root);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  auto* splitter = new QSplitter(Qt::Horizontal, root);
  splitter->setChildrenCollapsible(false);

  m_imageStack = new QStackedWidget(splitter);
  m_gridPage = new QWidget(m_imageStack);

  auto* gridScrollArea = new QScrollArea(m_gridPage);
  gridScrollArea->setWidgetResizable(true);
  gridScrollArea->setFrameShape(QFrame::NoFrame);

  m_gridContent = new QWidget(gridScrollArea);
  m_gridLayout = new QGridLayout(m_gridContent);
  m_gridLayout->setContentsMargins(12, 12, 12, 12);
  m_gridLayout->setSpacing(10);
  m_gridLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

  for (int i = 0; i < 4; ++i)
  {
    m_gridLayout->setColumnStretch(i, 1);
    m_gridLayout->setRowStretch(i, 1);
  }

  gridScrollArea->setWidget(m_gridContent);
  auto* gridPageLayout = new QVBoxLayout(m_gridPage);
  gridPageLayout->setContentsMargins(0, 0, 0, 0);
  gridPageLayout->addWidget(gridScrollArea);
  m_imageStack->addWidget(m_gridPage);

  auto* largePage = new QWidget(m_imageStack);
  auto* largeLayout = new QVBoxLayout(largePage);
  largeLayout->setContentsMargins(16, 16, 16, 16);
  largeLayout->setSpacing(10);

  m_largeTitle = new QLabel(trText("labels.noCameraSelected"), largePage);
  m_largeTitle->setObjectName("largeTitle");
  m_largeImage = new ImageViewWidget(largePage);
  m_largeImage->setStyleSheet("background:#101418;color:#9aa4ad;");
  m_largeImage->setMinimumSize(320, 240);
  m_largeImage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_largeImage->setRoiChangedHandler([this](const QRect& roi) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    QString error;

    if (m_activeDrawingRecipe == ActiveDrawingRecipe::SurfaceDefects)
    {
      if (!m_recipeManager.saveSurfaceDefectRoi(m_selectedCameraId, roi, &error))
      {
        appendLog(error);
        return;
      }

      appendLog(QString("%1: %2 x=%3 y=%4 w=%5 h=%6")
                  .arg(trText("log.surfaceRoiSaved"))
                  .arg(m_selectedCameraId)
                  .arg(roi.x())
                  .arg(roi.y())
                  .arg(roi.width())
                  .arg(roi.height()));
      return;
    }

    if (m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry)
    {
      deactivateImageDrawingTools();
      return;
    }

    if (!m_recipeManager.saveLocalizationRoi(m_selectedCameraId, roi, &error))
    {
      appendLog(error);
      return;
    }

    appendLog(QString("%1: %2 x=%3 y=%4 w=%5 h=%6")
                .arg(trText("log.localizationRoiSaved"))
                .arg(m_selectedCameraId)
                .arg(roi.x())
                .arg(roi.y())
                .arg(roi.width())
                .arg(roi.height()));
  });
  m_largeImage->setExclusionRectAddedHandler([this](const QRect& rect) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    QString error;

    if (m_activeDrawingRecipe == ActiveDrawingRecipe::SurfaceDefects)
    {
      if (!m_recipeManager.addSurfaceDefectExclusionRect(m_selectedCameraId, rect, &error))
      {
        appendLog(error);
        return;
      }

      appendLog(QString("%1: %2 x=%3 y=%4 w=%5 h=%6")
                  .arg(trText("log.surfaceExclusionSaved"))
                  .arg(m_selectedCameraId)
                  .arg(rect.x())
                  .arg(rect.y())
                  .arg(rect.width())
                  .arg(rect.height()));
      return;
    }

    if (!m_recipeManager.addLocalizationExclusionRect(m_selectedCameraId, rect, &error))
    {
      appendLog(error);
      return;
    }

    appendLog(QString("%1: %2 x=%3 y=%4 w=%5 h=%6")
                .arg(trText("log.localizationExclusionSaved"))
                .arg(m_selectedCameraId)
                .arg(rect.x())
                .arg(rect.y())
                .arg(rect.width())
                .arg(rect.height()));
  });
  m_largeImage->setExclusionRectsChangedHandler([this](const QVector<QRect>& rects) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    QString error;

    if (m_activeDrawingRecipe == ActiveDrawingRecipe::SurfaceDefects)
    {
      if (!m_recipeManager.saveSurfaceDefectExclusionRects(m_selectedCameraId, rects, &error))
      {
        appendLog(error);
        return;
      }

      appendLog(QString("%1: %2 (%3)")
                  .arg(trText("log.surfaceExclusionsUpdated"))
                  .arg(m_selectedCameraId)
                  .arg(rects.size()));
      return;
    }

    if (!m_recipeManager.saveLocalizationExclusionRects(m_selectedCameraId, rects, &error))
    {
      appendLog(error);
      return;
    }

    appendLog(QString("%1: %2 (%3)")
                .arg(trText("log.localizationExclusionsUpdated"))
                .arg(m_selectedCameraId)
                .arg(rects.size()));
  });
  m_largeImage->setCircleChangedHandler([this](bool outerCircle, const ImageCircle& circle) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    QString error;
    if (m_surfaceCircleTarget == SurfaceCircleTarget::None)
    {
      const SurfaceAnnulusLocalizationConfig current = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      const int innerRadius = qMax(1, circle.radius - current.edgeBandInner);
      const int outerRadius = circle.radius + current.edgeBandOuter;

      if (!m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, true, circle.center, outerRadius, &error) ||
          !m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, false, circle.center, innerRadius, &error) ||
          !m_recipeManager.saveSurfaceEdgeCircle(m_selectedCameraId, circle.center, circle.radius, &error))
      {
        appendLog(error);
        return;
      }

      m_largeImage->setCircles({
        {circle.center, outerRadius},
        {circle.center, innerRadius}
      });

      appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                  .arg(trText("log.surfaceThreePointCircleSaved"))
                  .arg(m_selectedCameraId)
                  .arg(circle.center.x())
                  .arg(circle.center.y())
                  .arg(circle.radius));

      const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
          (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
      {
        testSurfaceAnnulusLocalization(m_selectedCamera);
      }
      return;
    }

    if (m_surfaceCircleTarget == SurfaceCircleTarget::Edge)
    {
      if (!m_recipeManager.saveSurfaceEdgeCircle(m_selectedCameraId, circle.center, circle.radius, &error))
      {
        appendLog(error);
        return;
      }

      const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      m_largeImage->setCircles({
        {circle.center, circle.radius + annulus.edgeBandOuter},
        {circle.center, qMax(1, circle.radius - annulus.edgeBandInner)}
      });

      appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                  .arg(trText("log.surfaceEdgeCircleSaved"))
                  .arg(m_selectedCameraId)
                  .arg(circle.center.x())
                  .arg(circle.center.y())
                  .arg(circle.radius));
      testSurfaceAnnulusLocalization(m_selectedCamera);
      return;
    }

    const bool targetOuter = m_surfaceCircleTarget == SurfaceCircleTarget::Inner ? false : outerCircle;

    if (!m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, targetOuter, circle.center, circle.radius, &error))
    {
      appendLog(error);
      return;
    }

    const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
    QVector<ImageCircle> circles;
    if (annulus.hasOuterCircle)
    {
      circles.append({annulus.center, annulus.outerRadius});
    }
    if (annulus.hasInnerCircle)
    {
      circles.append({annulus.center, annulus.innerRadius});
    }
    m_largeImage->setCircles(circles);

    appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                .arg(targetOuter ? trText("log.surfaceOuterCircleSaved") : trText("log.surfaceInnerCircleSaved"))
                .arg(m_selectedCameraId)
                .arg(circle.center.x())
                .arg(circle.center.y())
                .arg(circle.radius));
  });
  m_largeImage->setThreePointCircleHandler([this](const QVector<QPoint>& points) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    if (m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry && m_geometryDrawingTarget == GeometryDrawingTarget::Circle)
    {
      handleGeometryCirclePoints(m_selectedCamera, points);
      return;
    }

    ImageCircle circle;

    if (!circleFromThreePoints(points, circle))
    {
      appendLog(trText("log.surfaceEdgeCircleInvalid") + ": " + m_selectedCameraId);
      return;
    }

    QString error;

    if (m_surfaceCircleTarget == SurfaceCircleTarget::None)
    {
      const SurfaceAnnulusLocalizationConfig current = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      const int innerRadius = qMax(1, circle.radius - current.edgeBandInner);
      const int outerRadius = circle.radius + current.edgeBandOuter;

      if (!m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, true, circle.center, outerRadius, &error) ||
          !m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, false, circle.center, innerRadius, &error) ||
          !m_recipeManager.saveSurfaceEdgeCircle(m_selectedCameraId, circle.center, circle.radius, &error))
      {
        appendLog(error);
        return;
      }

      m_largeImage->setCircles({
        {circle.center, outerRadius},
        {circle.center, innerRadius}
      });

      appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                  .arg(trText("log.surfaceThreePointCircleSaved"))
                  .arg(m_selectedCameraId)
                  .arg(circle.center.x())
                  .arg(circle.center.y())
                  .arg(circle.radius));

      const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
          (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
      {
        testSurfaceAnnulusLocalization(m_selectedCamera);
      }
      return;
    }

    if (m_surfaceCircleTarget == SurfaceCircleTarget::Edge)
    {
      if (!m_recipeManager.saveSurfaceEdgeCircle(m_selectedCameraId, circle.center, circle.radius, &error))
      {
        appendLog(error);
        return;
      }

      const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      m_largeImage->setCircles({
        {circle.center, circle.radius + annulus.edgeBandOuter},
        {circle.center, qMax(1, circle.radius - annulus.edgeBandInner)}
      });

      appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                  .arg(trText("log.surfaceEdgeCircleSaved"))
                  .arg(m_selectedCameraId)
                  .arg(circle.center.x())
                  .arg(circle.center.y())
                  .arg(circle.radius));
      testSurfaceAnnulusLocalization(m_selectedCamera);
      return;
    }

    const bool targetOuter = m_surfaceCircleTarget != SurfaceCircleTarget::Inner;

    if (!m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, targetOuter, circle.center, circle.radius, &error))
    {
      appendLog(error);
      return;
    }

    const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
    QVector<ImageCircle> circles;
    if (annulus.hasOuterCircle)
    {
      circles.append({annulus.center, annulus.outerRadius});
    }
    if (annulus.hasInnerCircle)
    {
      circles.append({annulus.center, annulus.innerRadius});
    }
    m_largeImage->setCircles(circles);

    appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                .arg(targetOuter ? trText("log.surfaceOuterCircleSaved") : trText("log.surfaceInnerCircleSaved"))
                .arg(m_selectedCameraId)
                .arg(circle.center.x())
                .arg(circle.center.y())
                .arg(circle.radius));
  });
  m_largeImage->setTwoPointLineHandler([this](const QVector<QPoint>& points) {
    if (m_selectedCameraId.isEmpty() ||
        m_activeDrawingRecipe != ActiveDrawingRecipe::Geometry ||
        m_geometryDrawingTarget != GeometryDrawingTarget::Line ||
        points.size() < 2)
    {
      return;
    }

    const PartPose& pose = m_cameraRuntime[m_selectedCameraId].currentPose();
    if (!pose.valid)
    {
      appendLog(trText("log.partPoseMissing") + ": " + m_selectedCameraId);
      deactivateImageDrawingTools();
      return;
    }

    GeometryLineRuntimeConfig& config = activeGeometryLineConfig(m_selectedCameraId);
    config.partStart = imageToPart(pose, cv::Point2d(points[0].x(), points[0].y()));
    config.partEnd = imageToPart(pose, cv::Point2d(points[1].x(), points[1].y()));
    config.hasLine = true;

    const cv::Point2d imageStart(points[0].x(), points[0].y());
    const cv::Point2d imageEnd(points[1].x(), points[1].y());
    const cv::Point2d center = (imageStart + imageEnd) * 0.5;
    const double length = std::hypot(imageEnd.x - imageStart.x, imageEnd.y - imageStart.y);
    const double angle = std::atan2(imageEnd.y - imageStart.y, imageEnd.x - imageStart.x) * 180.0 / CV_PI;
    m_largeImage->setGeometryArea({
      QPointF(center.x, center.y),
      QSizeF(length, config.bandHalfWidth * 2.0),
      angle
    });
    m_largeImage->setGeometryPoints({
      QPointF(imageStart.x, imageStart.y),
      QPointF(imageEnd.x, imageEnd.y)
    });
    m_largeImage->setGeometryLines({
      {QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y)}
    });
    m_geometryDrawingTarget = GeometryDrawingTarget::Line;
    m_largeImage->setGeometryAreaEditingEnabled(true);

    testGeometryLine(m_selectedCamera);
  });
  m_largeImage->setGeometryPointPickedHandler([this](const QPointF& imagePoint) {
    if (m_selectedCameraId.isEmpty() ||
        m_activeDrawingRecipe != ActiveDrawingRecipe::Geometry)
    {
      return;
    }

    if (m_geometryDrawingTarget == GeometryDrawingTarget::Line)
    {
      handleGeometryLinePoint(m_selectedCamera, imagePoint);
    }
    else if (m_geometryDrawingTarget == GeometryDrawingTarget::Point)
    {
      handleGeometryPointGuidePoint(m_selectedCamera, imagePoint);
    }
  });
  m_largeImage->setGeometryPointMovedHandler([this](const QPointF& imagePoint) {
    if (m_selectedCameraId.isEmpty() ||
        m_activeDrawingRecipe != ActiveDrawingRecipe::Geometry)
    {
      return;
    }

    if (m_geometryDrawingTarget == GeometryDrawingTarget::Line)
    {
      LineGeometryMouseController& controller = m_lineGeometryMouseControllers[m_selectedCameraId];
      controller.handleMove(imagePoint);
      updateGeometryLineOverlay(m_selectedCamera);
    }
    else if (m_geometryDrawingTarget == GeometryDrawingTarget::Point)
    {
      LineGeometryMouseController& controller = m_pointGeometryMouseControllers[m_selectedCameraId];
      controller.handleMove(imagePoint);
      updateGeometryPointOverlay(m_selectedCamera);
    }
  });
  m_largeImage->setGeometryOverlayPointMovedHandler([this](int pointIndex, const QPointF& imagePoint) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    if (m_geometryDrawingTarget == GeometryDrawingTarget::Line)
    {
      handleGeometryLineHandleMoved(m_selectedCamera, pointIndex, imagePoint);
    }
    else if (m_geometryDrawingTarget == GeometryDrawingTarget::Point)
    {
      handleGeometryPointHandleMoved(m_selectedCamera, pointIndex, imagePoint);
    }
  });
  m_largeImage->setGeometryAreaChangedHandler([this](const ImageRotatedRect& area) {
    if (m_selectedCameraId.isEmpty() || m_geometryDrawingTarget != GeometryDrawingTarget::Line)
    {
      return;
    }

    const PartPose& pose = m_cameraRuntime[m_selectedCameraId].currentPose();
    if (!pose.valid)
    {
      return;
    }

    const double angle = area.angleDegrees * CV_PI / 180.0;
    const cv::Point2d axis(std::cos(angle), std::sin(angle));
    const cv::Point2d center(area.center.x(), area.center.y());
    GeometryLineRuntimeConfig& config = activeGeometryLineConfig(m_selectedCameraId);
    config.partStart = imageToPart(pose, center - axis * (area.size.width() * 0.5));
    config.partEnd = imageToPart(pose, center + axis * (area.size.width() * 0.5));
    config.bandHalfWidth = std::max(2, static_cast<int>(std::round(area.size.height() * 0.5)));
    config.hasLine = true;
    testGeometryLine(m_selectedCamera);
    showGeometryLinePanel(m_selectedCamera);
  });

  largeLayout->addWidget(m_largeTitle);
  largeLayout->addWidget(m_largeImage, 1);
  m_imageStack->addWidget(largePage);

  auto* panelScrollArea = new QScrollArea(splitter);
  panelScrollArea->setWidgetResizable(true);
  panelScrollArea->setFrameShape(QFrame::NoFrame);
  panelScrollArea->setMinimumWidth(390);
  panelScrollArea->setMaximumWidth(540);

  auto* panel = new QWidget(panelScrollArea);
  panel->setMinimumWidth(390);
  auto* panelLayout = new QVBoxLayout(panel);
  panelLayout->setContentsMargins(10, 10, 10, 10);
  panelLayout->setSpacing(8);

  m_systemStatus = new QLabel(trText("status.systemReady"), panel);
  m_systemStatus->setObjectName("panelStatus");
  panelLayout->addWidget(m_systemStatus);

  m_metricsPanel = new MetricsPanelWidget(panel);
  m_metricsPanel->setFixedHeight(120);
  panelLayout->addWidget(m_metricsPanel);

  auto* cameraBox = new QGroupBox(trText("groups.selectedCamera"), panel);
  auto* cameraLayout = new QVBoxLayout(cameraBox);
  m_cameraDetails = new QLabel(trText("labels.selectThumbnail"), cameraBox);
  m_cameraDetails->setWordWrap(true);
  cameraLayout->addWidget(m_cameraDetails);
  panelLayout->addWidget(cameraBox);

  auto* toolsBox = new QGroupBox(trText("groups.cameraTools"), panel);
  m_toolsLayout = new QVBoxLayout(toolsBox);
  m_toolsContainer = toolsBox;
  panelLayout->addWidget(toolsBox, 1);
  panelScrollArea->setWidget(panel);

  splitter->addWidget(m_imageStack);
  splitter->addWidget(panelScrollArea);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);

  rootLayout->addWidget(splitter);
  setCentralWidget(root);

  setStyleSheet(
    "QMainWindow,QWidget{background:#0f1419;color:#eef2f6;font-family:'Segoe UI';font-size:13px;}"
    "QGroupBox{border:1px solid #313b46;border-radius:6px;margin-top:8px;padding:8px 7px 7px 7px;font-weight:600;}"
    "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;color:#cdd6df;}"
    "QPushButton{background:#24313d;border:1px solid #3b4652;border-radius:5px;padding:6px;color:#f5f7fa;}"
    "QPushButton:hover{background:#2e3d4b;}"
    "QTextEdit{background:#111820;border:1px solid #313b46;border-radius:5px;color:#d7dee6;}"
    "#largeTitle{font-size:20px;font-weight:700;color:#f4f7fb;}"
    "#panelStatus{font-size:17px;font-weight:700;color:#f4f7fb;}"
    "#toolPanelTitle{font-size:15px;font-weight:700;color:#f4f7fb;}"
    "#toolPanelNote{color:#9aa4ad;}"
  );
}

void MainWindow::buildMenu()
{
  menuBar()->clear();

  QMenu* recipesMenu = menuBar()->addMenu(trText("menu.recipes"));
  recipesMenu->addAction(trText("menu.selectRecipe"), this, [this]() { selectRecipe(); });
  recipesMenu->addAction(trText("menu.newRecipe"), this, [this]() { createRecipe(); });
  recipesMenu->addAction(trText("menu.duplicateRecipe"), this, [this]() { duplicateRecipe(); });
  recipesMenu->addAction(trText("menu.importRecipe"), this, [this]() { importRecipe(); });
  recipesMenu->addAction(trText("menu.exportRecipe"), this, [this]() { exportRecipe(); });

  QMenu* camerasMenu = menuBar()->addMenu(trText("menu.cameras"));
  for (const CameraConfig& camera : m_config.activeCameras())
  {
    camerasMenu->addAction(QString("%1 | %2").arg(camera.id, camera.displayName), this, [this, camera]() {
      configureCameraSource(camera);
    });
  }

  QMenu* configMenu = menuBar()->addMenu(trText("menu.configurations"));
  configMenu->addAction(trText("menu.cameras"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.cameras"));
  });
  configMenu->addAction(trText("menu.paths"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.paths"));
  });
  configMenu->addAction(trText("menu.diagnostics"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.diagnostics"));
  });

  QMenu* languageMenu = menuBar()->addMenu(trText("menu.language"));
  languageMenu->addAction("Italiano", this, [this]() { changeLanguage("it"); });
  languageMenu->addAction("English", this, [this]() { changeLanguage("en"); });

  QMenu* systemMenu = menuBar()->addMenu(trText("menu.system"));
  systemMenu->addAction(trText("commands.start"), this, [this]() {
    appendLog(trText("log.command") + ": " + trText("commands.start"));
  });
  systemMenu->addAction(trText("commands.stop"), this, [this]() {
    appendLog(trText("log.command") + ": " + trText("commands.stop"));
  });
  systemMenu->addAction(trText("commands.gridView"), this, [this]() { showGridView(); });
  systemMenu->addAction(trText("commands.reloadConfig"), this, [this]() { loadConfiguration(); });
  systemMenu->addAction(trText("commands.toggleFullScreen"), this, [this]() { toggleFullScreen(); });
  systemMenu->addAction(trText("commands.setMaxThreads"), this, [this]() { setThreadLimitPrompt(); });
  systemMenu->addSeparator();
  systemMenu->addAction(trText("commands.exit"), qApp, &QApplication::quit);
}

void MainWindow::setThreadLimitPrompt()
{
  bool ok = false;
  const int current = QThreadPool::globalInstance()->maxThreadCount();
  const unsigned int hw = std::thread::hardware_concurrency();
  const int hwCount = hw == 0 ? 1 : static_cast<int>(hw);
  const int value = QInputDialog::getInt(
    this,
    trText("commands.setMaxThreads"),
    trText("labels.maxThreads"),
    current,
    0,
    1024,
    1,
    &ok);

  if (!ok)
  {
    return;
  }

  if (value <= 0)
  {
    AsyncExecutor::setDefaultMaxThreadsToHardware();
    appendLog(QString("%1: %2").arg(trText("log.threadLimitSet"), QString("auto=%1").arg(hwCount)));
    QSettings settings;
    settings.setValue("system/maxThreads", 0);
    return;
  }

  AsyncExecutor::setMaxThreads(value);
  appendLog(QString("%1: %2").arg(trText("log.threadLimitSet"), QString::number(value)));
  QSettings settings;
  settings.setValue("system/maxThreads", value);
}

void MainWindow::incPendingJobs(const QString& cameraId)
{
  const int v = m_cameraPendingJobs.value(cameraId, 0) + 1;
  m_cameraPendingJobs[cameraId] = v;
  m_cameraProcessingBusy[cameraId] = true;
  if (m_metricsPanel)
  {
    m_metricsPanel->addMetric(QString("pendingJobs_%1").arg(cameraId), v);
  }
}

void MainWindow::decPendingJobs(const QString& cameraId)
{
  int v = m_cameraPendingJobs.value(cameraId, 0) - 1;
  if (v < 0)
  {
    v = 0;
  }
  m_cameraPendingJobs[cameraId] = v;
  m_cameraProcessingBusy[cameraId] = (v > 0);
  if (m_metricsPanel)
  {
    m_metricsPanel->addMetric(QString("pendingJobs_%1").arg(cameraId), v);
  }
}

void MainWindow::rebuildUi()
{
  QWidget* oldCentralWidget = centralWidget();
  setCentralWidget(nullptr);

  if (oldCentralWidget)
  {
    oldCentralWidget->deleteLater();
  }

  m_tiles.clear();
  m_selectedCameraId.clear();
  m_selectedCamera = {};
  m_selectedPreview = {};
  m_selectedImagePath.clear();
  buildUi();
  loadConfiguration();
}

void MainWindow::changeLanguage(const QString& languageCode)
{
  QString error;
  if (!m_translations.loadLanguage(languageCode, &error))
  {
    appendLog(error);
    return;
  }

  rebuildUi();
  appendLog(trText("log.languageChanged") + ": " + languageCode);
}

void MainWindow::toggleFullScreen()
{
  if (isFullScreen())
  {
    showMaximized();
    appendLog(trText("log.windowMode") + ": " + trText("commands.maximized"));
  }
  else
  {
    showFullScreen();
    appendLog(trText("log.windowMode") + ": " + trText("commands.fullScreen"));
  }

  updateLargePreview();
}

void MainWindow::selectRecipe()
{
  const QStringList recipes = RecipeManager::availableRecipes();

  if (recipes.isEmpty())
  {
    QMessageBox::information(this, trText("menu.recipes"), trText("messages.noRecipes"));
    return;
  }

  bool ok = false;
  const QString selectedRecipe = QInputDialog::getItem(
    this,
    trText("menu.selectRecipe"),
    trText("labels.recipe"),
    recipes,
    qMax(0, recipes.indexOf(m_recipeManager.recipeId())),
    false,
    &ok);

  if (!ok || selectedRecipe.isEmpty())
  {
    return;
  }

  setActiveRecipe(selectedRecipe);
}

void MainWindow::createRecipe()
{
  bool ok = false;
  const QString recipeName = QInputDialog::getText(
    this,
    trText("menu.newRecipe"),
    trText("labels.recipeName"),
    QLineEdit::Normal,
    "",
    &ok);

  if (!ok)
  {
    return;
  }

  const QString recipeId = RecipeManager::normalizeRecipeId(recipeName);
  QString error;

  if (!RecipeManager::createRecipe(recipeId, &error))
  {
    QMessageBox::warning(this, trText("menu.newRecipe"), error);
    return;
  }

  setActiveRecipe(recipeId);
}

void MainWindow::duplicateRecipe()
{
  bool ok = false;
  const QString recipeName = QInputDialog::getText(
    this,
    trText("menu.duplicateRecipe"),
    trText("labels.recipeName"),
    QLineEdit::Normal,
    m_recipeManager.recipeId() + "_copy",
    &ok);

  if (!ok)
  {
    return;
  }

  const QString recipeId = RecipeManager::normalizeRecipeId(recipeName);
  QString error;

  if (!RecipeManager::duplicateRecipe(m_recipeManager.recipeId(), recipeId, &error))
  {
    QMessageBox::warning(this, trText("menu.duplicateRecipe"), error);
    return;
  }

  setActiveRecipe(recipeId);
}

void MainWindow::importRecipe()
{
  const QString sourceDirectory = QFileDialog::getExistingDirectory(
    this,
    trText("menu.importRecipe"),
    RecipeManager::recipesRootPath());

  if (sourceDirectory.isEmpty())
  {
    return;
  }

  QString importedRecipeId;
  QString error;

  if (!RecipeManager::importRecipeDirectory(sourceDirectory, &importedRecipeId, &error))
  {
    QMessageBox::warning(this, trText("menu.importRecipe"), error);
    return;
  }

  setActiveRecipe(importedRecipeId);
}

void MainWindow::exportRecipe()
{
  const QString destinationDirectory = QFileDialog::getExistingDirectory(
    this,
    trText("menu.exportRecipe"),
    RecipeManager::recipesRootPath());

  if (destinationDirectory.isEmpty())
  {
    return;
  }

  QString error;

  if (!RecipeManager::exportRecipeDirectory(m_recipeManager.recipeId(), destinationDirectory, &error))
  {
    QMessageBox::warning(this, trText("menu.exportRecipe"), error);
    return;
  }

  appendLog(trText("log.recipeExported") + ": " + m_recipeManager.recipeId());
}

void MainWindow::configureCameraSource(const CameraConfig& camera)
{
  const QString currentFolder = camera.folder.isEmpty()
    ? projectPath("data/images")
    : resolvedCameraFolder(camera);
  const QString selectedFolder = QFileDialog::getExistingDirectory(
    this,
    QString("%1 | %2").arg(trText("menu.cameras"), camera.id),
    currentFolder);

  if (selectedFolder.isEmpty())
  {
    return;
  }

  const QDir projectDirectory(QString::fromUtf8(PROJECT_SOURCE_DIR));
  QString storedFolder = projectDirectory.relativeFilePath(selectedFolder);

  if (storedFolder.startsWith(".."))
  {
    storedFolder = QDir::toNativeSeparators(selectedFolder);
  }

  QString error;
  const QString configPath = projectPath("config/cameras.json");
  if (!m_config.saveCameraSource(configPath, camera.id, "file", storedFolder, &error))
  {
    QMessageBox::warning(this, trText("menu.cameras"), error);
    return;
  }

  m_cameraRuntime.erase(camera.id);
  loadConfiguration();
  appendLog(QString("%1: %2 -> %3").arg(trText("log.cameraSourceSaved"), camera.id, storedFolder));
}

void MainWindow::configureCameraSampleImage(const CameraConfig& camera)
{
  const QString currentFolder = camera.folder.isEmpty()
    ? RecipeManager::recipesRootPath()
    : resolvedCameraFolder(camera);
  const QString selectedFile = QFileDialog::getOpenFileName(
    this,
    QString("%1 | %2").arg(trText("actions.assignSampleImage"), camera.id),
    currentFolder,
    "Images (*.bmp *.jpg *.jpeg *.png *.tif *.tiff)");

  if (selectedFile.isEmpty())
  {
    return;
  }

  QString error;
  if (!m_recipeManager.importCameraSampleImage(camera.id, selectedFile, &error))
  {
    QMessageBox::warning(this, trText("actions.assignSampleImage"), error);
    return;
  }

  if (camera.id == m_selectedCameraId)
  {
    m_selectedImagePath = cameraSampleImagePath(camera);
    m_cameraRuntime[camera.id].stop();
    m_selectedPreview = QPixmap(m_selectedImagePath);
    m_largeImage->setImage(m_selectedPreview);
    showCameraSetupPanel(camera);
  }

  appendLog(QString("%1: %2").arg(trText("log.cameraSampleSaved"), camera.id));
}

void MainWindow::configureCameraTestImages(const CameraConfig& camera)
{
  const QString currentFolder = camera.folder.isEmpty()
    ? RecipeManager::recipesRootPath()
    : resolvedCameraFolder(camera);
  const QString selectedFolder = QFileDialog::getExistingDirectory(
    this,
    QString("%1 | %2").arg(trText("actions.assignTestImages"), camera.id),
    currentFolder);

  if (selectedFolder.isEmpty())
  {
    return;
  }

  QString error;
  if (!m_recipeManager.importCameraTestImages(camera.id, selectedFolder, &error))
  {
    QMessageBox::warning(this, trText("actions.assignTestImages"), error);
    return;
  }

  if (camera.id == m_selectedCameraId)
  {
    m_cameraRuntime[camera.id].stop();
    showCameraSetupPanel(camera);
  }

  appendLog(QString("%1: %2").arg(trText("log.cameraTestImagesSaved"), camera.id));
}

void MainWindow::acquireCameraSampleImage(const CameraConfig& camera)
{
  cv::Mat sample;
  const auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt != m_cameraRuntime.end() && !runtimeIt->second.currentFrame().empty())
  {
    sample = runtimeIt->second.currentFrame().clone();
  }
  else
  {
    QString imageError;
    sample = currentInputImage(camera, &imageError);
    if (sample.empty())
    {
      appendLog(imageError);
      return;
    }
  }

  QString error;
  if (!m_recipeManager.ensureCameraImageFolders(camera.id, &error))
  {
    QMessageBox::warning(this, trText("actions.acquireSampleImage"), error);
    return;
  }

  const QString samplePath = QDir(m_recipeManager.cameraSampleImagesPath(camera.id)).filePath("sample.png");
  if (!cv::imwrite(samplePath.toStdString(), sample))
  {
    QMessageBox::warning(
      this,
      trText("actions.acquireSampleImage"),
      trText("log.cameraSampleAcquireFailed") + ": " + samplePath);
    return;
  }

  if (camera.id == m_selectedCameraId)
  {
    m_selectedImagePath = samplePath;
    m_selectedPreview = matToPixmap(sample);
    m_largeImage->setImage(m_selectedPreview);
    showCameraSetupPanel(camera);
  }

  appendLog(QString("%1: %2").arg(trText("log.cameraSampleAcquired"), camera.id));
}

void MainWindow::ensureRecipeCameraFolders()
{
  for (const CameraConfig& camera : m_config.activeCameras())
  {
    QString error;
    if (!m_recipeManager.ensureCameraImageFolders(camera.id, &error))
    {
      appendLog(error);
    }
  }
}

void MainWindow::setActiveRecipe(const QString& recipeId)
{
  m_recipeManager.setRecipeId(recipeId);
  m_geometryLineConfigs.clear();
  m_activeGeometryLineIndexes.clear();
  m_geometryPointConfigs.clear();
  m_activeGeometryPointIndexes.clear();
  m_geometryCircleConfigs.clear();
  m_activeGeometryCircleIndexes.clear();
  QString error;

  if (!RecipeManager::saveActiveRecipeId(m_recipeManager.recipeId(), &error))
  {
    appendLog(error);
  }

  appendLog(trText("log.activeRecipe") + ": " + m_recipeManager.recipeId());
  ensureRecipeCameraFolders();
  refreshSelectedCameraRecipeData();
}

void MainWindow::refreshSelectedCameraRecipeData()
{
  if (m_selectedCameraId.isEmpty() || !m_largeImage)
  {
    return;
  }

  QRect roi;

  if (isGrayscaleLocalizationCamera(m_selectedCamera))
  {
    if (m_recipeManager.loadSurfaceDefectRoi(m_selectedCameraId, roi))
    {
      m_largeImage->setRoi(roi);
    }
    else
    {
      m_largeImage->clearRoi();
    }

    m_largeImage->setExclusionRects(m_recipeManager.loadSurfaceDefectExclusionRects(m_selectedCameraId));
    m_largeImage->clearCircles();
    GeometryOverlay overlay;
    appendCurrentPartPoseOverlay(m_selectedCamera, overlay);
    m_largeImage->setGeometryOverlay(overlay);
    return;
  }
  if (m_recipeManager.loadLocalizationRoi(m_selectedCameraId, roi))
  {
    m_largeImage->setRoi(roi);
  }
  else
  {
    m_largeImage->clearRoi();
  }

  m_largeImage->setExclusionRects(m_recipeManager.loadLocalizationExclusionRects(m_selectedCameraId));
}

void MainWindow::loadConfiguration()
{
  QString error;
  const QString configPath = projectPath("config/cameras.json");

  if (!m_config.load(configPath, &error))
  {
    m_systemStatus->setText(trText("status.configError"));
    appendLog(error);
    return;
  }

  buildMenu();

  while (QLayoutItem* item = m_gridLayout->takeAt(0))
  {
    if (QWidget* widget = item->widget())
    {
      widget->deleteLater();
    }

    delete item;
  }

  m_tiles.clear();
  const QVector<CameraConfig> cameras = m_config.activeCameras();
  ensureRecipeCameraFolders();

  for (int i = 0; i < cameras.size(); ++i)
  {
    auto* tile = new CameraTileWidget(cameras[i], m_gridContent);
    tile->setPreview(loadCameraPreview(cameras[i]));
    tile->setClickHandler([this](const CameraConfig& camera) { selectCamera(camera); });
    m_gridLayout->addWidget(tile, i / 4, i % 4);
    m_tiles.append(tile);
  }

  m_systemStatus->setText(QString("%1 | %2 %3")
                            .arg(trText("status.systemReady"))
                            .arg(cameras.size())
                            .arg(trText("status.activeCameras")));
  appendLog(QString("%1: %2 %3 %4 %5")
              .arg(trText("log.configLoaded"))
              .arg(cameras.size())
              .arg(trText("status.activeCameras"))
              .arg(trText("labels.max"))
              .arg(m_config.maxCameras()));

  if (!cameras.isEmpty())
  {
    updateControlPanel(nullptr);
  }

  showGridView();
}

void MainWindow::showGridView()
{
  deactivateImageDrawingTools();
  m_imageStack->setCurrentIndex(0);
  m_selectedCameraId.clear();
  m_selectedCamera = {};
  m_selectedPreview = {};
  m_selectedImagePath.clear();

  for (CameraTileWidget* tile : m_tiles)
  {
    tile->setSelected(false);
  }

  updateControlPanel(nullptr);
  appendLog(trText("log.gridView"));
}

void MainWindow::selectCamera(const CameraConfig& camera)
{
  m_selectedCameraId = camera.id;
  m_selectedCamera = camera;
  m_selectedImagePath = cameraSampleImagePath(camera);
  deactivateImageDrawingTools();
  m_largeImage->clearRoi();
  m_largeImage->clearExclusionRects();
  m_largeImage->clearCircles();
  m_largeImage->clearGeometryOverlay();

  for (CameraTileWidget* tile : m_tiles)
  {
    tile->setSelected(tile->camera().id == camera.id);
  }

  auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt != m_cameraRuntime.end() && !runtimeIt->second.currentFrame().empty())
  {
    m_selectedPreview = matToPixmap(runtimeIt->second.currentFrame());
  }
  else
  {
    m_selectedPreview = m_selectedImagePath.isEmpty() ? QPixmap() : QPixmap(m_selectedImagePath);
  }
  m_largeTitle->setText(camera.displayName + " | " + camera.id);

  if (m_selectedPreview.isNull())
  {
    m_largeImage->clearImage();
  }
  else
  {
    m_largeImage->setImage(m_selectedPreview);
    updateLargePreview();
  }

  QRect savedRoi;
  if (isGrayscaleLocalizationCamera(camera))
  {
    if (m_recipeManager.loadSurfaceDefectRoi(camera.id, savedRoi))
    {
      m_largeImage->setRoi(savedRoi);
    }

    m_largeImage->setExclusionRects(m_recipeManager.loadSurfaceDefectExclusionRects(camera.id));
    m_largeImage->clearCircles();
    GeometryOverlay overlay;
    appendCurrentPartPoseOverlay(camera, overlay);
    m_largeImage->setGeometryOverlay(overlay);
  }
  else if (m_recipeManager.loadLocalizationRoi(camera.id, savedRoi))
  {
    m_largeImage->setRoi(savedRoi);
    m_largeImage->setExclusionRects(m_recipeManager.loadLocalizationExclusionRects(camera.id));
  }

  m_imageStack->setCurrentIndex(1);
  QApplication::processEvents();
  updateLargePreview();
  updateControlPanel(&camera);
  appendLog(trText("log.cameraSelected") + ": " + camera.id);
}

void MainWindow::updateControlPanel(const CameraConfig* camera)
{
  clearToolPanel();

  if (!camera)
  {
    m_cameraDetails->setText(trText("labels.selectThumbnail"));

    auto* commands = new QGroupBox(trText("groups.generalCommands"), m_toolsContainer);
    auto* commandsLayout = new QGridLayout(commands);
    const QVector<QPair<QString, QString>> commandsList = {
      {"start", "commands.start"},
      {"stop", "commands.stop"},
      {"resetErrors", "commands.resetErrors"},
      {"reloadConfig", "commands.reloadConfig"}
    };

    for (int i = 0; i < commandsList.size(); ++i)
    {
      const QString commandId = commandsList[i].first;
      const QString commandLabel = trText(commandsList[i].second);
      auto* button = new QPushButton(commandLabel, commands);
      button->setMinimumHeight(38);
      commandsLayout->addWidget(button, i / 2, i % 2);

      if (commandId == "gridView")
      {
        connect(button, &QPushButton::clicked, this, [this]() { showGridView(); });
      }
      else if (commandId == "reloadConfig")
      {
        connect(button, &QPushButton::clicked, this, [this]() { loadConfiguration(); });
      }
      else
      {
        connect(button, &QPushButton::clicked, this, [this, commandLabel]() {
          appendLog(trText("log.command") + ": " + commandLabel);
        });
      }
    }

    m_toolsLayout->addWidget(commands);
    m_toolsLayout->addStretch(1);
    return;
  }

  m_cameraDetails->setText(QString("%1 %2 | %3")
    .arg(trText("labels.camera"))
    .arg(camera->slot)
    .arg(camera->displayName));

  showCameraToolList(*camera);
}

void MainWindow::deactivateImageDrawingTools()
{
  if (!m_largeImage)
  {
    return;
  }

  m_largeImage->setRoiDrawingEnabled(false);
  m_largeImage->setExclusionDrawingEnabled(false);
  m_largeImage->setOuterCircleDrawingEnabled(false);
  m_largeImage->setInnerCircleDrawingEnabled(false);
  m_largeImage->setThreePointCircleDrawingEnabled(false);
  m_largeImage->setGeometryPointPickingEnabled(false);
  m_largeImage->setGeometryOverlayPointEditingEnabled(false);
  m_largeImage->setTwoPointLineDrawingEnabled(false);
  m_activeDrawingRecipe = ActiveDrawingRecipe::None;
  m_surfaceCircleTarget = SurfaceCircleTarget::None;
  m_geometryDrawingTarget = GeometryDrawingTarget::None;
}

void MainWindow::showCameraToolList(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();

  auto* gridButton = new QPushButton(trText("commands.gridView"), m_toolsContainer);
  gridButton->setMinimumHeight(38);
  connect(gridButton, &QPushButton::clicked, this, [this]() { showGridView(); });
  m_toolsLayout->addWidget(gridButton);

  auto* setupButton = new QPushButton(trText("tools.setup"), m_toolsContainer);
  setupButton->setMinimumHeight(38);
  connect(setupButton, &QPushButton::clicked, this, [this, camera]() { showCameraSetupPanel(camera); });
  m_toolsLayout->addWidget(setupButton);

  if (isGrayscaleLocalizationCamera(camera))
  {
    auto* localizationButton = new QPushButton(trText("tools.localization"), m_toolsContainer);
    localizationButton->setMinimumHeight(40);
    connect(localizationButton, &QPushButton::clicked, this, [this, camera]() {
      showLocalizationStrategyList(camera);
    });
    m_toolsLayout->addWidget(localizationButton);
  }

  for (const QString& tool : camera.profile.guiTools)
  {
    if (isGrayscaleLocalizationCamera(camera) && tool == "surfaceLocalization")
    {
      continue;
    }

    auto* button = new QPushButton(ToolCatalog::label(tool, m_translations), m_toolsContainer);
    connect(button, &QPushButton::clicked, this, [this, camera, tool]() {
      showToolPanel(camera, tool);
    });
    m_toolsLayout->addWidget(button);
  }

  m_toolsLayout->addStretch(1);
}

void MainWindow::showLocalizationStrategyList(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(trText("tools.localization") + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* strategyTitle = new QLabel(trText("groups.localizationStrategies"), panel);
  strategyTitle->setObjectName("toolPanelNote");
  strategyTitle->setWordWrap(true);
  layout->addWidget(strategyTitle);

  for (const SurfaceLocalizationStrategyDefinition& strategy : SurfaceLocalizationStrategies::all(m_translations))
  {
    auto* button = new QPushButton(strategy.label, panel);
    button->setMinimumHeight(40);
    connect(button, &QPushButton::clicked, this, [this, camera, strategy]() {
      showSurfaceLocalizationStrategyPanel(camera, strategy.id);
    });
    layout->addWidget(button);
  }

  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);
  connect(backButton, &QPushButton::clicked, this, [this, camera]() {
    showCameraToolList(camera);
    appendLog(trText("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + trText("tools.localization"));
}

bool MainWindow::isBwDimensionalCamera(const CameraConfig& camera) const
{
  return camera.profile.imageMode == "bw" &&
    camera.profile.inspectionTypes.contains("dimensional");
}

bool MainWindow::isGrayscaleLocalizationCamera(const CameraConfig& camera) const
{
  const bool hasAi = camera.profile.inspectionTypes.contains("ai");
  const bool hasGrayscaleWork = camera.profile.inspectionTypes.contains("measurement") ||
    camera.profile.inspectionTypes.contains("surface");

  return camera.profile.imageMode == "grayscale" && (!hasAi || hasGrayscaleWork);
}

void MainWindow::clearToolPanel()
{
  m_setupPanel = nullptr;
  m_setupCameraId.clear();
  while (QLayoutItem* item = m_toolsLayout->takeAt(0))
  {
    if (QWidget* widget = item->widget())
    {
      widget->deleteLater();
    }

    delete item;
  }
}

void MainWindow::appendLog(const QString& message)
{
  if (!m_log)
  {
    return;
  }

  const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
  m_log->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::updateLargePreview()
{
  if (!m_largeImage || m_selectedPreview.isNull())
  {
    return;
  }

  const QSize targetSize = m_largeImage->contentsRect().size();

  if (targetSize.isEmpty())
  {
    return;
  }

  m_largeImage->setImage(m_selectedPreview);
}

QString MainWindow::trText(const QString& key) const
{
  return m_translations.text(key);
}

QString MainWindow::runtimeStatusText(CameraRuntime::Status status) const
{
  switch (status)
  {
  case CameraRuntime::Status::Ready:
    return trText("status.ready");
  case CameraRuntime::Status::Running:
    return trText("status.running");
  case CameraRuntime::Status::Error:
    return trText("status.error");
  case CameraRuntime::Status::Stopped:
  default:
    return trText("status.stopped");
  }
}

PartPose MainWindow::partPoseFromLocalizationResult(const CameraConfig& camera, const LocalizationResult& result) const
{
  if (!result.found)
  {
    return makeInvalidPartPose(camera.id);
  }

  PartPose pose;
  pose.valid = true;
  pose.cameraId = camera.id;
  pose.method = "bw_localization";
  pose.origin = result.center;
  pose.angleRadians = result.angleRadians;
  pose.score = result.area;
  pose.xAxis = normalizedOrDefault(result.xAxisEnd - result.xAxisStart, {std::cos(result.angleRadians), std::sin(result.angleRadians)});
  pose.yAxis = normalizedOrDefault(result.yAxisEnd - result.yAxisStart, {-pose.xAxis.y, pose.xAxis.x});
  return pose;
}

PartPose MainWindow::partPoseFromSurfaceReference(const CameraConfig& camera, const SurfaceLocalizationReference& reference) const
{
  if (!reference.found)
  {
    return makeInvalidPartPose(camera.id);
  }

  PartPose pose;
  pose.valid = true;
  pose.cameraId = camera.id;
  pose.method = QString::fromStdString(reference.method);
  pose.origin = reference.center;
  pose.angleRadians = reference.angleRadians;
  pose.score = reference.score;

  const cv::Point2d fallbackX(std::cos(reference.angleRadians), std::sin(reference.angleRadians));
  pose.xAxis = normalizedOrDefault(reference.xAxisEnd - reference.xAxisStart, fallbackX);
  pose.yAxis = normalizedOrDefault(reference.yAxisEnd - reference.yAxisStart, {-pose.xAxis.y, pose.xAxis.x});
  return pose;
}

QPixmap MainWindow::loadCameraPreview(const CameraConfig& camera) const
{
  const QString imagePath = cameraSampleImagePath(camera);

  if (imagePath.isEmpty())
  {
    return {};
  }

  return QPixmap(imagePath);
}

void MainWindow::reloadCameraReferenceImage(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId || !m_largeImage)
  {
    return;
  }

  m_selectedImagePath = cameraSampleImagePath(camera);
  m_selectedPreview = m_selectedImagePath.isEmpty() ? QPixmap() : QPixmap(m_selectedImagePath);
  if (m_selectedPreview.isNull())
  {
    m_largeImage->clearImage();
    return;
  }

  m_largeImage->setImage(m_selectedPreview);
  updateLargePreview();
}

QPixmap MainWindow::matToPixmap(const cv::Mat& image) const
{
  if (image.empty())
  {
    return {};
  }

  cv::Mat rgb;

  if (image.channels() == 1)
  {
    cv::cvtColor(image, rgb, cv::COLOR_GRAY2RGB);
  }
  else
  {
    cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
  }

  QImage qimage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
  return QPixmap::fromImage(qimage.copy());
}

cv::Mat MainWindow::currentInputImage(const CameraConfig& camera, QString* errorMessage) const
{
  const auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt != m_cameraRuntime.end() && !runtimeIt->second.currentFrame().empty())
  {
    return runtimeIt->second.currentFrame().clone();
  }

  QString imagePath = camera.id == m_selectedCameraId ? m_selectedImagePath : QString();
  if (imagePath.isEmpty())
  {
    imagePath = cameraSampleImagePath(camera);
  }

  if (imagePath.isEmpty())
  {
    if (errorMessage)
    {
      *errorMessage = trText("log.imageMissing") + ": " + camera.id;
    }

    return {};
  }

  cv::Mat input = cv::imread(imagePath.toStdString(), cv::IMREAD_COLOR);
  if (input.empty() && errorMessage)
  {
    *errorMessage = trText("log.imageMissing") + ": " + imagePath;
  }

  return input;
}

QString MainWindow::cameraSampleImagePath(const CameraConfig& camera) const
{
  const QString recipeSample = m_recipeManager.firstCameraSampleImagePath(camera.id);
  if (!recipeSample.isEmpty())
  {
    return recipeSample;
  }

  return firstImageInFolder(camera.folder);
}

QString MainWindow::cameraTestImagesFolder(const CameraConfig& camera) const
{
  if (!m_recipeManager.firstCameraTestImagePath(camera.id).isEmpty())
  {
    return m_recipeManager.cameraTestImagesPath(camera.id);
  }

  return resolvedCameraFolder(camera);
}

QString MainWindow::resolvedCameraFolder(const CameraConfig& camera) const
{
  if (camera.folder.isEmpty())
  {
    return projectPath("data/images");
  }

  return resolveProjectPath(camera.folder);
}

QString MainWindow::firstImageInFolder(const QString& folder) const
{
  if (folder.isEmpty())
  {
    return {};
  }

  QDir directory(resolveProjectPath(folder));
  const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tif", "*.tiff"};
  const QFileInfoList files = directory.entryInfoList(filters, QDir::Files, QDir::Name);

  if (files.isEmpty())
  {
    return {};
  }

  return files.first().absoluteFilePath();
}



