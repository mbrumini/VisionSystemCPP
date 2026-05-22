#include "gui/ToolIconBarWidget.h"

#include "gui/IconCatalog.h"

#include <QToolButton>
#include <QStyle>
#include <QVBoxLayout>

ToolIconBarWidget::ToolIconBarWidget(QWidget* parent)
  : QFrame(parent)
{
  setObjectName("toolIconBar");
  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(6, 6, 6, 6);
  m_layout->setSpacing(6);
}

void ToolIconBarWidget::setTools(const QVector<ToolIconDefinition>& tools)
{
  m_tools = tools;
  rebuild();
}

void ToolIconBarWidget::setActiveTool(const QString& toolId)
{
  const QString previous = m_activeToolId;
  m_activeToolId = toolId;
  updateButton(previous);
  updateButton(toolId);
}

void ToolIconBarWidget::setToolClickHandler(std::function<void(const QString&)> handler)
{
  m_clickHandler = std::move(handler);
}

void ToolIconBarWidget::rebuild()
{
  while (QLayoutItem* item = m_layout->takeAt(0))
  {
    if (QWidget* widget = item->widget())
    {
      widget->deleteLater();
    }
    delete item;
  }
  m_buttons.clear();

  for (const ToolIconDefinition& tool : m_tools)
  {
    auto* button = new QToolButton(this);
    button->setIcon(IconCatalog::icon(tool.iconId.isEmpty() ? tool.id : tool.iconId));
    button->setIconSize(QSize(24, 24));
    button->setToolTip(tool.label);
    button->setMinimumSize(44, 40);
    button->setCheckable(true);
    QObject::connect(button, &QToolButton::clicked, this, [this, tool]() {
      if (m_clickHandler)
      {
        m_clickHandler(tool.id);
      }
    });
    m_layout->addWidget(button);
    m_buttons.insert(tool.id, button);
    updateButton(tool.id);
  }

  m_layout->addStretch(1);
}

void ToolIconBarWidget::updateButton(const QString& toolId)
{
  QToolButton* button = m_buttons.value(toolId, nullptr);
  if (!button)
  {
    return;
  }

  const bool active = toolId == m_activeToolId;
  button->setChecked(active);
  button->setProperty("active", active);
  button->style()->unpolish(button);
  button->style()->polish(button);
}
