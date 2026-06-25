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
  setMinimumSize(230, 180);
  setMaximumHeight(520);
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

  m_measurementStatsLabel = new QLabel(this);
  m_measurementStatsLabel->setObjectName("tileMeasurements");
  m_measurementStatsLabel->setText(QString());
  m_measurementStatsLabel->setTextFormat(Qt::RichText);
  m_measurementStatsLabel->setWordWrap(false);
  m_measurementStatsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);
  layout->addWidget(m_titleLabel);
  layout->addWidget(m_statusLabel);
  layout->addWidget(m_measurementStatsLabel, 1);

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

void CameraTileWidget::setMeasurementStats(const CameraTileMeasurementTableLabels& labels,
                                           const QVector<CameraTileMeasurementStat>& stats)
{
  if (stats.isEmpty())
  {
    m_measurementStatsLabel->setText(labels.emptyText);
    return;
  }

  QString html = QString("<table width='100%' cellspacing='0' cellpadding='2'>"
                         "<tr><th align='left'>%1</th><th align='right'>%2</th>"
                         "<th align='right'>%3</th><th align='right'>%4</th><th align='right'>%5</th></tr>")
    .arg(labels.measurementColumn.toHtmlEscaped(),
         labels.currentColumn.toHtmlEscaped(),
         labels.averageColumn.toHtmlEscaped(),
         labels.minimumColumn.toHtmlEscaped(),
         labels.maximumColumn.toHtmlEscaped());
  for (const CameraTileMeasurementStat& stat : stats)
  {
    const QString color = stat.judgement == "NOK" ? "#ff4f5e" : (stat.judgement == "OK" ? "#35c46a" : "#d7dee6");
    html += QString("<tr style='color:%1'><td>%2</td><td align='right'>%3</td>"
                    "<td align='right'>%4</td><td align='right'>%5</td><td align='right'>%6</td></tr>")
      .arg(color,
           stat.name.toHtmlEscaped(),
           stat.current.toHtmlEscaped(),
           stat.average.toHtmlEscaped(),
           stat.minimum.toHtmlEscaped(),
           stat.maximum.toHtmlEscaped());
  }
  html += "</table>";
  m_measurementStatsLabel->setText(html);
}

void CameraTileWidget::setSelected(bool selected)
{
  m_selected = selected;

  if (m_selected)
  {
    setStyleSheet("#cameraTile{border:2px solid #2f80ed;background:#1d2731;border-radius:6px;}"
                  "#tileTitle{font-weight:600;color:#f4f7fb;}"
                  "#tileStatus{color:#b7c0c8;font-size:8pt;}"
                  "#tileMeasurements{color:#d7dee6;font-size:8pt;}");
  }
  else
  {
    setStyleSheet("#cameraTile{border:1px solid #323b45;background:#171d23;border-radius:6px;}"
                  "#tileTitle{font-weight:600;color:#edf1f5;}"
                  "#tileStatus{color:#8f9aa5;font-size:8pt;}"
                  "#tileMeasurements{color:#d7dee6;font-size:8pt;}");
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
