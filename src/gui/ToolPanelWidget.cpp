#include "ToolPanelWidget.h"

#include "gui/IconCatalog.h"

#include <QGridLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
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
}

ToolPanelWidget::ToolPanelWidget(
  const ToolDefinition& tool,
  const QString& cameraLabel,
  const QString& noteText,
  const QString& backText,
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
    auto* button = createTouchButton(action.id, action.label, actions);
    QObject::connect(button, &QToolButton::clicked, this, [actionHandler, action]() {
      if (actionHandler)
      {
        actionHandler(action);
      }
    });
    actionsLayout->addWidget(button, i / 4, i % 4);
  }

  layout->addWidget(actions);

  auto* note = new QLabel(noteText, this);
  note->setWordWrap(true);
  note->setObjectName("toolPanelNote");
  layout->addWidget(note);

  auto* backButton = createTouchButton("back", backText, this);
  QObject::connect(backButton, &QToolButton::clicked, this, [backHandler]() {
    if (backHandler)
    {
      backHandler();
    }
  });
  layout->addWidget(backButton);
  layout->addStretch(1);
}
