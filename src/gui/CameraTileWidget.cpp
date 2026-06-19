#include "CameraTileWidget.h"

#include <QMouseEvent>
#include <QSizePolicy>
#include <QString>
#include <QVBoxLayout>

namespace
{
QString cameraRoleText(const CameraConfig& camera)
{
  const bool hasAi = camera.profile.inspectionTypes.contains("ai");
  const bool hasNonAiInspection = camera.profile.inspectionTypes.contains("measurement") ||
    camera.profile.inspectionTypes.contains("surface") ||
    camera.profile.inspectionTypes.contains("dimensional");

  if (camera.profile.imageMode == "bw")
  {
    return "BN misurazioni";
  }

  if (hasAi && hasNonAiInspection)
  {
    return "Scala grigi + AI";
  }

  if (hasAi)
  {
    return "AI";
  }

  if (camera.profile.imageMode == "grayscale")
  {
    return "Scala grigi";
  }

  return camera.profile.imageMode;
}
}

CameraTileWidget::CameraTileWidget(const CameraConfig& camera, QWidget* parent)
  : QFrame(parent), m_camera(camera)
{
  setObjectName("cameraTile");
  setFrameShape(QFrame::StyledPanel);
  setCursor(Qt::PointingHandCursor);
  setMinimumSize(150, 112);
  setMaximumHeight(190);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  m_imageLabel = new QLabel(this);
  m_imageLabel->setAlignment(Qt::AlignCenter);
  m_imageLabel->setMinimumHeight(72);
  m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_imageLabel->setStyleSheet("background:#101418;color:#9aa4ad;");
  m_imageLabel->hide();

  m_titleLabel = new QLabel(QString("Cam %1").arg(camera.slot), this);
  m_titleLabel->setObjectName("tileTitle");

  m_statusLabel = new QLabel(cameraRoleText(camera), this);
  m_statusLabel->setObjectName("tileStatus");

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);
  layout->addWidget(m_titleLabel);
  layout->addWidget(m_statusLabel);

  setSelected(false);
}

const CameraConfig& CameraTileWidget::camera() const
{
  return m_camera;
}

void CameraTileWidget::setPreview(const QPixmap& preview)
{
  m_preview = preview;
  updatePreview();
}

void CameraTileWidget::setResultText(const QString& text)
{
  m_statusLabel->setText(text);
}

void CameraTileWidget::setSelected(bool selected)
{
  m_selected = selected;

  if (m_selected)
  {
    setStyleSheet("#cameraTile{border:2px solid #2f80ed;background:#1d2731;border-radius:6px;}"
                  "#tileTitle{font-weight:600;color:#f4f7fb;}"
                  "#tileStatus{color:#b7c0c8;font-size:8pt;}");
  }
  else
  {
    setStyleSheet("#cameraTile{border:1px solid #323b45;background:#171d23;border-radius:6px;}"
                  "#tileTitle{font-weight:600;color:#edf1f5;}"
                  "#tileStatus{color:#8f9aa5;font-size:8pt;}");
  }
}

void CameraTileWidget::setClickHandler(std::function<void(const CameraConfig&)> handler)
{
  m_clickHandler = std::move(handler);
}

void CameraTileWidget::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton && m_clickHandler)
  {
    m_clickHandler(m_camera);
  }

  QFrame::mousePressEvent(event);
}

void CameraTileWidget::resizeEvent(QResizeEvent* event)
{
  QFrame::resizeEvent(event);
  updatePreview();
}

void CameraTileWidget::updatePreview()
{
  if (m_preview.isNull())
  {
    m_imageLabel->setText("NO IMAGE");
  }
  else
  {
    m_imageLabel->setText("");
    m_imageLabel->setPixmap(m_preview.scaled(m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
}
