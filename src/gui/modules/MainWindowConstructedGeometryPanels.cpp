#include "gui/modules/MainWindowConstructedGeometryModule.h"

#include "gui/modules/ConstructedGeometryCircleSource.h"
#include "gui/modules/ConstructedGeometryLineSource.h"
#include "gui/modules/ConstructedGeometryPointSource.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/geometry/GeometryPanelNavigation.h"
#include "gui/TouchIconButton.h"
#include "runtime/CameraRuntime.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
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

  auto* bisectorButton = createTouchIconButton("angleBisector", tr("actions.angleBisector"), panel);
  QObject::connect(bisectorButton, &QPushButton::clicked, window(), [this, camera]() { showAngleBisectorPanel(camera); });
  buttonLayout->addWidget(bisectorButton, 1, 2);

  auto* tangentButton = createTouchIconButton("tangentLine", tr("actions.tangentLine"), panel);
  QObject::connect(tangentButton, &QPushButton::clicked, window(), [this, camera]() { showTangentLinePanel(camera); });
  buttonLayout->addWidget(tangentButton, 1, 3);

  auto* projectButton = createTouchIconButton("projectPoint", tr("actions.projectPoint"), panel);
  QObject::connect(projectButton, &QPushButton::clicked, window(), [this, camera]() { showProjectPointPanel(camera); });
  buttonLayout->addWidget(projectButton, 2, 0);

  auto* backButton = createTouchIconButton("back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (!GeometryPanelNavigation::returnToSetup(context(), camera))
    {
      context().showCameraToolList(camera);
    }
  });
  buttonLayout->addWidget(backButton, 2, 1);
  layout->addWidget(buttonGrid);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.constructedGeometryPanel") + ": " + camera.id);
}

void MainWindowConstructedGeometryModule::showLineLineIntersectionPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(cameraRuntime()[camera.id].geometries());
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.lineLineIntersection"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* firstCombo = new QComboBox(panel);
  auto* secondCombo = new QComboBox(panel);
  addLineSources(firstCombo, lineSources);
  addLineSources(secondCombo, lineSources);
  if (secondCombo->count() > 1) { secondCombo->setCurrentIndex(1); }

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
}

void MainWindowConstructedGeometryModule::showLineCircleIntersectionPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(set);
  const QVector<ConstructedGeometryCircleSource> circleSources = constructedGeometryCircleSources(set);
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.lineCircleIntersection"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* lineCombo = new QComboBox(panel);
  auto* circleCombo = new QComboBox(panel);
  addLineSources(lineCombo, lineSources);
  addCircleSources(circleCombo, circleSources);
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
}

void MainWindowConstructedGeometryModule::showCircleCircleIntersectionPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryCircleSource> circleSources =
    constructedGeometryCircleSources(cameraRuntime()[camera.id].geometries());
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.circleCircleIntersection"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* firstCombo = new QComboBox(panel);
  auto* secondCombo = new QComboBox(panel);
  addCircleSources(firstCombo, circleSources);
  addCircleSources(secondCombo, circleSources);
  if (secondCombo->count() > 1) { secondCombo->setCurrentIndex(1); }
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
}

void MainWindowConstructedGeometryModule::showCircleCenterPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryCircleSource> circleSources = constructedGeometryCircleSources(set);
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.circleCenter"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* circleCombo = new QComboBox(panel);
  addCircleSources(circleCombo, circleSources);
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
}

void MainWindowConstructedGeometryModule::showMidpointPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryPointSource> pointSources = constructedGeometryPointSources(cameraRuntime()[camera.id].geometries());
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.midpoint"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* firstCombo = new QComboBox(panel);
  auto* secondCombo = new QComboBox(panel);
  addPointSources(firstCombo, pointSources);
  addPointSources(secondCombo, pointSources);
  if (secondCombo->count() > 1) { secondCombo->setCurrentIndex(1); }
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
}

void MainWindowConstructedGeometryModule::showOffsetLinePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(cameraRuntime()[camera.id].geometries());
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
}

void MainWindowConstructedGeometryModule::showAngleBisectorPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(cameraRuntime()[camera.id].geometries());
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.angleBisector"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* firstCombo = new QComboBox(panel);
  auto* secondCombo = new QComboBox(panel);
  addLineSources(firstCombo, lineSources);
  addLineSources(secondCombo, lineSources);
  if (secondCombo->count() > 1) { secondCombo->setCurrentIndex(1); }
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
}

void MainWindowConstructedGeometryModule::showTangentLinePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryPointSource> pointSources = constructedGeometryPointSources(set);
  const QVector<ConstructedGeometryCircleSource> circleSources = constructedGeometryCircleSources(set);
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.tangentLine"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* pointCombo = new QComboBox(panel);
  auto* circleCombo = new QComboBox(panel);
  addPointSources(pointCombo, pointSources);
  addCircleSources(circleCombo, circleSources);
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
}

void MainWindowConstructedGeometryModule::showProjectPointPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshConstructedGeometrySources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryPointSource> pointSources = constructedGeometryPointSources(set);
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(set);
  auto* panel = createPanel(toolsContainer());
  auto* layout = createPanelLayout(panel);
  addTitle(layout, panel, QString("%1 | %2").arg(tr("actions.projectPoint"), camera.id));
  auto* formLayout = addForm(layout, panel);

  auto* pointCombo = new QComboBox(panel);
  auto* lineCombo = new QComboBox(panel);
  addPointSources(pointCombo, pointSources);
  addLineSources(lineCombo, lineSources);
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
}
