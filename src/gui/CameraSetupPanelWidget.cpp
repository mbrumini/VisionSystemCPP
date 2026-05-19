#include "CameraSetupPanelWidget.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace
{
void connectButton(QPushButton* button, const std::function<void()>& handler)
{
  QObject::connect(button, &QPushButton::clicked, button, [handler]() {
    if (handler)
    {
      handler();
    }
  });
}
}

CameraSetupPanelWidget::CameraSetupPanelWidget(
  const CameraSetupPanelTexts& texts,
  int intervalMs,
  std::function<void(int)> intervalChanged,
  std::function<void()> acquireSample,
  std::function<void()> importSample,
  std::function<void()> importTests,
  std::function<void()> assignImageFolder,
  std::function<void()> start,
  std::function<void()> stop,
  std::function<void()> nextFrame,
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
      auto* button = new QPushButton(texts.toolLabels[i], toolsBox);
      button->setMinimumHeight(34);
      toolsLayout->addWidget(button, i / 2, i % 2);
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

  auto* acquireSampleButton = new QPushButton(texts.acquireSample, buttons);
  auto* sampleButton = new QPushButton(texts.importSample, buttons);
  auto* testImagesButton = new QPushButton(texts.importTests, buttons);
  auto* sourceButton = new QPushButton(texts.assignImageFolder, buttons);
  auto* startButton = new QPushButton(texts.start, buttons);
  auto* stopButton = new QPushButton(texts.stop, buttons);
  auto* stepButton = new QPushButton(texts.nextFrame, buttons);
  auto* backButton = new QPushButton(texts.back, buttons);

  buttonsLayout->addWidget(acquireSampleButton, 0, 0, 1, 2);
  buttonsLayout->addWidget(sampleButton, 1, 0);
  buttonsLayout->addWidget(testImagesButton, 1, 1);
  buttonsLayout->addWidget(sourceButton, 2, 0, 1, 2);
  buttonsLayout->addWidget(startButton, 3, 0);
  buttonsLayout->addWidget(stopButton, 3, 1);
  buttonsLayout->addWidget(stepButton, 4, 0, 1, 2);
  buttonsLayout->addWidget(backButton, 5, 0, 1, 2);
  layout->addWidget(buttons);

  connectButton(acquireSampleButton, acquireSample);
  connectButton(sampleButton, importSample);
  connectButton(testImagesButton, importTests);
  connectButton(sourceButton, assignImageFolder);
  connectButton(startButton, start);
  connectButton(stopButton, stop);
  connectButton(stepButton, nextFrame);
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
