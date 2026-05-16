#include "ToolPanelWidget.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ToolPanelWidget::ToolPanelWidget(
  const ToolDefinition& tool,
  const QString& cameraLabel,
  std::function<void()> backHandler,
  std::function<void(const ToolActionDefinition&)> actionHandler,
  QWidget* parent)
  : QWidget(parent)
{
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(tool.label + " | " + cameraLabel, this);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* actions = new QWidget(this);
  auto* actionsLayout = new QGridLayout(actions);
  actionsLayout->setContentsMargins(0, 0, 0, 0);
  actionsLayout->setSpacing(8);

  for (int i = 0; i < tool.actions.size(); ++i)
  {
    const ToolActionDefinition action = tool.actions[i];
    auto* button = new QPushButton(action.label, actions);
    button->setMinimumHeight(38);
    QObject::connect(button, &QPushButton::clicked, this, [actionHandler, action]() {
      if (actionHandler)
      {
        actionHandler(action);
      }
    });
    actionsLayout->addWidget(button, i / 2, i % 2);
  }

  layout->addWidget(actions);

  auto* note = new QLabel("Placeholder: questi pulsanti verranno collegati alle funzioni operative.", this);
  note->setWordWrap(true);
  note->setObjectName("toolPanelNote");
  layout->addWidget(note);

  auto* backButton = new QPushButton("Torna ai tool camera", this);
  QObject::connect(backButton, &QPushButton::clicked, this, [backHandler]() {
    if (backHandler)
    {
      backHandler();
    }
  });
  layout->addWidget(backButton);
  layout->addStretch(1);
}
