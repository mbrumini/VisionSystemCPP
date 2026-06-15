#include "gui/MainWindow.h"

void MainWindow::applyApplicationStyle()
{
  setStyleSheet(
    "QMainWindow,QWidget{background:#0f1419;color:#eef2f6;font-family:'Segoe UI';font-size:13px;}"
    "QGroupBox{border:1px solid #313b46;border-radius:6px;margin-top:8px;padding:8px 7px 7px 7px;font-weight:600;}"
    "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;color:#cdd6df;}"
    "QPushButton{background:#24313d;border:1px solid #3b4652;border-radius:5px;padding:6px;color:#f5f7fa;}"
    "QPushButton:hover{background:#2e3d4b;}"
    "QPushButton#touchIconButton{min-width:58px;min-height:58px;max-width:68px;max-height:68px;border-radius:6px;padding:5px;}"
    "QToolButton{background:#24313d;border:1px solid #3b4652;border-radius:5px;padding:5px;color:#f5f7fa;}"
    "QToolButton:hover{background:#2e3d4b;}"
    "QToolButton:checked{background:#244a78;border:1px solid #4d9cff;}"
    "#cameraStrip QToolButton{min-width:78px;min-height:70px;max-height:78px;font-weight:700;padding:4px;}"
    "#cameraStrip QToolButton[state='READY']{border-color:#35c46a;}"
    "#cameraStrip QToolButton[state='OK']{border-color:#35c46a;}"
    "#cameraStrip QToolButton[state='BUSY']{border-color:#f5d547;}"
    "#cameraStrip QToolButton[state='NOK'],#cameraStrip QToolButton[state='ERROR']{border-color:#ff4f5e;}"
    "#cameraStrip QToolButton:checked{background:#244a78;border:2px solid #4d9cff;}"
    "QToolButton#touchIconButton{min-width:58px;min-height:58px;max-width:68px;max-height:68px;border-radius:6px;}"
    "QTextEdit{background:#111820;border:1px solid #313b46;border-radius:5px;color:#d7dee6;}"
    "QTableWidget{background:#111820;alternate-background-color:#151e27;border:1px solid #313b46;border-radius:5px;color:#d7dee6;gridline-color:#2d3741;}"
    "QHeaderView::section{background:#1d2731;color:#eef2f6;border:0;padding:4px;font-weight:600;}"
    "#commandToolbar,#cameraStrip,#measurementResults,#toolIconBar{background:#151c23;border:1px solid #303a45;border-radius:6px;}"
    "#toolbarRecipe{color:#cdd6df;font-weight:600;padding-right:12px;}"
    "#toolbarStatus{color:#35c46a;font-weight:700;}"
    "#measurementResultsTitle{font-weight:700;color:#f4f7fb;}"
    "#largeTitle{font-size:20px;font-weight:700;color:#f4f7fb;}"
    "#panelStatus{font-size:17px;font-weight:700;color:#f4f7fb;}"
    "#toolPanelTitle{font-size:15px;font-weight:700;color:#f4f7fb;}"
    "#toolPanelNote{color:#9aa4ad;}"
  );
}

