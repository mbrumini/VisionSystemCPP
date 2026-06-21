#include "CameraSetupPanelWidget.h"

#include "gui/IconCatalog.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
void connectButton(QToolButton* button, const std::function<void()>& handler)
{
  QObject::connect(button, &QToolButton::clicked, button, [handler]() {
    if (handler)
    {
      handler();
    }
  });
}

QToolButton* createTouchButton(const QString& iconId, const QString& label, QWidget* parent)
{
  auto* button = new QToolButton(parent);
  button->setObjectName("touchIconButton");
  button->setIcon(IconCatalog::icon(iconId));
  button->setIconSize(QSize(28, 28));
  button->setToolTip(label);
  button->setAccessibleName(label);
  button->setMinimumSize(58, 58);
  button->setMaximumSize(68, 68);
  button->setAutoRaise(false);
  return button;
}

QString toolIconId(const QString& label)
{
  const QString text = label.toLower();
  if (text.contains("punto"))
  {
    return "pointGeometry";
  }
  if (text.contains("linea"))
  {
    return "lineGeometry";
  }
  if (text.contains("cerchio"))
  {
    return "circleGeometry";
  }
  if (text.contains("arco"))
  {
    return "arcGeometry";
  }
  return "geometries";
}
}

CameraSetupPanelWidget::CameraSetupPanelWidget(
  const CameraSetupPanelTexts& texts,
  int intervalMs,
  std::function<void(int)> intervalChanged,
  std::function<void()> acquireSample,
  std::function<void()> importSample,
  std::function<void()> showSample,
  std::function<void()> importTests,
  std::function<void()> acquisitionSettings,
  std::function<void()> toggleGrab,
  std::function<void()> nextFrame,
  std::function<void()> results,
  std::function<void()> drawAoi,
  QVector<std::function<void()>> toolHandlers,
  std::function<void()> back,
  QWidget* parent)
  : QWidget(parent)
{
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(texts.title, this);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  m_details = new QLabel(texts.details, this);
  m_details->setWordWrap(true);
  layout->addWidget(m_details);

  auto* intervalLabel = new QLabel(texts.frameInterval, this);
  layout->addWidget(intervalLabel);

  auto* intervalSpin = new QSpinBox(this);
  intervalSpin->setRange(0, 10000);
  intervalSpin->setSingleStep(50);
  intervalSpin->setSuffix(" ms");
  intervalSpin->setValue(intervalMs);
  QObject::connect(intervalSpin, qOverload<int>(&QSpinBox::valueChanged), this, [intervalChanged](int value) {
    if (intervalChanged)
    {
      intervalChanged(value);
    }
  });
  layout->addWidget(intervalSpin);

  Q_UNUSED(toolHandlers);

  auto* recipeImagesBox = new QGroupBox(texts.recipeImagesTitle, this);
  auto* recipeImagesLayout = new QGridLayout(recipeImagesBox);
  recipeImagesLayout->setContentsMargins(8, 10, 8, 8);
  recipeImagesLayout->setSpacing(6);
  auto* acquireSampleButton = createTouchButton("acquireSample", texts.acquireSample, recipeImagesBox);
  auto* sampleButton = createTouchButton("assignSampleImage", texts.importSample, recipeImagesBox);
  auto* showSampleButton = createTouchButton("sampleImage", texts.showSample, recipeImagesBox);
  auto* testImagesButton = createTouchButton("assignTestImages", texts.importTests, recipeImagesBox);
  recipeImagesLayout->addWidget(acquireSampleButton, 0, 0);
  recipeImagesLayout->addWidget(sampleButton, 0, 1);
  recipeImagesLayout->addWidget(showSampleButton, 0, 2);
  recipeImagesLayout->addWidget(testImagesButton, 0, 3);
  layout->addWidget(recipeImagesBox);

  auto* cameraSetupBox = new QGroupBox(texts.cameraSetupTitle, this);
  auto* cameraSetupLayout = new QGridLayout(cameraSetupBox);
  cameraSetupLayout->setContentsMargins(8, 10, 8, 8);
  cameraSetupLayout->setSpacing(6);
  auto* acquisitionButton = createTouchButton("lighting", texts.acquisitionSettings, cameraSetupBox);
  auto* stepButton = createTouchButton("nextFrame", texts.nextFrame, cameraSetupBox);
  auto* resultsButton = createTouchButton("measurements", texts.results, cameraSetupBox);
  auto* aoiButton = createTouchButton("roi", texts.aoi, cameraSetupBox);
  cameraSetupLayout->addWidget(acquisitionButton, 0, 0);
  cameraSetupLayout->addWidget(stepButton, 0, 1);
  cameraSetupLayout->addWidget(resultsButton, 0, 2);
  cameraSetupLayout->addWidget(aoiButton, 0, 3);
  layout->addWidget(cameraSetupBox);

  auto* backButton = createTouchButton("back", texts.back, this);
  layout->addWidget(backButton, 0, Qt::AlignLeft);

  connectButton(acquireSampleButton, acquireSample);
  connectButton(sampleButton, importSample);
  connectButton(showSampleButton, showSample);
  connectButton(testImagesButton, importTests);
  connectButton(acquisitionButton, acquisitionSettings);
  Q_UNUSED(toggleGrab);
  connectButton(stepButton, nextFrame);
  connectButton(resultsButton, results);
  connectButton(aoiButton, drawAoi);
  connectButton(backButton, back);

  layout->addStretch(1);
}

void CameraSetupPanelWidget::setDetailsText(const QString& details)
{
  if (m_details)
  {
    m_details->setText(details);
  }
}

void CameraSetupPanelWidget::setDetailsVisible(bool visible)
{
  if (m_details)
  {
    m_details->setVisible(visible);
  }
}
