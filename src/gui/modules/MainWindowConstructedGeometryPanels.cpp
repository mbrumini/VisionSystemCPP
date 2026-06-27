#include "gui/modules/MainWindowConstructedGeometryModule.h"

#include "gui/modules/ConstructedGeometryCircleSource.h"
#include "gui/modules/ConstructedGeometryLineSource.h"
#include "gui/modules/ConstructedGeometryPointSource.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/geometry/GeometryPanelNavigation.h"
#include "gui/TouchIconButton.h"
#include "runtime/CameraRuntime.h"

#include <QComboBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace
{
QPointF toPointF(const cv::Point2d& point)
{
  return QPointF(point.x, point.y);
}

QWidget* createPanel(QWidget* parent)
{
  return new QWidget(parent);
}

QVBoxLayout* createPanelLayout(QWidget* panel)
{
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);
  return layout;
}

void addTitle(QVBoxLayout* layout, QWidget* panel, const QString& title)
{
  auto* label = new QLabel(title, panel);
  label->setObjectName("toolPanelTitle");
  label->setWordWrap(true);
  layout->addWidget(label);
}

QGridLayout* addForm(QVBoxLayout* layout, QWidget* panel)
{
  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);
  layout->addWidget(form);
  return formLayout;
}

void addLineSources(QComboBox* combo, const QVector<ConstructedGeometryLineSource>& sources)
{
  for (const ConstructedGeometryLineSource& source : sources)
  {
    combo->addItem(source.label, source.id);
  }
}

void addCircleSources(QComboBox* combo, const QVector<ConstructedGeometryCircleSource>& sources)
{
  for (const ConstructedGeometryCircleSource& source : sources)
  {
    combo->addItem(source.label, source.id);
  }
}

void addPointSources(QComboBox* combo, const QVector<ConstructedGeometryPointSource>& sources)
{
  for (const ConstructedGeometryPointSource& source : sources)
  {
    combo->addItem(source.label, source.id);
  }
}

void appendSelectionPoint(GeometryOverlay& overlay,
                          const PointGeometry& point,
                          const QString& label,
                          const QColor& color)
{
  overlay.points.append({toPointF(point.point), label, color, 9.0});
}

void appendSelectionLine(GeometryOverlay& overlay,
                         const LineGeometry& line,
                         const QString& label,
                         const QColor& color)
{
  const QPointF start = toPointF(line.start);
  const QPointF end = toPointF(line.end);
  overlay.lines.append({start, end, color, 9});
  overlay.points.append({(start + end) * 0.5, label, color, 8.0});
}

void appendSelectionCircle(GeometryOverlay& overlay,
                           const CircleGeometry& circle,
                           const QString& label,
                           const QColor& color)
{
  constexpr int kSegments = 96;
  constexpr double kPi = 3.14159265358979323846;
  QPointF previous;
  for (int index = 0; index <= kSegments; ++index)
  {
    const double angle = 2.0 * kPi * static_cast<double>(index) / static_cast<double>(kSegments);
    const QPointF current(
      circle.center.x + std::cos(angle) * circle.radius,
      circle.center.y + std::sin(angle) * circle.radius);
    if (index > 0)
    {
      overlay.lines.append({previous, current, color, 7});
    }
    previous = current;
  }
  overlay.points.append({toPointF(circle.center), label, color, 8.0});
}
}

void MainWindowConstructedGeometryModule::refreshConstructedGeometrySources(const CameraConfig& camera)
{
  if (context().setup)
  {
    context().setup->refreshPoseForCurrentFrame(camera);
  }
  if (context().geometry)
  {
    context().geometry->testConfiguredGeometryLines(camera);
  }
  rebuildConstructedGeometryRecipe(camera);
  if (context().geometry)
  {
    context().geometry->syncRuntimeGeometryLabels(camera);
  }
}

QHash<QString, QString> MainWindowConstructedGeometryModule::geometryAliasesForCamera(const CameraConfig& camera) const
{
  if (!context().geometry)
  {
    return {};
  }

  return context().geometry->geometryAliasMap(camera.id);
}

void MainWindowConstructedGeometryModule::showConstructedGeometryPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);

  addTitle(layout, panel, QString("%1 | %2").arg(tr("tools.constructedGeometries"), camera.id));

  auto* note = new QLabel(tr("labels.constructedGeometryNote"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* buttonGrid = new QWidget(panel);
  auto* buttonLayout = new QGridLayout(buttonGrid);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(6);

  auto* lineLineButton = createTouchIconButton("lineLineIntersection", tr("actions.lineLineIntersection"), panel);
  QObject::connect(lineLineButton, &QPushButton::clicked, window(), [this, camera]() { showLineLineIntersectionPanel(camera); });
  buttonLayout->addWidget(lineLineButton, 0, 0);

  auto* lineCircleButton = createTouchIconButton("lineCircleIntersection", tr("actions.lineCircleIntersection"), panel);
  QObject::connect(lineCircleButton, &QPushButton::clicked, window(), [this, camera]() { showLineCircleIntersectionPanel(camera); });
  buttonLayout->addWidget(lineCircleButton, 0, 1);

  auto* circleCircleButton = createTouchIconButton("circleCircleIntersection", tr("actions.circleCircleIntersection"), panel);
  QObject::connect(circleCircleButton, &QPushButton::clicked, window(), [this, camera]() { showCircleCircleIntersectionPanel(camera); });
  buttonLayout->addWidget(circleCircleButton, 0, 2);

  auto* circleCenterButton = createTouchIconButton("circleCenter", tr("actions.circleCenter"), panel);
  QObject::connect(circleCenterButton, &QPushButton::clicked, window(), [this, camera]() { showCircleCenterPanel(camera); });
  buttonLayout->addWidget(circleCenterButton, 0, 3);

  auto* midpointButton = createTouchIconButton("midpoint", tr("actions.midpoint"), panel);
  QObject::connect(midpointButton, &QPushButton::clicked, window(), [this, camera]() { showMidpointPanel(camera); });
  buttonLayout->addWidget(midpointButton, 1, 0);

  auto* offsetButton = createTouchIconButton("offsetLine", tr("actions.offsetLine"), panel);
  QObject::connect(offsetButton, &QPushButton::clicked, window(), [this, camera]() { showOffsetLinePanel(camera); });
  buttonLayout->addWidget(offsetButton, 1, 1);

  auto* midlineButton = createTouchIconButton("offsetLine", tr("actions.midline"), panel);
  QObject::connect(midlineButton, &QPushButton::clicked, window(), [this, camera]() { showMidlinePanel(camera); });
  buttonLayout->addWidget(midlineButton, 1, 2);

  auto* bisectorButton = createTouchIconButton("angleBisector", tr("actions.angleBisector"), panel);
  QObject::connect(bisectorButton, &QPushButton::clicked, window(), [this, camera]() { showAngleBisectorPanel(camera); });
  buttonLayout->addWidget(bisectorButton, 1, 3);

  auto* tangentButton = createTouchIconButton("tangentLine", tr("actions.tangentLine"), panel);
  QObject::connect(tangentButton, &QPushButton::clicked, window(), [this, camera]() { showTangentLinePanel(camera); });
  buttonLayout->addWidget(tangentButton, 2, 0);

  auto* projectButton = createTouchIconButton("projectPoint", tr("actions.projectPoint"), panel);
  QObject::connect(projectButton, &QPushButton::clicked, window(), [this, camera]() { showProjectPointPanel(camera); });
  buttonLayout->addWidget(projectButton, 2, 1);

  auto* backButton = createTouchIconButton("back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (!GeometryPanelNavigation::returnToSetup(context(), camera))
    {
      context().showCameraToolList(camera);
    }
  });
  buttonLayout->addWidget(backButton, 2, 2);
  layout->addWidget(buttonGrid);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.constructedGeometryPanel") + ": " + camera.id);
}

void MainWindowConstructedGeometryModule::showLineLineIntersectionPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryLineSource> lineSources =
    constructedGeometryLineSources(cameraRuntime()[camera.id].geometries(), geometryAliasesForCamera(camera));
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.lineLineIntersection"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* firstCombo = new QComboBox(panel);
  auto* secondCombo = new QComboBox(panel);
  addLineSources(firstCombo, lineSources);
  addLineSources(secondCombo, lineSources);
  if (secondCombo->count() > 1) { secondCombo->setCurrentIndex(1); }

  const auto updateSourceOverlay = [this, camera, firstCombo, secondCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString firstId = firstCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, firstId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("A %1").arg(firstId), QColor("#ffcc00"));
    }
    const QString secondId = secondCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, secondId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("B %1").arg(secondId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(firstCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(secondCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);

  formLayout->addWidget(new QLabel(tr("labels.sourceLineA"), panel), 0, 0);
  formLayout->addWidget(firstCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceLineB"), panel), 1, 0);
  formLayout->addWidget(secondCombo, 1, 1);

  auto* createButton = createTouchIconButton("lineLineIntersection", tr("actions.lineLineIntersection"), panel);
  createButton->setEnabled(lineSources.size() >= 2);
  QObject::connect(createButton, &QPushButton::clicked, window(), [this, camera, firstCombo, secondCombo]() {
    createLineLineIntersection(camera, firstCombo->currentData().toString(), secondCombo->currentData().toString());
  });
  layout->addWidget(createButton);

  auto* backButton = createTouchIconButton("back", tr("tools.constructedGeometries"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showConstructedGeometryPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowConstructedGeometryModule::showLineCircleIntersectionPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(set, geometryAliasesForCamera(camera));
  const QVector<ConstructedGeometryCircleSource> circleSources = constructedGeometryCircleSources(set, geometryAliasesForCamera(camera));
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.lineCircleIntersection"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* lineCombo = new QComboBox(panel);
  auto* circleCombo = new QComboBox(panel);
  addLineSources(lineCombo, lineSources);
  addCircleSources(circleCombo, circleSources);

  const auto updateSourceOverlay = [this, camera, lineCombo, circleCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString lineId = lineCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, lineId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("L %1").arg(lineId), QColor("#ffcc00"));
    }
    const QString circleId = circleCombo->currentData().toString();
    CircleGeometry circle;
    if (constructedGeometryCircleSourceValue(refreshedSet, circleId, circle))
    {
      appendSelectionCircle(overlay, circle, QStringLiteral("C %1").arg(circleId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(lineCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(circleCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);

  formLayout->addWidget(new QLabel(tr("labels.sourceLine"), panel), 0, 0);
  formLayout->addWidget(lineCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceCircle"), panel), 1, 0);
  formLayout->addWidget(circleCombo, 1, 1);

  auto* createButton = createTouchIconButton("lineCircleIntersection", tr("actions.lineCircleIntersection"), panel);
  createButton->setEnabled(!lineSources.isEmpty() && !circleSources.isEmpty());
  QObject::connect(createButton, &QPushButton::clicked, window(), [this, camera, lineCombo, circleCombo]() {
    createLineCircleIntersection(camera, lineCombo->currentData().toString(), circleCombo->currentData().toString());
  });
  layout->addWidget(createButton);

  auto* backButton = createTouchIconButton("back", tr("tools.constructedGeometries"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showConstructedGeometryPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowConstructedGeometryModule::showCircleCircleIntersectionPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryCircleSource> circleSources =
    constructedGeometryCircleSources(cameraRuntime()[camera.id].geometries(), geometryAliasesForCamera(camera));
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.circleCircleIntersection"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* firstCombo = new QComboBox(panel);
  auto* secondCombo = new QComboBox(panel);
  addCircleSources(firstCombo, circleSources);
  addCircleSources(secondCombo, circleSources);
  if (secondCombo->count() > 1) { secondCombo->setCurrentIndex(1); }

  const auto updateSourceOverlay = [this, camera, firstCombo, secondCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    CircleGeometry circle;
    const QString firstId = firstCombo->currentData().toString();
    if (constructedGeometryCircleSourceValue(refreshedSet, firstId, circle))
    {
      appendSelectionCircle(overlay, circle, QStringLiteral("A %1").arg(firstId), QColor("#ffcc00"));
    }
    const QString secondId = secondCombo->currentData().toString();
    if (constructedGeometryCircleSourceValue(refreshedSet, secondId, circle))
    {
      appendSelectionCircle(overlay, circle, QStringLiteral("B %1").arg(secondId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(firstCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(secondCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);

  formLayout->addWidget(new QLabel(tr("labels.sourceCircleA"), panel), 0, 0);
  formLayout->addWidget(firstCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceCircleB"), panel), 1, 0);
  formLayout->addWidget(secondCombo, 1, 1);

  auto* createButton = createTouchIconButton("circleCircleIntersection", tr("actions.circleCircleIntersection"), panel);
  createButton->setEnabled(circleSources.size() >= 2);
  QObject::connect(createButton, &QPushButton::clicked, window(), [this, camera, firstCombo, secondCombo]() {
    createCircleCircleIntersection(camera, firstCombo->currentData().toString(), secondCombo->currentData().toString());
  });
  layout->addWidget(createButton);

  auto* backButton = createTouchIconButton("back", tr("tools.constructedGeometries"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showConstructedGeometryPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowConstructedGeometryModule::showCircleCenterPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryCircleSource> circleSources =
    constructedGeometryCircleSources(set, geometryAliasesForCamera(camera));
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.circleCenter"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* circleCombo = new QComboBox(panel);
  addCircleSources(circleCombo, circleSources);

  const auto updateSourceOverlay = [this, camera, circleCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString circleId = circleCombo->currentData().toString();
    CircleGeometry circle;
    if (constructedGeometryCircleSourceValue(refreshedSet, circleId, circle))
    {
      appendSelectionCircle(overlay, circle, QStringLiteral("C %1").arg(circleId), QColor("#ffcc00"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(circleCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);

  formLayout->addWidget(new QLabel(tr("labels.sourceCircleOrArc"), panel), 0, 0);
  formLayout->addWidget(circleCombo, 0, 1);

  auto* createButton = createTouchIconButton("circleCenter", tr("actions.circleCenter"), panel);
  createButton->setEnabled(!circleSources.isEmpty());
  QObject::connect(createButton, &QPushButton::clicked, window(), [this, camera, circleCombo]() {
    createCircleCenter(camera, circleCombo->currentData().toString());
  });
  layout->addWidget(createButton);

  auto* backButton = createTouchIconButton("back", tr("tools.constructedGeometries"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showConstructedGeometryPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowConstructedGeometryModule::showMidpointPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryPointSource> pointSources =
    constructedGeometryPointSources(cameraRuntime()[camera.id].geometries(), geometryAliasesForCamera(camera));
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.midpoint"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* firstCombo = new QComboBox(panel);
  auto* secondCombo = new QComboBox(panel);
  addPointSources(firstCombo, pointSources);
  addPointSources(secondCombo, pointSources);
  if (secondCombo->count() > 1) { secondCombo->setCurrentIndex(1); }

  const auto updateSourceOverlay = [this, camera, firstCombo, secondCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString firstId = firstCombo->currentData().toString();
    if (const PointGeometry* point = findConstructedGeometryPointSource(refreshedSet, firstId))
    {
      appendSelectionPoint(overlay, *point, QStringLiteral("A %1").arg(firstId), QColor("#ffcc00"));
    }
    const QString secondId = secondCombo->currentData().toString();
    if (const PointGeometry* point = findConstructedGeometryPointSource(refreshedSet, secondId))
    {
      appendSelectionPoint(overlay, *point, QStringLiteral("B %1").arg(secondId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(firstCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(secondCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);

  formLayout->addWidget(new QLabel(tr("labels.sourcePointA"), panel), 0, 0);
  formLayout->addWidget(firstCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourcePointB"), panel), 1, 0);
  formLayout->addWidget(secondCombo, 1, 1);

  auto* createButton = createTouchIconButton("midpoint", tr("actions.midpoint"), panel);
  createButton->setEnabled(pointSources.size() >= 2);
  QObject::connect(createButton, &QPushButton::clicked, window(), [this, camera, firstCombo, secondCombo]() {
    createMidpoint(camera, firstCombo->currentData().toString(), secondCombo->currentData().toString());
  });
  layout->addWidget(createButton);

  auto* backButton = createTouchIconButton("back", tr("tools.constructedGeometries"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showConstructedGeometryPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowConstructedGeometryModule::showOffsetLinePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryLineSource> lineSources =
    constructedGeometryLineSources(cameraRuntime()[camera.id].geometries(), geometryAliasesForCamera(camera));
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.offsetLine"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* lineCombo = new QComboBox(panel);
  addLineSources(lineCombo, lineSources);
  auto* offsetSpin = new QDoubleSpinBox(panel);
  offsetSpin->setRange(-10000.0, 10000.0);
  offsetSpin->setDecimals(2);
  offsetSpin->setSingleStep(1.0);
  offsetSpin->setValue(10.0);

  const auto updateSourceOverlay = [this, camera, lineCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString lineId = lineCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, lineId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("L %1").arg(lineId), QColor("#ffcc00"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(lineCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);

  formLayout->addWidget(new QLabel(tr("labels.sourceLine"), panel), 0, 0);
  formLayout->addWidget(lineCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.offsetPixels"), panel), 1, 0);
  formLayout->addWidget(offsetSpin, 1, 1);

  auto* createButton = createTouchIconButton("offsetLine", tr("actions.offsetLine"), panel);
  createButton->setEnabled(!lineSources.isEmpty());
  QObject::connect(createButton, &QPushButton::clicked, window(), [this, camera, lineCombo, offsetSpin]() {
    createOffsetLine(camera, lineCombo->currentData().toString(), offsetSpin->value());
  });
  layout->addWidget(createButton);

  auto* backButton = createTouchIconButton("back", tr("tools.constructedGeometries"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showConstructedGeometryPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowConstructedGeometryModule::showMidlinePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryLineSource> lineSources =
    constructedGeometryLineSources(cameraRuntime()[camera.id].geometries(), geometryAliasesForCamera(camera));
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.midline"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* firstCombo = new QComboBox(panel);
  auto* secondCombo = new QComboBox(panel);
  addLineSources(firstCombo, lineSources);
  addLineSources(secondCombo, lineSources);
  if (secondCombo->count() > 1) { secondCombo->setCurrentIndex(1); }

  const auto updateSourceOverlay = [this, camera, firstCombo, secondCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString firstId = firstCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, firstId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("A %1").arg(firstId), QColor("#ffcc00"));
    }
    const QString secondId = secondCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, secondId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("B %1").arg(secondId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(firstCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(secondCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  formLayout->addWidget(new QLabel(tr("labels.sourceLineA"), panel), 0, 0);
  formLayout->addWidget(firstCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceLineB"), panel), 1, 0);
  formLayout->addWidget(secondCombo, 1, 1);

  auto* createButton = createTouchIconButton("offsetLine", tr("actions.midline"), panel);
  createButton->setEnabled(lineSources.size() >= 2);
  QObject::connect(createButton, &QPushButton::clicked, window(), [this, camera, firstCombo, secondCombo]() {
    createMidline(camera, firstCombo->currentData().toString(), secondCombo->currentData().toString());
  });
  layout->addWidget(createButton);

  auto* backButton = createTouchIconButton("back", tr("tools.constructedGeometries"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showConstructedGeometryPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowConstructedGeometryModule::showAngleBisectorPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryLineSource> lineSources =
    constructedGeometryLineSources(cameraRuntime()[camera.id].geometries(), geometryAliasesForCamera(camera));
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.angleBisector"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* firstCombo = new QComboBox(panel);
  auto* secondCombo = new QComboBox(panel);
  addLineSources(firstCombo, lineSources);
  addLineSources(secondCombo, lineSources);
  if (secondCombo->count() > 1) { secondCombo->setCurrentIndex(1); }

  const auto updateSourceOverlay = [this, camera, firstCombo, secondCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString firstId = firstCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, firstId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("A %1").arg(firstId), QColor("#ffcc00"));
    }
    const QString secondId = secondCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, secondId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("B %1").arg(secondId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(firstCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(secondCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);

  formLayout->addWidget(new QLabel(tr("labels.sourceLineA"), panel), 0, 0);
  formLayout->addWidget(firstCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceLineB"), panel), 1, 0);
  formLayout->addWidget(secondCombo, 1, 1);

  auto* createButton = createTouchIconButton("angleBisector", tr("actions.angleBisector"), panel);
  createButton->setEnabled(lineSources.size() >= 2);
  QObject::connect(createButton, &QPushButton::clicked, window(), [this, camera, firstCombo, secondCombo]() {
    createAngleBisectors(camera, firstCombo->currentData().toString(), secondCombo->currentData().toString());
  });
  layout->addWidget(createButton);

  auto* backButton = createTouchIconButton("back", tr("tools.constructedGeometries"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showConstructedGeometryPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowConstructedGeometryModule::showTangentLinePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryPointSource> pointSources = constructedGeometryPointSources(set, geometryAliasesForCamera(camera));
  const QVector<ConstructedGeometryCircleSource> circleSources = constructedGeometryCircleSources(set, geometryAliasesForCamera(camera));
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.tangentLine"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* pointCombo = new QComboBox(panel);
  auto* circleCombo = new QComboBox(panel);
  addPointSources(pointCombo, pointSources);
  addCircleSources(circleCombo, circleSources);

  const auto updateSourceOverlay = [this, camera, pointCombo, circleCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString pointId = pointCombo->currentData().toString();
    if (const PointGeometry* point = findConstructedGeometryPointSource(refreshedSet, pointId))
    {
      appendSelectionPoint(overlay, *point, QStringLiteral("P %1").arg(pointId), QColor("#ffcc00"));
    }
    const QString circleId = circleCombo->currentData().toString();
    CircleGeometry circle;
    if (constructedGeometryCircleSourceValue(refreshedSet, circleId, circle))
    {
      appendSelectionCircle(overlay, circle, QStringLiteral("C %1").arg(circleId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(pointCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(circleCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);

  formLayout->addWidget(new QLabel(tr("labels.sourcePoint"), panel), 0, 0);
  formLayout->addWidget(pointCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceCircle"), panel), 1, 0);
  formLayout->addWidget(circleCombo, 1, 1);

  auto* createButton = createTouchIconButton("tangentLine", tr("actions.tangentLine"), panel);
  createButton->setEnabled(!pointSources.isEmpty() && !circleSources.isEmpty());
  QObject::connect(createButton, &QPushButton::clicked, window(), [this, camera, pointCombo, circleCombo]() {
    createTangentLines(camera, pointCombo->currentData().toString(), circleCombo->currentData().toString());
  });
  layout->addWidget(createButton);

  auto* backButton = createTouchIconButton("back", tr("tools.constructedGeometries"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showConstructedGeometryPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}

void MainWindowConstructedGeometryModule::showProjectPointPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryPointSource> pointSources = constructedGeometryPointSources(set, geometryAliasesForCamera(camera));
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(set, geometryAliasesForCamera(camera));
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.projectPoint"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* pointCombo = new QComboBox(panel);
  auto* lineCombo = new QComboBox(panel);
  addPointSources(pointCombo, pointSources);
  addLineSources(lineCombo, lineSources);

  const auto updateSourceOverlay = [this, camera, pointCombo, lineCombo]() {
    if (!context().geometry || !largeImage())
    {
      return;
    }
    context().geometry->showRuntimeGeometryOverlay(camera);
    const GeometrySet& refreshedSet = cameraRuntime()[camera.id].geometries();
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    const QString pointId = pointCombo->currentData().toString();
    if (const PointGeometry* point = findConstructedGeometryPointSource(refreshedSet, pointId))
    {
      appendSelectionPoint(overlay, *point, QStringLiteral("P %1").arg(pointId), QColor("#ffcc00"));
    }
    const QString lineId = lineCombo->currentData().toString();
    if (const LineGeometry* line = findConstructedGeometryLineSource(refreshedSet, lineId))
    {
      appendSelectionLine(overlay, *line, QStringLiteral("L %1").arg(lineId), QColor("#00e5ff"));
    }
    largeImage()->setGeometryOverlay(overlay);
  };
  QObject::connect(pointCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);
  QObject::connect(lineCombo, qOverload<int>(&QComboBox::currentIndexChanged), window(), updateSourceOverlay);

  formLayout->addWidget(new QLabel(tr("labels.sourcePoint"), panel), 0, 0);
  formLayout->addWidget(pointCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceLine"), panel), 1, 0);
  formLayout->addWidget(lineCombo, 1, 1);

  auto* createButton = createTouchIconButton("projectPoint", tr("actions.projectPoint"), panel);
  createButton->setEnabled(!pointSources.isEmpty() && !lineSources.isEmpty());
  QObject::connect(createButton, &QPushButton::clicked, window(), [this, camera, pointCombo, lineCombo]() {
    createProjectedPoint(camera, pointCombo->currentData().toString(), lineCombo->currentData().toString());
  });
  layout->addWidget(createButton);

  auto* backButton = createTouchIconButton("back", tr("tools.constructedGeometries"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showConstructedGeometryPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);
  toolsLayout()->addWidget(panel);
  updateSourceOverlay();
}
