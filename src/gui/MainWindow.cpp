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
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
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
    if (annulus.method == "edge" && annulus.hasEdgeCircle)
    {
      circles = {
        {annulus.edgeCenter, annulus.edgeRadius + annulus.edgeBandOuter},
        {annulus.edgeCenter, qMax(1, annulus.edgeRadius - annulus.edgeBandInner)}
      };
    }
    m_largeImage->setCircles(circles);
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
    const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
    QVector<ImageCircle> circles;
    if (annulus.hasOuterCircle)
    {
      circles.append({annulus.center, annulus.outerRadius});
    }
    if (annulus.hasInnerCircle)
    {
      circles.append({annulus.center, annulus.innerRadius});
    }
    if (annulus.method == "edge" && annulus.hasEdgeCircle)
    {
      circles = {
        {annulus.edgeCenter, annulus.edgeRadius + annulus.edgeBandOuter},
        {annulus.edgeCenter, qMax(1, annulus.edgeRadius - annulus.edgeBandInner)}
      };
    }
    m_largeImage->setCircles(circles);
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

void MainWindow::showCameraSetupPanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();

  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  CameraSetupPanelTexts texts;
  texts.title = QString("%1 | %2").arg(trText("tools.setup"), camera.id);
  texts.details = cameraSetupDetailsText(camera);
  texts.frameInterval = trText("labels.frameInterval");
  texts.acquireSample = trText("actions.acquireSampleImage");
  texts.importSample = trText("actions.assignSampleImage");
  texts.importTests = trText("actions.assignTestImages");
  texts.assignImageFolder = trText("actions.assignImageFolder");
  texts.start = trText("commands.start");
  texts.stop = trText("commands.stop");
  texts.nextFrame = trText("actions.nextFrame");
  texts.back = trText("commands.backToCameraTools");

  auto* panel = new CameraSetupPanelWidget(
    texts,
    runtime.intervalMs(),
    [this, camera](int value) {
      CameraRuntime& runtime = m_cameraRuntime[camera.id];
      runtime.setIntervalMs(value);
      if (runtime.running() && camera.id == m_selectedCameraId)
      {
        m_simulationTimer->start(runtime.intervalMs());
      }
    },
    [this, camera]() { acquireCameraSampleImage(camera); },
    [this, camera]() { configureCameraSampleImage(camera); },
    [this, camera]() { configureCameraTestImages(camera); },
    [this, camera]() { configureCameraSource(camera); },
    [this, camera]() { startCameraSimulation(camera); },
    [this, camera]() { stopCameraSimulation(camera); },
    [this, camera]() { stepCameraSimulation(camera); },
    [this, camera]() {
      stopCameraSimulation(camera);
      reloadCameraReferenceImage(camera);
      showCameraToolList(camera);
    },
    m_toolsContainer);
  m_setupPanel = panel;
  m_setupCameraId = camera.id;
  m_toolsLayout->addWidget(panel);
}

void MainWindow::showToolPanel(const CameraConfig& camera, const QString& toolId)
{
  deactivateImageDrawingTools();

  if (toolId == "geometries")
  {
    showGeometryPanel(camera);
    return;
  }

  if (toolId == "surfaceLocalization")
  {
    showSurfaceLocalizationPanel(camera);
    return;
  }

  clearToolPanel();

  const ToolDefinition tool = ToolCatalog::tool(toolId, m_translations);
  const QString noteText = tool.id == "surfaceLocalization"
    ? trText("labels.surfaceLocalizationNote")
    : trText("labels.placeholderNote");
  auto* panel = new ToolPanelWidget(
    tool,
    QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot),
    noteText,
    trText("commands.backToCameraTools"),
    [this, camera]() {
      showCameraToolList(camera);
      appendLog(trText("log.backToCameraTools") + ": " + camera.id);
    },
    [this, camera, tool](const ToolActionDefinition& action) {
      if (tool.id == "localization" && action.id == "searchRoi")
      {
        activateLocalizationRoiDrawing(camera);
        return;
      }

      if (tool.id == "localization" && action.id == "addExclusion")
      {
        activateLocalizationExclusionDrawing(camera);
        return;
      }

      if (tool.id == "localization" && action.id == "clearExclusions")
      {
        clearLocalizationExclusions(camera);
        return;
      }

      if (tool.id == "localization" && action.id == "testLocalization")
      {
        testLocalization(camera);
        return;
      }

      if (tool.id == "geometries" && action.id == "lineGeometry")
      {
        activateGeometryLineDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceSearchRoi")
      {
        activateSurfaceDefectRoiDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceOuterCircle")
      {
        activateSurfaceOuterCircleDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceInnerCircle")
      {
        activateSurfaceInnerCircleDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceThreshold")
      {
        saveSurfaceThresholdAndPreview(camera, m_recipeManager.loadSurfaceAnnulusLocalization(camera.id).thresholdMax);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "testSurfaceAnnulus")
      {
        testSurfaceAnnulusLocalization(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceAddExclusion")
      {
        activateSurfaceDefectExclusionDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceClearExclusions")
      {
        clearSurfaceDefectExclusions(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "testSurfaceLocalization")
      {
        testSurfaceLocalization(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "testSurfaceStrategy")
      {
        testSurfaceLocalizationStrategy(camera);
        return;
      }

      appendLog(trText("log.placeholder") + ": " + tool.label + " -> " + action.label);
    },
    m_toolsContainer);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + tool.label);
}

void MainWindow::showGeometryPanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(trText("tools.geometries"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* pointButton = new QPushButton(trText("actions.pointGeometry"), panel);
  connect(pointButton, &QPushButton::clicked, this, [this, camera]() { showGeometryPointPanel(camera); });
  layout->addWidget(pointButton);

  auto* lineButton = new QPushButton(trText("actions.lineGeometry"), panel);
  connect(lineButton, &QPushButton::clicked, this, [this, camera]() { showGeometryLinePanel(camera); });
  layout->addWidget(lineButton);

  auto* circleButton = new QPushButton(trText("actions.circleGeometry"), panel);
  connect(circleButton, &QPushButton::clicked, this, [this, camera]() { showGeometryCirclePanel(camera); });
  layout->addWidget(circleButton);

  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);
  connect(backButton, &QPushButton::clicked, this, [this, camera]() { showCameraToolList(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  m_toolsLayout->addWidget(panel);
}

void MainWindow::showGeometryPointPanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();
  refreshPoseForCurrentFrame(camera);
  loadGeometryPointRecipe(camera);

  QVector<GeometryPointRuntimeConfig>& pointConfigs = m_geometryPointConfigs[camera.id];
  GeometryPointRuntimeConfig& pointConfig = activeGeometryPointConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(trText("actions.pointGeometry"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* poseLabel = new QLabel(pose.valid ? trText("labels.partPose") : trText("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* pointSelector = new QComboBox(panel);
  for (int i = 0; i < pointConfigs.size(); ++i)
  {
    pointSelector->addItem(pointConfigs[i].id, i);
  }
  pointSelector->setCurrentIndex(qBound(0, m_activeGeometryPointIndexes.value(camera.id, 0), pointConfigs.size() - 1));
  auto* newPointButton = new QPushButton(trText("actions.newGeometryPoint"), panel);
  auto* deletePointButton = new QPushButton(trText("actions.deleteGeometryPoint"), panel);

  auto* pointControls = new QWidget(panel);
  auto* pointControlsLayout = new QGridLayout(pointControls);
  pointControlsLayout->setContentsMargins(0, 0, 0, 0);
  pointControlsLayout->setHorizontalSpacing(6);
  pointControlsLayout->setVerticalSpacing(4);
  pointControlsLayout->addWidget(new QLabel(trText("actions.pointGeometry"), pointControls), 0, 0);
  pointControlsLayout->addWidget(pointSelector, 0, 1);
  pointControlsLayout->addWidget(newPointButton, 0, 2);
  pointControlsLayout->addWidget(deletePointButton, 0, 3);
  pointControlsLayout->setColumnStretch(1, 1);
  layout->addWidget(pointControls);

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(6);

  auto* edgeSensitivity = new QSpinBox(form);
  edgeSensitivity->setRange(1, 255);
  edgeSensitivity->setValue(pointConfig.edgeSensitivity);

  auto* subpixelEdge = new QCheckBox(trText("labels.subpixelEdge"), form);
  subpixelEdge->setChecked(pointConfig.useSubpixel);

  auto* edgeTransition = new QComboBox(form);
  edgeTransition->addItem(trText("labels.transitionLightToDark"), "light_to_dark");
  edgeTransition->addItem(trText("labels.transitionDarkToLight"), "dark_to_light");
  edgeTransition->setCurrentIndex(pointConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);

  auto* edgePickMode = new QComboBox(form);
  edgePickMode->addItem(trText("labels.edgePickFirst"), "first");
  edgePickMode->addItem(trText("labels.edgePickLast"), "last");
  edgePickMode->addItem(trText("labels.edgePickBest"), "best");
  edgePickMode->setCurrentIndex(static_cast<int>(pointConfig.pickMode));

  int row = 0;
  formLayout->addWidget(new QLabel(trText("labels.edgeSensitivity"), form), row, 0);
  formLayout->addWidget(edgeSensitivity, row++, 1);
  if (isBwDimensionalCamera(camera))
  {
    formLayout->addWidget(subpixelEdge, row++, 0, 1, 2);
  }
  formLayout->addWidget(new QLabel(trText("labels.edgeTransition"), form), row, 0);
  formLayout->addWidget(edgeTransition, row++, 1);
  formLayout->addWidget(new QLabel(trText("labels.edgePickMode"), form), row, 0);
  formLayout->addWidget(edgePickMode, row++, 1);
  layout->addWidget(form);

  connect(pointSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    if (index < 0)
    {
      return;
    }

    m_activeGeometryPointIndexes[camera.id] = index;
    showGeometryPointPanel(camera);
  });
  connect(newPointButton, &QPushButton::clicked, this, [this, camera]() {
    addGeometryPoint(camera);
    saveGeometryPointRecipe(camera);
    showGeometryPointPanel(camera);
    activateGeometryPointDrawing(camera);
  });
  connect(deletePointButton, &QPushButton::clicked, this, [this, camera]() {
    removeActiveGeometryPoint(camera);
  });

  connect(edgeSensitivity, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryPointConfig(camera.id).edgeSensitivity = value;
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });
  connect(subpixelEdge, &QCheckBox::toggled, this, [this, camera](bool checked) {
    activeGeometryPointConfig(camera.id).useSubpixel = checked;
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });
  connect(edgeTransition, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    activeGeometryPointConfig(camera.id).transition =
      index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });
  connect(edgePickMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    if (index == 1)
    {
      activeGeometryPointConfig(camera.id).pickMode = EdgeLinePickMode::Last;
    }
    else if (index == 2)
    {
      activeGeometryPointConfig(camera.id).pickMode = EdgeLinePickMode::Best;
    }
    else
    {
      activeGeometryPointConfig(camera.id).pickMode = EdgeLinePickMode::First;
    }
    saveGeometryPointRecipe(camera);
    testGeometryPoint(camera);
  });

  auto* testButton = new QPushButton(trText("actions.testGeometry"), panel);
  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);

  connect(testButton, &QPushButton::clicked, this, [this, camera]() { testGeometryPoint(camera); });
  connect(backButton, &QPushButton::clicked, this, [this, camera]() { showGeometryPanel(camera); });

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setHorizontalSpacing(8);
  buttonsLayout->setVerticalSpacing(8);
  buttonsLayout->addWidget(testButton, 0, 0, 1, 2);
  buttonsLayout->addWidget(backButton, 1, 0, 1, 2);
  layout->addWidget(buttons);
  layout->addStretch(1);

  if (pointConfig.hasGuide && pose.valid)
  {
    const cv::Point2d imageStart = partToImage(pose, pointConfig.partStart);
    const cv::Point2d imageEnd = partToImage(pose, pointConfig.partEnd);
    m_pointGeometryMouseControllers[camera.id].setLine(
      QPointF(imageStart.x, imageStart.y),
      QPointF(imageEnd.x, imageEnd.y),
      3.0);
    m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
    m_geometryDrawingTarget = GeometryDrawingTarget::Point;
    m_largeImage->setGeometryOverlayPointEditingEnabled(true);
    testGeometryPoint(camera);
  }
  else if (pointConfig.hasImageGuide)
  {
    m_pointGeometryMouseControllers[camera.id].setLine(
      QPointF(pointConfig.imageStart.x, pointConfig.imageStart.y),
      QPointF(pointConfig.imageEnd.x, pointConfig.imageEnd.y),
      3.0);
    m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
    m_geometryDrawingTarget = GeometryDrawingTarget::Point;
    m_largeImage->setGeometryOverlayPointEditingEnabled(true);
    testGeometryPoint(camera);
  }
  else
  {
    updateGeometryPointOverlay(camera);
  }

  m_toolsLayout->addWidget(panel);
}

void MainWindow::showGeometryLinePanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();
  refreshPoseForCurrentFrame(camera);
  loadGeometryLinesRecipe(camera);

  QVector<GeometryLineRuntimeConfig>& lineConfigs = m_geometryLineConfigs[camera.id];
  GeometryLineRuntimeConfig& lineConfig = activeGeometryLineConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(trText("actions.lineGeometry"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* poseLabel = new QLabel(pose.valid ? trText("labels.partPose") : trText("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* lineSelector = new QComboBox(panel);
  for (int i = 0; i < lineConfigs.size(); ++i)
  {
    lineSelector->addItem(lineConfigs[i].id, i);
  }
  lineSelector->setCurrentIndex(qBound(0, m_activeGeometryLineIndexes.value(camera.id, 0), lineConfigs.size() - 1));
  auto* newLineButton = new QPushButton(trText("actions.newGeometryLine"), panel);
  auto* deleteLineButton = new QPushButton(trText("actions.deleteGeometryLine"), panel);

  auto* bandHalfWidth = new QSpinBox(panel);
  bandHalfWidth->setRange(2, 500);
  bandHalfWidth->setValue(lineConfig.bandHalfWidth);
  bandHalfWidth->setSuffix(" px");
  bandHalfWidth->setFixedWidth(82);
  auto* edgeSensitivity = new QSpinBox(panel);
  edgeSensitivity->setRange(1, 255);
  edgeSensitivity->setValue(lineConfig.edgeSensitivity);
  edgeSensitivity->setFixedWidth(68);
  auto* edgeCleanupDerivative = new QSpinBox(panel);
  edgeCleanupDerivative->setRange(0, 100);
  edgeCleanupDerivative->setValue(lineConfig.edgeCleanupDerivative);
  edgeCleanupDerivative->setSuffix(" px");
  edgeCleanupDerivative->setFixedWidth(82);
  auto* edgeStatisticalFilter = new QSpinBox(panel);
  edgeStatisticalFilter->setRange(0, 100);
  edgeStatisticalFilter->setValue(lineConfig.edgeStatisticalFilter);
  edgeStatisticalFilter->setSuffix(" px");
  edgeStatisticalFilter->setFixedWidth(82);
  auto* subpixelEdge = new QCheckBox(trText("labels.subpixelEdge"), panel);
  subpixelEdge->setChecked(lineConfig.useSubpixel);
  auto* scanDirection = new QComboBox(panel);
  scanDirection->addItem(trText("labels.scanNormalPositive"), "normal_positive");
  scanDirection->addItem(trText("labels.scanNormalNegative"), "normal_negative");
  scanDirection->setCurrentIndex(lineConfig.scanDirection == EdgeLineScanDirection::NormalNegative ? 1 : 0);
  auto* edgeTransition = new QComboBox(panel);
  edgeTransition->addItem(trText("labels.transitionLightToDark"), "light_to_dark");
  edgeTransition->addItem(trText("labels.transitionDarkToLight"), "dark_to_light");
  edgeTransition->setCurrentIndex(lineConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);
  auto* edgePickMode = new QComboBox(panel);
  edgePickMode->addItem(trText("labels.edgePickFirst"), "first");
  edgePickMode->addItem(trText("labels.edgePickLast"), "last");
  edgePickMode->addItem(trText("labels.edgePickBest"), "best");
  edgePickMode->setCurrentIndex(static_cast<int>(lineConfig.pickMode));

  auto* lineControls = new QWidget(panel);
  auto* lineControlsLayout = new QGridLayout(lineControls);
  lineControlsLayout->setContentsMargins(0, 0, 0, 0);
  lineControlsLayout->setHorizontalSpacing(6);
  lineControlsLayout->setVerticalSpacing(4);
  lineControlsLayout->addWidget(new QLabel(trText("labels.geometryLine"), lineControls), 0, 0);
  lineControlsLayout->addWidget(lineSelector, 0, 1);
  lineControlsLayout->addWidget(newLineButton, 0, 2);
  lineControlsLayout->addWidget(deleteLineButton, 0, 3);
  lineControlsLayout->addWidget(new QLabel(trText("labels.geometryLineBand"), lineControls), 1, 0);
  lineControlsLayout->addWidget(bandHalfWidth, 1, 1);
  lineControlsLayout->addWidget(new QLabel(trText("labels.edgeSensitivity"), lineControls), 1, 2);
  lineControlsLayout->addWidget(edgeSensitivity, 1, 3);
  lineControlsLayout->setColumnStretch(1, 1);
  layout->addWidget(lineControls);

  auto* filterControls = new QWidget(panel);
  auto* filterControlsLayout = new QGridLayout(filterControls);
  filterControlsLayout->setContentsMargins(0, 0, 0, 0);
  filterControlsLayout->setHorizontalSpacing(6);
  filterControlsLayout->setVerticalSpacing(4);
  filterControlsLayout->addWidget(new QLabel(trText("labels.edgeCleanupDerivative"), filterControls), 0, 0);
  filterControlsLayout->addWidget(edgeCleanupDerivative, 0, 1);
  filterControlsLayout->addWidget(new QLabel(trText("labels.edgeStatisticalFilter"), filterControls), 0, 2);
  filterControlsLayout->addWidget(edgeStatisticalFilter, 0, 3);
  filterControlsLayout->setColumnStretch(3, 1);
  layout->addWidget(filterControls);

  if (isBwDimensionalCamera(camera))
  {
    layout->addWidget(subpixelEdge);
  }

  auto* scanControls = new QWidget(panel);
  auto* scanControlsLayout = new QGridLayout(scanControls);
  scanControlsLayout->setContentsMargins(0, 0, 0, 0);
  scanControlsLayout->setHorizontalSpacing(6);
  scanControlsLayout->setVerticalSpacing(4);
  scanControlsLayout->addWidget(new QLabel(trText("labels.scanDirection"), scanControls), 0, 0);
  scanControlsLayout->addWidget(scanDirection, 0, 1);
  scanControlsLayout->addWidget(new QLabel(trText("labels.edgeTransition"), scanControls), 0, 2);
  scanControlsLayout->addWidget(edgeTransition, 0, 3);
  scanControlsLayout->addWidget(new QLabel(trText("labels.edgePickMode"), scanControls), 1, 0);
  scanControlsLayout->addWidget(edgePickMode, 1, 1, 1, 3);
  scanControlsLayout->setColumnStretch(1, 1);
  scanControlsLayout->setColumnStretch(3, 1);
  layout->addWidget(scanControls);

  connect(lineSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    if (index < 0)
    {
      return;
    }

    m_activeGeometryLineIndexes[camera.id] = index;
    showGeometryLinePanel(camera);
  });
  connect(newLineButton, &QPushButton::clicked, this, [this, camera]() {
    addGeometryLine(camera);
    saveGeometryLinesRecipe(camera);
    showGeometryLinePanel(camera);
    activateGeometryLineDrawing(camera);
  });
  connect(deleteLineButton, &QPushButton::clicked, this, [this, camera]() {
    removeActiveGeometryLine(camera);
  });
  connect(bandHalfWidth, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryLineConfig(camera.id).bandHalfWidth = value;
    m_lineGeometryMouseControllers[camera.id].setBandHalfWidth(value);
    updateGeometryLineOverlay(camera);
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(edgeSensitivity, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryLineConfig(camera.id).edgeSensitivity = value;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(edgeCleanupDerivative, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryLineConfig(camera.id).edgeCleanupDerivative = value;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(edgeStatisticalFilter, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryLineConfig(camera.id).edgeStatisticalFilter = value;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(subpixelEdge, &QCheckBox::toggled, this, [this, camera](bool checked) {
    activeGeometryLineConfig(camera.id).useSubpixel = checked;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(scanDirection, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    activeGeometryLineConfig(camera.id).scanDirection =
      index == 1 ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(edgeTransition, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    activeGeometryLineConfig(camera.id).transition =
      index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });
  connect(edgePickMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    if (index == 1)
    {
      activeGeometryLineConfig(camera.id).pickMode = EdgeLinePickMode::Last;
    }
    else if (index == 2)
    {
      activeGeometryLineConfig(camera.id).pickMode = EdgeLinePickMode::Best;
    }
    else
    {
      activeGeometryLineConfig(camera.id).pickMode = EdgeLinePickMode::First;
    }
    saveGeometryLinesRecipe(camera);
    testGeometryLine(camera);
  });

  auto* testButton = new QPushButton(trText("actions.testGeometry"), panel);
  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);

  connect(testButton, &QPushButton::clicked, this, [this, camera]() { testGeometryLine(camera); });
  connect(backButton, &QPushButton::clicked, this, [this, camera]() { showGeometryPanel(camera); });

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setHorizontalSpacing(8);
  buttonsLayout->setVerticalSpacing(8);
  buttonsLayout->addWidget(testButton, 0, 0, 1, 2);
  buttonsLayout->addWidget(backButton, 1, 0, 1, 2);
  layout->addWidget(buttons);
  layout->addStretch(1);

  bool shouldRefreshLine = false;
  if (lineConfig.hasLine && pose.valid)
  {
    const cv::Point2d imageStart = partToImage(pose, lineConfig.partStart);
    const cv::Point2d imageEnd = partToImage(pose, lineConfig.partEnd);
    LineGeometryMouseController& controller = m_lineGeometryMouseControllers[camera.id];
    controller.setLine(QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y), lineConfig.bandHalfWidth);
    m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
    m_geometryDrawingTarget = GeometryDrawingTarget::Line;
    updateGeometryLineOverlay(camera);
    m_largeImage->setGeometryOverlayPointEditingEnabled(true);
    shouldRefreshLine = true;
  }
  else if (lineConfig.hasImageLine)
  {
    LineGeometryMouseController& controller = m_lineGeometryMouseControllers[camera.id];
    controller.setLine(
      QPointF(lineConfig.imageStart.x, lineConfig.imageStart.y),
      QPointF(lineConfig.imageEnd.x, lineConfig.imageEnd.y),
      lineConfig.bandHalfWidth);
    m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
    m_geometryDrawingTarget = GeometryDrawingTarget::Line;
    m_largeImage->setGeometryOverlayPointEditingEnabled(true);
    shouldRefreshLine = true;
  }

  m_toolsLayout->addWidget(panel);
  if (shouldRefreshLine)
  {
    testGeometryLine(camera);
  }
  else
  {
    updateGeometryLineOverlay(camera);
  }
}

void MainWindow::showGeometryCirclePanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();
  refreshPoseForCurrentFrame(camera);
  m_largeImage->clearCircles();
  loadGeometryCirclesRecipe(camera);

  QVector<GeometryCircleRuntimeConfig>& circleConfigs = m_geometryCircleConfigs[camera.id];
  GeometryCircleRuntimeConfig& circleConfig = activeGeometryCircleConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(trText("actions.circleGeometry"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* poseLabel = new QLabel(pose.valid ? trText("labels.partPose") : trText("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* circleSelector = new QComboBox(panel);
  for (int i = 0; i < circleConfigs.size(); ++i)
  {
    circleSelector->addItem(circleConfigs[i].id, i);
  }
  circleSelector->setCurrentIndex(qBound(0, m_activeGeometryCircleIndexes.value(camera.id, 0), circleConfigs.size() - 1));
  auto* newCircleButton = new QPushButton(trText("actions.newGeometryCircle"), panel);
  auto* deleteCircleButton = new QPushButton(trText("actions.deleteGeometryCircle"), panel);

  auto* top = new QWidget(panel);
  auto* topLayout = new QGridLayout(top);
  topLayout->setContentsMargins(0, 0, 0, 0);
  topLayout->setHorizontalSpacing(6);
  topLayout->addWidget(new QLabel(trText("actions.circleGeometry"), top), 0, 0);
  topLayout->addWidget(circleSelector, 0, 1);
  topLayout->addWidget(newCircleButton, 0, 2);
  topLayout->addWidget(deleteCircleButton, 0, 3);
  topLayout->setColumnStretch(1, 1);
  layout->addWidget(top);

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(6);
  auto* innerBand = new QSpinBox(form);
  innerBand->setRange(1, 500);
  innerBand->setSuffix(" px");
  innerBand->setValue(circleConfig.innerBand);
  auto* outerBand = new QSpinBox(form);
  outerBand->setRange(1, 500);
  outerBand->setSuffix(" px");
  outerBand->setValue(circleConfig.outerBand);
  auto* sensitivity = new QSpinBox(form);
  sensitivity->setRange(1, 255);
  sensitivity->setValue(circleConfig.edgeSensitivity);
  auto* cleanup = new QSpinBox(form);
  cleanup->setRange(0, 100);
  cleanup->setSuffix(" px");
  cleanup->setValue(circleConfig.edgeCleanupDerivative);
  auto* statFilter = new QSpinBox(form);
  statFilter->setRange(0, 100);
  statFilter->setSuffix(" px");
  statFilter->setValue(circleConfig.edgeStatisticalFilter);
  auto* subpixel = new QCheckBox(trText("labels.subpixelEdge"), form);
  subpixel->setChecked(circleConfig.useSubpixel);
  auto* transition = new QComboBox(form);
  transition->addItem(trText("labels.transitionLightToDark"), "light_to_dark");
  transition->addItem(trText("labels.transitionDarkToLight"), "dark_to_light");
  transition->setCurrentIndex(circleConfig.transition == EdgeLineTransition::DarkToLight ? 1 : 0);
  auto* pickMode = new QComboBox(form);
  pickMode->addItem(trText("labels.edgePickFirst"), "first");
  pickMode->addItem(trText("labels.edgePickLast"), "last");
  pickMode->addItem(trText("labels.edgePickBest"), "best");
  pickMode->setCurrentIndex(static_cast<int>(circleConfig.pickMode));

  int row = 0;
  formLayout->addWidget(new QLabel(trText("labels.edgeBandInner"), form), row, 0);
  formLayout->addWidget(innerBand, row, 1);
  formLayout->addWidget(new QLabel(trText("labels.edgeBandOuter"), form), row, 2);
  formLayout->addWidget(outerBand, row++, 3);
  formLayout->addWidget(new QLabel(trText("labels.edgeSensitivity"), form), row, 0);
  formLayout->addWidget(sensitivity, row, 1);
  formLayout->addWidget(new QLabel(trText("labels.edgeCleanupDerivative"), form), row, 2);
  formLayout->addWidget(cleanup, row++, 3);
  formLayout->addWidget(new QLabel(trText("labels.edgeStatisticalFilter"), form), row, 0);
  formLayout->addWidget(statFilter, row++, 1);
  if (isBwDimensionalCamera(camera))
  {
    formLayout->addWidget(subpixel, row++, 0, 1, 4);
  }
  formLayout->addWidget(new QLabel(trText("labels.edgeTransition"), form), row, 0);
  formLayout->addWidget(transition, row, 1);
  formLayout->addWidget(new QLabel(trText("labels.edgePickMode"), form), row, 2);
  formLayout->addWidget(pickMode, row++, 3);
  layout->addWidget(form);

  connect(circleSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    if (index < 0)
    {
      return;
    }

    m_activeGeometryCircleIndexes[camera.id] = index;
    showGeometryCirclePanel(camera);
  });
  connect(newCircleButton, &QPushButton::clicked, this, [this, camera]() {
    addGeometryCircle(camera);
    saveGeometryCirclesRecipe(camera);
    showGeometryCirclePanel(camera);
    activateGeometryCircleDrawing(camera);
  });
  connect(deleteCircleButton, &QPushButton::clicked, this, [this, camera]() { removeActiveGeometryCircle(camera); });
  connect(innerBand, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).innerBand = value;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  connect(outerBand, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).outerBand = value;
    saveGeometryCirclesRecipe(camera);
    showConfiguredGeometryCircles(camera);
  });
  connect(sensitivity, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).edgeSensitivity = value;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  connect(cleanup, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).edgeCleanupDerivative = value;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  connect(statFilter, qOverload<int>(&QSpinBox::valueChanged), this, [this, camera](int value) {
    activeGeometryCircleConfig(camera.id).edgeStatisticalFilter = value;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  connect(subpixel, &QCheckBox::toggled, this, [this, camera](bool checked) {
    activeGeometryCircleConfig(camera.id).useSubpixel = checked;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  connect(transition, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    activeGeometryCircleConfig(camera.id).transition = index == 1 ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });
  connect(pickMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, camera](int index) {
    activeGeometryCircleConfig(camera.id).pickMode = index == 1 ? EdgeLinePickMode::Last : (index == 2 ? EdgeLinePickMode::Best : EdgeLinePickMode::First);
    saveGeometryCirclesRecipe(camera);
    testGeometryCircle(camera);
  });

  auto* testButton = new QPushButton(trText("actions.testGeometry"), panel);
  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);
  connect(testButton, &QPushButton::clicked, this, [this, camera]() { testGeometryCircle(camera); });
  connect(backButton, &QPushButton::clicked, this, [this, camera]() { showGeometryPanel(camera); });
  layout->addWidget(testButton);
  layout->addWidget(backButton);
  layout->addStretch(1);
  m_toolsLayout->addWidget(panel);
  showConfiguredGeometryCircles(camera);
}

GeometryLineRuntimeConfig& MainWindow::activeGeometryLineConfig(const QString& cameraId)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[cameraId];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig first;
    first.id = "line_1";
    lines.append(first);
    m_activeGeometryLineIndexes[cameraId] = 0;
    return lines[0];
  }

  int index = qBound(0, m_activeGeometryLineIndexes.value(cameraId, 0), lines.size() - 1);
  m_activeGeometryLineIndexes[cameraId] = index;
  return lines[index];
}

const GeometryLineRuntimeConfig& MainWindow::activeGeometryLineConfig(const QString& cameraId) const
{
  const QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs.value(cameraId);
  static const GeometryLineRuntimeConfig fallback;
  if (lines.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeGeometryLineIndexes.value(cameraId, 0), lines.size() - 1);
  return lines[index];
}

void MainWindow::addGeometryLine(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig first;
    first.id = "line_1";
    lines.append(first);
    m_activeGeometryLineIndexes[camera.id] = 0;
    return;
  }

  GeometryLineRuntimeConfig& activeLine = activeGeometryLineConfig(camera.id);
  if (!activeLine.hasImageLine && !activeLine.hasLine)
  {
    return;
  }

  GeometryLineRuntimeConfig line = activeGeometryLineConfig(camera.id);
  line.id = QString("line_%1").arg(lines.size() + 1);
  line.hasImageLine = false;
  line.hasLine = false;
  lines.append(line);
  m_activeGeometryLineIndexes[camera.id] = lines.size() - 1;
}

void MainWindow::removeActiveGeometryLine(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
    m_activeGeometryLineIndexes[camera.id] = 0;
    showGeometryLinePanel(camera);
    return;
  }

  const int index = qBound(0, m_activeGeometryLineIndexes.value(camera.id, 0), lines.size() - 1);
  lines.removeAt(index);
  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
  }

  m_activeGeometryLineIndexes[camera.id] = qBound(0, index, lines.size() - 1);
  m_lineGeometryMouseControllers[camera.id].begin(activeGeometryLineConfig(camera.id).bandHalfWidth);
  saveGeometryLinesRecipe(camera);
  updateGeometryLineOverlay(camera);
  showGeometryLinePanel(camera);
}

void MainWindow::loadGeometryPointRecipe(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  if (!points.isEmpty())
  {
    return;
  }

  const QVector<GeometryPointRecipeConfig> recipes = m_recipeManager.loadGeometryPoints(camera.id);
  for (const GeometryPointRecipeConfig& recipe : recipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryPointRuntimeConfig point;
    point.enabled = recipe.enabled;
    point.id = recipe.id;
    point.partStart = cv::Point2d(recipe.partStart.x(), recipe.partStart.y());
    point.partEnd = cv::Point2d(recipe.partEnd.x(), recipe.partEnd.y());
    point.edgeSensitivity = recipe.edgeSensitivity;
    point.useSubpixel = recipe.useSubpixel;
    point.transition = transitionFromRecipe(recipe.transition);
    point.pickMode = pickModeFromRecipe(recipe.pickMode);
    point.hasGuide = true;
    points.append(point);
  }

  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }
  m_activeGeometryPointIndexes[camera.id] = qBound(0, m_activeGeometryPointIndexes.value(camera.id, 0), points.size() - 1);
}

void MainWindow::loadGeometryCirclesRecipe(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[camera.id];
  if (!circles.isEmpty())
  {
    return;
  }

  const QVector<GeometryCircleRecipeConfig> recipes = m_recipeManager.loadGeometryCircles(camera.id);
  for (const GeometryCircleRecipeConfig& recipe : recipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryCircleRuntimeConfig circle;
    circle.enabled = recipe.enabled;
    circle.id = recipe.id;
    circle.partCenter = cv::Point2d(recipe.partCenter.x(), recipe.partCenter.y());
    circle.radius = recipe.radius;
    circle.innerBand = recipe.innerBand;
    circle.outerBand = recipe.outerBand;
    circle.edgeSensitivity = recipe.edgeSensitivity;
    circle.edgeCleanupDerivative = recipe.edgeCleanupDerivative;
    circle.edgeStatisticalFilter = recipe.edgeStatisticalFilter;
    circle.useSubpixel = recipe.useSubpixel;
    circle.transition = transitionFromRecipe(recipe.transition);
    circle.pickMode = pickModeFromRecipe(recipe.pickMode);
    circle.hasCircle = true;
    circles.append(circle);
  }

  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }
  m_activeGeometryCircleIndexes[camera.id] = qBound(0, m_activeGeometryCircleIndexes.value(camera.id, 0), circles.size() - 1);
}

void MainWindow::saveGeometryPointRecipe(const CameraConfig& camera)
{
  QVector<GeometryPointRecipeConfig> recipes;
  for (const GeometryPointRuntimeConfig& point : m_geometryPointConfigs[camera.id])
  {
    if (!point.hasGuide)
    {
      continue;
    }

    GeometryPointRecipeConfig recipe;
    recipe.enabled = point.enabled;
    recipe.id = point.id;
    recipe.partStart = QPointF(point.partStart.x, point.partStart.y);
    recipe.partEnd = QPointF(point.partEnd.x, point.partEnd.y);
    recipe.edgeSensitivity = point.edgeSensitivity;
    recipe.useSubpixel = point.useSubpixel;
    recipe.transition = transitionToRecipe(point.transition);
    recipe.pickMode = pickModeToRecipe(point.pickMode);
    recipes.append(recipe);
  }

  QString error;
  if (!m_recipeManager.saveGeometryPoints(camera.id, recipes, &error))
  {
    appendLog(error);
  }
}

void MainWindow::saveGeometryCirclesRecipe(const CameraConfig& camera)
{
  QVector<GeometryCircleRecipeConfig> recipes;
  for (const GeometryCircleRuntimeConfig& circle : m_geometryCircleConfigs[camera.id])
  {
    if (!circle.hasCircle)
    {
      continue;
    }

    GeometryCircleRecipeConfig recipe;
    recipe.enabled = circle.enabled;
    recipe.id = circle.id;
    recipe.partCenter = QPointF(circle.partCenter.x, circle.partCenter.y);
    recipe.radius = circle.radius;
    recipe.innerBand = circle.innerBand;
    recipe.outerBand = circle.outerBand;
    recipe.edgeSensitivity = circle.edgeSensitivity;
    recipe.edgeCleanupDerivative = circle.edgeCleanupDerivative;
    recipe.edgeStatisticalFilter = circle.edgeStatisticalFilter;
    recipe.useSubpixel = circle.useSubpixel;
    recipe.transition = transitionToRecipe(circle.transition);
    recipe.pickMode = pickModeToRecipe(circle.pickMode);
    recipes.append(recipe);
  }

  QString error;
  if (!m_recipeManager.saveGeometryCircles(camera.id, recipes, &error))
  {
    appendLog(error);
  }
}

void MainWindow::addGeometryPoint(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
    m_activeGeometryPointIndexes[camera.id] = 0;
    return;
  }

  GeometryPointRuntimeConfig& activePoint = activeGeometryPointConfig(camera.id);
  if (!activePoint.hasImageGuide && !activePoint.hasGuide)
  {
    return;
  }

  GeometryPointRuntimeConfig point = activeGeometryPointConfig(camera.id);
  point.id = QString("point_%1").arg(points.size() + 1);
  point.hasImageGuide = false;
  point.hasGuide = false;
  points.append(point);
  m_activeGeometryPointIndexes[camera.id] = points.size() - 1;
}

void MainWindow::addGeometryCircle(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[camera.id];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
    m_activeGeometryCircleIndexes[camera.id] = 0;
    return;
  }

  GeometryCircleRuntimeConfig& activeCircle = activeGeometryCircleConfig(camera.id);
  if (!activeCircle.hasImageCircle && !activeCircle.hasCircle)
  {
    return;
  }

  GeometryCircleRuntimeConfig circle = activeCircle;
  circle.id = QString("circle_%1").arg(circles.size() + 1);
  circle.hasImageCircle = false;
  circle.hasCircle = false;
  circles.append(circle);
  m_activeGeometryCircleIndexes[camera.id] = circles.size() - 1;
}


void MainWindow::removeActiveGeometryPoint(const CameraConfig& camera)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
    m_activeGeometryPointIndexes[camera.id] = 0;
    showGeometryPointPanel(camera);
    return;
  }

  const int index = qBound(0, m_activeGeometryPointIndexes.value(camera.id, 0), points.size() - 1);
  points.removeAt(index);
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }

  m_activeGeometryPointIndexes[camera.id] = qBound(0, index, points.size() - 1);
  m_pointGeometryMouseControllers[camera.id].begin(3.0);
  saveGeometryPointRecipe(camera);
  updateGeometryPointOverlay(camera);
  showGeometryPointPanel(camera);
}

void MainWindow::removeActiveGeometryCircle(const CameraConfig& camera)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[camera.id];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
    m_activeGeometryCircleIndexes[camera.id] = 0;
    showGeometryCirclePanel(camera);
    return;
  }

  const int index = qBound(0, m_activeGeometryCircleIndexes.value(camera.id, 0), circles.size() - 1);
  circles.removeAt(index);
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }

  m_activeGeometryCircleIndexes[camera.id] = qBound(0, index, circles.size() - 1);
  saveGeometryCirclesRecipe(camera);
  showGeometryCirclePanel(camera);
}

GeometryPointRuntimeConfig& MainWindow::activeGeometryPointConfig(const QString& cameraId)
{
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[cameraId];
  if (points.isEmpty())
  {
    GeometryPointRuntimeConfig point;
    point.id = "point_1";
    points.append(point);
  }

  int index = qBound(0, m_activeGeometryPointIndexes.value(cameraId, 0), points.size() - 1);
  m_activeGeometryPointIndexes[cameraId] = index;
  return points[index];
}

GeometryCircleRuntimeConfig& MainWindow::activeGeometryCircleConfig(const QString& cameraId)
{
  QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs[cameraId];
  if (circles.isEmpty())
  {
    GeometryCircleRuntimeConfig circle;
    circle.id = "circle_1";
    circles.append(circle);
  }

  int index = qBound(0, m_activeGeometryCircleIndexes.value(cameraId, 0), circles.size() - 1);
  m_activeGeometryCircleIndexes[cameraId] = index;
  return circles[index];
}

const GeometryPointRuntimeConfig& MainWindow::activeGeometryPointConfig(const QString& cameraId) const
{
  const QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs.value(cameraId);
  static const GeometryPointRuntimeConfig fallback;
  if (points.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeGeometryPointIndexes.value(cameraId, 0), points.size() - 1);
  return points[index];
}

const GeometryCircleRuntimeConfig& MainWindow::activeGeometryCircleConfig(const QString& cameraId) const
{
  const QVector<GeometryCircleRuntimeConfig>& circles = m_geometryCircleConfigs.value(cameraId);
  static const GeometryCircleRuntimeConfig fallback;
  if (circles.isEmpty())
  {
    return fallback;
  }

  const int index = qBound(0, m_activeGeometryCircleIndexes.value(cameraId, 0), circles.size() - 1);
  return circles[index];
}

void MainWindow::loadGeometryLinesRecipe(const CameraConfig& camera)
{
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  if (!lines.isEmpty())
  {
    return;
  }

  const QVector<GeometryLineRecipeConfig> recipes = m_recipeManager.loadGeometryLines(camera.id);
  for (const GeometryLineRecipeConfig& recipe : recipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryLineRuntimeConfig line;
    line.id = recipe.id;
    line.enabled = recipe.enabled;
    line.partStart = cv::Point2d(recipe.partStart.x(), recipe.partStart.y());
    line.partEnd = cv::Point2d(recipe.partEnd.x(), recipe.partEnd.y());
    line.bandHalfWidth = recipe.bandHalfWidth;
    line.edgeSensitivity = recipe.edgeSensitivity;
    line.edgeCleanupDerivative = recipe.edgeCleanupDerivative;
    line.edgeStatisticalFilter = recipe.edgeStatisticalFilter;
    line.useSubpixel = recipe.useSubpixel;
    line.scanDirection = scanDirectionFromRecipe(recipe.scanDirection);
    line.transition = transitionFromRecipe(recipe.transition);
    line.pickMode = pickModeFromRecipe(recipe.pickMode);
    line.hasLine = true;
    lines.append(line);
  }

  if (lines.isEmpty())
  {
    GeometryLineRuntimeConfig line;
    line.id = "line_1";
    lines.append(line);
  }
  m_activeGeometryLineIndexes[camera.id] = qBound(0, m_activeGeometryLineIndexes.value(camera.id, 0), lines.size() - 1);
}

void MainWindow::saveGeometryLinesRecipe(const CameraConfig& camera)
{
  QVector<GeometryLineRecipeConfig> recipes;
  for (const GeometryLineRuntimeConfig& line : m_geometryLineConfigs[camera.id])
  {
    if (!line.hasLine)
    {
      continue;
    }

    GeometryLineRecipeConfig recipe;
    recipe.enabled = line.enabled;
    recipe.id = line.id;
    recipe.partStart = QPointF(line.partStart.x, line.partStart.y);
    recipe.partEnd = QPointF(line.partEnd.x, line.partEnd.y);
    recipe.bandHalfWidth = line.bandHalfWidth;
    recipe.edgeSensitivity = line.edgeSensitivity;
    recipe.edgeCleanupDerivative = line.edgeCleanupDerivative;
    recipe.edgeStatisticalFilter = line.edgeStatisticalFilter;
    recipe.useSubpixel = line.useSubpixel;
    recipe.scanDirection = scanDirectionToRecipe(line.scanDirection);
    recipe.transition = transitionToRecipe(line.transition);
    recipe.pickMode = pickModeToRecipe(line.pickMode);
    recipes.append(recipe);
  }

  QString error;
  if (!m_recipeManager.saveGeometryLines(camera.id, recipes, &error))
  {
    appendLog(error);
  }
}

void MainWindow::activateGeometryLineDrawing(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  deactivateImageDrawingTools();
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Line;
  m_lineGeometryMouseControllers[camera.id].begin(activeGeometryLineConfig(camera.id).bandHalfWidth);
  m_largeImage->setGeometryOverlayPointEditingEnabled(false);
  m_largeImage->setGeometryPointPickingEnabled(true);
  updateGeometryLineOverlay(camera);
  appendLog(trText("log.geometryLineDrawing") + ": " + camera.id);
}

void MainWindow::handleGeometryLinePoint(const CameraConfig& camera, const QPointF& imagePoint)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  LineGeometryMouseController& controller = m_lineGeometryMouseControllers[camera.id];
  controller.setBandHalfWidth(activeGeometryLineConfig(camera.id).bandHalfWidth);
  const bool completed = controller.handleClick(imagePoint);
  updateGeometryLineOverlay(camera);

  if (!completed)
  {
    return;
  }

  const LineGeometryEditorState& state = controller.state();
  GeometryLineRuntimeConfig& config = activeGeometryLineConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageLine = true;

  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasLine = true;
  }

  m_largeImage->setGeometryPointPickingEnabled(false);
  m_largeImage->setGeometryOverlayPointEditingEnabled(true);
  updateGeometryLineOverlay(camera);
  appendLog(trText("log.geometryLineGuideSaved") + ": " + camera.id);
  saveGeometryLinesRecipe(camera);
  testGeometryLine(camera);
}

void MainWindow::handleGeometryLineHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint)
{
  if (camera.id != m_selectedCameraId || pointIndex < 0 || pointIndex > 1)
  {
    return;
  }

  LineGeometryMouseController& controller = m_lineGeometryMouseControllers[camera.id];
  controller.movePoint(pointIndex, imagePoint);

  const LineGeometryEditorState& state = controller.state();
  if (!state.hasStart || !state.hasEnd)
  {
    updateGeometryLineOverlay(camera);
    return;
  }

  GeometryLineRuntimeConfig& config = activeGeometryLineConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageLine = true;

  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasLine = true;
  }
  else
  {
    config.hasLine = false;
  }

  updateGeometryLineOverlay(camera);
  saveGeometryLinesRecipe(camera);
  testGeometryLine(camera);
}

GeometryOverlay MainWindow::configuredGeometryLinesOverlay(const CameraConfig& camera, bool includeActive) const
{
  GeometryOverlay overlay;
  const PartPose& pose = m_cameraRuntime.at(camera.id).currentPose();
  const QVector<GeometryLineRuntimeConfig> lines = m_geometryLineConfigs.value(camera.id);
  const int activeIndex = m_activeGeometryLineIndexes.value(camera.id, 0);

  for (int i = 0; i < lines.size(); ++i)
  {
    if (!includeActive && i == activeIndex)
    {
      continue;
    }

    const GeometryLineRuntimeConfig& line = lines[i];
    const bool usePartLine = pose.valid && line.hasLine;
    if (!usePartLine && !line.hasImageLine)
    {
      continue;
    }

    const cv::Point2d imageStart = usePartLine ? partToImage(pose, line.partStart) : line.imageStart;
    const cv::Point2d imageEnd = usePartLine ? partToImage(pose, line.partEnd) : line.imageEnd;
    const QPointF start(imageStart.x, imageStart.y);
    const QPointF end(imageEnd.x, imageEnd.y);
    const QPointF center = (start + end) * 0.5;
    const QPointF delta = end - start;
    const double length = std::hypot(delta.x(), delta.y());
    const double angleDegrees = std::atan2(delta.y(), delta.x()) * 180.0 / CV_PI;
    const bool highlightActive =
      m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry &&
      m_geometryDrawingTarget == GeometryDrawingTarget::Line &&
      i == activeIndex;
    const QColor lineColor = highlightActive ? QColor("#ff4fd8") : QColor(170, 205, 220, 170);
    const QColor bandColor = highlightActive ? QColor(0, 210, 255, 35) : QColor(120, 170, 190, 22);
    overlay.bands.append({center, QSizeF(length, line.bandHalfWidth * 2.0), angleDegrees, lineColor, bandColor});
    overlay.lines.append({start, end, lineColor, highlightActive ? 3 : 1});
    if (includeActive && highlightActive)
    {
      overlay.points.append({start, "1"});
      overlay.points.append({end, "2"});
    }
  }

  return overlay;
}

void MainWindow::updateGeometryLineOverlay(const CameraConfig& camera, const GeometryOverlay& extraOverlay)
{
  GeometryOverlay overlay = configuredGeometryLinesOverlay(camera, false);
  GeometryOverlay activeOverlay = m_lineGeometryMouseControllers[camera.id].overlay();
  for (const GeometryOverlayPoint& point : activeOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : activeOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : activeOverlay.bands)
  {
    overlay.bands.append(band);
  }

  for (const GeometryOverlayPoint& point : extraOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : extraOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : extraOverlay.bands)
  {
    overlay.bands.append(band);
  }
  appendCurrentPartPoseOverlay(camera, overlay);
  m_largeImage->setGeometryOverlay(overlay);
}

void MainWindow::testGeometryLine(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  GeometryLineRuntimeConfig& lineConfig = activeGeometryLineConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid && !lineConfig.hasLine && lineConfig.hasImageLine)
  {
    lineConfig.partStart = imageToPart(pose, lineConfig.imageStart);
    lineConfig.partEnd = imageToPart(pose, lineConfig.imageEnd);
    lineConfig.hasLine = true;
  }

  const bool usePartLine = pose.valid && lineConfig.hasLine;
  if (!usePartLine && !lineConfig.hasImageLine)
  {
    appendLog(trText("log.geometryLineRoiMissing") + ": " + camera.id);
    return;
  }

  const cv::Point2d imageStart = usePartLine ? partToImage(pose, lineConfig.partStart) : lineConfig.imageStart;
  const cv::Point2d imageEnd = usePartLine ? partToImage(pose, lineConfig.partEnd) : lineConfig.imageEnd;
  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  EdgeLineDetectorConfig config;
  config.id = lineConfig.id;
  config.label = lineConfig.id;
  config.guideStart = imageStart;
  config.guideEnd = imageEnd;
  config.bandHalfWidth = lineConfig.bandHalfWidth;
  config.edgeSensitivity = lineConfig.edgeSensitivity;
  config.edgeCleanupDerivative = lineConfig.edgeCleanupDerivative;
  config.edgeStatisticalFilter = lineConfig.edgeStatisticalFilter;
  config.useSubpixel = isBwDimensionalCamera(camera) && lineConfig.useSubpixel;
  config.scanDirection = lineConfig.scanDirection;
  config.transition = lineConfig.transition;
  config.pickMode = lineConfig.pickMode;

  // Run detection in background to avoid blocking UI using helper
  auto job = [input, config]() -> EdgeLineDetectorResult {
    EdgeLineDetector detector;
    return detector.detect(input, config);
  };

  const QString __pendingCameraId_testGeometryLine = camera.id;
  incPendingJobs(__pendingCameraId_testGeometryLine);
  runAsyncTask(decltype(job)(job), this, [this, camera, imageStart, imageEnd, __pendingCameraId_testGeometryLine](const EdgeLineDetectorResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testGeometryLine](void*) { decPendingJobs(__pendingCameraId_testGeometryLine); });
    if (!result.processed || result.diagnosticImage.empty())
    {
      appendLog(result.message.isEmpty() ? trText("log.geometryLineFailed") + ": " + camera.id : result.message);
      return;
    }

    m_selectedPreview = matToPixmap(result.diagnosticImage);
    m_largeImage->setImage(m_selectedPreview);
    m_largeImage->setRoi(QRect(result.searchRoi.x, result.searchRoi.y, result.searchRoi.width, result.searchRoi.height));

    LineGeometryMouseController& controller = m_lineGeometryMouseControllers[camera.id];
    controller.setLine(QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y), activeGeometryLineConfig(camera.id).bandHalfWidth);
    m_geometryDrawingTarget = GeometryDrawingTarget::Line;
    updateGeometryLineOverlay(camera);

    if (!result.found)
    {
      appendLog(result.message.isEmpty() ? trText("log.geometryLineNotFound") + ": " + camera.id : result.message);
      return;
    }

    GeometrySet& geometries = m_cameraRuntime[camera.id].geometries();
    for (int i = geometries.lines.size() - 1; i >= 0; --i)
    {
      if (geometries.lines[i].meta.id == result.line.meta.id)
      {
        geometries.lines.removeAt(i);
      }
    }
    geometries.lines.append(result.line);
    GeometryOverlay detectedOverlay;
    detectedOverlay.lines.append({
      QPointF(result.line.start.x, result.line.start.y),
      QPointF(result.line.end.x, result.line.end.y),
      QColor("#35c46a"),
      3
    });
    updateGeometryLineOverlay(camera, detectedOverlay);

    appendLog(QString("%1: %2 x1=%3 y1=%4 x2=%5 y2=%6")
                .arg(trText("log.geometryLineFound"))
                .arg(camera.id)
                .arg(result.line.start.x, 0, 'f', 1)
                .arg(result.line.start.y, 0, 'f', 1)
                .arg(result.line.end.x, 0, 'f', 1)
                .arg(result.line.end.y, 0, 'f', 1));
  });
}

void MainWindow::testConfiguredGeometryLines(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  loadGeometryLinesRecipe(camera);
  QVector<GeometryLineRuntimeConfig>& lines = m_geometryLineConfigs[camera.id];
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  cv::Mat diagnostic;
  if (input.channels() == 1)
  {
    cv::cvtColor(input, diagnostic, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(diagnostic);
  }

  GeometrySet& geometries = m_cameraRuntime[camera.id].geometries();
  geometries.lines.clear();
  GeometryOverlay detectedOverlay;

  for (int i = 0; i < lines.size(); ++i)
  {
    GeometryLineRuntimeConfig& line = lines[i];
    if (!line.enabled)
    {
      continue;
    }

    if (pose.valid && !line.hasLine && line.hasImageLine)
    {
      line.partStart = imageToPart(pose, line.imageStart);
      line.partEnd = imageToPart(pose, line.imageEnd);
      line.hasLine = true;
    }

    const bool usePartLine = pose.valid && line.hasLine;
    if (!usePartLine && !line.hasImageLine)
    {
      continue;
    }

    const cv::Point2d imageStart = usePartLine ? partToImage(pose, line.partStart) : line.imageStart;
    const cv::Point2d imageEnd = usePartLine ? partToImage(pose, line.partEnd) : line.imageEnd;

    EdgeLineDetectorConfig config;
    config.id = line.id;
    config.label = line.id;
    config.guideStart = imageStart;
    config.guideEnd = imageEnd;
    config.bandHalfWidth = line.bandHalfWidth;
    config.edgeSensitivity = line.edgeSensitivity;
    config.edgeCleanupDerivative = line.edgeCleanupDerivative;
    config.edgeStatisticalFilter = line.edgeStatisticalFilter;
    config.useSubpixel = isBwDimensionalCamera(camera) && line.useSubpixel;
    config.scanDirection = line.scanDirection;
    config.transition = line.transition;
    config.pickMode = line.pickMode;

    EdgeLineDetector detector;
    const EdgeLineDetectorResult result = detector.detect(input, config);
    if (!result.processed)
    {
      continue;
    }

    for (const cv::Point2d& point : result.edgePoints)
    {
      cv::circle(
        diagnostic,
        cv::Point(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y))),
        2,
        cv::Scalar(0, 0, 255),
        cv::FILLED,
        cv::LINE_AA);
    }

    if (!result.found)
    {
      continue;
    }

    geometries.lines.append(result.line);
    cv::line(
      diagnostic,
      cv::Point(static_cast<int>(std::round(result.line.start.x)), static_cast<int>(std::round(result.line.start.y))),
      cv::Point(static_cast<int>(std::round(result.line.end.x)), static_cast<int>(std::round(result.line.end.y))),
      cv::Scalar(0, 255, 0),
      2,
      cv::LINE_AA);
    detectedOverlay.lines.append({
      QPointF(result.line.start.x, result.line.start.y),
      QPointF(result.line.end.x, result.line.end.y),
      QColor("#35c46a"),
      3
    });
  }

  loadGeometryPointRecipe(camera);
  QVector<GeometryPointRuntimeConfig>& points = m_geometryPointConfigs[camera.id];
  geometries.points.clear();
  for (GeometryPointRuntimeConfig& point : points)
  {
    if (!point.enabled || !point.hasGuide || !pose.valid)
    {
      continue;
    }

    const cv::Point2d imageStart = partToImage(pose, point.partStart);
    const cv::Point2d imageEnd = partToImage(pose, point.partEnd);

    EdgePointDetectorConfig config;
    config.id = point.id;
    config.label = point.id;
    config.scanStart = imageStart;
    config.scanEnd = imageEnd;
    config.edgeSensitivity = point.edgeSensitivity;
    config.useSubpixel = isBwDimensionalCamera(camera) && point.useSubpixel;
    config.transition = point.transition;
    config.pickMode = point.pickMode;

    EdgePointDetector detector;
    const EdgePointDetectorResult result = detector.detect(input, config);
    if (result.processed)
    {
      cv::arrowedLine(
        diagnostic,
        cv::Point(static_cast<int>(std::round(imageStart.x)), static_cast<int>(std::round(imageStart.y))),
        cv::Point(static_cast<int>(std::round(imageEnd.x)), static_cast<int>(std::round(imageEnd.y))),
        cv::Scalar(255, 0, 255),
        2,
        cv::LINE_AA,
        0,
        0.08);
      for (const cv::Point2d& candidate : result.candidates)
      {
        cv::circle(
          diagnostic,
          cv::Point(static_cast<int>(std::round(candidate.x)), static_cast<int>(std::round(candidate.y))),
          2,
          cv::Scalar(0, 120, 255),
          cv::FILLED,
          cv::LINE_AA);
      }
      if (result.found)
      {
        geometries.points.append(result.point);
        GeometryDiagnosticDrawing::drawCyanPointCross(diagnostic, result.point.point);
        GeometryDiagnosticDrawing::appendCyanPointCross(detectedOverlay, result.point.point);
      }
    }
  }

  m_selectedPreview = matToPixmap(diagnostic);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->clearRoi();
  GeometryOverlay setupOverlay = configuredGeometryLinesOverlay(camera, true);
  GeometryOverlay pointOverlay = configuredGeometryPointsOverlay(camera, true);
  setupOverlay.points.clear();
  for (const GeometryOverlayLine& line : pointOverlay.lines)
  {
    setupOverlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : pointOverlay.bands)
  {
    setupOverlay.bands.append(band);
  }
  for (const GeometryOverlayLine& line : detectedOverlay.lines)
  {
    setupOverlay.lines.append(line);
  }
  for (const GeometryOverlayPoint& point : detectedOverlay.points)
  {
    setupOverlay.points.append(point);
  }
  appendCurrentPartPoseOverlay(camera, setupOverlay);
  m_largeImage->setGeometryOverlay(setupOverlay);
}

void MainWindow::activateGeometryPointDrawing(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  deactivateImageDrawingTools();
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Point;
  m_pointGeometryMouseControllers[camera.id].begin(3.0);
  m_largeImage->setGeometryOverlayPointEditingEnabled(false);
  m_largeImage->setGeometryPointPickingEnabled(true);
  updateGeometryPointOverlay(camera);
  appendLog(trText("log.geometryPointDrawing") + ": " + camera.id);
}

void MainWindow::activateGeometryCircleDrawing(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  deactivateImageDrawingTools();
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Circle;
  m_largeImage->setThreePointCircleDrawingEnabled(true);
  showConfiguredGeometryCircles(camera);
  appendLog(trText("log.geometryCircleDrawing") + ": " + camera.id);
}


void MainWindow::handleGeometryCirclePoints(const CameraConfig& camera, const QVector<QPoint>& points)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  ImageCircle imageCircle;
  if (!circleFromThreePoints(points, imageCircle))
  {
    appendLog(trText("log.surfaceEdgeCircleInvalid") + ": " + camera.id);
    return;
  }

  GeometryCircleRuntimeConfig& config = activeGeometryCircleConfig(camera.id);
  config.imageCenter = cv::Point2d(imageCircle.center.x(), imageCircle.center.y());
  config.radius = imageCircle.radius;
  config.hasImageCircle = true;

  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid)
  {
    config.partCenter = imageToPart(pose, config.imageCenter);
    config.hasCircle = true;
  }
  else
  {
    config.hasCircle = false;
  }

  m_largeImage->setThreePointCircleDrawingEnabled(false);
  m_activeDrawingRecipe = ActiveDrawingRecipe::Geometry;
  m_geometryDrawingTarget = GeometryDrawingTarget::Circle;
  saveGeometryCirclesRecipe(camera);
  showConfiguredGeometryCircles(camera);
  testGeometryCircle(camera);
}


void MainWindow::showConfiguredGeometryCircles(const CameraConfig& camera)
{
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  QVector<ImageCircle> circles;
  GeometryOverlay overlay;
  for (const GeometryCircleRuntimeConfig& circle : m_geometryCircleConfigs.value(camera.id))
  {
    const bool usePartCircle = pose.valid && circle.hasCircle;
    if (!usePartCircle && !circle.hasImageCircle)
    {
      continue;
    }

    const cv::Point2d center = usePartCircle ? partToImage(pose, circle.partCenter) : circle.imageCenter;
    if (circle.radius > 0.0)
    {
      circles.append({QPoint(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y))), static_cast<int>(std::round(circle.radius))});
      circles.append({QPoint(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y))), static_cast<int>(std::round(circle.radius + circle.outerBand))});
      circles.append({QPoint(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y))), qMax(1, static_cast<int>(std::round(circle.radius - circle.innerBand)))});
    }
  }
  m_largeImage->setCircles(circles);
  appendCurrentPartPoseOverlay(camera, overlay);
  m_largeImage->setGeometryOverlay(overlay);
}


void MainWindow::testGeometryCircle(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  GeometryCircleRuntimeConfig& circleConfig = activeGeometryCircleConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid && !circleConfig.hasCircle && circleConfig.hasImageCircle)
  {
    circleConfig.partCenter = imageToPart(pose, circleConfig.imageCenter);
    circleConfig.hasCircle = true;
    saveGeometryCirclesRecipe(camera);
  }

  const bool usePartCircle = pose.valid && circleConfig.hasCircle;
  if (!usePartCircle && !circleConfig.hasImageCircle)
  {
    showConfiguredGeometryCircles(camera);
    appendLog(trText("log.geometryCircleMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const cv::Point2d guideCenter = usePartCircle ? partToImage(pose, circleConfig.partCenter) : circleConfig.imageCenter;
  EdgeCircleDetectorConfig config;
  config.id = circleConfig.id;
  config.label = circleConfig.id;
  config.guideCenter = guideCenter;
  config.guideRadius = circleConfig.radius;
  config.innerBand = circleConfig.innerBand;
  config.outerBand = circleConfig.outerBand;
  config.edgeSensitivity = circleConfig.edgeSensitivity;
  config.edgeCleanupDerivative = circleConfig.edgeCleanupDerivative;
  config.edgeStatisticalFilter = circleConfig.edgeStatisticalFilter;
  config.useSubpixel = isBwDimensionalCamera(camera) && circleConfig.useSubpixel;
  config.transition = circleConfig.transition;
  config.pickMode = circleConfig.pickMode;

  EdgeCircleDetector detector;
  const EdgeCircleDetectorResult result = detector.detect(input, config);
  if (!result.processed || result.diagnosticImage.empty())
  {
    appendLog(result.message.isEmpty() ? trText("log.geometryCircleFailed") + ": " + camera.id : result.message);
    return;
  }

  m_selectedPreview = matToPixmap(result.diagnosticImage);
  m_largeImage->setImage(m_selectedPreview);
  showConfiguredGeometryCircles(camera);

  if (!result.found)
  {
    appendLog(result.message.isEmpty() ? trText("log.geometryCircleNotFound") + ": " + camera.id : result.message);
    return;
  }

  GeometrySet& geometries = m_cameraRuntime[camera.id].geometries();
  for (int i = geometries.circles.size() - 1; i >= 0; --i)
  {
    if (geometries.circles[i].meta.id == circleConfig.id)
    {
      geometries.circles.removeAt(i);
    }
  }
  geometries.circles.append(result.circle);
  appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
              .arg(trText("log.geometryCircleFound"))
              .arg(camera.id)
              .arg(result.circle.center.x, 0, 'f', 1)
              .arg(result.circle.center.y, 0, 'f', 1)
              .arg(result.circle.radius, 0, 'f', 1));
}


void MainWindow::handleGeometryPointGuidePoint(const CameraConfig& camera, const QPointF& imagePoint)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  LineGeometryMouseController& controller = m_pointGeometryMouseControllers[camera.id];
  const bool completed = controller.handleClick(imagePoint);
  updateGeometryPointOverlay(camera);

  if (!completed)
  {
    return;
  }

  const LineGeometryEditorState& state = controller.state();
  GeometryPointRuntimeConfig& config = activeGeometryPointConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageGuide = true;

  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasGuide = true;
  }

  m_largeImage->setGeometryPointPickingEnabled(false);
  m_largeImage->setGeometryOverlayPointEditingEnabled(true);
  updateGeometryPointOverlay(camera);
  appendLog(trText("log.geometryPointGuideSaved") + ": " + camera.id);
  saveGeometryPointRecipe(camera);
  testGeometryPoint(camera);
}


void MainWindow::handleGeometryPointHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint)
{
  if (camera.id != m_selectedCameraId || pointIndex < 0 || pointIndex > 1)
  {
    return;
  }

  LineGeometryMouseController& controller = m_pointGeometryMouseControllers[camera.id];
  controller.movePoint(pointIndex, imagePoint);

  const LineGeometryEditorState& state = controller.state();
  if (!state.hasStart || !state.hasEnd)
  {
    updateGeometryPointOverlay(camera);
    return;
  }

  GeometryPointRuntimeConfig& config = activeGeometryPointConfig(camera.id);
  config.imageStart = cv::Point2d(state.imageStart.x(), state.imageStart.y());
  config.imageEnd = cv::Point2d(state.imageEnd.x(), state.imageEnd.y());
  config.hasImageGuide = true;

  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid)
  {
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasGuide = true;
  }
  else
  {
    config.hasGuide = false;
  }

  updateGeometryPointOverlay(camera);
  saveGeometryPointRecipe(camera);
  testGeometryPoint(camera);
}


GeometryOverlay MainWindow::configuredGeometryPointsOverlay(const CameraConfig& camera, bool includeActive) const
{
  GeometryOverlay overlay;
  const PartPose& pose = m_cameraRuntime.at(camera.id).currentPose();
  const QVector<GeometryPointRuntimeConfig> points = m_geometryPointConfigs.value(camera.id);
  const int activeIndex = m_activeGeometryPointIndexes.value(camera.id, 0);

  for (int i = 0; i < points.size(); ++i)
  {
    if (!includeActive && i == activeIndex)
    {
      continue;
    }

    const GeometryPointRuntimeConfig& point = points[i];
    const bool usePartGuide = pose.valid && point.hasGuide;
    if (!usePartGuide && !point.hasImageGuide)
    {
      continue;
    }

    const cv::Point2d imageStart = usePartGuide ? partToImage(pose, point.partStart) : point.imageStart;
    const cv::Point2d imageEnd = usePartGuide ? partToImage(pose, point.partEnd) : point.imageEnd;
    const QPointF start(imageStart.x, imageStart.y);
    const QPointF end(imageEnd.x, imageEnd.y);
    const QPointF center = (start + end) * 0.5;
    const QPointF delta = end - start;
    const double length = std::hypot(delta.x(), delta.y());
    const double angleDegrees = std::atan2(delta.y(), delta.x()) * 180.0 / CV_PI;
    const bool highlightActive =
      m_activeDrawingRecipe == ActiveDrawingRecipe::Geometry &&
      m_geometryDrawingTarget == GeometryDrawingTarget::Point &&
      i == activeIndex;
    const QColor lineColor = highlightActive ? QColor("#ff4fd8") : QColor(170, 205, 220, 170);
    const QColor bandColor = highlightActive ? QColor(255, 79, 216, 24) : QColor(120, 170, 190, 18);
    overlay.bands.append({center, QSizeF(length, 6.0), angleDegrees, lineColor, bandColor});
    overlay.lines.append({start, end, lineColor, highlightActive ? 3 : 1});
    if (includeActive && highlightActive)
    {
      overlay.points.append({start, "1"});
      overlay.points.append({end, "2"});
    }
  }

  return overlay;
}


void MainWindow::updateGeometryPointOverlay(const CameraConfig& camera, const GeometryOverlay& extraOverlay)
{
  GeometryOverlay overlay = configuredGeometryPointsOverlay(camera, false);
  GeometryOverlay activeOverlay = m_pointGeometryMouseControllers[camera.id].overlay();
  for (const GeometryOverlayPoint& point : activeOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : activeOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : activeOverlay.bands)
  {
    overlay.bands.append(band);
  }
  for (const GeometryOverlayPoint& point : extraOverlay.points)
  {
    overlay.points.append(point);
  }
  for (const GeometryOverlayLine& line : extraOverlay.lines)
  {
    overlay.lines.append(line);
  }
  for (const GeometryOverlayBand& band : extraOverlay.bands)
  {
    overlay.bands.append(band);
  }
  appendCurrentPartPoseOverlay(camera, overlay);
  m_largeImage->setGeometryOverlay(overlay);
}


void MainWindow::appendCurrentPartPoseOverlay(const CameraConfig& camera, GeometryOverlay& overlay) const
{
  const auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt == m_cameraRuntime.end())
  {
    return;
  }

  const PartPose& pose = runtimeIt->second.currentPose();
  if (!pose.valid)
  {
    return;
  }

  GeometryDiagnosticDrawing::appendCyanPointCross(overlay, pose.origin);
}


void MainWindow::testGeometryPoint(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  GeometryPointRuntimeConfig& pointConfig = activeGeometryPointConfig(camera.id);
  const PartPose& pose = m_cameraRuntime[camera.id].currentPose();
  if (pose.valid && !pointConfig.hasGuide && pointConfig.hasImageGuide)
  {
    pointConfig.partStart = imageToPart(pose, pointConfig.imageStart);
    pointConfig.partEnd = imageToPart(pose, pointConfig.imageEnd);
    pointConfig.hasGuide = true;
  }

  const bool usePartGuide = pose.valid && pointConfig.hasGuide;
  if (!usePartGuide && !pointConfig.hasImageGuide)
  {
    updateGeometryPointOverlay(camera);
    return;
  }

  const cv::Point2d imageStart = usePartGuide ? partToImage(pose, pointConfig.partStart) : pointConfig.imageStart;
  const cv::Point2d imageEnd = usePartGuide ? partToImage(pose, pointConfig.partEnd) : pointConfig.imageEnd;
  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  EdgePointDetectorConfig config;
  config.id = pointConfig.id;
  config.label = pointConfig.id;
  config.scanStart = imageStart;
  config.scanEnd = imageEnd;
  config.edgeSensitivity = pointConfig.edgeSensitivity;
  config.useSubpixel = isBwDimensionalCamera(camera) && pointConfig.useSubpixel;
  config.transition = pointConfig.transition;
  config.pickMode = pointConfig.pickMode;

  EdgePointDetector detector;
  const EdgePointDetectorResult result = detector.detect(input, config);
  if (!result.processed || result.diagnosticImage.empty())
  {
    appendLog(result.message.isEmpty() ? trText("log.geometryPointFailed") + ": " + camera.id : result.message);
    return;
  }

  m_selectedPreview = matToPixmap(result.diagnosticImage);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->clearRoi();

  LineGeometryMouseController& controller = m_pointGeometryMouseControllers[camera.id];
  controller.setLine(QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y), 3.0);
  m_geometryDrawingTarget = GeometryDrawingTarget::Point;

  GeometryOverlay detectedOverlay;
  if (result.found)
  {
    GeometryDiagnosticDrawing::appendCyanPointCross(detectedOverlay, result.point.point);
  }
  updateGeometryPointOverlay(camera, detectedOverlay);

  if (!result.found)
  {
    appendLog(result.message.isEmpty() ? trText("log.geometryPointNotFound") + ": " + camera.id : result.message);
    return;
  }

  GeometrySet& geometries = m_cameraRuntime[camera.id].geometries();
  for (int i = geometries.points.size() - 1; i >= 0; --i)
  {
    if (geometries.points[i].meta.id == pointConfig.id)
    {
      geometries.points.removeAt(i);
    }
  }
  geometries.points.append(result.point);

  appendLog(QString("%1: %2 x=%3 y=%4")
              .arg(trText("log.geometryPointFound"))
              .arg(camera.id)
              .arg(result.point.point.x, 0, 'f', 1)
              .arg(result.point.point.y, 0, 'f', 1));
}


void MainWindow::startCameraSimulation(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    selectCamera(camera);
  }

  if (camera.type != "file")
  {
    appendLog(trText("log.cameraSourceUnsupported") + ": " + camera.id);
    return;
  }

  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  QString error;
  const QString testFolder = cameraTestImagesFolder(camera);
  if (!runtime.start(camera, testFolder, &error))
  {
    appendLog(error.isEmpty() ? trText("log.cameraStartFailed") + ": " + camera.id : error);
    showCameraSetupPanel(camera);
    return;
  }

  m_simulationTimer->start(runtime.intervalMs());
  appendLog(trText("log.cameraStarted") + ": " + camera.id);
  advanceCameraFrame(camera);
  showCameraSetupPanel(camera);
}


void MainWindow::stopCameraSimulation(const CameraConfig& camera)
{
  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  runtime.stop();

  if (camera.id == m_selectedCameraId)
  {
    m_simulationTimer->stop();
    showCameraSetupPanel(camera);
  }

  appendLog(trText("log.cameraStopped") + ": " + camera.id);
}


void MainWindow::stepCameraSimulation(const CameraConfig& camera)
{
  if (camera.type != "file")
  {
    appendLog(trText("log.cameraSourceUnsupported") + ": " + camera.id);
    return;
  }

  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  advanceCameraFrame(camera);
  showCameraSetupPanel(camera);
}


void MainWindow::advanceCameraFrame(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  QString error;
  const QString testFolder = cameraTestImagesFolder(camera);
  if (!runtime.step(camera, testFolder, &error))
  {
    m_simulationTimer->stop();
    appendLog(error.isEmpty() ? trText("log.cameraNoMoreFrames") + ": " + camera.id : error);
    return;
  }

  m_selectedPreview = matToPixmap(runtime.currentFrame());
  for (CameraTileWidget* tile : m_tiles)
  {
    if (tile->camera().id == camera.id)
    {
      tile->setPreview(m_selectedPreview);
      break;
    }
  }
  m_largeImage->setImage(m_selectedPreview);
  QElapsedTimer scanTimer;
  scanTimer.start();
  processCurrentCameraFrame(camera);
  m_lastSetupScanElapsedMs[camera.id] = scanTimer.elapsed();
  updateCameraSetupDetails(camera);
}


void MainWindow::processCurrentCameraFrame(const CameraConfig& camera)
{
  auto runConfiguredGeometry = [this, camera]() {
    testConfiguredGeometryLines(camera);
  };

  if (isBwDimensionalCamera(camera))
  {
    testLocalization(camera);
    runConfiguredGeometry();
    return;
  }

  if (!isGrayscaleLocalizationCamera(camera))
  {
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
    runConfiguredGeometry();
    return;
  }

  QRect roi;
  if (m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    testSurfaceEdgePcaLocalization(camera);
    runConfiguredGeometry();
  }
}


void MainWindow::refreshPoseForCurrentFrame(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  if (isBwDimensionalCamera(camera))
  {
    testLocalization(camera);
    return;
  }

  if (!isGrayscaleLocalizationCamera(camera))
  {
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
    return;
  }

  QRect roi;
  if (m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    testSurfaceEdgePcaLocalization(camera);
  }
}


void MainWindow::updateCameraSetupDetails(const CameraConfig& camera)
{
  if (!m_setupPanel || m_setupCameraId != camera.id)
  {
    return;
  }

  m_setupPanel->setDetailsText(cameraSetupDetailsText(camera));
}


QString MainWindow::cameraSetupDetailsText(const CameraConfig& camera) const
{
  const auto runtimeIt = m_cameraRuntime.find(camera.id);
  const CameraRuntime* runtime = runtimeIt == m_cameraRuntime.end() ? nullptr : &runtimeIt->second;
  const PartPose pose = runtime ? runtime->currentPose() : makeInvalidPartPose(camera.id);
  const QString samplePath = m_recipeManager.firstCameraSampleImagePath(camera.id);
  const QString testPath = m_recipeManager.firstCameraTestImagePath(camera.id);
  const qint64 scanElapsedMs = m_lastSetupScanElapsedMs.value(camera.id, -1);

  return QString("%1: %2\n%3: %4\n%5: %6\n%7: %8\n%9: %10\n%11: %12\n%13: %14\n%15: %16")
    .arg(trText("labels.source"), camera.type)
    .arg(trText("labels.folder"), resolvedCameraFolder(camera))
    .arg(trText("labels.sampleImage"), samplePath.isEmpty() ? trText("status.invalid") : samplePath)
    .arg(trText("labels.testImages"), testPath.isEmpty() ? trText("status.invalid") : m_recipeManager.cameraTestImagesPath(camera.id))
    .arg(trText("labels.status"), runtime ? runtimeStatusText(runtime->status()) : trText("status.stopped"))
    .arg(trText("labels.frame"), runtime ? QString::number(runtime->frameIndex()) : QStringLiteral("0"))
    .arg(
      trText("labels.partPose"),
      pose.valid
        ? QString("X=%1 Y=%2 A=%3 S=%4")
            .arg(pose.origin.x, 0, 'f', 1)
            .arg(pose.origin.y, 0, 'f', 1)
            .arg(pose.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
            .arg(pose.score, 0, 'f', 2)
        : trText("status.invalid"))
    .arg(trText("labels.scanTime"), scanElapsedMs >= 0 ? QString("%1 ms").arg(scanElapsedMs) : trText("status.invalid"));
}


void MainWindow::showSurfaceLocalizationStrategyPanel(const CameraConfig& camera, const QString& strategyId)
{
  deactivateImageDrawingTools();

  if (strategyId == "threshold" || strategyId == "edge")
  {
    QString error;
    if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, strategyId, &error))
    {
      appendLog(error);
      return;
    }

    showSurfaceLocalizationPanel(camera);
    return;
  }

  if (strategyId == "edgePca")
  {
    QString error;
    if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, strategyId, &error))
    {
      appendLog(error);
      return;
    }

    clearToolPanel();

    const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
    const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, m_translations);
    auto* panel = new QWidget(m_toolsContainer);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* title = new QLabel(strategy.label + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot), panel);
    title->setObjectName("toolPanelTitle");
    title->setWordWrap(true);
    layout->addWidget(title);

    auto* note = new QLabel(strategy.note, panel);
    note->setObjectName("toolPanelNote");
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QWidget(panel);
    auto* buttonsLayout = new QGridLayout(buttons);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(6);

    auto* roiButton = new QPushButton(trText("actions.surfaceSearchRoi"), buttons);
    auto* maskButton = new QPushButton(trText("actions.surfaceAddExclusion"), buttons);
    auto* clearButton = new QPushButton(trText("actions.surfaceClearExclusions"), buttons);
    auto* testButton = new QPushButton(trText("actions.testStrategy"), buttons);
    for (QPushButton* button : {roiButton, maskButton, clearButton, testButton})
    {
      button->setMinimumHeight(34);
    }
    connect(roiButton, &QPushButton::clicked, this, [this, camera]() { activateSurfaceDefectRoiDrawing(camera); });
    connect(maskButton, &QPushButton::clicked, this, [this, camera]() { activateSurfaceDefectExclusionDrawing(camera); });
    connect(clearButton, &QPushButton::clicked, this, [this, camera]() { clearSurfaceDefectExclusions(camera); });
    connect(testButton, &QPushButton::clicked, this, [this, camera]() { testSurfaceEdgePcaLocalization(camera); });
    buttonsLayout->addWidget(roiButton, 0, 0);
    buttonsLayout->addWidget(maskButton, 0, 1);
    buttonsLayout->addWidget(clearButton, 1, 0);
    buttonsLayout->addWidget(testButton, 1, 1);
    layout->addWidget(buttons);

    auto* sensitivityBox = new QGroupBox(trText("labels.edgeSensitivity"), panel);
    auto* sensitivityLayout = new QGridLayout(sensitivityBox);
    sensitivityLayout->setContentsMargins(8, 10, 8, 8);
    auto* sensitivityValue = new QLabel(QString::number(annulus.edgeSensitivity), sensitivityBox);
    sensitivityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* sensitivitySlider = new QSlider(Qt::Horizontal, sensitivityBox);
    sensitivitySlider->setRange(1, 255);
    sensitivitySlider->setValue(annulus.edgeSensitivity);
    sensitivityLayout->addWidget(sensitivityValue, 0, 1);
    sensitivityLayout->addWidget(sensitivitySlider, 1, 0, 1, 2);
    connect(sensitivitySlider, &QSlider::valueChanged, this, [this, camera, sensitivityValue](int value) {
      sensitivityValue->setText(QString::number(value));
      QString error;
      if (!m_recipeManager.saveSurfaceEdgeSensitivity(camera.id, value, &error))
      {
        appendLog(error);
        return;
      }
      testSurfaceEdgePcaLocalization(camera);
    });
    layout->addWidget(sensitivityBox);

    auto* backButton = new QPushButton(trText("groups.localizationStrategies"), panel);
    connect(backButton, &QPushButton::clicked, this, [this, camera]() {
      showLocalizationStrategyList(camera);
      appendLog(trText("log.backToCameraTools") + ": " + camera.id);
    });
    layout->addWidget(backButton);
    layout->addStretch(1);

    m_toolsLayout->addWidget(panel);
    appendLog(trText("log.toolPanel") + ": " + strategy.label);
    testSurfaceEdgePcaLocalization(camera);
    return;
  }

  if (strategyId == "model")
  {
    QString error;
    if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, strategyId, &error))
    {
      appendLog(error);
      return;
    }

    clearToolPanel();

    const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, m_translations);
    auto* panel = new QWidget(m_toolsContainer);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* title = new QLabel(strategy.label + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot), panel);
    title->setObjectName("toolPanelTitle");
    title->setWordWrap(true);
    layout->addWidget(title);

    auto* note = new QLabel(strategy.note, panel);
    note->setObjectName("toolPanelNote");
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QWidget(panel);
    auto* buttonsLayout = new QGridLayout(buttons);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(6);

    auto* roiButton = new QPushButton(trText("actions.surfaceSearchRoi"), buttons);
    auto* previewButton = new QPushButton(trText("actions.previewModel"), buttons);
    auto* acquireButton = new QPushButton(trText("actions.acquireModel"), buttons);
    auto* shapeButton = new QPushButton(trText("actions.testShapeModel"), buttons);
    auto* templateButton = new QPushButton(trText("actions.testTemplateModel"), buttons);
    for (QPushButton* button : {roiButton, previewButton, acquireButton, shapeButton, templateButton})
    {
      button->setMinimumHeight(34);
    }
    connect(roiButton, &QPushButton::clicked, this, [this, camera]() { activateSurfaceDefectRoiDrawing(camera); });
    connect(previewButton, &QPushButton::clicked, this, [this, camera]() { previewSurfaceModel(camera); });
    connect(acquireButton, &QPushButton::clicked, this, [this, camera]() { acquireSurfaceModel(camera); });
    connect(shapeButton, &QPushButton::clicked, this, [this, camera]() { testSurfaceShapeModel(camera); });
    connect(templateButton, &QPushButton::clicked, this, [this, camera]() { testSurfaceTemplateModel(camera); });
    buttonsLayout->addWidget(roiButton, 0, 0);
    buttonsLayout->addWidget(previewButton, 0, 1);
    buttonsLayout->addWidget(acquireButton, 1, 0);
    buttonsLayout->addWidget(shapeButton, 1, 1);
    buttonsLayout->addWidget(templateButton, 2, 0, 1, 2);
    layout->addWidget(buttons);

    SurfaceModelConfig model = m_recipeManager.loadSurfaceModel(camera.id);
    auto* modelBox = new QGroupBox(trText("groups.strategyControls"), panel);
    auto* modelLayout = new QGridLayout(modelBox);
    modelLayout->setContentsMargins(8, 10, 8, 8);
    modelLayout->setHorizontalSpacing(8);
    modelLayout->setVerticalSpacing(4);

    auto addSlider = [this, camera, modelLayout](const QString& labelText, int row, int minValue, int maxValue, int value, std::function<void(int)> handler, double displayScale = 1.0) {
      auto* label = new QLabel(labelText, modelLayout->parentWidget());
      auto* valueLabel = new QLabel(QString::number(value * displayScale, 'f', displayScale == 1.0 ? 0 : 2), modelLayout->parentWidget());
      valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      auto* slider = new QSlider(Qt::Horizontal, modelLayout->parentWidget());
      slider->setRange(minValue, maxValue);
      slider->setValue(value);
      modelLayout->addWidget(label, row, 0);
      modelLayout->addWidget(valueLabel, row, 1);
      modelLayout->addWidget(slider, row + 1, 0, 1, 2);
      connect(slider, &QSlider::valueChanged, this, [valueLabel, handler, displayScale](int sliderValue) {
        valueLabel->setText(QString::number(sliderValue * displayScale, 'f', displayScale == 1.0 ? 0 : 2));
        handler(sliderValue);
      });
    };

    addSlider(trText("labels.edgeSensitivity"), 0, 1, 255, model.edgeSensitivity, [this, camera](int value) {
      QString error;
      if (!m_recipeManager.saveSurfaceModelEdgeSensitivity(camera.id, value, &error))
      {
        appendLog(error);
      }
      previewSurfaceModel(camera);
    });
    addSlider(trText("labels.modelMaxShapeDistance"), 2, 1, 500, static_cast<int>(model.maxShapeDistance * 100.0), [this, camera](int value) {
      QString error;
      if (!m_recipeManager.saveSurfaceModelMaxShapeDistance(camera.id, value / 100.0, &error))
      {
        appendLog(error);
      }
    }, 0.01);
    addSlider(trText("labels.modelMinTemplateScore"), 4, 0, 100, static_cast<int>(model.minTemplateScore * 100.0), [this, camera](int value) {
      QString error;
      if (!m_recipeManager.saveSurfaceModelMinTemplateScore(camera.id, value / 100.0, &error))
      {
        appendLog(error);
      }
    }, 0.01);
    addSlider(trText("labels.modelAngleStart"), 6, -180, 180, static_cast<int>(model.angleStartDegrees), [this, camera](int value) {
      const SurfaceModelConfig current = m_recipeManager.loadSurfaceModel(camera.id);
      QString error;
      if (!m_recipeManager.saveSurfaceModelAngleRange(camera.id, value, current.angleEndDegrees, current.angleStepDegrees, &error))
      {
        appendLog(error);
      }
    });
    addSlider(trText("labels.modelAngleEnd"), 8, -180, 180, static_cast<int>(model.angleEndDegrees), [this, camera](int value) {
      const SurfaceModelConfig current = m_recipeManager.loadSurfaceModel(camera.id);
      QString error;
      if (!m_recipeManager.saveSurfaceModelAngleRange(camera.id, current.angleStartDegrees, value, current.angleStepDegrees, &error))
      {
        appendLog(error);
      }
    });
    addSlider(trText("labels.modelAngleStep"), 10, 1, 45, static_cast<int>(model.angleStepDegrees), [this, camera](int value) {
      const SurfaceModelConfig current = m_recipeManager.loadSurfaceModel(camera.id);
      QString error;
      if (!m_recipeManager.saveSurfaceModelAngleRange(camera.id, current.angleStartDegrees, current.angleEndDegrees, value, &error))
      {
        appendLog(error);
      }
    });
    layout->addWidget(modelBox);

    auto* backButton = new QPushButton(trText("groups.localizationStrategies"), panel);
    connect(backButton, &QPushButton::clicked, this, [this, camera]() {
      showLocalizationStrategyList(camera);
      appendLog(trText("log.backToCameraTools") + ": " + camera.id);
    });
    layout->addWidget(backButton);
    layout->addStretch(1);

    m_toolsLayout->addWidget(panel);
    appendLog(trText("log.toolPanel") + ": " + strategy.label);
    return;
  }

  clearToolPanel();

  const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, m_translations);
  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  auto* title = new QLabel(strategy.label + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(strategy.note, panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* controlsBox = new QGroupBox(trText("groups.strategyControls"), panel);
  auto* controlsLayout = new QVBoxLayout(controlsBox);
  controlsLayout->setSpacing(8);
  controlsLayout->addWidget(new QLabel(trText("labels.strategyPlanned"), controlsBox));
  controlsLayout->addWidget(new QPushButton(trText("actions.configureStrategy"), controlsBox));
  controlsLayout->addWidget(new QPushButton(trText("actions.testStrategy"), controlsBox));
  layout->addWidget(controlsBox);

  auto* backButton = new QPushButton(trText("groups.localizationStrategies"), panel);
  connect(backButton, &QPushButton::clicked, this, [this, camera]() {
    showLocalizationStrategyList(camera);
    appendLog(trText("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + strategy.label);
}


void MainWindow::showSurfaceLocalizationPanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);

  SurfaceLocalizationPanelWidget::Handlers handlers;
  handlers.drawOuterCircle = [this, camera]() { activateSurfaceOuterCircleDrawing(camera); };
  handlers.drawInnerCircle = [this, camera]() { activateSurfaceInnerCircleDrawing(camera); };
  handlers.drawEdgeCircle = [this, camera]() { activateSurfaceEdgeCircleCenterRadiusDrawing(camera); };
  handlers.drawThreePointCircle = [this, camera]() { activateSurfaceThreePointCircleDrawing(camera, SurfaceCircleTarget::None); };
  handlers.addExclusion = [this, camera]() { activateSurfaceDefectExclusionDrawing(camera); };
  handlers.clearExclusions = [this, camera]() { clearSurfaceDefectExclusions(camera); };
  handlers.methodChanged = [this, camera](const QString& method) { saveSurfaceMethodAndPreview(camera, method); };
  handlers.thresholdChanged = [this, camera](int value) { saveSurfaceThresholdAndPreview(camera, value); };
  handlers.edgeSensitivityChanged = [this, camera](int value) { saveSurfaceEdgeAndPreview(camera, value); };
  handlers.edgeBandChanged = [this, camera](int innerWidth, int outerWidth) { saveSurfaceEdgeBandAndPreview(camera, innerWidth, outerWidth); };
  handlers.edgeFitMaxErrorChanged = [this, camera](int value) { saveSurfaceEdgeFitMaxErrorAndPreview(camera, value); };
  handlers.back = [this, camera]() {
    showCameraToolList(camera);
    appendLog(trText("log.backToCameraTools") + ": " + camera.id);
  };

  auto* panel = new SurfaceLocalizationPanelWidget(
    ToolCatalog::label("surfaceLocalization", m_translations) + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot),
    annulus,
    m_translations,
    handlers,
    m_toolsContainer);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + ToolCatalog::label("surfaceLocalization", m_translations));

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
  }
}


void MainWindow::activateLocalizationRoiDrawing(const CameraConfig& camera)
{
  if (!isBwDimensionalCamera(camera))
  {
    appendLog(trText("log.localizationNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_largeImage->setRoiDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::Localization;
  appendLog(trText("log.localizationRoiDrawing") + ": " + camera.id);
}


void MainWindow::activateLocalizationExclusionDrawing(const CameraConfig& camera)
{
  if (!isBwDimensionalCamera(camera))
  {
    appendLog(trText("log.localizationNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_largeImage->setExclusionDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::Localization;
  appendLog(trText("log.localizationExclusionDrawing") + ": " + camera.id);
}


void MainWindow::clearLocalizationExclusions(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.clearLocalizationExclusionRects(camera.id, &error))
  {
    appendLog(error);
    return;
  }

  m_largeImage->clearExclusionRects();
  appendLog(trText("log.localizationExclusionsCleared") + ": " + camera.id);
}


void MainWindow::activateSurfaceDefectRoiDrawing(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_largeImage->setRoiDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceRoiDrawing") + ": " + camera.id);
}

void MainWindow::activateSurfaceDefectExclusionDrawing(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_largeImage->setExclusionDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceExclusionDrawing") + ": " + camera.id);
}

void MainWindow::clearSurfaceDefectExclusions(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.clearSurfaceLocalizationGeometry(camera.id, &error))
  {
    appendLog(error);
    return;
  }

  m_largeImage->clearExclusionRects();
  m_largeImage->clearCircles();
  m_lastSurfaceLocalizationResults.remove(camera.id);
  m_selectedPreview = loadCameraPreview(camera);
  m_largeImage->setImage(m_selectedPreview);
  appendLog(trText("log.surfaceGeometryCleared") + ": " + camera.id);
}
void MainWindow::activateSurfaceOuterCircleDrawing(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_surfaceCircleTarget = SurfaceCircleTarget::Outer;
  m_largeImage->setOuterCircleDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceOuterCircleDrawing") + ": " + camera.id);
}
void MainWindow::activateSurfaceInnerCircleDrawing(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);

  if (!annulus.hasOuterCircle || annulus.outerRadius <= 0)
  {
    appendLog(trText("log.surfaceOuterCircleMissing") + ": " + camera.id);
    return;
  }

  m_surfaceCircleTarget = SurfaceCircleTarget::Inner;
  m_largeImage->setInnerCircleDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceInnerCircleDrawing") + ": " + camera.id);
}

void MainWindow::activateSurfaceEdgeCircleCenterRadiusDrawing(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, "edge", &error))
  {
    appendLog(error);
    return;
  }

  m_surfaceCircleTarget = SurfaceCircleTarget::Edge;
  m_largeImage->setOuterCircleDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceEdgeCircleDrawing") + ": " + camera.id);
}

void MainWindow::activateSurfaceEdgeCircleDrawing(const CameraConfig& camera)
{
  activateSurfaceThreePointCircleDrawing(camera, SurfaceCircleTarget::Edge);
}

void MainWindow::activateSurfaceThreePointCircleDrawing(const CameraConfig& camera, SurfaceCircleTarget target)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  if (target == SurfaceCircleTarget::Edge)
  {
    QString error;

    if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, "edge", &error))
    {
      appendLog(error);
      return;
    }
  }

  m_surfaceCircleTarget = target;
  m_largeImage->setThreePointCircleDrawingEnabled(true);
  m_activeDrawingRecipe = ActiveDrawingRecipe::SurfaceDefects;
  appendLog(trText("log.surfaceThreePointCircleDrawing") + ": " + camera.id);
}

void MainWindow::saveSurfaceThresholdAndPreview(const CameraConfig& camera, int thresholdValue)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.saveSurfaceAnnulusThreshold(camera.id, 0, thresholdValue, &error))
  {
    appendLog(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);

  if (annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.innerRadius > 0 && annulus.outerRadius > annulus.innerRadius)
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindow::saveSurfaceEdgeAndPreview(const CameraConfig& camera, int sensitivity)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.saveSurfaceEdgeSensitivity(camera.id, sensitivity, &error))
  {
    appendLog(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  showStoredSurfaceLocalizationGeometry(camera, annulus);

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.innerRadius > 0 && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindow::saveSurfaceEdgeBandAndPreview(const CameraConfig& camera, int innerWidth, int outerWidth)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.saveSurfaceEdgeBand(camera.id, innerWidth, outerWidth, &error))
  {
    appendLog(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);

  if (annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner)
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindow::saveSurfaceEdgeFitMaxErrorAndPreview(const CameraConfig& camera, int maxError)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;
  if (!m_recipeManager.saveSurfaceEdgeFitMaxError(camera.id, maxError, &error))
  {
    appendLog(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  showStoredSurfaceLocalizationGeometry(camera, annulus);

  if (annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner)
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindow::saveSurfaceMethodAndPreview(const CameraConfig& camera, const QString& method)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString error;

  if (!m_recipeManager.saveSurfaceLocalizationMethod(camera.id, method, &error))
  {
    appendLog(error);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  showStoredSurfaceLocalizationGeometry(camera, annulus);

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.innerRadius > 0 && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
  }
}

void MainWindow::showStoredSurfaceLocalizationGeometry(const CameraConfig& camera, const SurfaceAnnulusLocalizationConfig& annulus)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  m_selectedPreview = loadCameraPreview(camera);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->setExclusionRects(m_recipeManager.loadSurfaceDefectExclusionRects(camera.id));

  if (annulus.method == "edge")
  {
    if (annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner)
    {
      m_largeImage->setCircles({
        {annulus.edgeCenter, annulus.edgeRadius + annulus.edgeBandOuter},
        {annulus.edgeCenter, qMax(1, annulus.edgeRadius - annulus.edgeBandInner)}
      });
      return;
    }

    m_largeImage->clearCircles();
    return;
  }

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
}

void MainWindow::testSurfaceAnnulusLocalization(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);

  if (annulus.method == "edge")
  {
    if (!annulus.hasEdgeCircle || annulus.edgeRadius <= annulus.edgeBandInner)
    {
      appendLog(trText("log.surfaceEdgeCircleMissing") + ": " + camera.id);
      return;
    }
  }
  else if (!annulus.hasOuterCircle || !annulus.hasInnerCircle || annulus.innerRadius <= 0 || annulus.outerRadius <= annulus.innerRadius)
  {
    appendLog(trText("log.surfaceAnnulusMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  SurfaceAnnulusThresholdConfig processorConfig;
  if (annulus.method == "edge")
  {
    processorConfig.center = cv::Point(annulus.edgeCenter.x(), annulus.edgeCenter.y());
    processorConfig.outerRadius = annulus.edgeRadius + annulus.edgeBandOuter;
    processorConfig.innerRadius = qMax(1, annulus.edgeRadius - annulus.edgeBandInner);
  }
  else
  {
    processorConfig.center = cv::Point(annulus.center.x(), annulus.center.y());
    processorConfig.outerRadius = annulus.outerRadius;
    processorConfig.innerRadius = annulus.innerRadius;
  }
  processorConfig.threshold.minValue = annulus.thresholdMin;
  processorConfig.threshold.maxValue = annulus.thresholdMax;
  processorConfig.edgeSensitivity = annulus.edgeSensitivity;
  processorConfig.edgeFitMaxError = annulus.edgeFitMaxError;

  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, processorConfig, exclusionRects, annulus]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    if (annulus.method == "edge")
    {
      return processor.locateAnnulusByEdge(input, processorConfig, toCvRects(exclusionRects));
    }
    return processor.locateAnnulusByGrayscaleThreshold(input, processorConfig, toCvRects(exclusionRects));
  };
  const QString __pendingCameraId_testSurfaceAnnulus = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceAnnulus);
  runAsyncTask(decltype(job)(job), this, [this, camera, annulus, exclusionRects, __pendingCameraId_testSurfaceAnnulus](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceAnnulus](void*) { decPendingJobs(__pendingCameraId_testSurfaceAnnulus); });
    if (!result.processed || result.diagnosticImage.empty())
    {
      appendLog(trText("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    m_selectedPreview = matToPixmap(result.diagnosticImage);
    m_largeImage->setImage(m_selectedPreview);
    m_largeImage->setExclusionRects(exclusionRects);
    m_largeImage->setCircles(annulus.method == "edge"
      ? QVector<ImageCircle>{
          {annulus.edgeCenter, annulus.edgeRadius + annulus.edgeBandOuter},
          {annulus.edgeCenter, qMax(1, annulus.edgeRadius - annulus.edgeBandInner)}
        }
      : QVector<ImageCircle>{{annulus.center, annulus.outerRadius}, {annulus.center, annulus.innerRadius}});

    if (result.blobs.empty())
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      appendLog(QString("%1: %2 blobs=0 min=%3 max=%4")
                  .arg(trText("log.surfaceNotFound"))
                  .arg(camera.id)
                  .arg(annulus.thresholdMin)
                  .arg(annulus.thresholdMax));
      return;
    }

    const SurfaceBlob& mainBlob = result.blobs.front();
    if (result.localization.found)
    {
      m_lastSurfaceLocalizationResults.insert(camera.id, result.localization);
      m_cameraRuntime[camera.id].setCurrentPose(partPoseFromSurfaceReference(camera, result.localization));
    }
    else
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
    }

    appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5 score=%6 area=%7 blobs=%8 min=%9 max=%10")
                .arg(trText("log.surfaceFound"))
                .arg(camera.id)
                .arg(result.localization.found ? result.localization.center.x : mainBlob.center.x, 0, 'f', 1)
                .arg(result.localization.found ? result.localization.center.y : mainBlob.center.y, 0, 'f', 1)
                .arg(result.localization.radius, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2)
                .arg(mainBlob.area, 0, 'f', 1)
                .arg(result.blobs.size())
                .arg(annulus.thresholdMin)
                .arg(annulus.thresholdMax));
  });
}

void MainWindow::testSurfaceLocalization(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QRect roi;

  if (!m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    appendLog(trText("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const SurfaceDefectSettings recipeSettings = m_recipeManager.loadSurfaceDefectSettings(camera.id);
  SurfaceThresholdSettings thresholdSettings;
  thresholdSettings.minValue = recipeSettings.thresholdMin;
  thresholdSettings.maxValue = recipeSettings.thresholdMax;

  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, roi, exclusionRects, thresholdSettings]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.detectByGrayscaleThreshold(
      input,
      cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
      toCvRects(exclusionRects),
      thresholdSettings);
  };

  const QString __pendingCameraId_testSurfaceLocalization = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceLocalization);
  runAsyncTask(decltype(job)(job), this, [this, camera, roi, exclusionRects, thresholdSettings, __pendingCameraId_testSurfaceLocalization](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceLocalization](void*) { decPendingJobs(__pendingCameraId_testSurfaceLocalization); });
    if (!result.processed || result.diagnosticImage.empty())
    {
      appendLog(trText("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    m_selectedPreview = matToPixmap(result.diagnosticImage);
    m_largeImage->setImage(m_selectedPreview);
    m_largeImage->setRoi(roi);
    m_largeImage->setExclusionRects(exclusionRects);

    if (result.blobs.empty())
    {
      appendLog(QString("%1: %2 blobs=0 min=%3 max=%4")
                  .arg(trText("log.surfaceNotFound"))
                  .arg(camera.id)
                  .arg(thresholdSettings.minValue)
                  .arg(thresholdSettings.maxValue));
      return;
    }

    const SurfaceBlob& mainBlob = result.blobs.front();
    appendLog(QString("%1: %2 cx=%3 cy=%4 area=%5 blobs=%6 min=%7 max=%8")
                .arg(trText("log.surfaceFound"))
                .arg(camera.id)
                .arg(mainBlob.center.x, 0, 'f', 1)
                .arg(mainBlob.center.y, 0, 'f', 1)
                .arg(mainBlob.area, 0, 'f', 1)
                .arg(result.blobs.size())
                .arg(thresholdSettings.minValue)
                .arg(thresholdSettings.maxValue));
  });
}

void MainWindow::testSurfaceLocalizationStrategy(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const SurfaceLocalizationStrategyConfig recipeStrategy = m_recipeManager.loadSurfaceLocalizationStrategy(camera.id);

  if (recipeStrategy.name != "two_circles_axis" || recipeStrategy.features.size() < 2)
  {
    appendLog(trText("log.surfaceStrategyMissing") + ": " + camera.id);
    return;
  }

  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);
  auto processorStrategy = toProcessorStrategy(recipeStrategy);

  auto job = [input, processorStrategy, exclusionRects]() -> SurfaceStrategyResult {
    SurfaceDefectProcessor processor;
    return processor.locateTwoCirclesAxis(input, processorStrategy, toCvRects(exclusionRects));
  };

  const QString __pendingCameraId_testSurfaceLocalizationStrategy = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceLocalizationStrategy);
  runAsyncTask(decltype(job)(job), this, [this, camera, exclusionRects, __pendingCameraId_testSurfaceLocalizationStrategy](const SurfaceStrategyResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceLocalizationStrategy](void*) { decPendingJobs(__pendingCameraId_testSurfaceLocalizationStrategy); });
    if (result.diagnosticImage.empty())
    {
      appendLog(trText("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    m_selectedPreview = matToPixmap(result.diagnosticImage);
    m_largeImage->setImage(m_selectedPreview);
    m_largeImage->setExclusionRects(exclusionRects);

    if (!result.found)
    {
      appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    appendLog(QString("%1: %2 originX=%3 originY=%4 features=%5")
                .arg(trText("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.origin.x, 0, 'f', 1)
                .arg(result.origin.y, 0, 'f', 1)
                .arg(result.features.size()));
  });
}

void MainWindow::testSurfaceEdgePcaLocalization(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera))
  {
    appendLog(trText("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QRect roi;
  if (!m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    appendLog(trText("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, roi, exclusionRects, annulus]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.locateByEdgePca(
      input,
      cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
      toCvRects(exclusionRects),
      annulus.edgeSensitivity);
  };

  const QString __pendingCameraId_testSurfaceEdgePca = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceEdgePca);
  runAsyncTask(decltype(job)(job), this, [this, camera, roi, exclusionRects, __pendingCameraId_testSurfaceEdgePca](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceEdgePca](void*) { decPendingJobs(__pendingCameraId_testSurfaceEdgePca); });
    if (!result.processed || result.diagnosticImage.empty())
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      appendLog(trText("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    m_selectedPreview = matToPixmap(result.diagnosticImage);
    m_largeImage->setImage(m_selectedPreview);
    m_largeImage->setRoi(roi);
    m_largeImage->setExclusionRects(exclusionRects);
    m_largeImage->clearCircles();

    if (!result.localization.found)
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    m_lastSurfaceLocalizationResults.insert(camera.id, result.localization);
    m_cameraRuntime[camera.id].setCurrentPose(partPoseFromSurfaceReference(camera, result.localization));
    appendLog(QString("%1: %2 cx=%3 cy=%4 angle=%5 score=%6")
                .arg(trText("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.localization.center.x, 0, 'f', 1)
                .arg(result.localization.center.y, 0, 'f', 1)
                .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2));
  });
}

void MainWindow::acquireSurfaceModel(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera) || camera.id != m_selectedCameraId)
  {
    return;
  }

  QRect roi;
  if (!m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    appendLog(trText("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const SurfaceModelConfig current = m_recipeManager.loadSurfaceModel(camera.id);
  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);
  SurfaceModelTrainer trainer;
  const SurfaceModelTrainingResult training = trainer.trainFromRoi(
    input,
    cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
    toCvRects(exclusionRects),
    current.edgeSensitivity);

  if (!training.trained || training.templateImage.empty() || training.contour.empty())
  {
    appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
    return;
  }

  const QString templatePath = m_recipeManager.surfaceModelTemplateImagePath(camera.id);
  QDir().mkpath(QFileInfo(templatePath).dir().absolutePath());
  if (!cv::imwrite(templatePath.toStdString(), training.templateImage))
  {
    appendLog("Impossibile salvare template modello: " + templatePath);
    return;
  }

  QVector<QPoint> contour;
  contour.reserve(static_cast<int>(training.contour.size()));
  for (const cv::Point& point : training.contour)
  {
    contour.append(QPoint(point.x, point.y));
  }

  QString error;
  if (!m_recipeManager.saveSurfaceModel(camera.id, roi, contour, templatePath, &error))
  {
    appendLog(error);
    return;
  }

  m_selectedPreview = matToPixmap(training.diagnosticImage);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->setRoi(roi);
  m_largeImage->setExclusionRects(exclusionRects);
  appendLog(QString("%1: %2 points=%3").arg(trText("actions.acquireModel")).arg(camera.id).arg(contour.size()));
}

void MainWindow::previewSurfaceModel(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera) || camera.id != m_selectedCameraId)
  {
    return;
  }

  QRect roi;
  if (!m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    appendLog(trText("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const SurfaceModelConfig current = m_recipeManager.loadSurfaceModel(camera.id);
  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);
  SurfaceModelTrainer trainer;
  const SurfaceModelTrainingResult training = trainer.trainFromRoi(
    input,
    cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
    toCvRects(exclusionRects),
    current.edgeSensitivity);

  if (training.diagnosticImage.empty())
  {
    appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
    return;
  }

  m_selectedPreview = matToPixmap(training.diagnosticImage);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->setRoi(roi);
  m_largeImage->setExclusionRects(exclusionRects);
  appendLog(QString("%1: %2 points=%3")
              .arg(trText("actions.previewModel"))
              .arg(camera.id)
              .arg(training.contour.size()));
}

void MainWindow::testSurfaceShapeModel(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera) || camera.id != m_selectedCameraId)
  {
    return;
  }

  const SurfaceModelConfig model = m_recipeManager.loadSurfaceModel(camera.id);
  if (!model.hasModel)
  {
    appendLog(trText("log.surfaceStrategyMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty() || model.contour.isEmpty())
  {
    appendLog(input.empty() ? imageError : trText("surfaceModel.missing"));
    return;
  }

  SurfaceShapeMatchConfig config;
  config.searchRoi = cv::Rect(model.searchRoi.x(), model.searchRoi.y(), model.searchRoi.width(), model.searchRoi.height());
  config.modelContour.reserve(static_cast<size_t>(model.contour.size()));
  for (const QPoint& point : model.contour)
  {
    config.modelContour.emplace_back(point.x(), point.y());
  }
  config.edgeSensitivity = model.edgeSensitivity;
  config.maxShapeDistance = model.maxShapeDistance;

  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, config, exclusionRects]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.locateByShapeMatching(input, config, toCvRects(exclusionRects));
  };

  const QString __pendingCameraId_testSurfaceShapeModel = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceShapeModel);
  runAsyncTask(decltype(job)(job), this, [this, camera, model, exclusionRects, __pendingCameraId_testSurfaceShapeModel](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceShapeModel](void*) { decPendingJobs(__pendingCameraId_testSurfaceShapeModel); });
    if (!result.processed || result.diagnosticImage.empty() || !result.localization.found)
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    m_lastSurfaceLocalizationResults.insert(camera.id, result.localization);
    m_cameraRuntime[camera.id].setCurrentPose(partPoseFromSurfaceReference(camera, result.localization));
    m_selectedPreview = matToPixmap(result.diagnosticImage);
    m_largeImage->setImage(m_selectedPreview);
    m_largeImage->setRoi(model.searchRoi);
    m_largeImage->setExclusionRects(exclusionRects);
    appendLog(QString("%1: %2 shape cx=%3 cy=%4 angle=%5 score=%6")
                .arg(trText("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.localization.center.x, 0, 'f', 1)
                .arg(result.localization.center.y, 0, 'f', 1)
                .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2));
  });
}

void MainWindow::testSurfaceTemplateModel(const CameraConfig& camera)
{
  if (!isGrayscaleLocalizationCamera(camera) || camera.id != m_selectedCameraId)
  {
    return;
  }

  const SurfaceModelConfig model = m_recipeManager.loadSurfaceModel(camera.id);
  if (!model.hasModel)
  {
    appendLog(trText("log.surfaceStrategyMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  const cv::Mat modelImage = cv::imread(model.templateImagePath.toStdString(), cv::IMREAD_COLOR);
  if (input.empty() || modelImage.empty())
  {
    appendLog(input.empty() ? imageError : trText("log.imageMissing") + ": " + model.templateImagePath);
    return;
  }

  SurfaceTemplateMatchConfig config;
  config.searchRoi = cv::Rect(model.searchRoi.x(), model.searchRoi.y(), model.searchRoi.width(), model.searchRoi.height());
  config.modelImage = modelImage;
  config.edgeSensitivity = model.edgeSensitivity;
  config.minScore = model.minTemplateScore;
  config.angleStartDegrees = model.angleStartDegrees;
  config.angleEndDegrees = model.angleEndDegrees;
  config.angleStepDegrees = model.angleStepDegrees;

  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, config, exclusionRects]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.locateByTemplateMatching(input, config, toCvRects(exclusionRects));
  };

  const QString __pendingCameraId_testSurfaceTemplateModel = camera.id;
  incPendingJobs(__pendingCameraId_testSurfaceTemplateModel);
  runAsyncTask(decltype(job)(job), this, [this, camera, model, exclusionRects, __pendingCameraId_testSurfaceTemplateModel](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceTemplateModel](void*) { decPendingJobs(__pendingCameraId_testSurfaceTemplateModel); });
    if (!result.processed || result.diagnosticImage.empty() || !result.localization.found)
    {
      m_lastSurfaceLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      appendLog(trText("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    m_lastSurfaceLocalizationResults.insert(camera.id, result.localization);
    m_cameraRuntime[camera.id].setCurrentPose(partPoseFromSurfaceReference(camera, result.localization));
    m_selectedPreview = matToPixmap(result.diagnosticImage);
    m_largeImage->setImage(m_selectedPreview);
    m_largeImage->setRoi(model.searchRoi);
    m_largeImage->setExclusionRects(exclusionRects);
    appendLog(QString("%1: %2 template cx=%3 cy=%4 angle=%5 score=%6")
                .arg(trText("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.localization.center.x, 0, 'f', 1)
                .arg(result.localization.center.y, 0, 'f', 1)
                .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2));
  });
}

void MainWindow::testLocalization(const CameraConfig& camera)
{
  if (!isBwDimensionalCamera(camera))
  {
    appendLog(trText("log.localizationNotAvailable") + ": " + camera.id);
    return;
  }

  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  QRect roi;

  if (!m_recipeManager.loadLocalizationRoi(camera.id, roi))
  {
    appendLog(trText("log.localizationRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = currentInputImage(camera, &imageError);
  if (input.empty())
  {
    appendLog(imageError);
    return;
  }

  const LocalizationSettings settings = m_recipeManager.loadLocalizationSettings(camera.id);
  const QVector<QRect> exclusionRects = m_recipeManager.loadLocalizationExclusionRects(camera.id);

  auto job = [input, roi, exclusionRects, settings]() -> LocalizationResult {
    LocalizationProcessor processor;
    return processor.locateDarkObjectOnLightBackground(
      input,
      cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
      toCvRects(exclusionRects),
      settings.thresholdFactor,
      settings.thresholdOffset);
  };
  const QString __pendingCameraId_testLocalization = camera.id;
  incPendingJobs(__pendingCameraId_testLocalization);
  runAsyncTask(decltype(job)(job), this, [this, camera, roi, settings, __pendingCameraId_testLocalization](const LocalizationResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testLocalization](void*) { decPendingJobs(__pendingCameraId_testLocalization); });
    if (result.diagnosticImage.empty())
    {
      m_lastLocalizationResults.remove(camera.id);
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      appendLog(trText("log.localizationFailed") + ": " + camera.id);
      return;
    }

    m_selectedPreview = matToPixmap(result.diagnosticImage);
    m_largeImage->setImage(m_selectedPreview);
    m_largeImage->setRoi(roi);
    m_lastLocalizationResults.insert(camera.id, result);

    if (!result.found)
    {
      m_cameraRuntime[camera.id].clearCurrentPose(camera.id);
      appendLog(trText("log.localizationNotFound") + ": " + camera.id);
      return;
    }

    m_cameraRuntime[camera.id].setCurrentPose(partPoseFromLocalizationResult(camera, result));
    appendLog(QString("%1: %2 cx=%3 cy=%4 angle=%5 area=%6 thr=%7 factor=%8 offset=%9")
                .arg(trText("log.localizationFound"))
                .arg(camera.id)
                .arg(result.center.x, 0, 'f', 1)
                .arg(result.center.y, 0, 'f', 1)
                .arg(result.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.area, 0, 'f', 1)
                .arg(result.thresholdValue, 0, 'f', 1)
                .arg(settings.thresholdFactor, 0, 'f', 3)
                .arg(settings.thresholdOffset, 0, 'f', 1));
  });
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
