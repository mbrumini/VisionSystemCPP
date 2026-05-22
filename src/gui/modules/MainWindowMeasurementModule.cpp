#include "gui/modules/MainWindowMeasurementModule.h"

#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/geometry/GeometryPanelNavigation.h"
#include "gui/TouchIconButton.h"

#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>

MainWindowMeasurementModule::MainWindowMeasurementModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

void MainWindowMeasurementModule::refreshMeasurementSources(const CameraConfig& camera)
{
  if (context().setup)
  {
    context().setup->refreshPoseForCurrentFrame(camera);
  }
  if (context().geometry)
  {
    context().geometry->testConfiguredGeometryLines(camera);
  }
  if (context().constructedGeometry)
  {
    context().constructedGeometry->rebuildConstructedGeometryRecipe(camera);
  }
  rebuildMeasurementRecipe(camera);
}

void MainWindowMeasurementModule::showMeasurementPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();
  refreshMeasurementSources(camera);

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.measurements"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* buttonGrid = new QWidget(panel);
  auto* buttonLayout = new QGridLayout(buttonGrid);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(6);

  auto* pointPointButton = createTouchIconButton("pointPointDistance", tr("actions.pointPointDistance"), panel);
  QObject::connect(pointPointButton, &QPushButton::clicked, window(), [this, camera]() {
    showPointPointDistancePanel(camera);
  });
  buttonLayout->addWidget(pointPointButton, 0, 0);

  auto* pointLineButton = createTouchIconButton("pointLineDistance", tr("actions.pointLineDistance"), panel);
  QObject::connect(pointLineButton, &QPushButton::clicked, window(), [this, camera]() {
    showPointLineDistancePanel(camera);
  });
  buttonLayout->addWidget(pointLineButton, 0, 1);

  auto* lineLineButton = createTouchIconButton("lineLineDistance", tr("actions.lineLineDistance"), panel);
  QObject::connect(lineLineButton, &QPushButton::clicked, window(), [this, camera]() {
    showLineLineDistancePanel(camera);
  });
  buttonLayout->addWidget(lineLineButton, 0, 2);

  auto* circleDiameterButton = createTouchIconButton("circleDiameterMeasure", tr("actions.circleDiameterMeasure"), panel);
  QObject::connect(circleDiameterButton, &QPushButton::clicked, window(), [this, camera]() {
    showCircleDiameterPanel(camera);
  });
  buttonLayout->addWidget(circleDiameterButton, 0, 3);

  auto* lineAngleButton = createTouchIconButton("lineLineAngle", tr("actions.lineLineAngle"), panel);
  QObject::connect(lineAngleButton, &QPushButton::clicked, window(), [this, camera]() {
    showLineLineAnglePanel(camera);
  });
  buttonLayout->addWidget(lineAngleButton, 1, 0);

  auto* backButton = createTouchIconButton("back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (!GeometryPanelNavigation::returnToSetup(context(), camera))
    {
      context().showCameraToolList(camera);
    }
  });
  buttonLayout->addWidget(backButton, 1, 1);
  layout->addWidget(buttonGrid);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}
