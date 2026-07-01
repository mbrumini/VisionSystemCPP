#include "gui/MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QProxyStyle>

class HmiProxyStyle : public QProxyStyle
{
public:
  using QProxyStyle::QProxyStyle;

  int styleHint(StyleHint hint,
                const QStyleOption* option = nullptr,
                const QWidget* widget = nullptr,
                QStyleHintReturn* returnData = nullptr) const override
  {
    if (hint == QStyle::SH_ToolTip_WakeUpDelay)
    {
      return 1000;
    }
    if (hint == QStyle::SH_ToolTip_FallAsleepDelay)
    {
      return 300;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
  }
};

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  app.setWindowIcon(QIcon(":/icons/app_icon.ico"));
  app.setStyle(new HmiProxyStyle(app.style()));
  app.setStyleSheet(
    "QToolTip{"
    "background:#101820;"
    "color:#eef2f6;"
    "border:1px solid #4d9cff;"
    "border-radius:5px;"
    "padding:7px 9px;"
    "font-size:10pt;"
    "}"
  );

  MainWindow window;
  window.showMaximized();

  return app.exec();
}
