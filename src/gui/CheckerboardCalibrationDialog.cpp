#include "gui/CheckerboardCalibrationDialog.h"

#include "calibration/CalibrationPatternDetector.h"
#include "calibration/PlanarCalibrationEstimator.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <opencv2/calib3d.hpp>

CheckerboardCalibrationDialog::CheckerboardCalibrationDialog(const QVector<CameraConfig>& cameras,
                                                             const QString& selectedCameraId,
                                                             FrameProvider frameProvider,
                                                             PixmapConverter pixmapConverter,
                                                             CameraAction startLive,
                                                             CameraAction stopLive,
                                                             QWidget* parent)
  : QDialog(parent),
    m_cameras(cameras),
    m_frameProvider(std::move(frameProvider)),
    m_pixmapConverter(std::move(pixmapConverter)),
    m_startLive(std::move(startLive)),
    m_stopLive(std::move(stopLive))
{
  setWindowTitle("Sistema | Calibrazione checkerboard");
  resize(980, 720);

  auto* layout = new QVBoxLayout(this);
  auto* topRow = new QWidget(this);
  auto* topLayout = new QHBoxLayout(topRow);
  topLayout->setContentsMargins(0, 0, 0, 0);
  topLayout->setSpacing(12);

  auto* form = new QWidget(topRow);
  auto* formLayout = new QFormLayout(form);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setSpacing(8);

  m_cameraCombo = new QComboBox(form);
  for (int i = 0; i < m_cameras.size(); ++i)
  {
    const CameraConfig& camera = m_cameras[i];
    if (!camera.exists)
    {
      continue;
    }
    m_cameraCombo->addItem(QString("%1 | %2").arg(camera.id, camera.displayName), i);
    if (camera.id == selectedCameraId)
    {
      m_cameraCombo->setCurrentIndex(m_cameraCombo->count() - 1);
    }
  }

  m_columns = new QSpinBox(form);
  m_columns->setRange(2, 80);
  m_columns->setValue(9);
  m_rows = new QSpinBox(form);
  m_rows->setRange(2, 80);
  m_rows->setValue(6);
  m_pitch = new QDoubleSpinBox(form);
  m_pitch->setRange(0.001, 1000.0);
  m_pitch->setDecimals(4);
  m_pitch->setValue(1.0);
  m_pitch->setSuffix(" mm");

  auto* note = new QLabel("Colonne e righe sono gli angoli interni della scacchiera.", form);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);

  formLayout->addRow("Camera", m_cameraCombo);
  formLayout->addRow("Colonne interne", m_columns);
  formLayout->addRow("Righe interne", m_rows);
  formLayout->addRow("Lato quadretto", m_pitch);
  formLayout->addRow(note);
  topLayout->addWidget(form, 1);

  auto* controlColumn = new QWidget(topRow);
  auto* controlLayout = new QVBoxLayout(controlColumn);
  controlLayout->setContentsMargins(0, 0, 0, 0);
  controlLayout->setSpacing(8);

  m_freezeButton = new QPushButton("Ferma acquisizione", controlColumn);
  m_resumeButton = new QPushButton("Riprendi live", controlColumn);
  m_resumeButton->setEnabled(false);
  controlLayout->addWidget(m_freezeButton);
  controlLayout->addWidget(m_resumeButton);
  controlLayout->addStretch(1);
  topLayout->addWidget(controlColumn);
  layout->addWidget(topRow);

  m_preview = new QLabel(this);
  m_preview->setMinimumSize(760, 460);
  m_preview->setAlignment(Qt::AlignCenter);
  m_preview->setObjectName("imageCanvas");
  m_preview->setText("LIVE");
  layout->addWidget(m_preview, 1);

  m_status = new QLabel(this);
  m_status->setObjectName("toolPanelNote");
  m_status->setWordWrap(true);
  layout->addWidget(m_status);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  auto* calibrateButton = buttons->button(QDialogButtonBox::Ok);
  calibrateButton->setText("Calibra e salva");
  calibrateButton->setDefault(true);
  calibrateButton->setAutoDefault(true);
  layout->addWidget(buttons);

  m_liveTimer = new QTimer(this);
  m_liveTimer->setInterval(120);

  connect(m_liveTimer, &QTimer::timeout, this, [this]() { updateLiveFrame(); });
  connect(m_cameraCombo, &QComboBox::currentIndexChanged, this, [this](int) { restartLive(); });
  connect(m_freezeButton, &QPushButton::clicked, this, [this]() { freezeLiveFrame(); });
  connect(m_resumeButton, &QPushButton::clicked, this, [this]() { restartLive(); });
  connect(calibrateButton, &QPushButton::clicked, this, [this]() { calibrateCurrentFrame(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  if (m_cameraCombo->count() > 0)
  {
    restartLive();
  }
}

CheckerboardCalibrationDialog::Result CheckerboardCalibrationDialog::result() const
{
  return m_result;
}

int CheckerboardCalibrationDialog::selectedCameraIndex() const
{
  if (!m_cameraCombo)
  {
    return -1;
  }
  const int cameraIndex = m_cameraCombo->currentData().toInt();
  return cameraIndex >= 0 && cameraIndex < m_cameras.size() ? cameraIndex : -1;
}

CameraConfig CheckerboardCalibrationDialog::selectedCamera() const
{
  const int cameraIndex = selectedCameraIndex();
  return cameraIndex >= 0 ? m_cameras[cameraIndex] : CameraConfig();
}

void CheckerboardCalibrationDialog::setPreviewImage(const cv::Mat& image)
{
  if (image.empty())
  {
    m_preview->setPixmap(QPixmap());
    m_preview->setText("NO IMAGE");
    return;
  }
  const QPixmap pixmap = m_pixmapConverter ? m_pixmapConverter(image) : QPixmap();
  m_preview->setPixmap(pixmap.scaled(m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CheckerboardCalibrationDialog::ensureLiveStarted()
{
  const CameraConfig camera = selectedCamera();
  if (!camera.id.isEmpty() && m_startLive)
  {
    m_startLive(camera);
  }
}

void CheckerboardCalibrationDialog::updateLiveFrame()
{
  if (m_frozen)
  {
    return;
  }
  const CameraConfig camera = selectedCamera();
  if (camera.id.isEmpty() || !m_frameProvider)
  {
    return;
  }

  QString imageError;
  const cv::Mat input = m_frameProvider(camera, &imageError);
  if (input.empty())
  {
    m_status->setText(imageError.isEmpty() ? "Nessuna immagine live disponibile." : imageError);
    setPreviewImage({});
    return;
  }

  m_liveFrame = input.clone();
  setPreviewImage(m_liveFrame);
  m_status->setText(QString("Live: %1 | %2 x %3 px").arg(camera.id).arg(m_liveFrame.cols).arg(m_liveFrame.rows));
}

void CheckerboardCalibrationDialog::restartLive()
{
  m_frozen = false;
  m_frozenFrame.release();
  m_freezeButton->setEnabled(true);
  m_resumeButton->setEnabled(false);
  m_freezeButton->setText("Ferma acquisizione");
  ensureLiveStarted();
  updateLiveFrame();
  m_liveTimer->start();
}

void CheckerboardCalibrationDialog::freezeLiveFrame()
{
  updateLiveFrame();
  if (m_liveFrame.empty())
  {
    QMessageBox::warning(this, "Calibrazione checkerboard", "Non c'e' un frame live da bloccare.");
    return;
  }

  m_frozenFrame = m_liveFrame.clone();
  m_frozen = true;
  m_liveTimer->stop();

  const CameraConfig camera = selectedCamera();
  if (!camera.id.isEmpty() && m_stopLive)
  {
    m_stopLive(camera);
  }

  m_freezeButton->setEnabled(false);
  m_resumeButton->setEnabled(true);
  setPreviewImage(m_frozenFrame);
  m_status->setText(QString("Frame bloccato: %1 | %2 x %3 px").arg(camera.id).arg(m_frozenFrame.cols).arg(m_frozenFrame.rows));
}

void CheckerboardCalibrationDialog::calibrateCurrentFrame()
{
  const int cameraIndex = selectedCameraIndex();
  if (cameraIndex < 0)
  {
    return;
  }

  const CameraConfig camera = m_cameras[cameraIndex];
  cv::Mat input = m_frozen && !m_frozenFrame.empty() ? m_frozenFrame.clone() : m_liveFrame.clone();
  if (input.empty())
  {
    updateLiveFrame();
    input = m_liveFrame.clone();
  }
  if (input.empty())
  {
    QMessageBox::warning(this, "Calibrazione checkerboard", "Nessuna immagine valida da calibrare.");
    return;
  }

  CalibrationPatternConfig pattern;
  pattern.type = CalibrationPatternType::Checkerboard;
  pattern.gridSize = QSize(m_columns->value(), m_rows->value());
  pattern.pitchMm = m_pitch->value();

  CalibrationPatternDetector detector;
  const QVector<QPointF> corners = detector.detect(input, pattern);
  if (corners.size() != pattern.gridSize.width() * pattern.gridSize.height())
  {
    const QString message = QString("Checkerboard non trovata: attesi %1 punti, trovati %2.")
      .arg(pattern.gridSize.width() * pattern.gridSize.height())
      .arg(corners.size());
    m_status->setText(message);
    QMessageBox::warning(this, "Calibrazione checkerboard", message);
    return;
  }

  PlanarCalibrationEstimator estimator;
  CameraCalibrationModel model = estimator.estimateScaleOnly(camera.id, pattern, corners);
  if (!model.valid)
  {
    const QString message = QString("Calibrazione non valida: %1").arg(camera.id);
    m_status->setText(message);
    QMessageBox::warning(this, "Calibrazione checkerboard", message);
    return;
  }

  cv::Mat diagnostic = input.clone();
  std::vector<cv::Point2f> cvCorners;
  cvCorners.reserve(static_cast<size_t>(corners.size()));
  for (const QPointF& corner : corners)
  {
    cvCorners.emplace_back(static_cast<float>(corner.x()), static_cast<float>(corner.y()));
  }
  cv::drawChessboardCorners(
    diagnostic,
    cv::Size(pattern.gridSize.width(), pattern.gridSize.height()),
    cvCorners,
    true);
  setPreviewImage(diagnostic);

  model.sourceFile = m_frozen ? "live-frame-frozen" : "live-frame";
  m_result.cameraIndex = cameraIndex;
  m_result.model = model;
  accept();
}
