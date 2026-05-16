#pragma once

#include "gui/ToolCatalog.h"

#include <QWidget>
#include <functional>

class ToolPanelWidget : public QWidget
{
public:
  ToolPanelWidget(
    const ToolDefinition& tool,
    const QString& cameraLabel,
    const QString& noteText,
    const QString& backText,
    std::function<void()> backHandler,
    std::function<void(const ToolActionDefinition&)> actionHandler,
    QWidget* parent = nullptr);
};
