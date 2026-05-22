#include "gui/MainWindow.h"

#include "gui/modules/MainWindowCameraProfile.h"

#include "gui/SurfaceLocalizationPanelWidget.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"
#include "gui/TouchIconButton.h"
#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryMath.h"
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
#include <functional>
#include <vector>
#include <type_traits>
#include "util/AsyncExecutor.h"
#include <thread>
#include <memory>

using AsyncExecutor::runAsyncTask;

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
        m_setup.advanceCameraFrame(m_selectedCamera);
      }
    });
  }

  auto* root = new QWidget(this);
  auto* rootLayout = new QGridLayout(root);
  rootLayout->setContentsMargins(8, 8, 8, 8);
  rootLayout->setHorizontalSpacing(8);
  rootLayout->setVerticalSpacing(8);

  m_commandToolbar = new CommandToolbarWidget(root);
  m_commandToolbar->setLabels(trText("commands.start"),
                              trText("commands.stop"),
                              trText("commands.gridView"),
                              trText("commands.reloadConfig"),
                              trText("commands.toggleFullScreen"));
  m_commandToolbar->setStartHandler([this]() { startMachine(); });
  m_commandToolbar->setStopHandler([this]() { stopMachine(); });
  m_commandToolbar->setGridHandler([this]() { showGridView(); });
  m_commandToolbar->setReloadHandler([this]() { loadConfiguration(); });
  m_commandToolbar->setFullscreenHandler([this]() { toggleFullScreen(); });
  rootLayout->addWidget(m_commandToolbar, 0, 0, 1, 2);

  m_gridPage = new QWidget(root);
  m_gridPage->hide();
  m_gridContent = new QWidget(m_gridPage);
  m_gridContent->hide();
  m_gridLayout = new QGridLayout(m_gridContent);
  m_gridLayout->setContentsMargins(0, 0, 0, 0);
  m_gridLayout->setSpacing(0);

  m_imageStack = new QStackedWidget(root);

  auto* overviewPage = new QWidget(m_imageStack);
  auto* overviewLayout = new QVBoxLayout(overviewPage);
  overviewLayout->setContentsMargins(16, 16, 16, 16);
  overviewLayout->setSpacing(10);
  m_measurementResults = new MeasurementResultsWidget(overviewPage);
  m_measurementResults->setTitle(trText("tools.measurements"));
  overviewLayout->addWidget(m_measurementResults, 1);
  m_imageStack->addWidget(overviewPage);

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

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::SurfaceDefects)
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

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry)
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

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::SurfaceDefects)
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

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::SurfaceDefects)
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
    if (m_surface.circleTarget() == MainWindowSurfaceModule::CircleTarget::None)
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
        m_surface.testSurfaceAnnulusLocalization(m_selectedCamera);
      }
      return;
    }

    if (m_surface.circleTarget() == MainWindowSurfaceModule::CircleTarget::Edge)
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
      m_surface.testSurfaceAnnulusLocalization(m_selectedCamera);
      return;
    }

    const bool targetOuter = m_surface.circleTarget() == MainWindowSurfaceModule::CircleTarget::Inner ? false : outerCircle;

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

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry && m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Circle)
    {
      m_geometry.handleGeometryCirclePoints(m_selectedCamera, points);
      return;
    }

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry && m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Arc)
    {
      m_geometry.handleGeometryArcPoints(m_selectedCamera, points);
      return;
    }

    ImageCircle circle;

    if (!GeometryMath::circleFromThreePoints(points, circle))
    {
      appendLog(trText("log.surfaceEdgeCircleInvalid") + ": " + m_selectedCameraId);
      return;
    }

    QString error;

    if (m_surface.circleTarget() == MainWindowSurfaceModule::CircleTarget::None)
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
        m_surface.testSurfaceAnnulusLocalization(m_selectedCamera);
      }
      return;
    }

    if (m_surface.circleTarget() == MainWindowSurfaceModule::CircleTarget::Edge)
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
      m_surface.testSurfaceAnnulusLocalization(m_selectedCamera);
      return;
    }

    const bool targetOuter = m_surface.circleTarget() != MainWindowSurfaceModule::CircleTarget::Inner;

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
        m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry ||
        m_geometry.drawingTarget() != MainWindowGeometryModule::DrawingTarget::Line ||
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

    GeometryLineRuntimeConfig& config = m_geometry.activeGeometryLineConfig(m_selectedCameraId);
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
    m_geometry.setDrawingTarget(MainWindowGeometryModule::DrawingTarget::Line);
    m_largeImage->setGeometryAreaEditingEnabled(true);

    m_geometry.testGeometryLine(m_selectedCamera);
  });
  m_largeImage->setGeometryPointPickedHandler([this](const QPointF& imagePoint) {
    if (m_selectedCameraId.isEmpty() ||
        m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry)
    {
      return;
    }

    if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Line)
    {
      m_geometry.handleGeometryLinePoint(m_selectedCamera, imagePoint);
    }
    else if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Point)
    {
      m_geometry.handleGeometryPointGuidePoint(m_selectedCamera, imagePoint);
    }
  });
  m_largeImage->setGeometryPointMovedHandler([this](const QPointF& imagePoint) {
    if (m_selectedCameraId.isEmpty() ||
        m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry)
    {
      return;
    }

    if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Line)
    {
      LineGeometryMouseController& controller = m_geometry.lineMouseController(m_selectedCameraId);
      controller.handleMove(imagePoint);
      m_geometry.updateGeometryLineOverlay(m_selectedCamera);
    }
    else if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Point)
    {
      LineGeometryMouseController& controller = m_geometry.pointMouseController(m_selectedCameraId);
      controller.handleMove(imagePoint);
      m_geometry.updateGeometryPointOverlay(m_selectedCamera);
    }
  });
  m_largeImage->setGeometryOverlayPointMovedHandler([this](int pointIndex, const QPointF& imagePoint) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Line)
    {
      m_geometry.handleGeometryLineHandleMoved(m_selectedCamera, pointIndex, imagePoint);
    }
    else if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Point)
    {
      m_geometry.handleGeometryPointHandleMoved(m_selectedCamera, pointIndex, imagePoint);
    }
  });
  m_largeImage->setGeometryAreaChangedHandler([this](const ImageRotatedRect& area) {
    if (m_selectedCameraId.isEmpty() || m_geometry.drawingTarget() != MainWindowGeometryModule::DrawingTarget::Line)
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
    GeometryLineRuntimeConfig& config = m_geometry.activeGeometryLineConfig(m_selectedCameraId);
    config.partStart = imageToPart(pose, center - axis * (area.size.width() * 0.5));
    config.partEnd = imageToPart(pose, center + axis * (area.size.width() * 0.5));
    config.bandHalfWidth = std::max(2, static_cast<int>(std::round(area.size.height() * 0.5)));
    config.hasLine = true;
    m_geometry.testGeometryLine(m_selectedCamera);
    m_geometry.showGeometryLinePanel(m_selectedCamera);
  });

  largeLayout->addWidget(m_largeTitle);
  largeLayout->addWidget(m_largeImage, 1);
  m_imageStack->addWidget(largePage);

  m_cameraStrip = new CameraStripWidget(root);
  m_cameraStrip->setCameraClickHandler([this](const CameraConfig& camera) {
    if (camera.id == m_selectedCameraId)
    {
      showGridView();
      return;
    }
    selectCamera(camera);
  });

  auto* rightPanel = new QWidget(root);
  rightPanel->setMinimumWidth(480);
  rightPanel->setMaximumWidth(560);
  auto* rightLayout = new QHBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(8);

  m_toolIconBar = new ToolIconBarWidget(rightPanel);
  m_toolIconBar->setToolClickHandler([this](const QString& toolId) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }
    if (m_machineRunning)
    {
      appendLog("START: pannelli modifica disabilitati");
      updateControlPanel(&m_selectedCamera);
      return;
    }
    m_toolIconBar->setActiveTool(toolId);
    if (toolId == "setup")
    {
      m_setup.showCameraSetupPanel(m_selectedCamera);
    }
    else if (toolId == "constructedGeometries")
    {
      m_constructedGeometry.showConstructedGeometryPanel(m_selectedCamera);
    }
    else if (toolId == "measurements")
    {
      m_measurement.showMeasurementPanel(m_selectedCamera);
    }
    else if (toolId == "localization")
    {
      showLocalizationStrategyList(m_selectedCamera);
    }
    else
    {
      m_setup.showToolPanel(m_selectedCamera, toolId);
    }
  });
  rightLayout->addWidget(m_toolIconBar);

  auto* panelScrollArea = new QScrollArea(rightPanel);
  panelScrollArea->setWidgetResizable(true);
  panelScrollArea->setFrameShape(QFrame::NoFrame);
  panelScrollArea->setMinimumWidth(370);
  panelScrollArea->setMaximumWidth(470);

  auto* panel = new QWidget(panelScrollArea);
  panel->setMinimumWidth(370);
  auto* panelLayout = new QVBoxLayout(panel);
  panelLayout->setContentsMargins(10, 10, 10, 10);
  panelLayout->setSpacing(8);

  m_systemStatus = new QLabel(trText("status.systemReady"), panel);
  m_systemStatus->setObjectName("panelStatus");
  panelLayout->addWidget(m_systemStatus);

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

  auto* logBox = new QGroupBox(trText("groups.eventLog"), panel);
  auto* logLayout = new QVBoxLayout(logBox);
  m_log = new QTextEdit(logBox);
  m_log->setReadOnly(true);
  m_log->setMaximumHeight(130);
  logLayout->addWidget(m_log);
  panelLayout->addWidget(logBox);
  panelScrollArea->setWidget(panel);

  rightLayout->addWidget(panelScrollArea, 1);

  rootLayout->addWidget(m_imageStack, 1, 0);
  rootLayout->addWidget(rightPanel, 1, 1, 3, 1);
  rootLayout->addWidget(m_cameraStrip, 2, 0);
  rootLayout->setColumnStretch(0, 1);
  rootLayout->setColumnStretch(1, 0);
  rootLayout->setRowStretch(1, 1);
  setCentralWidget(root);

  setStyleSheet(
    "QMainWindow,QWidget{background:#0f1419;color:#eef2f6;font-family:'Segoe UI';font-size:13px;}"
    "QGroupBox{border:1px solid #313b46;border-radius:6px;margin-top:8px;padding:8px 7px 7px 7px;font-weight:600;}"
    "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;color:#cdd6df;}"
    "QPushButton{background:#24313d;border:1px solid #3b4652;border-radius:5px;padding:6px;color:#f5f7fa;}"
    "QPushButton:hover{background:#2e3d4b;}"
    "QPushButton#touchIconButton{min-width:58px;min-height:58px;max-width:68px;max-height:68px;border-radius:6px;padding:5px;}"
    "QToolButton{background:#24313d;border:1px solid #3b4652;border-radius:5px;padding:5px;color:#f5f7fa;}"
    "QToolButton:hover{background:#2e3d4b;}"
    "QToolButton:checked{background:#244a78;border:1px solid #4d9cff;}"
    "#cameraStrip QToolButton{min-width:78px;min-height:70px;max-height:78px;font-weight:700;padding:4px;}"
    "#cameraStrip QToolButton[state='READY']{border-color:#35c46a;}"
    "#cameraStrip QToolButton[state='OK']{border-color:#35c46a;}"
    "#cameraStrip QToolButton[state='BUSY']{border-color:#f5d547;}"
    "#cameraStrip QToolButton[state='NOK'],#cameraStrip QToolButton[state='ERROR']{border-color:#ff4f5e;}"
    "#cameraStrip QToolButton:checked{background:#244a78;border:2px solid #4d9cff;}"
    "QToolButton#touchIconButton{min-width:58px;min-height:58px;max-width:68px;max-height:68px;border-radius:6px;}"
    "QTextEdit{background:#111820;border:1px solid #313b46;border-radius:5px;color:#d7dee6;}"
    "QTableWidget{background:#111820;alternate-background-color:#151e27;border:1px solid #313b46;border-radius:5px;color:#d7dee6;gridline-color:#2d3741;}"
    "QHeaderView::section{background:#1d2731;color:#eef2f6;border:0;padding:4px;font-weight:600;}"
    "#commandToolbar,#cameraStrip,#measurementResults,#toolIconBar{background:#151c23;border:1px solid #303a45;border-radius:6px;}"
    "#toolbarRecipe{color:#cdd6df;font-weight:600;padding-right:12px;}"
    "#toolbarStatus{color:#35c46a;font-weight:700;}"
    "#measurementResultsTitle{font-weight:700;color:#f4f7fb;}"
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
  recipesMenu->addAction(trText("menu.selectRecipe"), this, [this]() { m_recipes.selectRecipe(); });
  recipesMenu->addAction(trText("menu.newRecipe"), this, [this]() { m_recipes.createRecipe(); });
  recipesMenu->addAction(trText("menu.duplicateRecipe"), this, [this]() { m_recipes.duplicateRecipe(); });
  recipesMenu->addAction(trText("menu.importRecipe"), this, [this]() { m_recipes.importRecipe(); });
  recipesMenu->addAction(trText("menu.exportRecipe"), this, [this]() { m_recipes.exportRecipe(); });

  QMenu* camerasMenu = menuBar()->addMenu(trText("menu.cameras"));
  for (const CameraConfig& camera : m_config.activeCameras())
  {
    camerasMenu->addAction(QString("%1 | %2").arg(camera.id, camera.displayName), this, [this, camera]() {
      m_cameraConfig.configureCameraSource(camera);
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
    startMachine();
  });
  systemMenu->addAction(trText("commands.stop"), this, [this]() {
    stopMachine();
  });
  systemMenu->addAction(trText("commands.gridView"), this, [this]() { showGridView(); });
  systemMenu->addAction(trText("commands.reloadConfig"), this, [this]() { loadConfiguration(); });
  systemMenu->addAction(trText("commands.toggleFullScreen"), this, [this]() { toggleFullScreen(); });
  systemMenu->addAction(trText("commands.setMaxThreads"), this, [this]() { setThreadLimitPrompt(); });
  QAction* detailedLogAction = systemMenu->addAction(trText("commands.enableDetailedLog"));
  detailedLogAction->setCheckable(true);
  detailedLogAction->setChecked(m_detailedLogger.enabled());
  connect(detailedLogAction, &QAction::toggled, this, [this](bool checked) {
    setDetailedLogEnabled(checked);
  });
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
  updateCameraStripStatus(cameraId);
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
  updateCameraStripStatus(cameraId);
  if (cameraId == m_selectedCameraId)
  {
    updateMeasurementResults();
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

void MainWindow::showGridView()
{
  deactivateImageDrawingTools();
  m_imageStack->setCurrentIndex(0);
  m_selectedCameraId.clear();
  m_selectedCamera = {};
  m_selectedPreview = {};
  m_selectedImagePath.clear();
  m_largeTitle->setText("Overview produzione");
  m_largeImage->clearImage();
  m_largeImage->clearRoi();
  m_largeImage->clearExclusionRects();
  m_largeImage->clearCircles();
  m_largeImage->clearGeometryOverlay();
  m_largeImage->hide();

  for (CameraTileWidget* tile : m_tiles)
  {
    tile->setSelected(false);
  }
  if (m_cameraStrip)
  {
    m_cameraStrip->setSelectedCamera({});
  }
  updateMeasurementResults();

  updateControlPanel(nullptr);
  appendLog(trText("log.gridView"));
}

void MainWindow::startMachine()
{
  m_machineRunning = true;
  deactivateImageDrawingTools();
  if (m_systemStatus)
  {
    m_systemStatus->setText(QString("START | %1 %2")
                              .arg(m_config.activeCameras().size())
                              .arg(trText("status.activeCameras")));
  }
  if (m_commandToolbar)
  {
    m_commandToolbar->setStatusText(m_systemStatus ? m_systemStatus->text() : "START");
  }
  updateControlPanel(m_selectedCameraId.isEmpty() ? nullptr : &m_selectedCamera);
  appendLog(trText("log.command") + ": " + trText("commands.start"));
}

void MainWindow::stopMachine()
{
  m_machineRunning = false;
  if (m_systemStatus)
  {
    m_systemStatus->setText(QString("%1 | %2 %3")
                              .arg(trText("status.systemReady"))
                              .arg(m_config.activeCameras().size())
                              .arg(trText("status.activeCameras")));
  }
  if (m_commandToolbar)
  {
    m_commandToolbar->setStatusText(m_systemStatus ? m_systemStatus->text() : trText("status.systemReady"));
  }
  updateControlPanel(m_selectedCameraId.isEmpty() ? nullptr : &m_selectedCamera);
  appendLog(trText("log.command") + ": " + trText("commands.stop"));
}

void MainWindow::selectCamera(const CameraConfig& camera)
{
  m_selectedCameraId = camera.id;
  m_selectedCamera = camera;
  m_selectedImagePath = m_imaging.cameraSampleImagePath(camera);
  deactivateImageDrawingTools();
  m_largeImage->show();
  m_largeImage->clearRoi();
  m_largeImage->clearExclusionRects();
  m_largeImage->clearCircles();
  m_largeImage->clearGeometryOverlay();

  for (CameraTileWidget* tile : m_tiles)
  {
    tile->setSelected(tile->camera().id == camera.id);
  }
  if (m_cameraStrip)
  {
    m_cameraStrip->setSelectedCamera(camera.id);
  }

  auto runtimeIt = m_cameraRuntime.find(camera.id);
  if (runtimeIt != m_cameraRuntime.end() && !runtimeIt->second.currentFrame().empty())
  {
    m_selectedPreview = m_imaging.matToPixmap(runtimeIt->second.currentFrame());
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
  if (MainWindowCameraProfile::isGrayscaleLocalization(camera, m_config))
  {
    if (m_recipeManager.loadSurfaceDefectRoi(camera.id, savedRoi))
    {
      m_largeImage->setRoi(savedRoi);
    }

    m_largeImage->setExclusionRects(m_recipeManager.loadSurfaceDefectExclusionRects(camera.id));
    m_largeImage->clearCircles();
    GeometryOverlay overlay;
    m_geometry.appendCurrentPartPoseOverlay(camera, overlay);
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
  updateMeasurementResults();
  appendLog(trText("log.cameraSelected") + ": " + camera.id);
}

void MainWindow::updateControlPanel(const CameraConfig* camera)
{
  clearToolPanel();

  if (!camera)
  {
    if (m_measurementResults)
    {
      m_measurementResults->show();
    }
    m_cameraDetails->setText("Overview produzione: misure aggregate di tutte le telecamere attive.");
    if (m_toolIconBar)
    {
      m_toolIconBar->setTools({});
    }
    const QVector<CameraConfig> activeCameras = m_config.activeCameras();
    int busyCount = 0;
    for (const CameraConfig& activeCamera : activeCameras)
    {
      if (m_cameraProcessingBusy.value(activeCamera.id, false))
      {
        ++busyCount;
      }
    }

    auto* title = new QLabel("Statistiche produzione", m_toolsContainer);
    title->setObjectName("toolPanelTitle");
    title->setWordWrap(true);
    m_toolsLayout->addWidget(title);

    auto* stats = new QLabel(
      QString("Telecamere attive: %1\nTelecamere occupate: %2\nTelecamere pronte: %3\nModalita: %4")
        .arg(activeCameras.size())
        .arg(busyCount)
        .arg(qMax(0, activeCameras.size() - busyCount))
        .arg(m_machineRunning ? "START" : "STOP"),
      m_toolsContainer);
    stats->setObjectName("toolPanelNote");
    stats->setWordWrap(true);
    m_toolsLayout->addWidget(stats);

    m_toolsLayout->addStretch(1);
    return;
  }

  m_cameraDetails->setText(QString("%1 %2 | %3")
    .arg(trText("labels.camera"))
    .arg(camera->slot)
    .arg(camera->displayName));

  if (m_measurementResults)
  {
    m_measurementResults->setVisible(m_machineRunning);
  }

  if (m_machineRunning)
  {
    if (m_toolIconBar)
    {
      m_toolIconBar->setTools({});
    }
    auto* title = new QLabel("Monitoraggio camera", m_toolsContainer);
    title->setObjectName("toolPanelTitle");
    title->setWordWrap(true);
    m_toolsLayout->addWidget(title);
    auto* note = new QLabel("Macchina in START: immagine, overlay e misure sono consultabili; setup, geometrie, misure e parametri non sono modificabili.", m_toolsContainer);
    note->setObjectName("toolPanelNote");
    note->setWordWrap(true);
    m_toolsLayout->addWidget(note);
    const bool busy = m_cameraProcessingBusy.value(camera->id, false);
    auto* stats = new QLabel(
      QString("Stato camera: %1\nFrame pending: %2\nUltimo scan: %3 ms")
        .arg(busy ? "BUSY" : "READY")
        .arg(m_cameraPendingJobs.value(camera->id, 0))
        .arg(m_lastSetupScanElapsedMs.value(camera->id, 0)),
      m_toolsContainer);
    stats->setObjectName("toolPanelNote");
    stats->setWordWrap(true);
    m_toolsLayout->addWidget(stats);
    m_toolsLayout->addStretch(1);
    return;
  }

  showCameraToolList(*camera);
}

void MainWindow::updateMeasurementResults()
{
  if (!m_measurementResults)
  {
    return;
  }

  if (m_selectedCameraId.isEmpty())
  {
    QVector<CameraMeasurementResultRow> rows;
    for (const CameraConfig& camera : m_config.activeCameras())
    {
      const auto runtimeIt = m_cameraRuntime.find(camera.id);
      if (runtimeIt == m_cameraRuntime.end())
      {
        continue;
      }

      const QVector<MeasurementResult>& measurements = runtimeIt->second.geometries().measurements;
      for (const MeasurementResult& measurement : measurements)
      {
        rows.append({camera.id, measurement});
      }
    }
    m_measurementResults->setAllCameraMeasurements(rows);
    return;
  }

  m_measurementResults->setMeasurements(
    m_selectedCameraId,
    m_cameraRuntime[m_selectedCameraId].geometries().measurements);
}

void MainWindow::updateCameraStripStatus(const QString& cameraId)
{
  if (!m_cameraStrip || cameraId.isEmpty())
  {
    return;
  }

  const bool busy = m_cameraProcessingBusy.value(cameraId, false);
  m_cameraStrip->setCameraBusy(cameraId, busy);
  if (!busy)
  {
    m_cameraStrip->setCameraResult(cameraId, "READY");
  }
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
  m_largeImage->setThreePointArcDrawingEnabled(false);
  m_largeImage->setGeometryPointPickingEnabled(false);
  m_largeImage->setGeometryOverlayPointEditingEnabled(false);
  m_largeImage->setTwoPointLineDrawingEnabled(false);
  m_activeDrawingRecipe = MainWindowActiveDrawingRecipe::None;
  m_surface.setCircleTarget(MainWindowSurfaceModule::CircleTarget::None);
  m_geometry.setDrawingTarget(MainWindowGeometryModule::DrawingTarget::None);
}

void MainWindow::showCameraToolList(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  m_returnToSetupCameraId.clear();
  clearToolPanel();

  QVector<ToolIconDefinition> tools = {
    {"setup", trText("tools.setup"), "setup"},
    {"constructedGeometries", trText("tools.constructedGeometries"), "constructedGeometries"},
    {"measurements", trText("tools.measurements"), "measurements"}
  };

  if (MainWindowCameraProfile::isGrayscaleLocalization(camera, m_config))
  {
    tools.prepend({"localization", trText("tools.localization"), "localization"});
  }

  for (const QString& tool : camera.profile.guiTools)
  {
    if (MainWindowCameraProfile::isGrayscaleLocalization(camera, m_config) && tool == "surfaceLocalization")
    {
      continue;
    }
    if (tool == "measurements")
    {
      continue;
    }
    tools.append({tool, ToolCatalog::label(tool, m_translations), tool});
  }

  if (m_toolIconBar)
  {
    m_toolIconBar->setTools(tools);
    m_toolIconBar->setActiveTool({});
  }

  auto* note = new QLabel(trText("labels.noToolSelected"), m_toolsContainer);
  note->setWordWrap(true);
  note->setObjectName("toolPanelNote");
  m_toolsLayout->addWidget(note);
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

  auto* strategyGrid = new QWidget(panel);
  auto* strategyLayout = new QGridLayout(strategyGrid);
  strategyLayout->setContentsMargins(0, 0, 0, 0);
  strategyLayout->setSpacing(6);

  int strategyIndex = 0;
  for (const SurfaceLocalizationStrategyDefinition& strategy : SurfaceLocalizationStrategies::all(m_translations))
  {
    const QString iconId = strategy.id == "threshold" ? "surfaceThreshold" :
      (strategy.id == "edge" ? "surfaceEdge" :
       (strategy.id == "edgePca" ? "surfacePca" :
        (strategy.id == "model" ? "surfaceModel" : "aiModel")));
    auto* button = createTouchIconButton(iconId, strategy.label, strategyGrid);
    connect(button, &QPushButton::clicked, this, [this, camera, strategy]() {
      m_surface.showSurfaceLocalizationStrategyPanel(camera, strategy.id);
    });
    strategyLayout->addWidget(button, strategyIndex / 4, strategyIndex % 4);
    ++strategyIndex;
  }
  layout->addWidget(strategyGrid);

  auto* backButton = createTouchIconButton("back", trText("commands.backToCameraTools"), panel);
  connect(backButton, &QPushButton::clicked, this, [this, camera]() {
    showCameraToolList(camera);
    appendLog(trText("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + trText("tools.localization"));
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
  const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
  const QString line = QString("[%1] %2").arg(timestamp, message);
  m_detailedLogger.logLine(message);

  if (!m_log)
  {
    return;
  }

  m_log->append(line);
}

void MainWindow::setDetailedLogEnabled(bool enabled)
{
  QSettings settings;
  if (!enabled)
  {
    appendLog(trText("log.detailedLogDisabled"));
    m_detailedLogger.setEnabled(false, {});
    settings.setValue("system/detailedLogEnabled", false);
    return;
  }

  QString error;
  const QString logRoot = QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("logs");
  if (!m_detailedLogger.setEnabled(true, logRoot, &error))
  {
    appendLog(error);
    settings.setValue("system/detailedLogEnabled", false);
    return;
  }

  settings.setValue("system/detailedLogEnabled", true);
  appendLog(QString("%1: %2").arg(trText("log.detailedLogEnabled"), m_detailedLogger.filePath()));
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

