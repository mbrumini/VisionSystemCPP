#include "gui/modules/MainWindowSetupModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/IconCatalog.h"
#include "gui/modules/MainWindowCameraConfigModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/setup/AiClassificationPaths.h"
#include "gui/modules/setup/AiModelComparison.h"
#include "gui/modules/setup/AiPythonRuntime.h"
#include "gui/modules/setup/AiTrainingGraph.h"
#include "gui/modules/setup/SetupCameraResolver.h"

#include <QDir>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>

namespace
{
QPushButton* createAiButton(const QString& iconId, const QString& label, QWidget* parent)
{
  auto* button = new QPushButton(IconCatalog::icon(iconId), label, parent);
  button->setObjectName("touchButton");
  button->setMinimumHeight(48);
  return button;
}

}

void MainWindowSetupModule::showAiPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.ai"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(tr("labels.aiNote"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* grid = new QWidget(panel);
  auto* gridLayout = new QGridLayout(grid);
  gridLayout->setContentsMargins(0, 0, 0, 0);
  gridLayout->setSpacing(6);

  auto* classificationButton = createAiButton("aiClassification", tr("tools.aiClassification"), grid);
  QObject::connect(classificationButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiClassificationPanel(camera);
  });
  gridLayout->addWidget(classificationButton, 0, 0);

  auto* anomalyButton = createAiButton("aiAnomaly", tr("tools.aiAnomaly"), grid);
  QObject::connect(anomalyButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiPlaceholderPanel(camera, "aiAnomaly");
  });
  gridLayout->addWidget(anomalyButton, 0, 1);

  auto* segmentationButton = createAiButton("aiSegmentation", tr("tools.aiSegmentation"), grid);
  QObject::connect(segmentationButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiSegmentationPanel(camera);
  });
  gridLayout->addWidget(segmentationButton, 1, 0);

  layout->addWidget(grid);

  auto* backButton = createAiButton("back", tr("commands.backToCameraTools"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    if (m_aiClassificationCaptureCameraId == camera.id)
    {
      stopAiClassificationCapture();
    }
    context().showCameraToolList(camera);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tr("tools.ai"));
}

void MainWindowSetupModule::showAiPlaceholderPanel(const CameraConfig& camera, const QString& toolId)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools." + toolId), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(tr("labels.aiToolPlanned"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* labelsButton = createAiButton("classes", tr("actions.configureAiLabels"), panel);
  QObject::connect(labelsButton, &QPushButton::clicked, window(), [this, toolId]() {
    log(tr("log.placeholder") + ": " + tr("actions.configureAiLabels") + " -> " + tr("tools." + toolId));
  });
  layout->addWidget(labelsButton);

  auto* trainingButton = createAiButton("aiTrain", tr("actions.trainAiModel"), panel);
  QObject::connect(trainingButton, &QPushButton::clicked, window(), [this, toolId]() {
    log(tr("log.placeholder") + ": " + tr("actions.trainAiModel") + " -> " + tr("tools." + toolId));
  });
  layout->addWidget(trainingButton);

  auto* backButton = createAiButton("back", tr("tools.ai"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiPanel(camera);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tr("tools." + toolId));
}

