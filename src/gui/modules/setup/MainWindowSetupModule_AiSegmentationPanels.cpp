#include "gui/modules/MainWindowSetupModule.h"

#include "config/RecipeJsonUtils.h"
#include "gui/modules/MainWindowContext.h"

#include <QDir>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QLabel* createSegmentationNote(const QString& text, QWidget* parent)
{
  auto* label = new QLabel(text, parent);
  label->setObjectName("toolPanelNote");
  label->setWordWrap(true);
  return label;
}

int imageCount(const QString& path)
{
  return QDir(path).entryInfoList(RecipeJsonUtils::imageNameFilters(), QDir::Files).size();
}
}

void MainWindowSetupModule::showAiSegmentationPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.aiSegmentation"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  layout->addWidget(createSegmentationNote(
    "Feature dinamiche pronte. Ogni feature avra' immagini, maschere e label separate; "
    "il labeling a pennello verra' agganciato qui.",
    panel));

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setSpacing(6);

  auto* addFeatureButton = new QPushButton("Aggiungi feature", buttons);
  addFeatureButton->setObjectName("touchButton");
  QObject::connect(addFeatureButton, &QPushButton::clicked, window(), [this, camera]() {
    bool ok = false;
    const QString featureName = QInputDialog::getText(
      window(),
      "Aggiungi feature segmentazione",
      "Nome feature",
      QLineEdit::Normal,
      {},
      &ok).trimmed();
    if (!ok || featureName.isEmpty())
    {
      return;
    }

    QString error;
    if (!recipes().addAiSegmentationFeature(camera.id, featureName, nullptr, &error))
    {
      log(error);
      return;
    }

    showAiSegmentationPanel(camera);
  });
  buttonsLayout->addWidget(addFeatureButton, 0, 0);

  auto* backButton = new QPushButton(tr("tools.ai"), buttons);
  backButton->setObjectName("touchButton");
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    showAiPanel(camera);
  });
  buttonsLayout->addWidget(backButton, 0, 1);
  layout->addWidget(buttons);

  const QVector<AiSegmentationFeatureConfig> features = recipes().loadAiSegmentationFeatures(camera.id);
  if (features.isEmpty())
  {
    layout->addWidget(createSegmentationNote(
      "Nessuna feature configurata. Aggiungi la prima feature, ad esempio bordo, foro, graffio o area difetto.",
      panel));
  }
  else
  {
    QStringList lines;
    for (const AiSegmentationFeatureConfig& feature : features)
    {
      const int images = imageCount(recipes().cameraAiSegmentationFeatureImagesPath(camera.id, feature));
      const int masks = imageCount(recipes().cameraAiSegmentationFeatureMasksPath(camera.id, feature));
      const int labels = QDir(recipes().cameraAiSegmentationFeatureLabelsPath(camera.id, feature))
        .entryInfoList(QStringList{"*.json", "*.txt"}, QDir::Files)
        .size();
      lines.append(QString("%1 - %2: %3 immagini, %4 maschere, %5 label")
        .arg(feature.id, 3, 10, QChar('0'))
        .arg(feature.name)
        .arg(images)
        .arg(masks)
        .arg(labels));
    }
    layout->addWidget(createSegmentationNote(lines.join('\n'), panel));
  }

  layout->addWidget(createSegmentationNote(
    QString("Raw segmentazione:\n%1").arg(recipes().cameraAiSegmentationRawImagesPath(camera.id)),
    panel));

  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tr("tools.aiSegmentation"));
}
