#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMap>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTime>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <functional>
#include <vector>

#include "TestVisionArtifacts.h"
#include "CampaignEditorDialog.h"

namespace
{
const wchar_t* kPipePath = L"\\\\.\\pipe\\VisionSystemSimulator";

struct TestPose
{
  int poseIndex = 0;
  int pass = 1;
  double xMm = 0.0;
  double yMm = 0.0;
  double angleDeg = 0.0;
};

struct SentFrameInfo
{
  TestPose pose;
  double expectedX = 0.0;
  double expectedY = 0.0;
  double expectedAngle = 0.0;
};

struct TestResult
{
  TestPose pose;
  int frameId = 0;
  bool valid = false;
  double expectedX = 0.0;
  double expectedY = 0.0;
  double expectedAngle = 0.0;
  double actualX = 0.0;
  double actualY = 0.0;
  double actualAngle = 0.0;
  double centerError = 0.0;
  double angleError = 0.0;
  double processingMs = 0.0;
};

QJsonObject loadJson(const QString& path, QString* error)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
  {
    *error = "Impossibile aprire scenario: " + path;
    return {};
  }
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    *error = "Scenario JSON non valido: " + parseError.errorString();
    return {};
  }
  return document.object();
}

cv::Mat createAsymmetricCross(int width, int height, int background)
{
  cv::Mat image(height, width, CV_8UC3, cv::Scalar(background, background, background));
  const cv::Point center(width / 2, height / 2);
  const cv::Scalar dark(20, 20, 20);
  cv::line(image, center + cv::Point(-150, 0), center + cv::Point(185, 0), dark, 26, cv::LINE_AA);
  cv::line(image, center + cv::Point(0, -120), center + cv::Point(0, 145), dark, 26, cv::LINE_AA);
  cv::circle(image, center + cv::Point(145, 0), 22, cv::Scalar(80, 80, 80), -1, cv::LINE_AA);
  cv::circle(image, center, 14, cv::Scalar(240, 240, 240), -1, cv::LINE_AA);
  cv::line(image, center, center + cv::Point(95, -70), dark, 12, cv::LINE_AA);
  return image;
}

cv::Mat createShape(const QString& id, int width, int height, int background)
{
  if (id == "cross")
  {
    return createAsymmetricCross(width, height, background);
  }

  cv::Mat image(height, width, CV_8UC3, cv::Scalar(background, background, background));
  const cv::Point center(width / 2, height / 2);
  const cv::Scalar dark(20, 20, 20);
  if (id == "rectangle")
  {
    cv::RotatedRect rect(center, cv::Size2f(300, 170), 0.0F);
    cv::Point2f points[4];
    rect.points(points);
    for (int i = 0; i < 4; ++i)
      cv::line(image, points[i], points[(i + 1) % 4], dark, 24, cv::LINE_AA);
    cv::circle(image, center + cv::Point(95, -45), 24, dark, -1, cv::LINE_AA);
  }
  else if (id == "circles")
  {
    cv::circle(image, center, 145, dark, 22, cv::LINE_AA);
    cv::circle(image, center, 75, dark, 18, cv::LINE_AA);
    cv::circle(image, center + cv::Point(95, 0), 16, dark, -1, cv::LINE_AA);
  }
  else if (id == "plate")
  {
    cv::rectangle(image, center + cv::Point(-170, -110), center + cv::Point(170, 110), dark, 20, cv::LINE_AA);
    for (const cv::Point& offset : {
           cv::Point(-105, -55), cv::Point(100, -55),
           cv::Point(-105, 55), cv::Point(100, 55)})
      cv::circle(image, center + offset, 25, dark, -1, cv::LINE_AA);
    cv::circle(image, center + cv::Point(35, 0), 42, dark, 15, cv::LINE_AA);
  }
  else if (id == "l_profile")
  {
    std::vector<cv::Point> polygon = {
      center + cv::Point(-150, -120),
      center + cv::Point(-70, -120),
      center + cv::Point(-70, 45),
      center + cv::Point(155, 45),
      center + cv::Point(155, 120),
      center + cv::Point(-150, 120)
    };
    cv::fillPoly(image, std::vector<std::vector<cv::Point>>{polygon}, dark);
    cv::circle(image, center + cv::Point(80, 82), 22, cv::Scalar(background, background, background), -1, cv::LINE_AA);
  }
  else
  {
    const int teeth = 12;
    std::vector<cv::Point> polygon;
    for (int i = 0; i < teeth * 2; ++i)
    {
      const double angle = i * CV_PI / teeth;
      const double radius = (i % 2 == 0) ? 145.0 : 112.0;
      polygon.emplace_back(
        center.x + static_cast<int>(std::round(radius * std::cos(angle))),
        center.y + static_cast<int>(std::round(radius * std::sin(angle))));
    }
    cv::fillPoly(image, std::vector<std::vector<cv::Point>>{polygon}, dark);
    cv::circle(image, center, 48, cv::Scalar(background, background, background), -1, cv::LINE_AA);
    cv::circle(image, center + cv::Point(72, 0), 15, cv::Scalar(background, background, background), -1, cv::LINE_AA);
  }
  return image;
}

cv::Mat transformImage(const cv::Mat& master, double angle, double txPx, double tyPx)
{
  const cv::Point2f center(master.cols * 0.5F, master.rows * 0.5F);
  cv::Mat matrix = cv::getRotationMatrix2D(center, angle, 1.0);
  matrix.at<double>(0, 2) += txPx;
  matrix.at<double>(1, 2) += tyPx;
  cv::Mat output;
  cv::warpAffine(
    master,
    output,
    matrix,
    master.size(),
    cv::INTER_LINEAR,
    cv::BORDER_CONSTANT,
    cv::Scalar(240, 240, 240));
  return output;
}

QByteArray jpegBase64(const cv::Mat& image)
{
  std::vector<uchar> bytes;
  std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 85};
  cv::imencode(".jpg", image, bytes, params);
  return QByteArray(
    reinterpret_cast<const char*>(bytes.data()),
    static_cast<qsizetype>(bytes.size())).toBase64();
}

QPixmap matToPixmap(const cv::Mat& image)
{
  cv::Mat rgb;
  cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
  return QPixmap::fromImage(
    QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy());
}

double normalizePcaAngleNear(double angle, double reference)
{
  while (angle - reference > 90.0) angle -= 180.0;
  while (angle - reference < -90.0) angle += 180.0;
  return angle;
}

double pcaAngleError(double actual, double expected)
{
  return std::abs(normalizePcaAngleNear(actual, expected) - expected);
}

double standardDeviation(const std::vector<double>& values)
{
  if (values.size() < 2)
  {
    return 0.0;
  }
  const double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
  double sum = 0.0;
  for (double value : values)
  {
    const double delta = value - mean;
    sum += delta * delta;
  }
  return std::sqrt(sum / values.size());
}

QVector<double> steppedValues(double minimum, double maximum, double step)
{
  QVector<double> result;
  if (step <= 0.0 || maximum < minimum)
  {
    return result;
  }
  for (double value = minimum; value <= maximum + step * 0.0001; value += step)
  {
    result.append(value);
    if (result.size() > 100000)
    {
      break;
    }
  }
  return result;
}
}

class PlotWidget : public QWidget
{
public:
  enum class Mode { Angle, CenterError };

  PlotWidget(QString title, Mode mode, QWidget* parent = nullptr)
    : QWidget(parent), m_title(std::move(title)), m_mode(mode)
  {
    setMinimumHeight(230);
  }

  void setResults(const QVector<TestResult>& results)
  {
    m_results = results;
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#111820"));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#e8edf2"));
    painter.drawText(QRect(12, 8, width() - 24, 24), Qt::AlignLeft, m_title);

    const QRectF area(52, 38, width() - 70, height() - 72);
    painter.setPen(QColor("#596572"));
    painter.drawRect(area);
    if (m_results.isEmpty())
    {
      painter.drawText(area, Qt::AlignCenter, "Nessun dato");
      return;
    }

    double minX = m_results.first().expectedAngle;
    double maxX = minX;
    double minY = m_mode == Mode::Angle
      ? normalizePcaAngleNear(
          m_results.first().actualAngle,
          m_results.first().expectedAngle)
      : 0.0;
    double maxY = minY;
    for (const TestResult& result : m_results)
    {
      minX = std::min(minX, result.expectedAngle);
      maxX = std::max(maxX, result.expectedAngle);
      const double y = m_mode == Mode::Angle
        ? normalizePcaAngleNear(result.actualAngle, result.expectedAngle)
        : result.centerError;
      minY = std::min(minY, y);
      maxY = std::max(maxY, y);
    }
    if (m_mode == Mode::Angle)
    {
      const double commonMinimum = std::min(minX, minY);
      const double commonMaximum = std::max(maxX, maxY);
      minX = commonMinimum;
      minY = commonMinimum;
      maxX = commonMaximum;
      maxY = commonMaximum;
    }
    if (std::abs(maxX - minX) < 0.001) maxX = minX + 1.0;
    if (std::abs(maxY - minY) < 0.001) maxY = minY + 1.0;

    auto mapPoint = [&](double x, double y) {
      return QPointF(
        area.left() + (x - minX) / (maxX - minX) * area.width(),
        area.bottom() - (y - minY) / (maxY - minY) * area.height());
    };

    const QVector<QColor> colors = {
      QColor("#4da3ff"), QColor("#40c980"), QColor("#ffad42"),
      QColor("#d576ff"), QColor("#ff667a")
    };
    if (m_mode == Mode::Angle)
    {
      painter.setPen(QPen(QColor("#8b949e"), 2, Qt::DashLine));
      painter.drawLine(mapPoint(minX, minX), mapPoint(maxX, maxX));
      painter.drawText(
        QRectF(area.left() + 8, area.top() + 6, 190, 20),
        Qt::AlignLeft,
        "Ideale: atteso = rilevato");
    }
    QMap<int, QVector<TestResult>> byPass;
    for (const TestResult& result : m_results)
    {
      byPass[result.pose.pass].append(result);
    }
    int colorIndex = 0;
    for (auto it = byPass.cbegin(); it != byPass.cend(); ++it, ++colorIndex)
    {
      painter.setPen(QPen(colors[colorIndex % colors.size()], 2));
      QPointF previous;
      bool hasPrevious = false;
      for (const TestResult& result : it.value())
      {
        const double y = m_mode == Mode::Angle
          ? normalizePcaAngleNear(result.actualAngle, result.expectedAngle)
          : result.centerError;
        const QPointF point = mapPoint(result.expectedAngle, y);
        if (hasPrevious) painter.drawLine(previous, point);
        const bool outsideTolerance =
          m_mode == Mode::Angle && result.angleError > 5.0;
        painter.setPen(QPen(
          outsideTolerance ? QColor("#ff4d5a") : colors[colorIndex % colors.size()],
          outsideTolerance ? 3 : 2));
        painter.setBrush(outsideTolerance
          ? QColor("#ff4d5a")
          : colors[colorIndex % colors.size()]);
        painter.drawEllipse(point, outsideTolerance ? 5 : 4, outsideTolerance ? 5 : 4);
        painter.setPen(QPen(colors[colorIndex % colors.size()], 2));
        previous = point;
        hasPrevious = true;
      }
      painter.drawText(
        QRectF(area.right() - 72, area.top() + colorIndex * 18, 68, 18),
        Qt::AlignRight,
        QString("Giro %1").arg(it.key()));
    }

    painter.setPen(QColor("#aab4be"));
    painter.drawText(8, area.top() + 12, QString::number(maxY, 'f', 1));
    painter.drawText(8, area.bottom(), QString::number(minY, 'f', 1));
    painter.drawText(area.left(), height() - 12, QString::number(minX, 'f', 1) + "°");
    painter.drawText(area.right() - 45, height() - 12, QString::number(maxX, 'f', 1) + "°");
  }

private:
  QString m_title;
  Mode m_mode;
  QVector<TestResult> m_results;
};

class RepeatabilityPlotWidget : public QWidget
{
public:
  explicit RepeatabilityPlotWidget(QWidget* parent = nullptr) : QWidget(parent)
  {
    setMinimumHeight(210);
  }

  void setResults(const QVector<TestResult>& results)
  {
    m_results = results;
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#111820"));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#e8edf2"));
    painter.drawText(QRect(12, 8, width() - 24, 24), Qt::AlignLeft,
                     "Delta centro rispetto al giro 1");
    const QRectF area(52, 38, width() - 70, height() - 65);
    painter.setPen(QColor("#596572"));
    painter.drawRect(area);

    QMap<int, TestResult> references;
    for (const TestResult& result : m_results)
      if (result.pose.pass == 1) references[result.pose.poseIndex] = result;

    struct Delta { int pose; int pass; double value; };
    QVector<Delta> deltas;
    double maximum = 0.001;
    int maxPose = 1;
    for (const TestResult& result : m_results)
    {
      if (result.pose.pass <= 1 || !references.contains(result.pose.poseIndex)) continue;
      const TestResult& reference = references[result.pose.poseIndex];
      const double value = std::hypot(
        result.actualX - reference.actualX,
        result.actualY - reference.actualY);
      deltas.append({result.pose.poseIndex, result.pose.pass, value});
      maximum = std::max(maximum, value);
      maxPose = std::max(maxPose, result.pose.poseIndex);
    }
    if (deltas.isEmpty())
    {
      painter.drawText(area, Qt::AlignCenter, "Servono almeno 2 giri");
      return;
    }

    const QVector<QColor> colors = {QColor("#40c980"), QColor("#ffad42"), QColor("#d576ff")};
    for (int pass = 2; pass <= 20; ++pass)
    {
      painter.setPen(QPen(colors[(pass - 2) % colors.size()], 2));
      QPointF previous;
      bool hasPrevious = false;
      for (const Delta& delta : deltas)
      {
        if (delta.pass != pass) continue;
        const QPointF current(
          area.left() + (delta.pose - 1.0) / std::max(1, maxPose - 1) * area.width(),
          area.bottom() - delta.value / maximum * area.height());
        if (hasPrevious) painter.drawLine(previous, current);
        painter.setBrush(colors[(pass - 2) % colors.size()]);
        painter.drawEllipse(current, 4, 4);
        previous = current;
        hasPrevious = true;
      }
    }
    painter.setPen(QColor("#aab4be"));
    painter.drawText(8, area.top() + 12, QString::number(maximum, 'f', 4));
    painter.drawText(8, area.bottom(), "0");
  }

private:
  QVector<TestResult> m_results;
};

class TestVisionWindow : public QMainWindow
{
public:
  explicit TestVisionWindow(const QString& scenarioPath, QWidget* parent = nullptr)
    : QMainWindow(parent)
    , m_scenarioPath(scenarioPath)
  {
    setWindowTitle("VisionSystem TestVision");
    resize(1200, 850);
    buildUi();

    QString error;
    if (!loadScenario(&error))
    {
      QMessageBox::critical(this, "TestVision", error);
      m_startButton->setEnabled(false);
    }
  }

  ~TestVisionWindow() override
  {
    closePipe();
  }

  void setSendOnlyMode(bool enabled)
  {
    m_sendOnlyMode = enabled;
    if (m_sendOnlyCheck)
    {
      m_sendOnlyCheck->setChecked(enabled);
    }
  }
  bool sendOnlyMode() const { return m_sendOnlyMode; }

  void startCampaignWorker(
    const QJsonObject& item,
    int cycle,
    std::function<void(const QJsonObject&)> completion,
    std::function<void(const QString&, const TestResult&)> resultCallback = {})
  {
    const QString cameraId = item.value("cameraId").toString("CAM01");
    const QString baseScenarioName = m_scenario.value("name").toString(
      QFileInfo(m_scenarioPath).completeBaseName());
    m_scenario["name"] = baseScenarioName + "_" + cameraId;
    m_scenario["output"] = QString("../reports/%1_%2.json")
      .arg(QFileInfo(m_scenarioPath).completeBaseName(), cameraId);
    m_workerItem = item;
    m_workerCycle = cycle;
    m_workerCompletion = std::move(completion);
    m_workerResultCallback = std::move(resultCallback);
    m_isCampaignWorker = true;
    applyCampaignItem(item);
    QTimer::singleShot(0, this, [this]() { startTest(); });
  }

  bool loadCampaignFromFile(
    const QString& path,
    QString* error,
    bool autoStart = false)
  {
    const QJsonObject campaign = loadJson(path, error);
    const QJsonArray items = campaign.value("items").toArray();
    if (campaign.isEmpty() || items.isEmpty())
    {
      if (error && error->isEmpty())
      {
        *error = "La campagna non contiene prove.";
      }
      return false;
    }
    m_campaignPath = path;
    m_campaign = campaign;
    m_campaignItems = items;
    m_campaignCycles = qMax(1, campaign.value("cycles").toInt(1));
    m_startCampaignButton->setEnabled(true);
    appendLog(QString("Campagna caricata: %1 | prove=%2 | cicli=%3 | multicamera=%4")
      .arg(campaign.value("name").toString(QFileInfo(path).completeBaseName()))
      .arg(items.size())
      .arg(m_campaignCycles)
      .arg(campaign.value("parallel").toBool(true) ? "si" : "no"));
    if (autoStart)
    {
      QTimer::singleShot(0, this, [this]() { startCampaign(); });
    }
    return true;
  }

  void startBroadcastFromCameras(const QStringList& cameras)
  {
    if (!m_cameraTargets)
    {
      return;
    }
    QMap<QString, QString> requested;
    for (const QString& configuredTarget : cameras)
    {
      const QStringList parts = configuredTarget.split(':');
      const QString cameraId = parts.value(0).trimmed().toUpper();
      const QString strategyId = parts.value(1).trimmed();
      requested.insert(cameraId, strategyId);
    }
    for (int row = 0; row < m_cameraTargets->rowCount(); ++row)
    {
      QTableWidgetItem* enabled = m_cameraTargets->item(row, 0);
      QTableWidgetItem* camera = m_cameraTargets->item(row, 1);
      const QString cameraId = camera->data(Qt::UserRole).toString();
      enabled->setCheckState(
        requested.contains(cameraId)
          ? Qt::Checked
          : Qt::Unchecked);
      if (requested.contains(cameraId) && !requested.value(cameraId).isEmpty())
      {
        auto* strategy = qobject_cast<QComboBox*>(
          m_cameraTargets->cellWidget(row, 2));
        const int strategyIndex = strategy
          ? strategy->findData(requested.value(cameraId))
          : -1;
        if (strategyIndex >= 0)
        {
          strategy->setCurrentIndex(strategyIndex);
        }
      }
    }
    QTimer::singleShot(0, this, [this]() { startTest(); });
  }

private:
  enum class State { Idle, WaitingHello, WaitingRecipe, WaitingAccepted, WaitingResult };

  QDoubleSpinBox* doubleSpin(double value, double minimum, double maximum, double step, int decimals = 3)
  {
    auto* spin = new QDoubleSpinBox(this);
    spin->setRange(minimum, maximum);
    spin->setDecimals(decimals);
    spin->setSingleStep(step);
    spin->setValue(value);
    return spin;
  }

  void buildUi()
  {
    auto* tabs = new QTabWidget(this);
    setCentralWidget(tabs);

    auto* execution = new QWidget(tabs);
    auto* executionLayout = new QVBoxLayout(execution);
    auto* splitter = new QSplitter(Qt::Horizontal, execution);

    auto* previewPanel = new QWidget(splitter);
    auto* previewLayout = new QVBoxLayout(previewPanel);
    m_preview = new QLabel(previewPanel);
    m_preview->setMinimumSize(640, 480);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet("background:#15191f;border:1px solid #4a5563;");
    previewLayout->addWidget(m_preview, 1);

    auto* statusForm = new QFormLayout();
    m_scenarioLabel = new QLabel(previewPanel);
    m_frameLabel = new QLabel("-", previewPanel);
    m_expectedLabel = new QLabel("-", previewPanel);
    m_actualLabel = new QLabel("-", previewPanel);
    m_stateLabel = new QLabel("Pronto", previewPanel);
    statusForm->addRow("Scenario", m_scenarioLabel);
    statusForm->addRow("Frame", m_frameLabel);
    statusForm->addRow("Atteso", m_expectedLabel);
    statusForm->addRow("Rilevato", m_actualLabel);
    statusForm->addRow("Stato", m_stateLabel);
    previewLayout->addLayout(statusForm);

    auto* configPanel = new QWidget(splitter);
    auto* configLayout = new QVBoxLayout(configPanel);
    auto* broadcast = new QWidget(tabs);
    auto* broadcastLayout = new QVBoxLayout(broadcast);
    auto* form = new QFormLayout();
    m_xMin = doubleSpin(0.0, -1000.0, 1000.0, 0.5);
    m_xMax = doubleSpin(0.0, -1000.0, 1000.0, 0.5);
    m_xStep = doubleSpin(1.0, 0.001, 1000.0, 0.5);
    m_yMin = doubleSpin(0.0, -1000.0, 1000.0, 0.5);
    m_yMax = doubleSpin(0.0, -1000.0, 1000.0, 0.5);
    m_yStep = doubleSpin(1.0, 0.001, 1000.0, 0.5);
    m_angleMin = doubleSpin(0.0, -360.0, 360.0, 5.0);
    m_angleMax = doubleSpin(45.0, -360.0, 360.0, 5.0);
    m_angleStep = doubleSpin(15.0, 0.001, 360.0, 5.0);
    m_pixelSize = doubleSpin(0.0654, 0.000001, 1000.0, 0.001, 6);
    m_shapeCombo = new QComboBox(configPanel);
    m_shapeCombo->addItem("Croce asimmetrica", "cross");
    m_shapeCombo->addItem("Rettangolo asimmetrico", "rectangle");
    m_shapeCombo->addItem("Cerchi concentrici", "circles");
    m_shapeCombo->addItem("Piastra con fori", "plate");
    m_shapeCombo->addItem("Profilo a L", "l_profile");
    m_shapeCombo->addItem("Ruota dentata", "gear");
    m_strategyCombo = new QComboBox(configPanel);
    m_strategyCombo->addItem("Massa / PCA", "massPca");
    m_strategyCombo->addItem("Edge / PCA", "edgePca");
    m_strategyCombo->addItem("Corona a soglia", "threshold");
    m_strategyCombo->addItem("Corona edge", "edge");
    m_strategyCombo->addItem("Due cerchi / asse", "two_circles_axis");
    m_strategyCombo->addItem("Shape model", "shapeModel");
    m_strategyCombo->addItem("Template model", "templateModel");
    m_strategyCombo->addItem("Localizzazione AI YOLO", "aiYolo");
    m_recipeCombo = new QComboBox(configPanel);
    m_cameraCombo = new QComboBox(configPanel);
    m_cameraCombo->addItem("Selezionata in Vision", "SELECTED");
    for (int i = 1; i <= 16; ++i)
    {
      const QString camId = QString("CAM%1").arg(i, 2, 10, QChar('0'));
      m_cameraCombo->addItem(camId, camId);
    }
    m_cameraTargets = new QTableWidget(16, 3, broadcast);
    m_cameraTargets->setHorizontalHeaderLabels({"Usa", "Camera", "Strategia"});
    m_cameraTargets->verticalHeader()->hide();
    m_cameraTargets->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_cameraTargets->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_cameraTargets->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_cameraTargets->setMinimumHeight(430);
    for (int i = 1; i <= 16; ++i)
    {
      const QString camId = QString("CAM%1").arg(i, 2, 10, QChar('0'));
      auto* enabled = new QTableWidgetItem();
      enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      enabled->setCheckState(Qt::Unchecked);
      m_cameraTargets->setItem(i - 1, 0, enabled);
      auto* camera = new QTableWidgetItem(camId);
      camera->setData(Qt::UserRole, camId);
      camera->setFlags(Qt::ItemIsEnabled);
      m_cameraTargets->setItem(i - 1, 1, camera);
      auto* strategy = new QComboBox(m_cameraTargets);
      for (int index = 0; index < m_strategyCombo->count(); ++index)
      {
        strategy->addItem(
          m_strategyCombo->itemText(index),
          m_strategyCombo->itemData(index));
      }
      m_cameraTargets->setCellWidget(i - 1, 2, strategy);
    }
    m_passes = new QSpinBox(configPanel);
    m_passes->setRange(1, 1000);
    m_passes->setValue(3);
    m_intervalMs = new QSpinBox(configPanel);
    m_intervalMs->setRange(0, 60000);
    m_intervalMs->setValue(200);
    form->addRow("Target telecamera", m_cameraCombo);
    form->addRow("Shape campione", m_shapeCombo);
    form->addRow("Strategia da testare", m_strategyCombo);
    form->addRow("Ricetta Vision", m_recipeCombo);
    form->addRow("X minimo (mm)", m_xMin);
    form->addRow("X massimo (mm)", m_xMax);
    form->addRow("Passo X (mm)", m_xStep);
    form->addRow("Y minimo (mm)", m_yMin);
    form->addRow("Y massimo (mm)", m_yMax);
    form->addRow("Passo Y (mm)", m_yStep);
    form->addRow("Angolo minimo (°)", m_angleMin);
    form->addRow("Angolo massimo (°)", m_angleMax);
    form->addRow("Passo angolo (°)", m_angleStep);
    form->addRow("Scala (mm/px)", m_pixelSize);
    form->addRow("Giri completi", m_passes);
    form->addRow("Intervallo frame (ms)", m_intervalMs);
    m_sendOnlyCheck = new QCheckBox("Solo invio immagini (non attendere risultati)", configPanel);
    m_sendOnlyCheck->setToolTip(
      "Invia i frame al ritmo configurato senza aspettare la risposta di Vision.");
    form->addRow(m_sendOnlyCheck);
    configLayout->addLayout(form);

    m_totalLabel = new QLabel(configPanel);
    m_totalLabel->setWordWrap(true);
    configLayout->addWidget(m_totalLabel);
    m_broadcastTable = new QTableWidget(broadcast);
    m_broadcastTable->setColumnCount(9);
    m_broadcastTable->setHorizontalHeaderLabels({
      "Camera", "Frame", "Valida", "X", "Y", "Angolo",
      "Err. centro", "Err. angolo", "ms"
    });
    m_broadcastTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_broadcastTable->setMinimumHeight(220);
    m_broadcastTable->hide();
    auto* buttons = new QHBoxLayout();
    m_startButton = new QPushButton("Start", configPanel);
    m_stopButton = new QPushButton("Stop", configPanel);
    m_sendSampleButton = new QPushButton("Invia campione a Vision", configPanel);
    m_generateDatasetButton = new QPushButton("Genera dataset AI", configPanel);
    m_loadCampaignButton = new QPushButton("Carica campagna", configPanel);
    m_editCampaignButton = new QPushButton("Crea / modifica campagna", configPanel);
    m_startCampaignButton = new QPushButton("Avvia campagna", configPanel);
    m_startCampaignButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    buttons->addWidget(m_sendSampleButton);
    buttons->addWidget(m_generateDatasetButton);
    buttons->addWidget(m_loadCampaignButton);
    buttons->addWidget(m_editCampaignButton);
    buttons->addWidget(m_startCampaignButton);
    buttons->addWidget(m_startButton);
    buttons->addWidget(m_stopButton);
    configLayout->addLayout(buttons);
    configLayout->addStretch(1);

    splitter->addWidget(previewPanel);
    splitter->addWidget(configPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    executionLayout->addWidget(splitter, 1);

    m_log = new QPlainTextEdit(execution);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(170);
    executionLayout->addWidget(m_log);
    tabs->addTab(execution, "Esecuzione");

    auto* broadcastTitle = new QLabel(
      "Seleziona le telecamere da usare e assegna a ciascuna la propria strategia.",
      broadcast);
    broadcastTitle->setWordWrap(true);
    broadcastLayout->addWidget(broadcastTitle);
    broadcastLayout->addWidget(m_cameraTargets, 3);
    auto* broadcastButtons = new QHBoxLayout();
    auto* startBroadcastButton = new QPushButton("Avvia broadcast", broadcast);
    auto* stopBroadcastButton = new QPushButton("Ferma broadcast", broadcast);
    broadcastButtons->addStretch(1);
    broadcastButtons->addWidget(startBroadcastButton);
    broadcastButtons->addWidget(stopBroadcastButton);
    broadcastLayout->addLayout(broadcastButtons);
    auto* sendOnlyBroadcast = new QCheckBox(
      "Solo invio immagini (non attendere risultati)", broadcast);
    sendOnlyBroadcast->setToolTip(m_sendOnlyCheck->toolTip());
    sendOnlyBroadcast->setChecked(m_sendOnlyCheck->isChecked());
    connect(sendOnlyBroadcast, &QCheckBox::toggled, m_sendOnlyCheck, &QCheckBox::setChecked);
    connect(m_sendOnlyCheck, &QCheckBox::toggled, sendOnlyBroadcast, &QCheckBox::setChecked);
    broadcastLayout->addWidget(sendOnlyBroadcast);
    auto* broadcastResultsTitle = new QLabel("Risultati sincronizzati", broadcast);
    broadcastLayout->addWidget(broadcastResultsTitle);
    broadcastLayout->addWidget(m_broadcastTable, 2);
    tabs->addTab(broadcast, "Multicamera");

    auto* analysis = new QWidget(tabs);
    auto* analysisLayout = new QVBoxLayout(analysis);
    m_summaryLabel = new QLabel("Nessun dato", analysis);
    m_summaryLabel->setWordWrap(true);
    analysisLayout->addWidget(m_summaryLabel);
    m_table = new QTableWidget(analysis);
    m_table->setColumnCount(14);
    m_table->setHorizontalHeaderLabels({
      "Posa", "Giro", "X mm", "Y mm", "A °",
      "X att.", "Y att.", "A att.",
      "X ril.", "Y ril.", "A ril.",
      "Err. centro", "Err. angolo", "ms"
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setAlternatingRowColors(true);
    analysisLayout->addWidget(m_table, 1);
    auto* plots = new QSplitter(Qt::Horizontal, analysis);
    m_anglePlot = new PlotWidget("Angolo atteso → rilevato", PlotWidget::Mode::Angle, plots);
    m_centerPlot = new PlotWidget("Errore centro rispetto all'angolo", PlotWidget::Mode::CenterError, plots);
    plots->addWidget(m_anglePlot);
    plots->addWidget(m_centerPlot);
    analysisLayout->addWidget(plots);
    m_repeatabilityPlot = new RepeatabilityPlotWidget(analysis);
    analysisLayout->addWidget(m_repeatabilityPlot);
    tabs->addTab(analysis, "Analisi");

    auto* multiAnalysis = new QWidget(tabs);
    auto* multiAnalysisLayout = new QVBoxLayout(multiAnalysis);
    m_multiSummaryTable = new QTableWidget(multiAnalysis);
    m_multiSummaryTable->setColumnCount(10);
    m_multiSummaryTable->setHorizontalHeaderLabels({
      "Camera", "Frame", "Validi", "Centro medio", "Centro max",
      "Angolo medio", "Angolo max", "Tempo medio", "Tempo max", "Stato"
    });
    m_multiSummaryTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
    m_multiSummaryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_multiSummaryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_multiSummaryTable->setMaximumHeight(260);
    multiAnalysisLayout->addWidget(m_multiSummaryTable);

    auto* multiSelectors = new QHBoxLayout();
    m_multiPrimaryCamera = new QComboBox(multiAnalysis);
    m_multiCompareCamera = new QComboBox(multiAnalysis);
    m_multiCompareCamera->addItem("Nessun confronto", "");
    multiSelectors->addWidget(new QLabel("Telecamera da analizzare", multiAnalysis));
    multiSelectors->addWidget(m_multiPrimaryCamera, 1);
    multiSelectors->addWidget(new QLabel("Confronta con", multiAnalysis));
    multiSelectors->addWidget(m_multiCompareCamera, 1);
    multiAnalysisLayout->addLayout(multiSelectors);

    auto* multiPlots = new QSplitter(Qt::Horizontal, multiAnalysis);
    auto* primaryGroup = new QGroupBox("Dettaglio telecamera", multiPlots);
    auto* primaryLayout = new QVBoxLayout(primaryGroup);
    m_multiPrimarySummary = new QLabel("Nessun dato", primaryGroup);
    m_multiPrimaryAnglePlot = new PlotWidget(
      "Angolo atteso → rilevato", PlotWidget::Mode::Angle, primaryGroup);
    m_multiPrimaryCenterPlot = new PlotWidget(
      "Errore centro rispetto all'angolo", PlotWidget::Mode::CenterError, primaryGroup);
    primaryLayout->addWidget(m_multiPrimarySummary);
    primaryLayout->addWidget(m_multiPrimaryAnglePlot);
    primaryLayout->addWidget(m_multiPrimaryCenterPlot);

    m_multiCompareGroup = new QGroupBox("Confronto", multiPlots);
    auto* compareLayout = new QVBoxLayout(m_multiCompareGroup);
    m_multiCompareSummary = new QLabel("Nessun confronto", m_multiCompareGroup);
    m_multiCompareAnglePlot = new PlotWidget(
      "Angolo atteso → rilevato", PlotWidget::Mode::Angle, m_multiCompareGroup);
    m_multiCompareCenterPlot = new PlotWidget(
      "Errore centro rispetto all'angolo", PlotWidget::Mode::CenterError, m_multiCompareGroup);
    compareLayout->addWidget(m_multiCompareSummary);
    compareLayout->addWidget(m_multiCompareAnglePlot);
    compareLayout->addWidget(m_multiCompareCenterPlot);
    m_multiCompareGroup->hide();
    multiPlots->addWidget(primaryGroup);
    multiPlots->addWidget(m_multiCompareGroup);
    multiAnalysisLayout->addWidget(multiPlots, 1);
    tabs->addTab(multiAnalysis, "Analisi multicamera");

    connect(m_startButton, &QPushButton::clicked, this, [this]() { startTest(); });
    connect(startBroadcastButton, &QPushButton::clicked, this, [this]() { startTest(); });
    connect(stopBroadcastButton, &QPushButton::clicked, this, [this]() {
      const QList<TestVisionWindow*> workers = m_broadcastWorkers;
      for (TestVisionWindow* worker : workers)
      {
        if (worker)
        {
          worker->stopTest("Broadcast arrestato dall'utente");
        }
      }
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
      if (m_broadcastRunning)
      {
        const QList<TestVisionWindow*> workers = m_broadcastWorkers;
        for (TestVisionWindow* worker : workers)
        {
          if (worker)
          {
            worker->stopTest("Broadcast arrestato dall'utente");
          }
        }
        return;
      }
      stopTest("Arrestato dall'utente");
    });
    connect(m_sendSampleButton, &QPushButton::clicked, this, [this]() { sendSample(); });
    connect(m_generateDatasetButton, &QPushButton::clicked, this, [this]() { generateAiDataset(); });
    connect(m_loadCampaignButton, &QPushButton::clicked, this, [this]() { loadCampaign(); });
    connect(m_editCampaignButton, &QPushButton::clicked, this, [this]() { editCampaign(); });
    connect(m_startCampaignButton, &QPushButton::clicked, this, [this]() { startCampaign(); });
    connect(m_shapeCombo, &QComboBox::currentIndexChanged, this, [this]() { updateMasterShape(); });
    connect(m_recipeCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
      m_expectedRecipeId = text.trimmed();
      updateScenarioLabel();
      syncStrategyFromRecipe();
    });
    connect(m_cameraCombo, &QComboBox::currentIndexChanged, this, [this]() {
      if (!m_cameraTargets)
      {
        return;
      }
      const QString cameraId = m_cameraCombo->currentData().toString();
      if (cameraId == "SELECTED")
      {
        return;
      }
      for (int row = 0; row < m_cameraTargets->rowCount(); ++row)
      {
        if (m_cameraTargets->item(row, 1)->data(Qt::UserRole).toString() == cameraId)
        {
          m_cameraTargets->item(row, 0)->setCheckState(Qt::Checked);
          break;
        }
      }
    });
    connect(m_multiSummaryTable, &QTableWidget::cellClicked, this, [this](int row, int) {
      if (row < 0 || row >= m_multiSummaryTable->rowCount())
      {
        return;
      }
      const QString cameraId = m_multiSummaryTable->item(row, 0)->text();
      const int index = m_multiPrimaryCamera->findData(cameraId);
      if (index >= 0)
      {
        m_multiPrimaryCamera->setCurrentIndex(index);
      }
    });
    connect(m_multiPrimaryCamera, &QComboBox::currentIndexChanged, this, [this]() {
      refreshBroadcastAnalysisSelection();
    });
    connect(m_multiCompareCamera, &QComboBox::currentIndexChanged, this, [this]() {
      refreshBroadcastAnalysisSelection();
    });
    for (QDoubleSpinBox* spin : {
           m_xMin, m_xMax, m_xStep, m_yMin, m_yMax, m_yStep,
           m_angleMin, m_angleMax, m_angleStep, m_pixelSize})
    {
      connect(spin, &QDoubleSpinBox::valueChanged, this, [this]() { updatePlanCount(); });
    }
    connect(m_passes, &QSpinBox::valueChanged, this, [this]() { updatePlanCount(); });
    m_pollTimer.setInterval(20);
    connect(&m_pollTimer, &QTimer::timeout, this, [this]() { pollPipe(); });
    m_sendTimer.setSingleShot(true);
    connect(&m_sendTimer, &QTimer::timeout, this, [this]() { sendNextFrame(); });
  }

  bool loadScenario(QString* error)
  {
    m_scenario = loadJson(m_scenarioPath, error);
    if (m_scenario.isEmpty()) return false;
    const QJsonObject canvas = m_scenario.value("canvas").toObject();
    m_canvasWidth = canvas.value("width").toInt(640);
    m_canvasHeight = canvas.value("height").toInt(480);
    m_canvasBackground = canvas.value("background").toInt(240);
    const QDir projectRoot = QFileInfo(m_scenarioPath).dir();
    QDir recipesDirectory(projectRoot.absoluteFilePath("../../recipes"));
    const QFileInfoList recipeDirectories =
      recipesDirectory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& recipeDirectory : recipeDirectories)
    {
      if (QFileInfo::exists(QDir(recipeDirectory.absoluteFilePath()).filePath("recipe.json")))
      {
        m_recipeCombo->addItem(recipeDirectory.fileName());
      }
    }
    const QString configuredRecipe = m_scenario.value("recipeId").toString();
    const int recipeIndex = m_recipeCombo->findText(configuredRecipe);
    if (recipeIndex >= 0)
    {
      m_recipeCombo->setCurrentIndex(recipeIndex);
    }
    const QString strategyId = m_scenario.value("strategyId").toString();
    const int strategyIndex = m_strategyCombo->findData(strategyId);
    if (strategyIndex >= 0)
    {
      m_strategyCombo->setCurrentIndex(strategyIndex);
    }
    const QString cameraId = m_scenario.value("cameraId").toString();
    const int camIndex = m_cameraCombo->findData(cameraId);
    if (camIndex >= 0)
    {
      m_cameraCombo->setCurrentIndex(camIndex);
    }
    else
    {
      m_cameraCombo->setCurrentIndex(0);
    }
    selectBroadcastCamera(cameraId);
    updateMasterShape();
    const QDir scenarioDir = QFileInfo(m_scenarioPath).dir();
    const QString masterPath = scenarioDir.absoluteFilePath(
      m_scenario.value("master").toObject().value("file").toString());
    QDir().mkpath(QFileInfo(masterPath).dir().absolutePath());
    cv::imwrite(masterPath.toStdString(), m_master);
    m_scenarioLabel->setText(QString("%1 | %2 / %3 | ricetta %4")
      .arg(m_scenario.value("name").toString())
      .arg(m_scenario.value("cameraId").toString())
      .arg(m_scenario.value("channel").toString())
      .arg(m_expectedRecipeId));
    m_preview->setPixmap(matToPixmap(m_master).scaled(
      m_preview->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    updatePlanCount();
    return true;
  }

  void updateMasterShape()
  {
    if (!m_shapeCombo)
    {
      return;
    }
    m_master = createShape(
      m_shapeCombo->currentData().toString(),
      m_canvasWidth,
      m_canvasHeight,
      m_canvasBackground);
    const QString suggestedRecipe = m_scenario.value("recipeId").toString(
      "test_" + m_shapeCombo->currentData().toString());
    if (m_recipeCombo && m_recipeCombo->currentText().isEmpty())
    {
      const int suggestedIndex = m_recipeCombo->findText(suggestedRecipe);
      if (suggestedIndex >= 0)
      {
        m_recipeCombo->setCurrentIndex(suggestedIndex);
      }
    }
    m_expectedRecipeId = m_recipeCombo ? m_recipeCombo->currentText().trimmed() : suggestedRecipe;
    updateScenarioLabel();
    if (m_preview && !m_master.empty())
    {
      m_preview->setPixmap(matToPixmap(m_master).scaled(
        m_preview->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    if (!m_scenario.isEmpty())
    {
      const QDir scenarioDir = QFileInfo(m_scenarioPath).dir();
      const QString shapeName = m_shapeCombo->currentData().toString();
      const QString path = scenarioDir.absoluteFilePath(
        QString("../assets/%1/master.png").arg(shapeName));
      QDir().mkpath(QFileInfo(path).dir().absolutePath());
      cv::imwrite(path.toStdString(), m_master);
    }
  }

  void updateScenarioLabel()
  {
    if (!m_scenarioLabel || m_scenario.isEmpty())
    {
      return;
    }
    m_scenarioLabel->setText(QString("%1 | %2 / %3 | ricetta %4")
      .arg(m_scenario.value("name").toString())
      .arg(m_scenario.value("cameraId").toString())
      .arg(m_scenario.value("channel").toString())
      .arg(m_expectedRecipeId.isEmpty() ? "<nessuna>" : m_expectedRecipeId));
  }

  bool connectPipe()
  {
    closePipe();
    m_sentFrames.clear();
    m_pipe = CreateFileW(kPipePath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (m_pipe == INVALID_HANDLE_VALUE)
    {
      QMessageBox::warning(
        this,
        "TestVision",
        QString("Vision non raggiungibile, errore Windows %1").arg(GetLastError()));
      return false;
    }
    m_state = State::WaitingHello;
    m_pollTimer.start();
    return true;
  }

  void sendSample()
  {
    if (m_state != State::Idle)
    {
      QMessageBox::information(this, "TestVision", "Arrestare il test prima di inviare un campione.");
      return;
    }
    m_pendingSample = true;
    m_stateLabel->setText("Connessione per invio campione...");
    connectPipe();
  }

  void writeSample()
  {
    QJsonObject sample;
    sample["type"] = "sample";
    sample["protocolVersion"] = m_scenario.value("protocolVersion").toInt(1);
    sample["scenarioId"] = m_scenario.value("name").toString();
    const QString targetCam = m_cameraCombo->currentData().toString();
    sample["cameraId"] = targetCam;
    sample["channel"] = targetCam;
    int targetSlot = 1;
    if (targetCam != "SELECTED")
    {
      targetSlot = targetCam.mid(3).toInt();
    }
    sample["slot"] = targetSlot;
    sample["frameId"] = 0;
    sample["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    sample["shapeId"] = m_shapeCombo->currentData().toString();
    sample["strategyId"] = m_strategyCombo->currentData().toString();
    sample["recipeId"] = m_expectedRecipeId;
    sample["imageFormat"] = "jpg";
    sample["imageBase64"] = QString::fromLatin1(jpegBase64(m_master));
    QByteArray payload = QJsonDocument(sample).toJson(QJsonDocument::Compact);
    payload.append('\n');
    DWORD written = 0;
    if (!WriteFile(m_pipe, payload.constData(), static_cast<DWORD>(payload.size()), &written, nullptr))
    {
      stopTest(QString("Invio campione fallito, errore Windows %1").arg(GetLastError()));
      return;
    }
    m_stateLabel->setText("Campione inviato; attendo Vision");
    appendLog("Invio campione: " + m_shapeCombo->currentText());
  }

  void updatePlanCount()
  {
    const int xCount = steppedValues(m_xMin->value(), m_xMax->value(), m_xStep->value()).size();
    const int yCount = steppedValues(m_yMin->value(), m_yMax->value(), m_yStep->value()).size();
    const int aCount = steppedValues(m_angleMin->value(), m_angleMax->value(), m_angleStep->value()).size();
    const qint64 total = static_cast<qint64>(xCount) * yCount * aCount * m_passes->value();
    m_totalLabel->setText(QString("Pose: %1 × %2 × %3 = %4\nGiri: %5\nTotale immagini: %6")
      .arg(xCount).arg(yCount).arg(aCount).arg(xCount * yCount * aCount)
      .arg(m_passes->value()).arg(total));
  }

  bool buildPlan(QString* error)
  {
    m_plan.clear();
    const QVector<double> xs = steppedValues(m_xMin->value(), m_xMax->value(), m_xStep->value());
    const QVector<double> ys = steppedValues(m_yMin->value(), m_yMax->value(), m_yStep->value());
    const QVector<double> angles = steppedValues(m_angleMin->value(), m_angleMax->value(), m_angleStep->value());
    if (xs.isEmpty() || ys.isEmpty() || angles.isEmpty())
    {
      *error = "Piano prova non valido: controllare minimi, massimi e passi.";
      return false;
    }
    int poseIndex = 0;
    QVector<TestPose> singlePass;
    for (double x : xs)
      for (double y : ys)
        for (double angle : angles)
          singlePass.append({++poseIndex, 1, x, y, angle});
    for (int pass = 1; pass <= m_passes->value(); ++pass)
    {
      for (TestPose pose : singlePass)
      {
        pose.pass = pass;
        m_plan.append(pose);
      }
    }
    return true;
  }

  void generateAiDataset()
  {
    QString error;
    if (!buildPlan(&error))
    {
      QMessageBox::warning(this, "TestVision", error);
      return;
    }
    if (m_master.empty())
    {
      QMessageBox::warning(this, "TestVision", "Immagine master non disponibile.");
      return;
    }

    QVector<TestVisionDatasetImage> images;
    for (const TestPose& pose : m_plan)
    {
      if (pose.pass != 1)
      {
        continue;
      }
      const double txPx = pose.xMm / m_pixelSize->value();
      const double tyPx = pose.yMm / m_pixelSize->value();
      TestVisionDatasetImage item;
      item.fileName = QString("image_%1.png").arg(pose.poseIndex, 6, 10, QChar('0'));
      item.image = transformImage(m_master, pose.angleDeg, txPx, tyPx);
      item.metadata["poseIndex"] = pose.poseIndex;
      item.metadata["xMm"] = pose.xMm;
      item.metadata["yMm"] = pose.yMm;
      item.metadata["angleDeg"] = pose.angleDeg;
      item.metadata["expectedCenterX"] = (m_canvasWidth - 1) * 0.5 + txPx;
      item.metadata["expectedCenterY"] = (m_canvasHeight - 1) * 0.5 + tyPx;
      item.metadata["shape"] = m_shapeCombo->currentData().toString();
      item.metadata["strategyId"] = m_strategyCombo->currentData().toString();
      images.append(std::move(item));
    }

    const QString recipeId = m_recipeCombo->currentText().trimmed();
    const QString cameraId = m_scenario.value("cameraId").toString("CAM01");
    const QString rawImagesDirectory = QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(
      QString("recipes/%1/images/%2/ai/localization_segmentation/raw")
        .arg(recipeId, cameraId));
    const QString scenarioName = m_scenario.value("name").toString(
      QFileInfo(m_scenarioPath).completeBaseName());
    const QString manifestPath = saveTestVisionLabelingBatch(
      rawImagesDirectory, scenarioName, images, &error);
    if (manifestPath.isEmpty())
    {
      QMessageBox::warning(this, "TestVision", error);
      return;
    }
    appendLog(QString("Immagini AI per labeling: %1 salvate in %2")
      .arg(images.size())
      .arg(QDir::toNativeSeparators(rawImagesDirectory)));
    appendLog("Manifest lotto: " + QDir::toNativeSeparators(manifestPath));
    m_stateLabel->setText(QString("Dataset AI creato: %1 immagini").arg(images.size()));
  }

  void loadCampaign()
  {
    const QString path = QFileDialog::getOpenFileName(
      this,
      "Carica campagna TestVision",
      QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("tests/campaigns"),
      "Campagne JSON (*.json)");
    if (path.isEmpty())
    {
      return;
    }
    QString error;
    if (!loadCampaignFromFile(path, &error))
    {
      QMessageBox::warning(
        this, "Campagna TestVision",
        error.isEmpty() ? "La campagna non contiene prove." : error);
      return;
    }
  }

  void editCampaign()
  {
    QStringList recipes;
    for (int index = 0; index < m_recipeCombo->count(); ++index)
    {
      recipes.append(m_recipeCombo->itemText(index));
    }
    CampaignEditorDialog dialog(m_campaign, recipes, this);
    if (dialog.exec() != QDialog::Accepted)
    {
      return;
    }
    const QJsonObject campaign = dialog.campaign();
    if (campaign.value("items").toArray().isEmpty())
    {
      QMessageBox::warning(this, "Campagna TestVision", "Aggiungi almeno una prova.");
      return;
    }
    QString suggestedPath = m_campaignPath;
    if (suggestedPath.isEmpty())
    {
      suggestedPath = QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(
        QString("tests/campaigns/%1.json")
          .arg(campaign.value("name").toString("nuova_campagna")));
    }
    const QString path = QFileDialog::getSaveFileName(
      this,
      "Salva campagna TestVision",
      suggestedPath,
      "Campagne JSON (*.json)");
    if (path.isEmpty())
    {
      return;
    }
    QDir().mkpath(QFileInfo(path).dir().absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      QMessageBox::warning(this, "Campagna TestVision", "Impossibile salvare " + path);
      return;
    }
    file.write(QJsonDocument(campaign).toJson(QJsonDocument::Indented));
    m_campaignPath = path;
    m_campaign = campaign;
    m_campaignItems = campaign.value("items").toArray();
    m_campaignCycles = qMax(1, campaign.value("cycles").toInt(1));
    m_startCampaignButton->setEnabled(true);
    appendLog(QString("Campagna salvata: %1 | prove=%2 | cicli=%3")
      .arg(QDir::toNativeSeparators(path))
      .arg(m_campaignItems.size())
      .arg(m_campaignCycles));
  }

  void applyCampaignItem(const QJsonObject& item)
  {
    const QString cameraId = item.value("cameraId").toString(
      m_scenario.value("cameraId").toString("CAM01"));
    const int cameraIndex = m_cameraCombo->findData(cameraId);
    if (cameraIndex >= 0)
    {
      m_cameraCombo->setCurrentIndex(cameraIndex);
    }
    selectBroadcastCamera(cameraId);
    const QString recipeId = item.value("recipeId").toString();
    const int recipeIndex = m_recipeCombo->findText(recipeId);
    if (recipeIndex >= 0)
    {
      m_recipeCombo->setCurrentIndex(recipeIndex);
    }
    const int strategyIndex = m_strategyCombo->findData(
      item.value("strategyId").toString());
    if (strategyIndex >= 0)
    {
      m_strategyCombo->setCurrentIndex(strategyIndex);
    }
    const int shapeIndex = m_shapeCombo->findData(item.value("shapeId").toString());
    if (shapeIndex >= 0)
    {
      m_shapeCombo->setCurrentIndex(shapeIndex);
    }
    m_xMin->setValue(item.value("xMinMm").toDouble(m_xMin->value()));
    m_xMax->setValue(item.value("xMaxMm").toDouble(m_xMax->value()));
    m_xStep->setValue(item.value("xStepMm").toDouble(m_xStep->value()));
    m_yMin->setValue(item.value("yMinMm").toDouble(m_yMin->value()));
    m_yMax->setValue(item.value("yMaxMm").toDouble(m_yMax->value()));
    m_yStep->setValue(item.value("yStepMm").toDouble(m_yStep->value()));
    m_angleMin->setValue(item.value("angleMinDeg").toDouble(m_angleMin->value()));
    m_angleMax->setValue(item.value("angleMaxDeg").toDouble(m_angleMax->value()));
    m_angleStep->setValue(item.value("angleStepDeg").toDouble(m_angleStep->value()));
    m_passes->setValue(qMax(1, item.value("passes").toInt(m_passes->value())));
    m_intervalMs->setValue(qMax(0, item.value("intervalMs").toInt(m_intervalMs->value())));
    updateMasterShape();
  }

  void selectBroadcastCamera(const QString& cameraId)
  {
    if (!m_cameraTargets || cameraId.isEmpty() || cameraId == "SELECTED")
    {
      return;
    }
    for (int row = 0; row < m_cameraTargets->rowCount(); ++row)
    {
      const bool match =
        m_cameraTargets->item(row, 1)->data(Qt::UserRole).toString() == cameraId;
      m_cameraTargets->item(row, 0)->setCheckState(
        match ? Qt::Checked : Qt::Unchecked);
      if (match)
      {
        auto* strategy = qobject_cast<QComboBox*>(
          m_cameraTargets->cellWidget(row, 2));
        const int strategyIndex = strategy
          ? strategy->findData(m_strategyCombo->currentData())
          : -1;
        if (strategyIndex >= 0)
        {
          strategy->setCurrentIndex(strategyIndex);
        }
      }
    }
  }

  QJsonArray selectedBroadcastItems() const
  {
    QJsonArray items;
    if (m_cameraTargets)
    {
      for (int row = 0; row < m_cameraTargets->rowCount(); ++row)
      {
        if (m_cameraTargets->item(row, 0)->checkState() != Qt::Checked)
        {
          continue;
        }
        auto* strategy = qobject_cast<QComboBox*>(
          m_cameraTargets->cellWidget(row, 2));
        items.append(QJsonObject{
          {"cameraId", m_cameraTargets->item(row, 1)
            ->data(Qt::UserRole).toString()},
          {"strategyId", strategy
            ? strategy->currentData().toString()
            : m_strategyCombo->currentData().toString()}
        });
      }
    }
    return items;
  }

  void startCampaign()
  {
    if (m_campaignItems.isEmpty())
    {
      return;
    }
    if (m_campaign.value("parallel").toBool(true))
    {
      startParallelCampaign();
      return;
    }
    m_campaignRunning = true;
    m_campaignCycle = 0;
    m_campaignItemIndex = -1;
    m_campaignSummaries = {};
    m_startCampaignButton->setEnabled(false);
    runNextCampaignItem();
  }

  void startParallelCampaign()
  {
    m_campaignRunning = true;
    m_campaignCycle = 0;
    m_campaignSummaries = {};
    m_parallelPendingItems = m_campaignItems;
    m_activeCampaignWorkers = 0;
    m_startCampaignButton->setEnabled(false);
    appendLog(QString("Campagna multicamera avviata: prove=%1 cicli=%2")
      .arg(m_campaignItems.size())
      .arg(m_campaignCycles));
    launchNextParallelBatch();
  }

  void launchNextParallelBatch()
  {
    if (!m_campaignRunning || m_activeCampaignWorkers > 0)
    {
      return;
    }
    if (m_parallelPendingItems.isEmpty())
    {
      ++m_campaignCycle;
      if (m_campaignCycle >= m_campaignCycles)
      {
        finishCampaign();
        return;
      }
      m_parallelPendingItems = m_campaignItems;
    }

    const QString recipeId =
      m_parallelPendingItems.first().toObject().value("recipeId").toString();
    QJsonArray batch;
    QJsonArray remaining;
    QSet<QString> usedCameras;
    for (const QJsonValue& value : m_parallelPendingItems)
    {
      QJsonObject item = value.toObject();
      const QString cameraId = item.value("cameraId").toString("CAM01");
      item["cameraId"] = cameraId;
      if (item.value("recipeId").toString() == recipeId &&
          !usedCameras.contains(cameraId))
      {
        usedCameras.insert(cameraId);
        batch.append(item);
      }
      else
      {
        remaining.append(item);
      }
    }
    m_parallelPendingItems = remaining;
    m_activeCampaignWorkers = batch.size();
    appendLog(QString(
      "Batch multicamera ciclo %1/%2: ricetta=%3 telecamere=%4")
      .arg(m_campaignCycle + 1)
      .arg(m_campaignCycles)
      .arg(recipeId)
      .arg(QStringList(usedCameras.values()).join(", ")));

    for (const QJsonValue& value : batch)
    {
      const QJsonObject item = value.toObject();
      const QString cameraId = item.value("cameraId").toString("CAM01");
      auto* worker = new TestVisionWindow(m_scenarioPath, this);
      worker->hide();
      worker->setSendOnlyMode(m_sendOnlyCheck && m_sendOnlyCheck->isChecked());
      worker->startCampaignWorker(
        item,
        m_campaignCycle + 1,
        [this, worker, cameraId](const QJsonObject& summary) {
          m_campaignSummaries.append(summary);
          appendLog(QString("Worker %1 terminato: %2")
            .arg(cameraId, summary.value("status").toString()));
          worker->deleteLater();
          --m_activeCampaignWorkers;
          if (m_activeCampaignWorkers == 0)
          {
            QTimer::singleShot(300, this, [this]() { launchNextParallelBatch(); });
          }
        });
    }
  }

  void runNextCampaignItem()
  {
    ++m_campaignItemIndex;
    if (m_campaignItemIndex >= m_campaignItems.size())
    {
      m_campaignItemIndex = 0;
      ++m_campaignCycle;
    }
    if (m_campaignCycle >= m_campaignCycles)
    {
      finishCampaign();
      return;
    }
    const QJsonObject item = m_campaignItems.at(m_campaignItemIndex).toObject();
    m_campaignItemFailure.clear();
    applyCampaignItem(item);
    appendLog(QString("Campagna ciclo %1/%2 prova %3/%4: ricetta=%5 strategia=%6")
      .arg(m_campaignCycle + 1)
      .arg(m_campaignCycles)
      .arg(m_campaignItemIndex + 1)
      .arg(m_campaignItems.size())
      .arg(m_recipeCombo->currentText())
      .arg(m_strategyCombo->currentData().toString()));
    startTest();
  }

  QJsonObject buildCampaignSummary(
    const QJsonObject& item,
    int cycle,
    const QString& pipelineFailure = {}) const
  {
    if (m_sendOnlyMode)
    {
      QJsonObject summary;
      summary["cycle"] = cycle;
      summary["cameraId"] = item.value("cameraId").toString(
        m_cameraCombo->currentData().toString());
      summary["recipeId"] = m_recipeCombo->currentText();
      summary["strategyId"] = m_strategyCombo->currentData().toString();
      summary["framesSent"] = m_currentFrameId;
      summary["status"] = "SENT";
      summary["problems"] = QJsonArray{};
      return summary;
    }

    double centerSum = 0.0;
    double angleSum = 0.0;
    double centerMax = 0.0;
    double angleMax = 0.0;
    double timeSum = 0.0;
    double steadyTimeSum = 0.0;
    double timeMax = 0.0;
    int validCount = 0;
    for (int resultIndex = 0; resultIndex < m_results.size(); ++resultIndex)
    {
      const TestResult& result = m_results[resultIndex];
      centerSum += result.centerError;
      angleSum += result.angleError;
      centerMax = std::max(centerMax, result.centerError);
      angleMax = std::max(angleMax, result.angleError);
      timeSum += result.processingMs;
      if (resultIndex > 0) steadyTimeSum += result.processingMs;
      timeMax = std::max(timeMax, result.processingMs);
      if (result.valid) ++validCount;
    }
    const int count = m_results.size();
    const double centerMean = count > 0 ? centerSum / count : 0.0;
    const double angleMean = count > 0 ? angleSum / count : 0.0;
    const double timeMean = count > 0 ? timeSum / count : 0.0;
    const double steadyTimeMean = count > 1
      ? steadyTimeSum / (count - 1)
      : timeMean;
    const double coldStartMs = count > 0 ? m_results.first().processingMs : 0.0;
    const QJsonObject limits = item.value("limits").toObject();
    QJsonArray problems;
    if (!pipelineFailure.isEmpty())
      problems.append("Errore pipeline: " + pipelineFailure);
    if (validCount != count)
      problems.append(QString("Pose non valide: %1/%2").arg(count - validCount).arg(count));
    if (centerMean > limits.value("centerMeanMaxPx").toDouble(5.0))
      problems.append(QString("Errore centro medio alto: %1 px").arg(centerMean, 0, 'f', 2));
    if (centerMax > limits.value("centerMaxPx").toDouble(10.0))
      problems.append(QString("Errore centro massimo alto: %1 px").arg(centerMax, 0, 'f', 2));
    if (angleMean > limits.value("angleMeanMaxDeg").toDouble(5.0))
      problems.append(QString("Errore angolo medio alto: %1°").arg(angleMean, 0, 'f', 2));
    if (angleMax > limits.value("angleMaxDeg").toDouble(15.0))
      problems.append(QString("Errore angolo massimo alto: %1°").arg(angleMax, 0, 'f', 2));
    if (steadyTimeMean > limits.value("processingMeanMaxMs").toDouble(250.0))
      problems.append(QString("Tempo medio a caldo alto: %1 ms").arg(steadyTimeMean, 0, 'f', 1));
    if (timeMax > limits.value("processingMaxMs").toDouble(1000.0))
      problems.append(QString("Picco tempo alto: %1 ms").arg(timeMax, 0, 'f', 1));

    QJsonObject summary;
    summary["cycle"] = cycle;
    summary["cameraId"] = item.value("cameraId").toString(
      m_cameraCombo->currentData().toString());
    summary["recipeId"] = m_recipeCombo->currentText();
    summary["strategyId"] = m_strategyCombo->currentData().toString();
    summary["frames"] = count;
    summary["validFrames"] = validCount;
    summary["centerMeanPx"] = centerMean;
    summary["centerMaxPx"] = centerMax;
    summary["angleMeanDeg"] = angleMean;
    summary["angleMaxDeg"] = angleMax;
    summary["processingMeanMs"] = timeMean;
    summary["processingSteadyMeanMs"] = steadyTimeMean;
    summary["coldStartMs"] = coldStartMs;
    summary["processingMaxMs"] = timeMax;
    summary["status"] = problems.isEmpty() ? "OK" : "PROBLEM";
    summary["problems"] = problems;
    return summary;
  }

  void recordCampaignSummary()
  {
    if (!m_campaignRunning)
    {
      return;
    }
    const QJsonObject item = m_campaignItems.at(m_campaignItemIndex).toObject();
    m_campaignSummaries.append(buildCampaignSummary(
      item, m_campaignCycle + 1, m_campaignItemFailure));
  }

  void finishCampaign()
  {
    m_campaignRunning = false;
    m_startCampaignButton->setEnabled(true);
    QJsonObject report;
    report["name"] = m_campaign.value("name").toString(
      QFileInfo(m_campaignPath).completeBaseName());
    report["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    report["cycles"] = m_campaignCycles;
    report["runs"] = m_campaignSummaries;
    const QString outputRoot = QDir(QString::fromUtf8(PROJECT_SOURCE_DIR))
      .filePath("tests/reports/campaigns");
    QDir().mkpath(outputRoot);
    const QString outputPath = QDir(outputRoot).filePath(
      QString("%1_%2.json")
        .arg(report.value("name").toString())
        .arg(testVisionTimestamp()));
    QFile file(outputPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    }
    QStringList textLines;
    textLines.append(QString("Campagna: %1").arg(report.value("name").toString()));
    textLines.append(QString("Generata: %1").arg(report.value("generatedAt").toString()));
    textLines.append(QString());
    for (const QJsonValue& value : m_campaignSummaries)
    {
      const QJsonObject run = value.toObject();
      const QString header = QString(
        "Ciclo %1 | %2 | %3 | %4 | %5")
        .arg(run.value("cycle").toInt())
        .arg(run.value("cameraId").toString())
        .arg(run.value("recipeId").toString())
        .arg(run.value("strategyId").toString())
        .arg(run.value("status").toString());
      textLines.append(header);
      const QJsonArray problems = run.value("problems").toArray();
      if (problems.isEmpty())
      {
        textLines.append("  Nessun problema rilevato.");
      }
      else
      {
        for (const QJsonValue& problem : problems)
          textLines.append("  - " + problem.toString());
      }
      textLines.append(QString(
        "  Centro medio/max: %1 / %2 px | Angolo medio/max: %3 / %4° | "
        "Cold start: %5 ms | Media a caldo: %6 ms")
        .arg(run.value("centerMeanPx").toDouble(), 0, 'f', 2)
        .arg(run.value("centerMaxPx").toDouble(), 0, 'f', 2)
        .arg(run.value("angleMeanDeg").toDouble(), 0, 'f', 2)
        .arg(run.value("angleMaxDeg").toDouble(), 0, 'f', 2)
        .arg(run.value("coldStartMs").toDouble(), 0, 'f', 1)
        .arg(run.value("processingSteadyMeanMs").toDouble(), 0, 'f', 1));
      textLines.append(QString());
    }
    const QString textPath = QFileInfo(outputPath).dir().filePath(
      QFileInfo(outputPath).completeBaseName() + ".txt");
    QFile textFile(textPath);
    if (textFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
      textFile.write(textLines.join('\n').toUtf8());
    }
    int problemRuns = 0;
    for (const QJsonValue& value : m_campaignSummaries)
      if (value.toObject().value("status").toString() != "OK") ++problemRuns;
    const QString message = QString(
      "Campagna terminata: %1 prove, %2 OK, %3 con problemi\n%4")
      .arg(m_campaignSummaries.size())
      .arg(m_campaignSummaries.size() - problemRuns)
      .arg(problemRuns)
      .arg(QDir::toNativeSeparators(textPath));
    appendLog(message);
    m_stateLabel->setText(message);
  }

  QJsonObject currentBroadcastItem(
    const QString& cameraId,
    const QString& strategyId) const
  {
    return {
      {"cameraId", cameraId},
      {"recipeId", m_recipeCombo->currentText().trimmed()},
      {"strategyId", strategyId},
      {"shapeId", m_shapeCombo->currentData().toString()},
      {"passes", m_passes->value()},
      {"intervalMs", m_intervalMs->value()},
      {"xMinMm", m_xMin->value()},
      {"xMaxMm", m_xMax->value()},
      {"xStepMm", m_xStep->value()},
      {"yMinMm", m_yMin->value()},
      {"yMaxMm", m_yMax->value()},
      {"yStepMm", m_yStep->value()},
      {"angleMinDeg", m_angleMin->value()},
      {"angleMaxDeg", m_angleMax->value()},
      {"angleStepDeg", m_angleStep->value()},
      {"limits", QJsonObject{
        {"centerMeanMaxPx", 1.0e9},
        {"centerMaxPx", 1.0e9},
        {"angleMeanMaxDeg", 1.0e9},
        {"angleMaxDeg", 1.0e9},
        {"processingMeanMaxMs", 1.0e9},
        {"processingMaxMs", 1.0e9}
      }}
    };
  }

  void appendBroadcastResult(const QString& cameraId, const TestResult& result)
  {
    if (!m_broadcastTable)
    {
      return;
    }
    const int row = m_broadcastTable->rowCount();
    m_broadcastTable->insertRow(row);
    const QStringList values = {
      cameraId,
      QString::number(result.frameId),
      result.valid ? "SI" : "NO",
      QString::number(result.actualX, 'f', 2),
      QString::number(result.actualY, 'f', 2),
      QString::number(result.actualAngle, 'f', 2),
      QString::number(result.centerError, 'f', 2),
      QString::number(result.angleError, 'f', 2),
      QString::number(result.processingMs, 'f', 1)
    };
    for (int column = 0; column < values.size(); ++column)
    {
      m_broadcastTable->setItem(row, column, new QTableWidgetItem(values[column]));
    }
    m_broadcastResultsByCamera[cameraId].append(result);
    refreshBroadcastAnalysisSummary();
    m_broadcastTable->scrollToBottom();
    m_stateLabel->setText(QString(
      "Broadcast: %1 frame %2 ricevuto | attivi %3")
      .arg(cameraId)
      .arg(result.frameId)
      .arg(m_activeBroadcastWorkers));
  }

  void refreshBroadcastAnalysisSummary()
  {
    if (!m_multiSummaryTable || !m_multiPrimaryCamera || !m_multiCompareCamera)
    {
      return;
    }

    const QString primaryCamera = m_multiPrimaryCamera->currentData().toString();
    const QString compareCamera = m_multiCompareCamera->currentData().toString();
    const QSignalBlocker primaryBlocker(m_multiPrimaryCamera);
    const QSignalBlocker compareBlocker(m_multiCompareCamera);
    m_multiPrimaryCamera->clear();
    m_multiCompareCamera->clear();
    m_multiCompareCamera->addItem("Nessun confronto", "");
    m_multiSummaryTable->setRowCount(0);

    QStringList cameraIds = m_broadcastResultsByCamera.keys();
    cameraIds.sort();
    for (const QString& cameraId : cameraIds)
    {
      const QVector<TestResult>& results = m_broadcastResultsByCamera[cameraId];
      double centerSum = 0.0;
      double centerMax = 0.0;
      double angleSum = 0.0;
      double angleMax = 0.0;
      double timeSum = 0.0;
      double timeMax = 0.0;
      int validCount = 0;
      for (const TestResult& result : results)
      {
        if (result.valid)
        {
          ++validCount;
        }
        centerSum += result.centerError;
        centerMax = std::max(centerMax, result.centerError);
        angleSum += result.angleError;
        angleMax = std::max(angleMax, result.angleError);
        timeSum += result.processingMs;
        timeMax = std::max(timeMax, result.processingMs);
      }
      const int count = results.size();
      const double centerMean = count > 0 ? centerSum / count : 0.0;
      const double angleMean = count > 0 ? angleSum / count : 0.0;
      const double timeMean = count > 0 ? timeSum / count : 0.0;
      const bool problem =
        validCount != count || centerMax > 10.0 ||
        angleMax > 15.0 || timeMax > 2000.0;
      const QStringList values = {
        cameraId,
        QString::number(count),
        QString("%1/%2").arg(validCount).arg(count),
        QString::number(centerMean, 'f', 2),
        QString::number(centerMax, 'f', 2),
        QString::number(angleMean, 'f', 2),
        QString::number(angleMax, 'f', 2),
        QString::number(timeMean, 'f', 1),
        QString::number(timeMax, 'f', 1),
        problem ? "PROBLEMA" : "OK"
      };
      const int row = m_multiSummaryTable->rowCount();
      m_multiSummaryTable->insertRow(row);
      for (int column = 0; column < values.size(); ++column)
      {
        auto* item = new QTableWidgetItem(values[column]);
        if (column == values.size() - 1)
        {
          item->setBackground(problem ? QColor("#7a2633") : QColor("#17633a"));
        }
        m_multiSummaryTable->setItem(row, column, item);
      }
      m_multiPrimaryCamera->addItem(cameraId, cameraId);
      m_multiCompareCamera->addItem(cameraId, cameraId);
    }

    int primaryIndex = m_multiPrimaryCamera->findData(primaryCamera);
    if (primaryIndex < 0 && m_multiPrimaryCamera->count() > 0)
    {
      primaryIndex = 0;
    }
    if (primaryIndex >= 0)
    {
      m_multiPrimaryCamera->setCurrentIndex(primaryIndex);
    }
    const int compareIndex = m_multiCompareCamera->findData(compareCamera);
    m_multiCompareCamera->setCurrentIndex(compareIndex >= 0 ? compareIndex : 0);
    refreshBroadcastAnalysisSelection();
  }

  void refreshBroadcastAnalysisSelection()
  {
    if (!m_multiPrimaryCamera || !m_multiPrimaryAnglePlot)
    {
      return;
    }
    auto summaryText = [](const QString& cameraId, const QVector<TestResult>& results) {
      if (results.isEmpty())
      {
        return QString("%1: nessun dato").arg(cameraId);
      }
      double centerSum = 0.0;
      double angleSum = 0.0;
      double timeSum = 0.0;
      for (const TestResult& result : results)
      {
        centerSum += result.centerError;
        angleSum += result.angleError;
        timeSum += result.processingMs;
      }
      return QString("%1 | frame %2 | centro medio %3 px | angolo medio %4° | tempo medio %5 ms")
        .arg(cameraId)
        .arg(results.size())
        .arg(centerSum / results.size(), 0, 'f', 2)
        .arg(angleSum / results.size(), 0, 'f', 2)
        .arg(timeSum / results.size(), 0, 'f', 1);
    };

    const QString primaryCamera = m_multiPrimaryCamera->currentData().toString();
    const QVector<TestResult> primaryResults =
      m_broadcastResultsByCamera.value(primaryCamera);
    m_multiPrimarySummary->setText(summaryText(primaryCamera, primaryResults));
    m_multiPrimaryAnglePlot->setResults(primaryResults);
    m_multiPrimaryCenterPlot->setResults(primaryResults);

    const QString compareCamera = m_multiCompareCamera->currentData().toString();
    const QVector<TestResult> compareResults =
      m_broadcastResultsByCamera.value(compareCamera);
    const bool hasComparison = !compareCamera.isEmpty();
    m_multiCompareGroup->setVisible(hasComparison);
    if (hasComparison)
    {
      m_multiCompareSummary->setText(summaryText(compareCamera, compareResults));
      m_multiCompareAnglePlot->setResults(compareResults);
      m_multiCompareCenterPlot->setResults(compareResults);
    }
  }

  void startBroadcastTest(const QJsonArray& targets)
  {
    if (targets.size() < 2 || m_broadcastRunning)
    {
      return;
    }
    m_broadcastRunning = true;
    m_activeBroadcastWorkers = targets.size();
    m_broadcastSummaries = {};
    m_broadcastResultsByCamera.clear();
    m_broadcastTable->setRowCount(0);
    refreshBroadcastAnalysisSummary();
    m_broadcastTable->show();
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    QStringList targetDescriptions;
    for (const QJsonValue& value : targets)
    {
      const QJsonObject target = value.toObject();
      targetDescriptions.append(QString("%1=%2")
        .arg(
          target.value("cameraId").toString(),
          target.value("strategyId").toString()));
    }
    appendLog(QString("Broadcast avviato: %1 telecamere | %2")
      .arg(targets.size())
      .arg(targetDescriptions.join(", ")));

    for (const QJsonValue& value : targets)
    {
      const QJsonObject target = value.toObject();
      const QString cameraId = target.value("cameraId").toString();
      const QString strategyId = target.value("strategyId").toString();
      auto* worker = new TestVisionWindow(m_scenarioPath, this);
      worker->hide();
      worker->setSendOnlyMode(m_sendOnlyCheck && m_sendOnlyCheck->isChecked());
      m_broadcastWorkers.append(worker);
      worker->startCampaignWorker(
        currentBroadcastItem(cameraId, strategyId),
        1,
        [this, worker, cameraId](const QJsonObject& summary) {
          m_broadcastSummaries.append(summary);
          appendLog(QString("Broadcast %1 terminato: %2")
            .arg(cameraId, summary.value("status").toString()));
          m_broadcastWorkers.removeAll(worker);
          worker->deleteLater();
          --m_activeBroadcastWorkers;
          if (m_activeBroadcastWorkers == 0)
          {
            m_broadcastRunning = false;
            m_startButton->setEnabled(true);
            m_stopButton->setEnabled(false);
            m_stateLabel->setText(QString(
              "Broadcast completato: %1 telecamere, %2 risultati")
              .arg(m_broadcastSummaries.size())
              .arg(m_broadcastTable->rowCount()));
          }
        },
        [this](const QString& cameraId, const TestResult& result) {
          appendBroadcastResult(cameraId, result);
        });
    }
  }

  void syncStrategyFromRecipe()
  {
    if (!m_strategyCombo || !m_recipeCombo || !m_cameraCombo)
    {
      return;
    }

    const QString recipeId = m_recipeCombo->currentText().trimmed();
    const QString cameraId = m_cameraCombo->currentData().toString();
    if (recipeId.isEmpty() || cameraId.isEmpty() || cameraId == "SELECTED")
    {
      return;
    }

    const QDir recipesDirectory(QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath("recipes"));
    const QString cameraPath = recipesDirectory.filePath(recipeId + "/cameras/" + cameraId + ".json");
    QFile cameraFile(cameraPath);
    if (!cameraFile.open(QIODevice::ReadOnly))
    {
      return;
    }

    const QJsonObject root = QJsonDocument::fromJson(cameraFile.readAll()).object();
    const QString method = root.value("tools").toObject()
      .value("surfaceLocalization").toObject()
      .value("method").toString();
    const int strategyIndex = m_strategyCombo->findData(method);
    if (strategyIndex >= 0 && m_strategyCombo->currentIndex() != strategyIndex)
    {
      m_strategyCombo->setCurrentIndex(strategyIndex);
      appendLog("Strategia allineata alla ricetta: " + method);
    }
  }

  void startTest()
  {
    if (!m_isCampaignWorker)
    {
      const QJsonArray broadcastTargets = selectedBroadcastItems();
      if (broadcastTargets.size() > 1)
      {
        startBroadcastTest(broadcastTargets);
        return;
      }
      if (broadcastTargets.size() == 1)
      {
        const QJsonObject target = broadcastTargets.first().toObject();
        const int cameraIndex = m_cameraCombo->findData(
          target.value("cameraId").toString());
        if (cameraIndex >= 0)
        {
          m_cameraCombo->setCurrentIndex(cameraIndex);
        }
        const int strategyIndex = m_strategyCombo->findData(
          target.value("strategyId").toString());
        if (strategyIndex >= 0)
        {
          m_strategyCombo->setCurrentIndex(strategyIndex);
        }
      }
    }
    QString error;
    if (!buildPlan(&error))
    {
      QMessageBox::warning(this, "TestVision", error);
      return;
    }
    syncStrategyFromRecipe();
    closePipe();
    m_currentIndex = -1;
    m_currentFrameId = 0;
    m_results.clear();
    m_hasBaseline = false;
    m_baselineResult = {};
    m_baselinePose = {};
    refreshAnalysis();
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_sendOnlyMode = m_sendOnlyCheck && m_sendOnlyCheck->isChecked();
    m_stateLabel->setText(
      m_sendOnlyMode ? "Connessione a Vision (solo invio)..." : "Connessione a Vision...");
    m_pendingSample = false;
    connectPipe();
  }

  void stopTest(const QString& reason)
  {
    m_sendTimer.stop();
    std::sort(m_results.begin(), m_results.end(), [](const TestResult& a, const TestResult& b) {
      return a.frameId < b.frameId;
    });
    if (m_table)
    {
      m_table->setRowCount(0);
      for (const TestResult& result : m_results)
      {
        appendResultRow(result);
      }
    }
    refreshAnalysis();

    if (!m_results.isEmpty())
    {
      saveReport();
    }
    appendLog(reason);
    m_stateLabel->setText(reason);
    m_pollTimer.stop();
    closePipe();
    m_state = State::Idle;
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    if (m_isCampaignWorker && !m_workerCompleted)
    {
      m_workerCompleted = true;
      QString failure = m_workerFailure;
      if (!reason.startsWith("Test completato") &&
          !reason.startsWith("Invio completato") &&
          failure.isEmpty())
      {
        failure = reason;
      }
      const QJsonObject summary =
        buildCampaignSummary(m_workerItem, m_workerCycle, failure);
      if (m_workerCompletion)
      {
        m_workerCompletion(summary);
      }
    }
  }

  void closePipe()
  {
    if (m_pipe != INVALID_HANDLE_VALUE)
    {
      CloseHandle(m_pipe);
      m_pipe = INVALID_HANDLE_VALUE;
    }
    m_buffer.clear();
  }

  void pollPipe()
  {
    DWORD available = 0;
    if (!PeekNamedPipe(m_pipe, nullptr, 0, nullptr, &available, nullptr))
    {
      stopTest(QString("Connessione persa, errore Windows %1").arg(GetLastError()));
      return;
    }
    if (available > 0)
    {
      std::vector<char> chunk(std::min<DWORD>(available, 64 * 1024));
      DWORD bytesRead = 0;
      if (!ReadFile(m_pipe, chunk.data(), static_cast<DWORD>(chunk.size()), &bytesRead, nullptr))
      {
        stopTest(QString("Lettura fallita, errore Windows %1").arg(GetLastError()));
        return;
      }
      m_buffer.append(chunk.data(), static_cast<qsizetype>(bytesRead));
    }
    while (true)
    {
      const qsizetype newline = m_buffer.indexOf('\n');
      if (newline < 0) break;
      const QByteArray line = m_buffer.left(newline).trimmed();
      m_buffer.remove(0, newline + 1);
      const QJsonDocument document = QJsonDocument::fromJson(line);
      if (document.isObject()) processMessage(document.object());
    }
  }

  void processMessage(const QJsonObject& message)
  {
    const QString type = message.value("type").toString();
    if (type == "hello" && m_state == State::WaitingHello)
    {
      appendLog("Vision collegato");
      requestRecipe();
      return;
    }
    if (type == "recipeAccepted" && m_state == State::WaitingRecipe)
    {
      appendLog("Ricetta confermata da Vision: " + message.value("recipeId").toString());
      if (m_pendingSample) writeSample();
      else sendNextFrame();
      return;
    }
    if (type == "recipeRejected" && m_state == State::WaitingRecipe)
    {
      stopTest("Ricetta rifiutata da Vision: " + message.value("message").toString());
      return;
    }
    if (type == "sampleAccepted")
    {
      m_pendingSample = false;
      stopTest("Campione ricevuto da Vision");
      return;
    }
    if (type == "frameAccepted")
    {
      if (m_sendOnlyMode)
      {
        return;
      }
      int fid = message.value("frameId").toInt();
      m_state = State::WaitingResult;
      m_stateLabel->setText(QString("Frame %1 accettato; attendo Vision").arg(fid));
      return;
    }
    if (type == "waitingStart" || type == "frameScheduled" ||
        type == "processingStarted" || type == "processingCompleted" ||
        type == "processingError")
    {
      if (m_sendOnlyMode && type != "processingError")
      {
        return;
      }
      int fid = message.value("frameId").toInt();
      const QString text = message.value("message").toString(type);
      if (type == "processingError")
      {
        m_lastProcessingError = text;
        if (m_campaignRunning)
        {
          m_abortCurrentCampaignItem = true;
          m_campaignItemFailure = text;
        }
        if (m_isCampaignWorker)
        {
          m_abortCurrentCampaignItem = true;
          m_workerFailure = text;
        }
      }
      m_stateLabel->setText(QString("Frame %1: %2").arg(fid).arg(text));
      appendLog(QString("%1 (Frame %2): %3").arg(type).arg(fid).arg(text));
      return;
    }
    if (type == "result")
    {
      if (m_sendOnlyMode)
      {
        return;
      }
      int resultFrameId = message.value("frameId").toInt();
      handleResult(message);
      if (resultFrameId == 1)
      {
        if (m_hasBaseline)
        {
          sendNextFrame();
        }
        else
        {
          stopTest("Impossibile stabilire il riferimento/baseline (Frame 1 non valido)");
        }
      }
      else
      {
        if (m_results.size() == m_plan.size() || m_abortCurrentCampaignItem)
        {
          QTimer::singleShot(0, this, [this]() { finishTest(); });
        }
      }
      return;
    }
    if (type == "error")
    {
      stopTest("Errore Vision: " + message.value("message").toString());
    }
  }

  void sendNextFrame()
  {
    ++m_currentIndex;
    if (m_currentIndex >= m_plan.size())
    {
      m_sendTimer.stop();
      return;
    }
    ++m_currentFrameId;
    m_currentPose = m_plan[m_currentIndex];
    const double pxPerMm = 1.0 / m_pixelSize->value();
    const double txPx = m_currentPose.xMm * pxPerMm;
    const double tyPx = m_currentPose.yMm * pxPerMm;
    m_currentImage = transformImage(m_master, m_currentPose.angleDeg, txPx, tyPx);
    m_preview->setPixmap(matToPixmap(m_currentImage).scaled(
      m_preview->size(), Qt::KeepAspectRatio, Qt::FastTransformation));

    if (m_hasBaseline)
    {
      const QJsonObject canvas = m_scenario.value("canvas").toObject();
      const double rotationCenterX = canvas.value("width").toDouble(640.0) * 0.5;
      const double rotationCenterY = canvas.value("height").toDouble(480.0) * 0.5;
      const double deltaAngle =
        m_currentPose.angleDeg - m_baselinePose.angleDeg;
      const double radians = -deltaAngle * CV_PI / 180.0;
      const double baselineDx = m_baselineResult.actualX - rotationCenterX;
      const double baselineDy = m_baselineResult.actualY - rotationCenterY;
      const double rotatedDx =
        baselineDx * std::cos(radians) - baselineDy * std::sin(radians);
      const double rotatedDy =
        baselineDx * std::sin(radians) + baselineDy * std::cos(radians);
      m_expectedX =
        rotationCenterX + rotatedDx +
        (m_currentPose.xMm - m_baselinePose.xMm) * pxPerMm;
      m_expectedY =
        rotationCenterY + rotatedDy +
        (m_currentPose.yMm - m_baselinePose.yMm) * pxPerMm;
      m_expectedAngle =
        m_baselineResult.actualAngle -
        deltaAngle;
    }
    else
    {
      m_expectedX = 0.0;
      m_expectedY = 0.0;
      m_expectedAngle = 0.0;
    }
    m_frameLabel->setText(QString("%1 / %2 | posa %3 | giro %4")
      .arg(m_currentIndex + 1).arg(m_plan.size())
      .arg(m_currentPose.poseIndex).arg(m_currentPose.pass));
    m_expectedLabel->setText(
      m_hasBaseline
        ? QString("X=%1 Y=%2 A=%3° (relativo al frame zero)")
            .arg(m_expectedX, 0, 'f', 2)
            .arg(m_expectedY, 0, 'f', 2)
            .arg(m_expectedAngle, 0, 'f', 2)
        : QString("Frame zero: acquisizione riferimento Vision"));
    m_actualLabel->setText("-");

    QJsonObject frame;
    frame["type"] = "frame";
    frame["protocolVersion"] = m_scenario.value("protocolVersion").toInt(1);
    frame["scenarioId"] = m_scenario.value("name").toString();
    const QString targetCam = m_cameraCombo->currentData().toString();
    frame["cameraId"] = targetCam;
    frame["channel"] = targetCam;
    int targetSlot = 1;
    if (targetCam != "SELECTED")
    {
      targetSlot = targetCam.mid(3).toInt();
    }
    else
    {
      targetSlot = m_scenario.value("slot").toInt(1);
    }
    frame["slot"] = targetSlot;
    frame["frameId"] = m_currentFrameId;
    frame["strategyId"] = m_strategyCombo->currentData().toString();
    frame["recipeId"] = m_expectedRecipeId;
    frame["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    frame["imageFormat"] = "jpg";
    frame["imageBase64"] = QString::fromLatin1(jpegBase64(m_currentImage));
    m_lastProcessingError.clear();
    m_abortCurrentCampaignItem = false;

    // Record the sent frame info so we can reconstruct results asynchronously
    SentFrameInfo info;
    info.pose = m_currentPose;
    info.expectedX = m_expectedX;
    info.expectedY = m_expectedY;
    info.expectedAngle = m_expectedAngle;
    if (!m_sendOnlyMode)
    {
      m_sentFrames.insert(m_currentFrameId, info);
    }

    QByteArray payload = QJsonDocument(frame).toJson(QJsonDocument::Compact);
    payload.append('\n');
    DWORD written = 0;
    if (!WriteFile(m_pipe, payload.constData(), static_cast<DWORD>(payload.size()), &written, nullptr))
    {
      stopTest(QString("Invio fallito, errore Windows %1").arg(GetLastError()));
      return;
    }
    m_state = State::WaitingAccepted;
    m_stateLabel->setText(
      m_sendOnlyMode
        ? QString("Frame %1 inviato (%2/%3)")
            .arg(m_currentFrameId)
            .arg(m_currentIndex + 1)
            .arg(m_plan.size())
        : QString("Frame %1 inviato").arg(m_currentFrameId));

    if (m_sendOnlyMode)
    {
      if (m_currentIndex + 1 < m_plan.size())
      {
        m_sendTimer.start(m_intervalMs->value());
      }
      else
      {
        QTimer::singleShot(0, this, [this]() { finishSendOnlyTest(); });
      }
    }
    else if (m_hasBaseline)
    {
      m_sendTimer.start(m_intervalMs->value());
    }
  }

  void finishSendOnlyTest()
  {
    m_sendTimer.stop();
    stopTest(QString("Invio completato: %1 frame").arg(m_currentFrameId));
    if (m_campaignRunning)
    {
      recordCampaignSummary();
      QTimer::singleShot(500, this, [this]() { runNextCampaignItem(); });
    }
  }

  void requestRecipe()
  {
    QJsonObject request;
    request["type"] = "setRecipe";
    request["protocolVersion"] = m_scenario.value("protocolVersion").toInt(1);
    request["recipeId"] = m_expectedRecipeId;
    QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    payload.append('\n');
    DWORD written = 0;
    if (!WriteFile(m_pipe, payload.constData(), static_cast<DWORD>(payload.size()), &written, nullptr))
    {
      stopTest(QString("Cambio ricetta fallito, errore Windows %1").arg(GetLastError()));
      return;
    }
    m_state = State::WaitingRecipe;
    m_stateLabel->setText("Caricamento ricetta " + m_expectedRecipeId);
  }

  void handleResult(const QJsonObject& message)
  {
    int resultFrameId = message.value("frameId").toInt();
    if (!m_sentFrames.contains(resultFrameId))
    {
      appendLog(QString("Risultato ricevuto per frame %1 non configurato").arg(resultFrameId));
      return;
    }
    const SentFrameInfo info = m_sentFrames.value(resultFrameId);

    const QJsonObject pose = message.value("pose").toObject();
    TestResult result;
    result.pose = info.pose;
    result.frameId = resultFrameId;
    result.valid = pose.value("valid").toBool();
    result.actualX = pose.value("x").toDouble();
    result.actualY = pose.value("y").toDouble();
    result.actualAngle = pose.value("angleDeg").toDouble();

    if (!m_hasBaseline && result.valid)
    {
      m_hasBaseline = true;
      m_baselinePose = info.pose;
      m_baselineResult = result;
      m_expectedX = result.actualX;
      m_expectedY = result.actualY;
      m_expectedAngle = result.actualAngle;
      result.expectedX = result.actualX;
      result.expectedY = result.actualY;
      result.expectedAngle = result.actualAngle;
      appendLog(QString("Frame zero Vision: X=%1 Y=%2 A=%3°")
        .arg(result.actualX, 0, 'f', 3)
        .arg(result.actualY, 0, 'f', 3)
        .arg(result.actualAngle, 0, 'f', 3));
    }
    else
    {
      result.expectedX = info.expectedX;
      result.expectedY = info.expectedY;
      result.expectedAngle = info.expectedAngle;
    }

    result.centerError = std::hypot(result.actualX - result.expectedX, result.actualY - result.expectedY);
    result.angleError = pcaAngleError(result.actualAngle, result.expectedAngle);
    result.processingMs = message.value("processingMs").toDouble();
    m_results.append(result);
    if (m_workerResultCallback)
    {
      m_workerResultCallback(
        m_workerItem.value("cameraId").toString(
          m_cameraCombo->currentData().toString()),
        result);
    }

    m_actualLabel->setText(QString("Frame %1 | X=%2 Y=%3 A=%4° | Ec=%5 px Ea=%6°")
      .arg(resultFrameId)
      .arg(result.actualX, 0, 'f', 2).arg(result.actualY, 0, 'f', 2)
      .arg(result.actualAngle, 0, 'f', 2).arg(result.centerError, 0, 'f', 2)
      .arg(result.angleError, 0, 'f', 2));
    m_stateLabel->setText(
      result.valid
        ? QString("Frame %1: Risultato ricevuto").arg(resultFrameId)
        : (m_lastProcessingError.isEmpty()
             ? QString("Frame %1: Posa non valida").arg(resultFrameId)
             : QString("Frame %1: Posa non valida: %2").arg(resultFrameId).arg(m_lastProcessingError)));
    if (!result.valid && !m_lastProcessingError.isEmpty())
    {
      appendLog(QString("Motivo posa non valida Frame %1: %2").arg(resultFrameId).arg(m_lastProcessingError));
    }
    m_lastProcessingError.clear();
    appendResultRow(result);
    refreshAnalysis();
  }

  void appendResultRow(const TestResult& result)
  {
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    const QStringList values = {
      QString::number(result.pose.poseIndex),
      QString::number(result.pose.pass),
      QString::number(result.pose.xMm, 'f', 3),
      QString::number(result.pose.yMm, 'f', 3),
      QString::number(result.pose.angleDeg, 'f', 3),
      QString::number(result.expectedX, 'f', 3),
      QString::number(result.expectedY, 'f', 3),
      QString::number(result.expectedAngle, 'f', 3),
      QString::number(result.actualX, 'f', 3),
      QString::number(result.actualY, 'f', 3),
      QString::number(result.actualAngle, 'f', 3),
      QString::number(result.centerError, 'f', 3),
      QString::number(result.angleError, 'f', 3),
      QString::number(result.processingMs, 'f', 1)
    };
    for (int column = 0; column < values.size(); ++column)
    {
      auto* item = new QTableWidgetItem(values[column]);
      if (!result.valid)
      {
        item->setBackground(QColor("#7a2633"));
      }
      m_table->setItem(row, column, item);
    }
  }

  void refreshAnalysis()
  {
    if (m_results.isEmpty())
    {
      m_table->setRowCount(0);
      m_summaryLabel->setText("Nessun dato");
      m_anglePlot->setResults({});
      m_centerPlot->setResults({});
      m_repeatabilityPlot->setResults({});
      return;
    }

    double centerSum = 0.0;
    double angleSum = 0.0;
    double centerMax = 0.0;
    double angleMax = 0.0;
    int invalidCount = 0;
    for (const TestResult& result : m_results)
    {
      if (!result.valid)
      {
        ++invalidCount;
      }
      centerSum += result.centerError;
      angleSum += result.angleError;
      centerMax = std::max(centerMax, result.centerError);
      angleMax = std::max(angleMax, result.angleError);
    }

    std::map<int, std::vector<TestResult>> grouped;
    for (const TestResult& result : m_results)
    {
      grouped[result.pose.poseIndex].push_back(result);
    }
    double worstRepeatCenter = 0.0;
    double worstRepeatAngle = 0.0;
    for (const auto& [poseIndex, values] : grouped)
    {
      std::vector<double> xs;
      std::vector<double> ys;
      std::vector<double> angles;
      for (const TestResult& result : values)
      {
        xs.push_back(result.actualX);
        ys.push_back(result.actualY);
        angles.push_back(normalizePcaAngleNear(
          result.actualAngle,
          result.expectedAngle));
      }
      worstRepeatCenter = std::max(
        worstRepeatCenter,
        std::hypot(standardDeviation(xs), standardDeviation(ys)));
      worstRepeatAngle = std::max(worstRepeatAngle, standardDeviation(angles));
    }

    m_summaryLabel->setText(
      QString("Frame: %1 (Falliti/Scartati: %2) | Errore centro medio/max: %3 / %4 px | "
              "Errore angolo medio/max: %5 / %6° | "
              "Ripetibilità peggiore σ centro/angolo: %7 px / %8°")
        .arg(m_results.size())
        .arg(invalidCount)
        .arg(centerSum / m_results.size(), 0, 'f', 3)
        .arg(centerMax, 0, 'f', 3)
        .arg(angleSum / m_results.size(), 0, 'f', 3)
        .arg(angleMax, 0, 'f', 3)
        .arg(worstRepeatCenter, 0, 'f', 4)
        .arg(worstRepeatAngle, 0, 'f', 4));
    m_anglePlot->setResults(m_results);
    m_centerPlot->setResults(m_results);
    m_repeatabilityPlot->setResults(m_results);
  }

  void finishTest()
  {
    stopTest(QString("Test completato: %1 frame").arg(m_results.size()));
    if (m_campaignRunning)
    {
      recordCampaignSummary();
      QTimer::singleShot(500, this, [this]() { runNextCampaignItem(); });
    }
  }

  void saveReport()
  {
    QJsonArray frames;
    for (const TestResult& result : m_results)
    {
      QJsonObject item;
      item["poseIndex"] = result.pose.poseIndex;
      item["pass"] = result.pose.pass;
      item["xMm"] = result.pose.xMm;
      item["yMm"] = result.pose.yMm;
      item["angleExpectedDeg"] = result.expectedAngle;
      item["expectedX"] = result.expectedX;
      item["expectedY"] = result.expectedY;
      item["actualX"] = result.actualX;
      item["actualY"] = result.actualY;
      item["actualAngleDeg"] = result.actualAngle;
      item["centerErrorPx"] = result.centerError;
      item["angleErrorDeg"] = result.angleError;
      item["processingMs"] = result.processingMs;
      item["valid"] = result.valid;
      frames.append(item);
    }
    QJsonObject report;
    report["scenario"] = m_scenario.value("name");
    report["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    report["pixelSizeMm"] = m_pixelSize->value();
    report["passes"] = m_passes->value();
    report["strategyId"] = m_strategyCombo->currentData().toString();
    report["recipeId"] = m_recipeCombo->currentText().trimmed();
    report["cameraId"] = m_cameraCombo->currentData().toString();
    report["frames"] = frames;
    const QDir scenarioDir = QFileInfo(m_scenarioPath).dir();
    const QString path = scenarioDir.absoluteFilePath(m_scenario.value("output").toString());
    QString error;
    const QString versionedPath = saveVersionedTestReport(path, report, &error);
    if (versionedPath.isEmpty())
    {
      appendLog(error);
      return;
    }
    appendLog("Report salvato: " + QDir::toNativeSeparators(versionedPath));
  }

  void appendLog(const QString& message)
  {
    m_log->appendPlainText(QString("%1  %2")
      .arg(QTime::currentTime().toString("HH:mm:ss.zzz"), message));
  }

  QString m_scenarioPath;
  QJsonObject m_scenario;
  cv::Mat m_master;
  cv::Mat m_currentImage;
  QVector<TestPose> m_plan;
  QVector<TestResult> m_results;
  TestPose m_currentPose;
  int m_currentIndex = -1;
  int m_currentFrameId = 0;
  double m_expectedX = 0.0;
  double m_expectedY = 0.0;
  double m_expectedAngle = 0.0;
  bool m_hasBaseline = false;
  TestPose m_baselinePose;
  TestResult m_baselineResult;

  QLabel* m_preview = nullptr;
  QLabel* m_scenarioLabel = nullptr;
  QLabel* m_frameLabel = nullptr;
  QLabel* m_expectedLabel = nullptr;
  QLabel* m_actualLabel = nullptr;
  QLabel* m_stateLabel = nullptr;
  QLabel* m_totalLabel = nullptr;
  QLabel* m_summaryLabel = nullptr;
  QPlainTextEdit* m_log = nullptr;
  QPushButton* m_startButton = nullptr;
  QPushButton* m_stopButton = nullptr;
  QTableWidget* m_table = nullptr;
  PlotWidget* m_anglePlot = nullptr;
  PlotWidget* m_centerPlot = nullptr;
  RepeatabilityPlotWidget* m_repeatabilityPlot = nullptr;
  QDoubleSpinBox* m_xMin = nullptr;
  QDoubleSpinBox* m_xMax = nullptr;
  QDoubleSpinBox* m_xStep = nullptr;
  QDoubleSpinBox* m_yMin = nullptr;
  QDoubleSpinBox* m_yMax = nullptr;
  QDoubleSpinBox* m_yStep = nullptr;
  QDoubleSpinBox* m_angleMin = nullptr;
  QDoubleSpinBox* m_angleMax = nullptr;
  QDoubleSpinBox* m_angleStep = nullptr;
  QDoubleSpinBox* m_pixelSize = nullptr;
  QSpinBox* m_passes = nullptr;
  QSpinBox* m_intervalMs = nullptr;
  QComboBox* m_shapeCombo = nullptr;
  QComboBox* m_strategyCombo = nullptr;
  QComboBox* m_recipeCombo = nullptr;
  QComboBox* m_cameraCombo = nullptr;
  QTableWidget* m_cameraTargets = nullptr;
  QTableWidget* m_broadcastTable = nullptr;
  QTableWidget* m_multiSummaryTable = nullptr;
  QComboBox* m_multiPrimaryCamera = nullptr;
  QComboBox* m_multiCompareCamera = nullptr;
  QLabel* m_multiPrimarySummary = nullptr;
  QLabel* m_multiCompareSummary = nullptr;
  PlotWidget* m_multiPrimaryAnglePlot = nullptr;
  PlotWidget* m_multiPrimaryCenterPlot = nullptr;
  PlotWidget* m_multiCompareAnglePlot = nullptr;
  PlotWidget* m_multiCompareCenterPlot = nullptr;
  QGroupBox* m_multiCompareGroup = nullptr;
  QPushButton* m_sendSampleButton = nullptr;
  QPushButton* m_generateDatasetButton = nullptr;
  QPushButton* m_loadCampaignButton = nullptr;
  QPushButton* m_editCampaignButton = nullptr;
  QPushButton* m_startCampaignButton = nullptr;
  QCheckBox* m_sendOnlyCheck = nullptr;
  int m_canvasWidth = 640;
  int m_canvasHeight = 480;
  int m_canvasBackground = 240;
  bool m_pendingSample = false;
  bool m_sendOnlyMode = false;
  QString m_expectedRecipeId;
  QString m_lastProcessingError;
  QString m_campaignPath;
  QJsonObject m_campaign;
  QJsonArray m_campaignItems;
  QJsonArray m_campaignSummaries;
  int m_campaignCycles = 1;
  int m_campaignCycle = 0;
  int m_campaignItemIndex = -1;
  bool m_campaignRunning = false;
  bool m_abortCurrentCampaignItem = false;
  QString m_campaignItemFailure;
  QJsonArray m_parallelPendingItems;
  int m_activeCampaignWorkers = 0;

  bool m_isCampaignWorker = false;
  bool m_workerCompleted = false;
  int m_workerCycle = 0;
  QJsonObject m_workerItem;
  QString m_workerFailure;
  std::function<void(const QJsonObject&)> m_workerCompletion;
  std::function<void(const QString&, const TestResult&)> m_workerResultCallback;
  bool m_broadcastRunning = false;
  int m_activeBroadcastWorkers = 0;
  QJsonArray m_broadcastSummaries;
  QMap<QString, QVector<TestResult>> m_broadcastResultsByCamera;
  QList<TestVisionWindow*> m_broadcastWorkers;

  QTimer m_pollTimer;
  QTimer m_sendTimer;
  QHash<int, SentFrameInfo> m_sentFrames;
  HANDLE m_pipe = INVALID_HANDLE_VALUE;
  QByteArray m_buffer;
  State m_state = State::Idle;
};

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  QString scenarioPath = QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(
    "tests/scenarios/cross_rotation_cam01.json");
  QString campaignPath;
  QStringList broadcastCameras;
  bool sendOnly = false;
  for (int index = 1; index < argc; ++index)
  {
    const QString argument = QString::fromLocal8Bit(argv[index]);
    if (argument == "--send-only")
    {
      sendOnly = true;
    }
    else if (argument == "--campaign" && index + 1 < argc)
    {
      campaignPath = QFileInfo(
        QString::fromLocal8Bit(argv[++index])).absoluteFilePath();
    }
    else if (argument == "--broadcast" && index + 1 < argc)
    {
      broadcastCameras = QString::fromLocal8Bit(argv[++index])
        .split(',', Qt::SkipEmptyParts);
      for (QString& cameraId : broadcastCameras)
      {
        const QStringList parts = cameraId.trimmed().split(':');
        cameraId = parts.value(0).toUpper();
        if (parts.size() > 1)
        {
          cameraId += ":" + parts.value(1);
        }
      }
    }
    else if (!argument.startsWith("--"))
    {
      scenarioPath = QFileInfo(argument).absoluteFilePath();
    }
  }
  TestVisionWindow window(scenarioPath);
  if (sendOnly)
  {
    window.setSendOnlyMode(true);
  }
  window.show();
  if (!campaignPath.isEmpty())
  {
    QString error;
    if (!window.loadCampaignFromFile(campaignPath, &error, true))
    {
      QMessageBox::critical(&window, "Campagna TestVision", error);
    }
  }
  else if (!broadcastCameras.isEmpty())
  {
    window.startBroadcastFromCameras(broadcastCameras);
  }
  return app.exec();
}
