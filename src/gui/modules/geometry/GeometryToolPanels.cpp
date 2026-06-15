#include "gui/modules/QtCompat.h"

#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/geometry/GeometryPanelNavigation.h"
#include "gui/TouchIconButton.h"

#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "processing/geometry/EdgePointDetector.h"
#include "util/AsyncExecutor.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

using AsyncExecutor::runAsyncTask;

void MainWindowGeometryModule::showGeometryPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.geometries"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* buttonGrid = new QWidget(panel);
  auto* buttonLayout = new QGridLayout(buttonGrid);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(6);

  auto* pointButton = createTouchIconButton("pointGeometry", tr("actions.pointGeometry"), panel);
  QObject::connect(pointButton, &QPushButton::clicked, window(), [this, camera]() { showGeometryPointPanel(camera); });
  buttonLayout->addWidget(pointButton, 0, 0);

  auto* lineButton = createTouchIconButton("lineGeometry", tr("actions.lineGeometry"), panel);
  QObject::connect(lineButton, &QPushButton::clicked, window(), [this, camera]() { showGeometryLinePanel(camera); });
  buttonLayout->addWidget(lineButton, 0, 1);

  auto* circleButton = createTouchIconButton("circleGeometry", tr("actions.circleGeometry"), panel);
  QObject::connect(circleButton, &QPushButton::clicked, window(), [this, camera]() { showGeometryCirclePanel(camera); });
  buttonLayout->addWidget(circleButton, 0, 2);

  auto* arcButton = createTouchIconButton("arcGeometry", tr("actions.arcGeometry"), panel);
  QObject::connect(arcButton, &QPushButton::clicked, window(), [this, camera]() { showGeometryArcPanel(camera); });
  buttonLayout->addWidget(arcButton, 0, 3);

  auto* backButton = createTouchIconButton("back",
    GeometryPanelNavigation::backLabel(context(), camera, tr("commands.backToCameraTools")),
    panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (!GeometryPanelNavigation::returnToSetup(context(), camera))
    {
      context().showCameraToolList(camera);
    }
  });
  buttonLayout->addWidget(backButton, 1, 0);
  layout->addWidget(buttonGrid);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
}

