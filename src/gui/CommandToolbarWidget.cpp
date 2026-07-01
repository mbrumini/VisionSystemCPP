#include "gui/CommandToolbarWidget.h"

#include "gui/IconCatalog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

CommandToolbarWidget::CommandToolbarWidget(QWidget* parent)
  : QFrame(parent)
{
  setObjectName("commandToolbar");
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(10, 6, 10, 6);
  layout->setSpacing(6);

  m_startButton = addButton("start", "Start");
  m_stopButton = addButton("stop", "Stop");
  m_gridButton = addButton("grid", "Vista griglia");
  m_reloadButton = addButton("reload", "Reload config");
  m_fullscreenButton = addButton("fullscreen", "Fullscreen");
  m_helpButton = addButton("help", "Help");

  layout->addWidget(m_startButton);
  layout->addWidget(m_stopButton);
  layout->addSpacing(8);
  layout->addWidget(m_gridButton);
  layout->addWidget(m_reloadButton);
  layout->addWidget(m_fullscreenButton);
  layout->addSpacing(8);
  layout->addWidget(m_helpButton);
  layout->addStretch(1);

  m_versionLabel = new QLabel(this);
  m_versionLabel->setObjectName("toolbarVersion");
  layout->addWidget(m_versionLabel);

  m_recipeLabel = new QLabel(this);
  m_recipeLabel->setObjectName("toolbarRecipe");
  layout->addWidget(m_recipeLabel);

  m_statusLabel = new QLabel(this);
  m_statusLabel->setObjectName("toolbarStatus");
  layout->addWidget(m_statusLabel);
}

QToolButton* CommandToolbarWidget::addButton(const QString& iconId, const QString& tooltip)
{
  auto* button = new QToolButton(this);
  button->setIcon(IconCatalog::icon(iconId));
  button->setIconSize(QSize(22, 22));
  button->setToolTip(tooltip);
  button->setMinimumSize(38, 34);
  button->setAutoRaise(false);
  return button;
}

void CommandToolbarWidget::setLabels(const QString& start,
                                     const QString& stop,
                                     const QString& grid,
                                     const QString& reload,
                                     const QString& fullscreen,
                                     const QString& help)
{
  m_startButton->setToolTip(start);
  m_stopButton->setToolTip(stop);
  m_gridButton->setToolTip(grid);
  m_reloadButton->setToolTip(reload);
  m_fullscreenButton->setToolTip(fullscreen);
  m_helpButton->setToolTip(help);
}

void CommandToolbarWidget::setStatusText(const QString& text)
{
  m_statusLabel->setText(text);
}

void CommandToolbarWidget::setRecipeText(const QString& text)
{
  m_recipeLabel->setText(text);
}

void CommandToolbarWidget::setVersionText(const QString& text)
{
  m_versionLabel->setText(text);
}

void CommandToolbarWidget::setStartHandler(std::function<void()> handler)
{
  QObject::connect(m_startButton, &QToolButton::clicked, this, std::move(handler));
}

void CommandToolbarWidget::setStopHandler(std::function<void()> handler)
{
  QObject::connect(m_stopButton, &QToolButton::clicked, this, std::move(handler));
}

void CommandToolbarWidget::setGridHandler(std::function<void()> handler)
{
  QObject::connect(m_gridButton, &QToolButton::clicked, this, std::move(handler));
}

void CommandToolbarWidget::setReloadHandler(std::function<void()> handler)
{
  QObject::connect(m_reloadButton, &QToolButton::clicked, this, std::move(handler));
}

void CommandToolbarWidget::setFullscreenHandler(std::function<void()> handler)
{
  QObject::connect(m_fullscreenButton, &QToolButton::clicked, this, std::move(handler));
}

void CommandToolbarWidget::setHelpHandler(std::function<void()> handler)
{
  QObject::connect(m_helpButton, &QToolButton::clicked, this, std::move(handler));
}
