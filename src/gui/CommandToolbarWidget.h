#pragma once

#include <QFrame>
#include <QString>
#include <functional>

class QLabel;
class QToolButton;

class CommandToolbarWidget : public QFrame
{
public:
  explicit CommandToolbarWidget(QWidget* parent = nullptr);

  void setLabels(const QString& start,
                 const QString& stop,
                 const QString& grid,
                 const QString& reload,
                 const QString& fullscreen);
  void setStatusText(const QString& text);
  void setRecipeText(const QString& text);

  void setStartHandler(std::function<void()> handler);
  void setStopHandler(std::function<void()> handler);
  void setGridHandler(std::function<void()> handler);
  void setReloadHandler(std::function<void()> handler);
  void setFullscreenHandler(std::function<void()> handler);

private:
  QToolButton* addButton(const QString& iconId, const QString& tooltip);

  QToolButton* m_startButton = nullptr;
  QToolButton* m_stopButton = nullptr;
  QToolButton* m_gridButton = nullptr;
  QToolButton* m_reloadButton = nullptr;
  QToolButton* m_fullscreenButton = nullptr;
  QLabel* m_statusLabel = nullptr;
  QLabel* m_recipeLabel = nullptr;
};
