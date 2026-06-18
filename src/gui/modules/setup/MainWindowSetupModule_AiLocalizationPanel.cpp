#include "gui/modules/MainWindowSetupModule.h"

#include "gui/modules/MainWindowCameraConfigModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/setup/AiLocalizationPaths.h"

#include <QDesktopServices>
#include <QDir>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

void MainWindowSetupModule::showAiLocalizationPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  QDir().mkpath(aiLocalizationRawPath(recipes(), camera.id));
  QDir().mkpath(aiLocalizationMasksPath(recipes(), camera.id));
  QDir().mkpath(aiLocalizationLabelsPath(recipes(), camera.id));
  QDir().mkpath(aiLocalizationDatasetPath(recipes(), camera.id));
  QDir().mkpath(aiLocalizationModelsPath(recipes(), camera.id));

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.aiLocalization"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  layout->addWidget(title);

  auto* note = new QLabel(tr("labels.aiLocalizationNote"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* steps = new QWidget(panel);
  auto* grid = new QGridLayout(steps);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setSpacing(6);

  auto makeButton = [steps](const QString& text) {
    auto* button = new QPushButton(text, steps);
    button->setObjectName("touchButton");
    button->setMinimumHeight(48);
    return button;
  };

  auto* acquire = makeButton(tr("actions.acquireAiRawImage") + " 1 shot");
  QObject::connect(acquire, &QPushButton::clicked, window(), [this, camera]() {
    context().cameraConfig->acquireCameraAiLocalizationRawImage(camera);
  });
  grid->addWidget(acquire, 0, 0);

  auto* openFolder = makeButton(tr("actions.openAiLocalizationFolder"));
  QObject::connect(openFolder, &QPushButton::clicked, window(), [this, camera]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(aiLocalizationRootPath(recipes(), camera.id)));
  });
  grid->addWidget(openFolder, 0, 1);

  auto* labeling = makeButton(tr("actions.labelPieceMasks"));
  labeling->setEnabled(false);
  labeling->setToolTip(tr("labels.aiToolPlanned"));
  grid->addWidget(labeling, 1, 0);

  auto* training = makeButton(tr("actions.trainAiModel"));
  training->setEnabled(false);
  training->setToolTip(tr("labels.aiToolPlanned"));
  grid->addWidget(training, 1, 1);

  auto* inference = makeButton(tr("actions.runInference"));
  inference->setEnabled(false);
  inference->setToolTip(tr("labels.aiToolPlanned"));
  grid->addWidget(inference, 2, 0, 1, 2);
  layout->addWidget(steps);

  auto* paths = new QLabel(
    QString("raw: %1\nmasks: %2\ndataset: %3\nmodels: %4")
      .arg(
        QDir::toNativeSeparators(aiLocalizationRawPath(recipes(), camera.id)),
        QDir::toNativeSeparators(aiLocalizationMasksPath(recipes(), camera.id)),
        QDir::toNativeSeparators(aiLocalizationDatasetPath(recipes(), camera.id)),
        QDir::toNativeSeparators(aiLocalizationModelsPath(recipes(), camera.id))),
    panel);
  paths->setObjectName("toolPanelNote");
  paths->setWordWrap(true);
  layout->addWidget(paths);

  auto* back = makeButton(tr("tools.ai"));
  QObject::connect(back, &QPushButton::clicked, window(), [this, camera]() {
    showAiPanel(camera);
  });
  layout->addWidget(back);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tr("tools.aiLocalization"));
}
