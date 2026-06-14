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
  std::function<void()> acquireAiSample,
  std::function<void()> importSample,
  std::function<void()> showSample,
  std::function<void()> importTests,
  std::function<void()> start,
  std::function<void()> stop,
  std::function<void()> nextFrame,
  std::function<void()> results,
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

  if (!texts.toolLabels.isEmpty())
  {
    auto* toolsBox = new QGroupBox(texts.toolsTitle, this);
    auto* toolsLayout = new QGridLayout(toolsBox);
    toolsLayout->setContentsMargins(8, 10, 8, 8);
    toolsLayout->setSpacing(6);
    for (int i = 0; i < texts.toolLabels.size(); ++i)
    {
      auto* button = createTouchButton(toolIconId(texts.toolLabels[i]), texts.toolLabels[i], toolsBox);
      toolsLayout->addWidget(button, i / 4, i % 4);
      if (i < toolHandlers.size())
      {
        connectButton(button, toolHandlers[i]);
      }
    }
    layout->addWidget(toolsBox);
  }

  auto* buttons = new QWidget(this);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setSpacing(6);

  auto* acquireSampleButton = createTouchButton("acquireSample", texts.acquireSample, buttons);
  auto* acquireAiSampleButton = createTouchButton("aiSample", texts.acquireAiSample, buttons);
  auto* sampleButton = createTouchButton("assignSampleImage", texts.importSample, buttons);
  auto* showSampleButton = createTouchButton("sampleImage", texts.showSample, buttons);
  auto* testImagesButton = createTouchButton("assignTestImages", texts.importTests, buttons);
  auto* startButton = createTouchButton("start", texts.start, buttons);
  auto* stopButton = createTouchButton("stop", texts.stop, buttons);
  auto* stepButton = createTouchButton("nextFrame", texts.nextFrame, buttons);
  auto* resultsButton = createTouchButton("measurements", texts.results, buttons);
  auto* backButton = createTouchButton("back", texts.back, buttons);

  buttonsLayout->addWidget(acquireSampleButton, 0, 0);
  buttonsLayout->addWidget(acquireAiSampleButton, 0, 1);
  buttonsLayout->addWidget(sampleButton, 0, 2);
  buttonsLayout->addWidget(showSampleButton, 0, 3);
  buttonsLayout->addWidget(testImagesButton, 1, 0);
  buttonsLayout->addWidget(startButton, 1, 1);
  buttonsLayout->addWidget(stopButton, 1, 2);
  buttonsLayout->addWidget(stepButton, 1, 3);
  buttonsLayout->addWidget(resultsButton, 2, 0);
  buttonsLayout->addWidget(backButton, 2, 1);
  layout->addWidget(buttons);

  connectButton(acquireSampleButton, acquireSample);
  connectButton(acquireAiSampleButton, acquireAiSample);
  connectButton(sampleButton, importSample);
  connectButton(showSampleButton, showSample);
  connectButton(testImagesButton, importTests);
  connectButton(startButton, start);
  connectButton(stopButton, stop);
  connectButton(stepButton, nextFrame);
  connectButton(resultsButton, results);
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
