#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
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
#include <vector>

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

QByteArray pngBase64(const cv::Mat& image)
{
  std::vector<uchar> bytes;
  cv::imencode(".png", image, bytes);
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
        painter.setBrush(colors[colorIndex % colors.size()]);
        painter.drawEllipse(point, 4, 4);
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
  explicit TestVisionWindow(const QString& scenarioPath)
    : m_scenarioPath(scenarioPath)
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

private:
  enum class State { Idle, WaitingHello, WaitingAccepted, WaitingResult };

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
    m_strategyCombo->addItem("Shape model", "shapeModel");
    m_strategyCombo->addItem("Template model", "templateModel");
    m_recipeCombo = new QComboBox(configPanel);
    m_passes = new QSpinBox(configPanel);
    m_passes->setRange(1, 1000);
    m_passes->setValue(3);
    m_intervalMs = new QSpinBox(configPanel);
    m_intervalMs->setRange(0, 60000);
    m_intervalMs->setValue(200);
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
    configLayout->addLayout(form);

    m_totalLabel = new QLabel(configPanel);
    m_totalLabel->setWordWrap(true);
    configLayout->addWidget(m_totalLabel);
    auto* buttons = new QHBoxLayout();
    m_startButton = new QPushButton("Start", configPanel);
    m_stopButton = new QPushButton("Stop", configPanel);
    m_sendSampleButton = new QPushButton("Invia campione a Vision", configPanel);
    m_stopButton->setEnabled(false);
    buttons->addWidget(m_sendSampleButton);
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

    connect(m_startButton, &QPushButton::clicked, this, [this]() { startTest(); });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() { stopTest("Arrestato dall'utente"); });
    connect(m_sendSampleButton, &QPushButton::clicked, this, [this]() { sendSample(); });
    connect(m_shapeCombo, &QComboBox::currentIndexChanged, this, [this]() { updateMasterShape(); });
    connect(m_recipeCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
      m_expectedRecipeId = text.trimmed();
      updateScenarioLabel();
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
      m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
        m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    sample["cameraId"] = m_scenario.value("cameraId").toString();
    sample["slot"] = m_scenario.value("slot").toInt(1);
    sample["channel"] = m_scenario.value("channel").toString();
    sample["frameId"] = 0;
    sample["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    sample["shapeId"] = m_shapeCombo->currentData().toString();
    sample["strategyId"] = m_strategyCombo->currentData().toString();
    sample["recipeId"] = m_expectedRecipeId;
    sample["imageFormat"] = "png";
    sample["imageBase64"] = QString::fromLatin1(pngBase64(m_master));
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

  void startTest()
  {
    QString error;
    if (!buildPlan(&error))
    {
      QMessageBox::warning(this, "TestVision", error);
      return;
    }
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
    m_stateLabel->setText("Connessione a Vision...");
    m_pendingSample = false;
    connectPipe();
  }

  void stopTest(const QString& reason)
  {
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
      if (m_pendingSample)
      {
        writeSample();
      }
      else
      {
        sendNextFrame();
      }
      return;
    }
    if (type == "sampleAccepted")
    {
      m_pendingSample = false;
      stopTest("Campione ricevuto da Vision");
      return;
    }
    if (type == "frameAccepted" && message.value("frameId").toInt() == m_currentFrameId)
    {
      m_state = State::WaitingResult;
      m_stateLabel->setText(QString("Frame %1 accettato; attendo Vision").arg(m_currentFrameId));
      return;
    }
    if ((type == "waitingStart" || type == "frameScheduled" ||
         type == "processingStarted" || type == "processingCompleted" ||
         type == "processingError") &&
        message.value("frameId").toInt() == m_currentFrameId)
    {
      const QString text = message.value("message").toString(type);
      if (type == "processingError")
      {
        m_lastProcessingError = text;
      }
      m_stateLabel->setText(text);
      appendLog(QString("%1: %2").arg(type, text));
      return;
    }
    if (type == "result" && message.value("frameId").toInt() == m_currentFrameId)
    {
      handleResult(message);
      QTimer::singleShot(m_intervalMs->value(), this, [this]() { sendNextFrame(); });
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
      finishTest();
      return;
    }
    ++m_currentFrameId;
    m_currentPose = m_plan[m_currentIndex];
    const double pxPerMm = 1.0 / m_pixelSize->value();
    const double txPx = m_currentPose.xMm * pxPerMm;
    const double tyPx = m_currentPose.yMm * pxPerMm;
    m_currentImage = transformImage(m_master, m_currentPose.angleDeg, txPx, tyPx);
    m_preview->setPixmap(matToPixmap(m_currentImage).scaled(
      m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

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
    frame["cameraId"] = m_scenario.value("cameraId").toString();
    frame["slot"] = m_scenario.value("slot").toInt(1);
    frame["channel"] = m_scenario.value("channel").toString();
    frame["frameId"] = m_currentFrameId;
    frame["strategyId"] = m_strategyCombo->currentData().toString();
    frame["recipeId"] = m_expectedRecipeId;
    frame["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    frame["imageFormat"] = "png";
    frame["imageBase64"] = QString::fromLatin1(pngBase64(m_currentImage));
    m_lastProcessingError.clear();
    QByteArray payload = QJsonDocument(frame).toJson(QJsonDocument::Compact);
    payload.append('\n');
    DWORD written = 0;
    if (!WriteFile(m_pipe, payload.constData(), static_cast<DWORD>(payload.size()), &written, nullptr))
    {
      stopTest(QString("Invio fallito, errore Windows %1").arg(GetLastError()));
      return;
    }
    m_state = State::WaitingAccepted;
    m_stateLabel->setText(QString("Frame %1 inviato").arg(m_currentFrameId));
  }

  void handleResult(const QJsonObject& message)
  {
    const QJsonObject pose = message.value("pose").toObject();
    TestResult result;
    result.pose = m_currentPose;
    result.frameId = m_currentFrameId;
    result.valid = pose.value("valid").toBool();
    result.actualX = pose.value("x").toDouble();
    result.actualY = pose.value("y").toDouble();
    result.actualAngle = pose.value("angleDeg").toDouble();

    if (!m_hasBaseline && result.valid)
    {
      m_hasBaseline = true;
      m_baselinePose = m_currentPose;
      m_baselineResult = result;
      m_expectedX = result.actualX;
      m_expectedY = result.actualY;
      m_expectedAngle = result.actualAngle;
      appendLog(QString("Frame zero Vision: X=%1 Y=%2 A=%3°")
        .arg(result.actualX, 0, 'f', 3)
        .arg(result.actualY, 0, 'f', 3)
        .arg(result.actualAngle, 0, 'f', 3));
    }

    result.expectedX = m_expectedX;
    result.expectedY = m_expectedY;
    result.expectedAngle = m_expectedAngle;
    result.centerError = std::hypot(result.actualX - result.expectedX, result.actualY - result.expectedY);
    result.angleError = pcaAngleError(result.actualAngle, result.expectedAngle);
    result.processingMs = message.value("processingMs").toDouble();
    m_results.append(result);

    m_actualLabel->setText(QString("X=%1 Y=%2 A=%3° | Ec=%4 px Ea=%5°")
      .arg(result.actualX, 0, 'f', 2).arg(result.actualY, 0, 'f', 2)
      .arg(result.actualAngle, 0, 'f', 2).arg(result.centerError, 0, 'f', 2)
      .arg(result.angleError, 0, 'f', 2));
    m_stateLabel->setText(
      result.valid
        ? "Risultato ricevuto"
        : (m_lastProcessingError.isEmpty()
             ? "Posa non valida"
             : "Posa non valida: " + m_lastProcessingError));
    if (!result.valid && !m_lastProcessingError.isEmpty())
    {
      appendLog("Motivo posa non valida: " + m_lastProcessingError);
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
      m_table->setItem(row, column, new QTableWidgetItem(values[column]));
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
    for (const TestResult& result : m_results)
    {
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
      QString("Frame: %1 | Errore centro medio/max: %2 / %3 px | "
              "Errore angolo medio/max: %4 / %5° | "
              "Ripetibilità peggiore σ centro/angolo: %6 px / %7°")
        .arg(m_results.size())
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
    report["frames"] = frames;
    const QDir scenarioDir = QFileInfo(m_scenarioPath).dir();
    const QString path = scenarioDir.absoluteFilePath(m_scenario.value("output").toString());
    QDir().mkpath(QFileInfo(path).dir().absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    }
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
  QPushButton* m_sendSampleButton = nullptr;
  int m_canvasWidth = 640;
  int m_canvasHeight = 480;
  int m_canvasBackground = 240;
  bool m_pendingSample = false;
  QString m_expectedRecipeId;
  QString m_lastProcessingError;

  QTimer m_pollTimer;
  HANDLE m_pipe = INVALID_HANDLE_VALUE;
  QByteArray m_buffer;
  State m_state = State::Idle;
};

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  const QString scenarioPath = argc > 1
    ? QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath()
    : QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(
        "tests/scenarios/cross_rotation_cam01.json");
  TestVisionWindow window(scenarioPath);
  window.show();
  return app.exec();
}
