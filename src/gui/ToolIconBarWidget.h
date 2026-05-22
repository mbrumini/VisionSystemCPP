#pragma once

#include <QFrame>
#include <QHash>
#include <QString>
#include <QVector>
#include <functional>

class QToolButton;
class QVBoxLayout;

struct ToolIconDefinition
{
  QString id;
  QString label;
  QString iconId;
};

class ToolIconBarWidget : public QFrame
{
public:
  explicit ToolIconBarWidget(QWidget* parent = nullptr);

  void setTools(const QVector<ToolIconDefinition>& tools);
  void setActiveTool(const QString& toolId);
  void setToolClickHandler(std::function<void(const QString&)> handler);

private:
  void rebuild();
  void updateButton(const QString& toolId);

  QVector<ToolIconDefinition> m_tools;
  QHash<QString, QToolButton*> m_buttons;
  QString m_activeToolId;
  QVBoxLayout* m_layout = nullptr;
  std::function<void(const QString&)> m_clickHandler;
};
