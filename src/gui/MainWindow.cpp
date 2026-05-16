#include "MainWindow.h"

#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
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
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <functional>
#include <vector>

namespace
{
QString projectPath(const QString& relativePath)
{
  return QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(relativePath);
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

  largeLayout->addWidget(m_largeTitle);
  largeLayout->addWidget(m_largeImage, 1);
  m_imageStack->addWidget(largePage);

  auto* panelScrollArea = new QScrollArea(splitter);
  panelScrollArea->setWidgetResizable(true);
  panelScrollArea->setFrameShape(QFrame::NoFrame);
  panelScrollArea->setMinimumWidth(340);
  panelScrollArea->setMaximumWidth(460);

  auto* panel = new QWidget(panelScrollArea);
  panel->setMinimumWidth(340);
  auto* panelLayout = new QVBoxLayout(panel);
  panelLayout->setContentsMargins(14, 14, 14, 14);
  panelLayout->setSpacing(12);

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
  panelScrollArea->setWidget(panel);

  splitter->addWidget(m_imageStack);
  splitter->addWidget(panelScrollArea);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);

  rootLayout->addWidget(splitter);
  setCentralWidget(root);

  setStyleSheet(
    "QMainWindow,QWidget{background:#0f1419;color:#eef2f6;font-family:'Segoe UI';font-size:13px;}"
    "QGroupBox{border:1px solid #313b46;border-radius:6px;margin-top:10px;padding:10px 8px 8px 8px;font-weight:600;}"
    "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;color:#cdd6df;}"
    "QPushButton{background:#24313d;border:1px solid #3b4652;border-radius:5px;padding:8px;color:#f5f7fa;}"
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
  systemMenu->addSeparator();
  systemMenu->addAction(trText("commands.exit"), qApp, &QApplication::quit);
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

void MainWindow::setActiveRecipe(const QString& recipeId)
{
  m_recipeManager.setRecipeId(recipeId);
  QString error;

  if (!RecipeManager::saveActiveRecipeId(m_recipeManager.recipeId(), &error))
  {
    appendLog(error);
  }

  appendLog(trText("log.activeRecipe") + ": " + m_recipeManager.recipeId());
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
  m_selectedImagePath = firstImageInFolder(camera.folder);
  m_largeImage->setRoiDrawingEnabled(false);
  m_largeImage->setExclusionDrawingEnabled(false);
  m_largeImage->setOuterCircleDrawingEnabled(false);
  m_largeImage->setInnerCircleDrawingEnabled(false);
  m_activeDrawingRecipe = ActiveDrawingRecipe::None;
  m_largeImage->clearRoi();
  m_largeImage->clearExclusionRects();
  m_largeImage->clearCircles();

  for (CameraTileWidget* tile : m_tiles)
  {
    tile->setSelected(tile->camera().id == camera.id);
  }

  m_selectedPreview = m_selectedImagePath.isEmpty() ? QPixmap() : QPixmap(m_selectedImagePath);
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
      {"reloadConfig", "commands.reloadConfig"},
      {"exit", "commands.exit"}
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
      else if (commandId == "exit")
      {
        connect(button, &QPushButton::clicked, qApp, &QApplication::quit);
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

void MainWindow::showCameraToolList(const CameraConfig& camera)
{
  clearToolPanel();

  auto* gridButton = new QPushButton(trText("commands.gridView"), m_toolsContainer);
  gridButton->setMinimumHeight(38);
  connect(gridButton, &QPushButton::clicked, this, [this]() { showGridView(); });
  m_toolsLayout->addWidget(gridButton);

  if (isGrayscaleLocalizationCamera(camera))
  {
    auto* strategyTitle = new QLabel(trText("groups.localizationStrategies"), m_toolsContainer);
    strategyTitle->setObjectName("toolPanelTitle");
    m_toolsLayout->addWidget(strategyTitle);

    for (const SurfaceLocalizationStrategyDefinition& strategy : SurfaceLocalizationStrategies::all(m_translations))
    {
      auto* button = new QPushButton(strategy.label, m_toolsContainer);
      button->setMinimumHeight(40);
      connect(button, &QPushButton::clicked, this, [this, camera, strategy]() {
        showSurfaceLocalizationStrategyPanel(camera, strategy.id);
      });
      m_toolsLayout->addWidget(button);
    }

    auto* toolsTitle = new QLabel(trText("groups.cameraTools"), m_toolsContainer);
    toolsTitle->setObjectName("toolPanelNote");
    m_toolsLayout->addWidget(toolsTitle);
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

void MainWindow::showToolPanel(const CameraConfig& camera, const QString& toolId)
{
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

void MainWindow::showSurfaceLocalizationStrategyPanel(const CameraConfig& camera, const QString& strategyId)
{
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

  clearToolPanel();

  const SurfaceLocalizationStrategyDefinition strategy = SurfaceLocalizationStrategies::strategy(strategyId, m_translations);
  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

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

  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);
  connect(backButton, &QPushButton::clicked, this, [this, camera]() {
    showCameraToolList(camera);
    appendLog(trText("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + strategy.label);
}

void MainWindow::showSurfaceLocalizationPanel(const CameraConfig& camera)
{
  clearToolPanel();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(ToolCatalog::label("surfaceLocalization", m_translations) + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* drawingBox = new QGroupBox(trText("labels.circleDrawing"), panel);
  auto* drawingLayout = new QHBoxLayout(drawingBox);
  drawingLayout->setContentsMargins(8, 12, 8, 8);
  drawingLayout->setSpacing(8);

  auto* centerRadiusButton = new QPushButton(trText("labels.circleDrawingCenterRadius"), drawingBox);
  centerRadiusButton->setCheckable(true);
  centerRadiusButton->setChecked(true);
  centerRadiusButton->setMinimumHeight(38);
  auto* threePointButton = new QPushButton(trText("labels.circleDrawingThreePoints"), drawingBox);
  threePointButton->setCheckable(true);
  threePointButton->setMinimumHeight(38);
  connect(centerRadiusButton, &QPushButton::clicked, this, [centerRadiusButton, threePointButton]() {
    centerRadiusButton->setChecked(true);
    threePointButton->setChecked(false);
  });
  connect(threePointButton, &QPushButton::clicked, this, [this, camera, centerRadiusButton, threePointButton]() {
    centerRadiusButton->setChecked(false);
    threePointButton->setChecked(true);
    activateSurfaceThreePointCircleDrawing(camera, SurfaceCircleTarget::None);
  });
  drawingLayout->addWidget(centerRadiusButton);
  drawingLayout->addWidget(threePointButton);
  layout->addWidget(drawingBox);

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setSpacing(8);

  const QVector<QPair<QString, std::function<void()>>> actions = {
    {trText("actions.surfaceOuterCircle"), [this, camera, threePointButton]() {
      if (threePointButton->isChecked())
      {
        activateSurfaceThreePointCircleDrawing(camera, SurfaceCircleTarget::None);
        return;
      }

      activateSurfaceOuterCircleDrawing(camera);
    }},
    {trText("actions.surfaceInnerCircle"), [this, camera, threePointButton]() {
      if (threePointButton->isChecked())
      {
        activateSurfaceThreePointCircleDrawing(camera, SurfaceCircleTarget::None);
        return;
      }

      activateSurfaceInnerCircleDrawing(camera);
    }},
    {trText("actions.surfaceEdgeCircle"), [this, camera, threePointButton]() {
      if (threePointButton->isChecked())
      {
        activateSurfaceThreePointCircleDrawing(camera, SurfaceCircleTarget::None);
        return;
      }

      activateSurfaceEdgeCircleCenterRadiusDrawing(camera);
    }},
    {trText("actions.surfaceAddExclusion"), [this, camera]() { activateSurfaceDefectExclusionDrawing(camera); }},
    {trText("actions.surfaceClearExclusions"), [this, camera]() { clearSurfaceDefectExclusions(camera); }}
  };

  for (int i = 0; i < actions.size(); ++i)
  {
    auto* button = new QPushButton(actions[i].first, buttons);
    button->setMinimumHeight(40);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(button, &QPushButton::clicked, this, actions[i].second);
    buttonsLayout->addWidget(button, i / 2, i % 2);
  }

  buttons->setMinimumHeight(144);
  layout->addWidget(buttons);
  layout->addSpacing(10);

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  auto* methodBox = new QGroupBox(trText("labels.localizationMethod"), panel);
  auto* methodLayout = new QVBoxLayout(methodBox);
  methodLayout->setContentsMargins(8, 12, 8, 8);
  methodLayout->setSpacing(8);

  auto* methodCombo = new QComboBox(methodBox);
  methodCombo->addItem(trText("labels.methodThreshold"), "threshold");
  methodCombo->addItem(trText("labels.methodEdge"), "edge");
  const int methodIndex = methodCombo->findData(annulus.method);
  methodCombo->setCurrentIndex(methodIndex >= 0 ? methodIndex : 0);
  methodLayout->addWidget(methodCombo);

  auto* parameterTitle = new QLabel(methodCombo->currentData().toString() == "edge" ? trText("labels.edgeSensitivity") : trText("labels.threshold"), methodBox);
  parameterTitle->setObjectName("toolPanelNote");
  methodLayout->addWidget(parameterTitle);

  auto* parameterLabel = new QLabel(QString::number(methodCombo->currentData().toString() == "edge" ? annulus.edgeSensitivity : annulus.thresholdMax), methodBox);
  parameterLabel->setAlignment(Qt::AlignCenter);
  parameterLabel->setObjectName("toolPanelNote");
  methodLayout->addWidget(parameterLabel);

  auto* parameterSlider = new QSlider(Qt::Horizontal, methodBox);
  parameterSlider->setRange(0, 255);
  parameterSlider->setValue(methodCombo->currentData().toString() == "edge" ? annulus.edgeSensitivity : annulus.thresholdMax);
  parameterSlider->setMinimumHeight(28);
  methodLayout->addWidget(parameterSlider);

  auto* edgeBandBox = new QGroupBox(trText("labels.edgeBand"), methodBox);
  auto* edgeBandLayout = new QGridLayout(edgeBandBox);
  edgeBandLayout->setContentsMargins(8, 12, 8, 8);
  edgeBandLayout->setSpacing(6);

  auto* innerBandTitle = new QLabel(trText("labels.edgeBandInner"), edgeBandBox);
  auto* innerBandValue = new QLabel(QString::number(annulus.edgeBandInner), edgeBandBox);
  auto* innerBandSlider = new QSlider(Qt::Horizontal, edgeBandBox);
  innerBandSlider->setRange(1, 200);
  innerBandSlider->setValue(annulus.edgeBandInner);

  auto* outerBandTitle = new QLabel(trText("labels.edgeBandOuter"), edgeBandBox);
  auto* outerBandValue = new QLabel(QString::number(annulus.edgeBandOuter), edgeBandBox);
  auto* outerBandSlider = new QSlider(Qt::Horizontal, edgeBandBox);
  outerBandSlider->setRange(1, 200);
  outerBandSlider->setValue(annulus.edgeBandOuter);

  edgeBandLayout->addWidget(innerBandTitle, 0, 0);
  edgeBandLayout->addWidget(innerBandValue, 0, 1);
  edgeBandLayout->addWidget(innerBandSlider, 1, 0, 1, 2);
  edgeBandLayout->addWidget(outerBandTitle, 2, 0);
  edgeBandLayout->addWidget(outerBandValue, 2, 1);
  edgeBandLayout->addWidget(outerBandSlider, 3, 0, 1, 2);
  edgeBandBox->setVisible(methodCombo->currentData().toString() == "edge");
  methodLayout->addWidget(edgeBandBox);

  connect(methodCombo, &QComboBox::currentIndexChanged, this, [this, camera, methodCombo, parameterTitle, parameterLabel, parameterSlider, edgeBandBox](int) {
    const QString method = methodCombo->currentData().toString();
    const SurfaceAnnulusLocalizationConfig current = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
    const int value = method == "edge" ? current.edgeSensitivity : current.thresholdMax;
    parameterTitle->setText(method == "edge" ? trText("labels.edgeSensitivity") : trText("labels.threshold"));
    parameterLabel->setText(QString::number(value));
    parameterSlider->blockSignals(true);
    parameterSlider->setValue(value);
    parameterSlider->blockSignals(false);
    edgeBandBox->setVisible(method == "edge");
    saveSurfaceMethodAndPreview(camera, method);
  });
  connect(parameterSlider, &QSlider::valueChanged, this, [this, camera, methodCombo, parameterLabel](int value) {
    parameterLabel->setText(QString::number(value));
    const QString method = methodCombo->currentData().toString();

    if (method == "edge")
    {
      saveSurfaceEdgeAndPreview(camera, value);
      return;
    }

    saveSurfaceThresholdAndPreview(camera, value);
  });
  connect(innerBandSlider, &QSlider::valueChanged, this, [this, camera, innerBandValue, outerBandSlider](int value) {
    innerBandValue->setText(QString::number(value));
    saveSurfaceEdgeBandAndPreview(camera, value, outerBandSlider->value());
  });
  connect(outerBandSlider, &QSlider::valueChanged, this, [this, camera, outerBandValue, innerBandSlider](int value) {
    outerBandValue->setText(QString::number(value));
    saveSurfaceEdgeBandAndPreview(camera, innerBandSlider->value(), value);
  });
  layout->addWidget(methodBox);
  layout->addSpacing(6);

  auto* note = new QLabel(trText("labels.surfaceLocalizationNote"), panel);
  note->setWordWrap(true);
  note->setObjectName("toolPanelNote");
  layout->addWidget(note);

  auto* backButton = new QPushButton(trText("commands.backToCameraTools"), panel);
  connect(backButton, &QPushButton::clicked, this, [this, camera]() {
    showCameraToolList(camera);
    appendLog(trText("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + ToolCatalog::label("surfaceLocalization", m_translations));
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

  if (m_selectedImagePath.isEmpty())
  {
    appendLog(trText("log.imageMissing") + ": " + camera.id);
    return;
  }

  const cv::Mat input = cv::imread(m_selectedImagePath.toStdString(), cv::IMREAD_COLOR);

  if (input.empty())
  {
    appendLog(trText("log.imageMissing") + ": " + m_selectedImagePath);
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

  SurfaceDefectProcessor processor;
  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);
  const SurfaceDefectResult result = annulus.method == "edge"
    ? processor.locateAnnulusByEdge(input, processorConfig, toCvRects(exclusionRects))
    : processor.locateAnnulusByGrayscaleThreshold(input, processorConfig, toCvRects(exclusionRects));

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
    appendLog(QString("%1: %2 min=%3 max=%4")
                .arg(trText("log.surfaceNotFound"))
                .arg(camera.id)
                .arg(annulus.thresholdMin)
                .arg(annulus.thresholdMax));
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
              .arg(annulus.thresholdMin)
              .arg(annulus.thresholdMax));
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

  if (m_selectedImagePath.isEmpty())
  {
    appendLog(trText("log.imageMissing") + ": " + camera.id);
    return;
  }

  const cv::Mat input = cv::imread(m_selectedImagePath.toStdString(), cv::IMREAD_COLOR);

  if (input.empty())
  {
    appendLog(trText("log.imageMissing") + ": " + m_selectedImagePath);
    return;
  }

  const SurfaceDefectSettings recipeSettings = m_recipeManager.loadSurfaceDefectSettings(camera.id);
  SurfaceThresholdSettings thresholdSettings;
  thresholdSettings.minValue = recipeSettings.thresholdMin;
  thresholdSettings.maxValue = recipeSettings.thresholdMax;

  SurfaceDefectProcessor processor;
  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);
  const SurfaceDefectResult result = processor.detectByGrayscaleThreshold(
    input,
    cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
    toCvRects(exclusionRects),
    thresholdSettings);

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

  if (m_selectedImagePath.isEmpty())
  {
    appendLog(trText("log.imageMissing") + ": " + camera.id);
    return;
  }

  const cv::Mat input = cv::imread(m_selectedImagePath.toStdString(), cv::IMREAD_COLOR);

  if (input.empty())
  {
    appendLog(trText("log.imageMissing") + ": " + m_selectedImagePath);
    return;
  }

  const SurfaceLocalizationStrategyConfig recipeStrategy = m_recipeManager.loadSurfaceLocalizationStrategy(camera.id);

  if (recipeStrategy.name != "two_circles_axis" || recipeStrategy.features.size() < 2)
  {
    appendLog(trText("log.surfaceStrategyMissing") + ": " + camera.id);
    return;
  }

  SurfaceDefectProcessor processor;
  const QVector<QRect> exclusionRects = m_recipeManager.loadSurfaceDefectExclusionRects(camera.id);
  const SurfaceStrategyResult result = processor.locateTwoCirclesAxis(
    input,
    toProcessorStrategy(recipeStrategy),
    toCvRects(exclusionRects));

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

  if (m_selectedImagePath.isEmpty())
  {
    appendLog(trText("log.imageMissing") + ": " + camera.id);
    return;
  }

  const cv::Mat input = cv::imread(m_selectedImagePath.toStdString(), cv::IMREAD_COLOR);

  if (input.empty())
  {
    appendLog(trText("log.imageMissing") + ": " + m_selectedImagePath);
    return;
  }

  LocalizationProcessor processor;
  const LocalizationSettings settings = m_recipeManager.loadLocalizationSettings(camera.id);
  const QVector<QRect> exclusionRects = m_recipeManager.loadLocalizationExclusionRects(camera.id);
  const LocalizationResult result = processor.locateDarkObjectOnLightBackground(
    input,
    cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
    toCvRects(exclusionRects),
    settings.thresholdFactor,
    settings.thresholdOffset);

  if (result.diagnosticImage.empty())
  {
    m_lastLocalizationResults.remove(camera.id);
    appendLog(trText("log.localizationFailed") + ": " + camera.id);
    return;
  }

  m_selectedPreview = matToPixmap(result.diagnosticImage);
  m_largeImage->setImage(m_selectedPreview);
  m_largeImage->setRoi(roi);
  m_lastLocalizationResults.insert(camera.id, result);

  if (!result.found)
  {
    appendLog(trText("log.localizationNotFound") + ": " + camera.id);
    return;
  }

  appendLog(QString("%1: %2 cx=%3 cy=%4 angle=%5 area=%6 bg=%7 thr=%8 factor=%9 offset=%10")
              .arg(trText("log.localizationFound"))
              .arg(camera.id)
              .arg(result.center.x, 0, 'f', 1)
              .arg(result.center.y, 0, 'f', 1)
              .arg(result.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
              .arg(result.area, 0, 'f', 1)
              .arg(result.backgroundLevel, 0, 'f', 1)
              .arg(result.thresholdValue, 0, 'f', 1)
              .arg(settings.thresholdFactor, 0, 'f', 3)
              .arg(settings.thresholdOffset, 0, 'f', 1));
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

QPixmap MainWindow::loadCameraPreview(const CameraConfig& camera) const
{
  const QString imagePath = firstImageInFolder(camera.folder);

  if (imagePath.isEmpty())
  {
    return {};
  }

  return QPixmap(imagePath);
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

QString MainWindow::firstImageInFolder(const QString& folder) const
{
  QDir directory(projectPath(folder));
  const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tif", "*.tiff"};
  const QFileInfoList files = directory.entryInfoList(filters, QDir::Files, QDir::Name);

  if (files.isEmpty())
  {
    return {};
  }

  return files.first().absoluteFilePath();
}
