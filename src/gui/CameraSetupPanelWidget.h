#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

struct CameraSetupPanelTexts
{
  QString title;
  QString details;
  QString frameInterval;
  QString acquireSample;
  QString acquireAiSample;
  QString importSample;
  QString showSample;
  QString importTests;
  QString start;
  QString stop;
  QString nextFrame;
  QString results;
  QString back;
  QString toolsTitle;
  QStringList toolLabels;
};

class QLabel;

class CameraSetupPanelWidget : public QWidget
{
public:
  CameraSetupPanelWidget(
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
    QWidget* parent = nullptr);

  void setDetailsText(const QString& details);
  void setDetailsVisible(bool visible);

private:
  QLabel* m_details = nullptr;
};
