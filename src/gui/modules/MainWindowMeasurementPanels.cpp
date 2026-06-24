#include "gui/modules/MainWindowMeasurementModule.h"

#include "gui/modules/ConstructedGeometryCircleSource.h"
#include "gui/modules/ConstructedGeometryLineSource.h"
#include "gui/modules/ConstructedGeometryPointSource.h"
#include "gui/TouchIconButton.h"
#include "runtime/CameraRuntime.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
void addSources(QComboBox* combo, const QVector<ConstructedGeometryPointSource>& sources)
{
  for (const ConstructedGeometryPointSource& source : sources)
  {
    combo->addItem(source.label, source.id);
  }
}

void addSources(QComboBox* combo, const QVector<ConstructedGeometryLineSource>& sources)
{
  for (const ConstructedGeometryLineSource& source : sources)
  {
    combo->addItem(source.label, source.id);
  }
}

void addSources(QComboBox* combo, const QVector<ConstructedGeometryCircleSource>& sources)
{
  for (const ConstructedGeometryCircleSource& source : sources)
  {
    combo->addItem(source.label, source.id);
  }
}

void applySecondarySourceDefault(QComboBox* secondaryCombo, int sourceCount)
{
  if (sourceCount >= 2)
  {
    secondaryCombo->setCurrentIndex(1);
  }
}

QVBoxLayout* createMeasurementPage(QWidget* panel, const QString& titleText)
{
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(titleText, panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  return layout;
}

void appendMeasurementHint(QWidget* panel, QVBoxLayout* layout, const QString& text)
{
  auto* hint = new QLabel(text, panel);
  hint->setWordWrap(true);
  hint->setObjectName("toolPanelHint");
  layout->addWidget(hint);
}

QPushButton* createSaveMeasurementButton(QWidget* panel, const QString& label)
{
  auto* saveButton = new QPushButton(label, panel);
  saveButton->setObjectName("primaryActionButton");
  saveButton->setMinimumHeight(44);
  return saveButton;
}
}

void MainWindowMeasurementModule::showPointPointDistancePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshMeasurementSources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryPointSource> pointSources =
    constructedGeometryPointSources(set, geometryAliasesForCamera(camera));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = createMeasurementPage(panel, QString("%1 | %2").arg(tr("actions.pointPointDistance"), camera.id));

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);

  auto* pointACombo = new QComboBox(form);
  auto* pointBCombo = new QComboBox(form);
  addSources(pointACombo, pointSources);
  addSources(pointBCombo, pointSources);
  applySecondarySourceDefault(pointBCombo, pointSources.size());

  formLayout->addWidget(new QLabel(tr("labels.sourcePointA"), form), 0, 0);
  formLayout->addWidget(pointACombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourcePointB"), form), 1, 0);
  formLayout->addWidget(pointBCombo, 1, 1);
  layout->addWidget(form);

  if (pointSources.isEmpty())
  {
    const GeometrySet& geometrySet = cameraRuntime()[camera.id].geometries();
    const bool hasLines = !geometrySet.lines.isEmpty() || !geometrySet.constructedLines.isEmpty();
    appendMeasurementHint(
      panel,
      layout,
      hasLines ? tr("hints.measurementNoPointSourcesUseLines") : tr("hints.measurementNoPointSources"));
  }

  const auto refreshPanel = [this, camera]() { showPointPointDistancePanel(camera); };
  auto* saveButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement"));
  saveButton->setEnabled(pointSources.size() >= 2);
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, pointACombo, pointBCombo, refreshPanel]() {
    createPointPointDistance(camera, pointACombo->currentData().toString(), pointBCombo->currentData().toString());
    refreshPanel();
  });
  appendMeasurementListControls(panel, layout, camera, refreshPanel);
  layout->addWidget(saveButton);

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}

void MainWindowMeasurementModule::showPointLineDistancePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshMeasurementSources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryPointSource> pointSources =
    constructedGeometryPointSources(set, geometryAliasesForCamera(camera));
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(set, geometryAliasesForCamera(camera));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = createMeasurementPage(panel, QString("%1 | %2").arg(tr("actions.pointLineDistance"), camera.id));

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);

  auto* pointCombo = new QComboBox(form);
  auto* lineCombo = new QComboBox(form);
  addSources(pointCombo, pointSources);
  addSources(lineCombo, lineSources);

  formLayout->addWidget(new QLabel(tr("labels.sourcePoint"), form), 0, 0);
  formLayout->addWidget(pointCombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceLine"), form), 1, 0);
  formLayout->addWidget(lineCombo, 1, 1);
  layout->addWidget(form);

  const auto refreshPanel = [this, camera]() { showPointLineDistancePanel(camera); };
  auto* saveButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement"));
  saveButton->setEnabled(!pointSources.isEmpty() && !lineSources.isEmpty());
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, pointCombo, lineCombo, refreshPanel]() {
    createPointLineDistance(camera, pointCombo->currentData().toString(), lineCombo->currentData().toString());
    refreshPanel();
  });
  appendMeasurementListControls(panel, layout, camera, refreshPanel);
  layout->addWidget(saveButton);

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}

void MainWindowMeasurementModule::showLineLineDistancePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshMeasurementSources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(set, geometryAliasesForCamera(camera));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = createMeasurementPage(panel, QString("%1 | %2").arg(tr("actions.lineLineDistance"), camera.id));

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);

  auto* lineACombo = new QComboBox(form);
  auto* lineBCombo = new QComboBox(form);
  addSources(lineACombo, lineSources);
  addSources(lineBCombo, lineSources);
  applySecondarySourceDefault(lineBCombo, lineSources.size());

  formLayout->addWidget(new QLabel(tr("labels.sourceLineA"), form), 0, 0);
  formLayout->addWidget(lineACombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceLineB"), form), 1, 0);
  formLayout->addWidget(lineBCombo, 1, 1);
  layout->addWidget(form);

  if (lineSources.size() < 2)
  {
    appendMeasurementHint(panel, layout, tr("hints.measurementNoLineSources"));
  }

  const auto refreshPanel = [this, camera]() { showLineLineDistancePanel(camera); };
  auto* saveButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement"));
  saveButton->setEnabled(lineSources.size() >= 2);
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, lineACombo, lineBCombo, refreshPanel]() {
    createLineLineDistance(camera, lineACombo->currentData().toString(), lineBCombo->currentData().toString());
    refreshPanel();
  });
  appendMeasurementListControls(panel, layout, camera, refreshPanel);
  layout->addWidget(saveButton);

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}

void MainWindowMeasurementModule::showCircleDiameterPanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshMeasurementSources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryCircleSource> circleSources =
    constructedGeometryCircleSources(set, geometryAliasesForCamera(camera));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = createMeasurementPage(panel, QString("%1 | %2").arg(tr("actions.circleDiameterMeasure"), camera.id));

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);

  auto* circleCombo = new QComboBox(form);
  addSources(circleCombo, circleSources);

  formLayout->addWidget(new QLabel(tr("labels.sourceCircle"), form), 0, 0);
  formLayout->addWidget(circleCombo, 0, 1);
  layout->addWidget(form);

  const auto refreshPanel = [this, camera]() { showCircleDiameterPanel(camera); };
  auto* saveButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement"));
  saveButton->setEnabled(!circleSources.isEmpty());
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, circleCombo, refreshPanel]() {
    createCircleDiameter(camera, circleCombo->currentData().toString());
    refreshPanel();
  });
  appendMeasurementListControls(panel, layout, camera, refreshPanel);
  layout->addWidget(saveButton);

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}

void MainWindowMeasurementModule::showLineLineAnglePanel(const CameraConfig& camera)
{
  context().clearToolPanel();
  refreshMeasurementSources(camera);

  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const QVector<ConstructedGeometryLineSource> lineSources = constructedGeometryLineSources(set, geometryAliasesForCamera(camera));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = createMeasurementPage(panel, QString("%1 | %2").arg(tr("actions.lineLineAngle"), camera.id));

  auto* form = new QWidget(panel);
  auto* formLayout = new QGridLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(8);

  auto* lineACombo = new QComboBox(form);
  auto* lineBCombo = new QComboBox(form);
  addSources(lineACombo, lineSources);
  addSources(lineBCombo, lineSources);
  applySecondarySourceDefault(lineBCombo, lineSources.size());

  formLayout->addWidget(new QLabel(tr("labels.sourceLineA"), form), 0, 0);
  formLayout->addWidget(lineACombo, 0, 1);
  formLayout->addWidget(new QLabel(tr("labels.sourceLineB"), form), 1, 0);
  formLayout->addWidget(lineBCombo, 1, 1);
  layout->addWidget(form);

  const auto refreshPanel = [this, camera]() { showLineLineAnglePanel(camera); };
  auto* saveButton = createSaveMeasurementButton(panel, tr("actions.saveMeasurement"));
  saveButton->setEnabled(lineSources.size() >= 2);
  QObject::connect(saveButton, &QPushButton::clicked, window(), [this, camera, lineACombo, lineBCombo, refreshPanel]() {
    createLineLineAngle(camera, lineACombo->currentData().toString(), lineBCombo->currentData().toString());
    refreshPanel();
  });
  appendMeasurementListControls(panel, layout, camera, refreshPanel);
  layout->addWidget(saveButton);

  auto* backButton = createTouchIconButton("back", tr("tools.measurements"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() { showMeasurementPanel(camera); });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}
