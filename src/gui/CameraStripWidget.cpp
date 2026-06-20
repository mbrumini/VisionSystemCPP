#include "gui/CameraStripWidget.h"

#include "gui/IconCatalog.h"

#include <QPainter>
#include <QHBoxLayout>
#include <QStyle>
#include <QToolButton>

namespace
{
QString cameraKind(const CameraConfig& camera)
{
  if (camera.profile.inspectionTypes.contains("ai"))
  {
    return "AI";
  }
  if (camera.profile.inspectionTypes.contains("dimensional"))
  {
    return "DIM";
  }
  if (camera.profile.inspectionTypes.contains("surface"))
  {
    return "SURF";
  }
  return "MIS";
}

QString cameraKindIconId(const CameraConfig& camera)
{
  if (camera.profile.inspectionTypes.contains("ai"))
  {
    return "tool-ai";
  }
  if (camera.profile.inspectionTypes.contains("dimensional"))
  {
    return "measurements";
  }
  if (camera.profile.inspectionTypes.contains("surface"))
  {
    return "surfaceDefects";
  }
  return "camera";
}

QIcon cameraStripIcon(const CameraConfig& camera, const QString& state)
{
  QPixmap canvas(58, 42);
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QColor border = state == "BUSY" ? QColor("#f5d547") :
    (state == "NOK" || state == "ERROR" ? QColor("#ff4f5e") : QColor("#35c46a"));
  const QRect imageRect(1, 1, 56, 40);
  painter.setPen(QPen(border, 2));
  painter.setBrush(QColor("#101820"));
  painter.drawRoundedRect(imageRect, 5, 5);

  const QIcon typeIcon = IconCatalog::icon(cameraKindIconId(camera));
  typeIcon.paint(&painter, imageRect.adjusted(13, 5, -13, -13));

  painter.setPen(QPen(QColor("#d7dee6"), 1));
  painter.setFont(QFont("Segoe UI", 7, QFont::Bold));
  painter.drawText(QRect(4, 27, 50, 11), Qt::AlignCenter, cameraKind(camera));

  painter.setPen(Qt::NoPen);
  painter.setBrush(border);
  painter.drawEllipse(QPointF(49, 10), 5, 5);
  return QIcon(canvas);
}
}

#include <QScrollArea>
#include <QVBoxLayout>

CameraStripWidget::CameraStripWidget(QWidget* parent)
  : QFrame(parent)
{
  setObjectName("cameraStrip");

  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  auto* scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setStyleSheet(
    "QScrollArea { background: transparent; }"
    "QWidget { background: transparent; }"
    "QScrollBar:horizontal {"
    "    border: none;"
    "    background: #111820;"
    "    height: 8px;"
    "    margin: 0px 0px 0px 0px;"
    "}"
    "QScrollBar::handle:horizontal {"
    "    background: #303a45;"
    "    min-width: 20px;"
    "    border-radius: 4px;"
    "}"
    "QScrollBar::handle:horizontal:hover {"
    "    background: #404b57;"
    "}"
    "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
    "    width: 0px;"
    "    background: none;"
    "}"
  );

  auto* scrollWidget = new QWidget(scrollArea);
  m_layout = new QHBoxLayout(scrollWidget);
  m_layout->setContentsMargins(10, 6, 10, 6);
  m_layout->setSpacing(8);

  scrollArea->setWidget(scrollWidget);
  mainLayout->addWidget(scrollArea);
}

void CameraStripWidget::setCameras(const QVector<CameraConfig>& cameras)
{
  m_cameras = cameras;
  rebuild();
}

void CameraStripWidget::setSelectedCamera(const QString& cameraId)
{
  const QString previous = m_selectedCameraId;
  m_selectedCameraId = cameraId;
  updateButton(previous);
  updateButton(cameraId);
}

void CameraStripWidget::setCameraBusy(const QString& cameraId, bool busy)
{
  m_busy[cameraId] = busy;
  updateButton(cameraId);
}

void CameraStripWidget::setCameraResult(const QString& cameraId, const QString& result)
{
  m_results[cameraId] = result;
  updateButton(cameraId);
}

void CameraStripWidget::setCameraClickHandler(std::function<void(const CameraConfig&)> handler)
{
  m_clickHandler = std::move(handler);
}

void CameraStripWidget::rebuild()
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

  for (const CameraConfig& camera : m_cameras)
  {
    auto* button = new QToolButton(m_layout->parentWidget());
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(58, 42));
    button->setMinimumSize(78, 70);
    button->setMaximumHeight(78);
    button->setCheckable(true);
    QObject::connect(button, &QToolButton::clicked, this, [this, camera]() {
      if (m_clickHandler)
      {
        m_clickHandler(camera);
      }
    });
    m_layout->addWidget(button);
    m_buttons.insert(camera.id, button);
    updateButton(camera.id);
  }

  m_layout->addStretch(1);
}

void CameraStripWidget::updateButton(const QString& cameraId)
{
  QToolButton* button = m_buttons.value(cameraId, nullptr);
  if (!button)
  {
    return;
  }

  const bool selected = cameraId == m_selectedCameraId;
  const bool busy = m_busy.value(cameraId, false);
  const QString result = m_results.value(cameraId, "READY");
  const QString state = busy ? "BUSY" : result;
  const auto cameraIt = std::find_if(m_cameras.cbegin(), m_cameras.cend(), [cameraId](const CameraConfig& camera) {
    return camera.id == cameraId;
  });
  if (cameraIt == m_cameras.cend())
  {
    return;
  }
  button->setChecked(selected);
  button->setIcon(cameraStripIcon(*cameraIt, state));
  button->setText(QString("%1\n%2").arg(cameraId, state));
  button->setToolTip(QString("%1 | %2 | %3").arg(cameraId, cameraKind(*cameraIt), state));
  button->setProperty("selected", selected);
  button->setProperty("state", state);
  button->style()->unpolish(button);
  button->style()->polish(button);
}
