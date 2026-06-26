#include "TestVisionStyle.h"

#include <QWidget>
namespace TestVisionStyle
{
QString stylesheet()
{
  return QStringLiteral(
    "QMainWindow,QWidget{background:#0f1419;color:#eef2f6;font-family:'Segoe UI';font-size:13px;}"
    "QTabWidget::pane{border:1px solid #313b46;border-radius:6px;top:-1px;background:#111820;}"
    "QTabBar::tab{background:#1a222c;color:#9aa4ad;border:1px solid #313b46;"
    "border-bottom:none;border-top-left-radius:6px;border-top-right-radius:6px;padding:8px 16px;margin-right:2px;}"
    "QTabBar::tab:selected{background:#24313d;color:#f5f7fa;border-color:#4d9cff;font-weight:600;}"
    "QTabBar::tab:hover{background:#202a34;color:#eef2f6;}"
    "QGroupBox{border:1px solid #313b46;border-radius:6px;margin-top:10px;padding:10px 8px 8px 8px;font-weight:600;}"
    "QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 6px;color:#cdd6df;}"
    "QPushButton{background:#24313d;border:1px solid #3b4652;border-radius:5px;padding:7px 12px;color:#f5f7fa;}"
    "QPushButton:hover{background:#2e3d4b;}"
    "QPushButton:disabled{background:#1a222c;color:#6f7d8a;border-color:#2d3741;}"
    "QPushButton#primaryButton{background:#1f6b3a;border-color:#35c46a;font-weight:700;}"
    "QPushButton#primaryButton:hover{background:#278a4a;}"
    "QPushButton#dangerButton{background:#6b1f2a;border-color:#ff4f5e;font-weight:700;}"
    "QPushButton#dangerButton:hover{background:#8a2734;}"
    "QPushButton#secondaryButton{background:#1d2a35;border-color:#4b6072;}"
    "QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox,QPlainTextEdit{"
    "background:#19222b;border:1px solid #3b4652;border-radius:5px;padding:4px 6px;color:#eef2f6;selection-background-color:#244a78;}"
    "QComboBox::drop-down,QSpinBox::up-button,QSpinBox::down-button,"
    "QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{border:none;width:18px;}"
    "QComboBox QAbstractItemView{background:#1d2a35;color:#f5f7fa;border:1px solid #4d9cff;"
    "selection-background-color:#244a78;outline:0;}"
    "QCheckBox{spacing:8px;color:#eef2f6;}"
    "QCheckBox::indicator{width:18px;height:18px;border:1px solid #6f7d8a;border-radius:3px;background:#19222b;}"
    "QCheckBox::indicator:checked{background:#2f80ed;border:2px solid #f5f7fa;}"
    "QTextEdit,QPlainTextEdit{background:#111820;border:1px solid #313b46;border-radius:5px;color:#d7dee6;}"
    "QTableWidget{background:#111820;alternate-background-color:#151e27;border:1px solid #313b46;"
    "border-radius:5px;color:#d7dee6;gridline-color:#2d3741;}"
    "QHeaderView::section{background:#1d2731;color:#eef2f6;border:0;padding:5px;font-weight:600;}"
    "QScrollArea{border:none;background:transparent;}"
    "QScrollBar:vertical{background:#111820;width:10px;margin:0;}"
    "QScrollBar::handle:vertical{background:#3b4652;border-radius:4px;min-height:24px;}"
    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
    "QLabel#appTitle{font-size:20px;font-weight:700;color:#f4f7fb;}"
    "QLabel#appSubtitle{color:#9aa4ad;font-size:12px;}"
    "QLabel#sectionHint{color:#9aa4ad;font-size:12px;}"
    "QLabel#statusBadge{background:#1d2731;border:1px solid #3b4652;border-radius:5px;"
    "padding:6px 10px;color:#cdd6df;}"
    "QLabel#statusBadge[state='running']{border-color:#f5d547;color:#f5d547;}"
    "QLabel#statusBadge[state='ok']{border-color:#35c46a;color:#35c46a;}"
    "QLabel#statusBadge[state='error']{border-color:#ff4f5e;color:#ff4f5e;}"
    "QLabel#planSummary{background:#151c23;border:1px solid #303a45;border-radius:6px;"
    "padding:8px 10px;color:#cdd6df;}"
    "QLabel#imagePreview{background:#101418;border:1px solid #313b46;border-radius:6px;color:#6f7d8a;}"
    "QSplitter::handle{background:#2d3741;}"
  );
}

void apply(QWidget* root)
{
  if (root)
  {
    root->setStyleSheet(stylesheet());
  }
}

} // namespace TestVisionStyle
