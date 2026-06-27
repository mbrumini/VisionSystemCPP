#include "CampaignEditorDialog.h"
#include "TestVisionExecutionProfile.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>

namespace
{
QDoubleSpinBox* numberBox(double value, double minimum, double maximum, double step)
{
  auto* box = new QDoubleSpinBox();
  box->setRange(minimum, maximum);
  box->setDecimals(3);
  box->setSingleStep(step);
  box->setValue(value);
  return box;
}

QJsonObject defaultItem()
{
  return {
    {"cameraId", "CAM01"},
    {"recipeId", ""},
    {"strategyId", "aiYolo"},
    {"shapeId", "plate"},
    {"passes", 2},
    {"intervalMs", 100},
    {"xMinMm", 0.0}, {"xMaxMm", 0.0}, {"xStepMm", 1.0},
    {"yMinMm", 0.0}, {"yMaxMm", 0.0}, {"yStepMm", 1.0},
    {"angleMinDeg", 0.0}, {"angleMaxDeg", 355.0}, {"angleStepDeg", 5.0},
    {"pixelSizeMm", 0.0654},
    {"canvasWidth", 640},
    {"canvasHeight", 480},
    {"canvasBackground", 240},
    {"frameDeliveryMode", "collectResults"},
    {"sendOnly", false},
    {"limits", QJsonObject{
      {"centerMeanMaxPx", 5.0}, {"centerMaxPx", 10.0},
      {"angleMeanMaxDeg", 5.0}, {"angleMaxDeg", 15.0},
      {"processingMeanMaxMs", 250.0}, {"processingMaxMs", 2000.0}
    }}
  };
}
}

CampaignEditorDialog::CampaignEditorDialog(
  const QJsonObject& campaign,
  const QStringList& recipes,
  QWidget* parent)
  : QDialog(parent)
  , m_campaign(campaign)
{
  setWindowTitle("Editor campagne TestVision");
  resize(980, 720);
  auto* root = new QVBoxLayout(this);

  auto* header = new QFormLayout();
  m_name = new QLineEdit(campaign.value("name").toString("nuova_campagna"), this);
  m_cycles = new QSpinBox(this);
  m_cycles->setRange(1, 100000);
  m_cycles->setValue(qMax(1, campaign.value("cycles").toInt(1)));
  m_parallel = new QCheckBox("Esegui contemporaneamente le telecamere compatibili", this);
  m_parallel->setChecked(campaign.value("parallel").toBool(true));
  header->addRow("Nome campagna", m_name);
  header->addRow("Cicli completi", m_cycles);
  header->addRow("Multicamera", m_parallel);
  root->addLayout(header);

  auto* splitter = new QSplitter(Qt::Horizontal, this);
  auto* listPanel = new QWidget(splitter);
  auto* listLayout = new QVBoxLayout(listPanel);
  m_items = new QListWidget(listPanel);
  listLayout->addWidget(new QLabel("Prove in sequenza", listPanel));
  listLayout->addWidget(m_items, 1);
  auto* listButtons = new QHBoxLayout();
  auto* add = new QPushButton("Aggiungi", listPanel);
  auto* duplicate = new QPushButton("Duplica", listPanel);
  auto* remove = new QPushButton("Rimuovi", listPanel);
  listButtons->addWidget(add);
  listButtons->addWidget(duplicate);
  listButtons->addWidget(remove);
  listLayout->addLayout(listButtons);

  auto* scroll = new QScrollArea(splitter);
  scroll->setWidgetResizable(true);
  auto* formPanel = new QWidget(scroll);
  auto* form = new QFormLayout(formPanel);
  m_camera = new QComboBox(formPanel);
  for (int index = 1; index <= 16; ++index)
  {
    const QString cameraId = QString("CAM%1").arg(index, 2, 10, QChar('0'));
    m_camera->addItem(cameraId, cameraId);
  }
  m_recipe = new QComboBox(formPanel);
  m_recipe->addItems(recipes);
  m_strategy = new QComboBox(formPanel);
  m_strategy->addItem("AI YOLO", "aiYolo");
  m_strategy->addItem("Massa / PCA", "massPca");
  m_strategy->addItem("Edge / PCA", "edgePca");
  m_strategy->addItem("Corona soglia", "threshold");
  m_strategy->addItem("Corona edge", "edge");
  m_strategy->addItem("Due cerchi / asse", "two_circles_axis");
  m_strategy->addItem("Shape model", "shapeModel");
  m_strategy->addItem("Template model", "templateModel");
  m_shape = new QComboBox(formPanel);
  m_shape->addItem("Croce asimmetrica", "cross");
  m_shape->addItem("Rettangolo asimmetrico", "rectangle");
  m_shape->addItem("Cerchi concentrici", "circles");
  m_shape->addItem("Piastra con fori", "plate");
  m_shape->addItem("Profilo a L", "l_profile");
  m_shape->addItem("Gambo vite", "screw_shank");
  m_shape->addItem("Stress tutti i tool", "stress_all_tools");
  m_shape->addItem("Ruota dentata", "gear");
  m_resolution = new QComboBox(formPanel);
  m_resolution->addItem("640 × 480", QVariant::fromValue(QSize(640, 480)));
  m_resolution->addItem("1280 × 720", QVariant::fromValue(QSize(1280, 720)));
  m_resolution->addItem("1920 × 1080", QVariant::fromValue(QSize(1920, 1080)));
  m_resolution->addItem("2048 × 1536", QVariant::fromValue(QSize(2048, 1536)));
  m_resolution->addItem("Personalizzata", QVariant::fromValue(QSize(-1, -1)));
  m_canvasWidth = new QSpinBox(formPanel);
  m_canvasWidth->setRange(64, 8192);
  m_canvasWidth->setSingleStep(16);
  m_canvasWidth->setValue(640);
  m_canvasHeight = new QSpinBox(formPanel);
  m_canvasHeight->setRange(64, 8192);
  m_canvasHeight->setSingleStep(16);
  m_canvasHeight->setValue(480);
  m_passes = new QSpinBox(formPanel);
  m_passes->setRange(1, 1000);
  m_intervalMs = new QSpinBox(formPanel);
  m_intervalMs->setRange(0, 60000);
  m_xMin = numberBox(0, -1000, 1000, 0.5);
  m_xMax = numberBox(0, -1000, 1000, 0.5);
  m_xStep = numberBox(1, 0.001, 1000, 0.5);
  m_yMin = numberBox(0, -1000, 1000, 0.5);
  m_yMax = numberBox(0, -1000, 1000, 0.5);
  m_yStep = numberBox(1, 0.001, 1000, 0.5);
  m_angleMin = numberBox(0, -360, 360, 5);
  m_angleMax = numberBox(355, -360, 360, 5);
  m_angleStep = numberBox(5, 0.001, 360, 5);
  m_pixelSize = numberBox(0.0654, 0.000001, 1000, 0.001);
  m_sendOnly = new QCheckBox("Solo invio immagini (raffica senza raccolta dati)", formPanel);
  m_centerMean = numberBox(5, 0, 10000, 1);
  m_centerMax = numberBox(10, 0, 10000, 1);
  m_angleMean = numberBox(5, 0, 360, 1);
  m_angleErrorMax = numberBox(15, 0, 360, 1);
  m_timeMean = numberBox(250, 0, 600000, 10);
  m_timeMax = numberBox(2000, 0, 600000, 100);

  form->addRow("Telecamera", m_camera);
  form->addRow("Ricetta", m_recipe);
  form->addRow("Localizzazione", m_strategy);
  form->addRow("Forma campione", m_shape);
  form->addRow("Risoluzione immagine", m_resolution);
  form->addRow("Larghezza (px)", m_canvasWidth);
  form->addRow("Altezza (px)", m_canvasHeight);
  form->addRow("Giri", m_passes);
  form->addRow("Intervallo frame (ms)", m_intervalMs);
  form->addRow("X min / max / passo", new QLabel("Configurazione nelle tre righe seguenti"));
  form->addRow("X minimo", m_xMin); form->addRow("X massimo", m_xMax); form->addRow("Passo X", m_xStep);
  form->addRow("Y minimo", m_yMin); form->addRow("Y massimo", m_yMax); form->addRow("Passo Y", m_yStep);
  form->addRow("Angolo minimo", m_angleMin); form->addRow("Angolo massimo", m_angleMax); form->addRow("Passo angolo", m_angleStep);
  form->addRow("Scala (mm/px)", m_pixelSize);
  form->addRow("Modalità invio", m_sendOnly);
  form->addRow("Limite centro medio (px)", m_centerMean);
  form->addRow("Limite centro massimo (px)", m_centerMax);
  form->addRow("Limite angolo medio (°)", m_angleMean);
  form->addRow("Limite angolo massimo (°)", m_angleErrorMax);
  form->addRow("Limite tempo medio caldo (ms)", m_timeMean);
  form->addRow("Limite picco tempo (ms)", m_timeMax);
  scroll->setWidget(formPanel);
  splitter->addWidget(listPanel);
  splitter->addWidget(scroll);
  splitter->setStretchFactor(1, 1);
  root->addWidget(splitter, 1);

  auto* buttons = new QDialogButtonBox(
    QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Save)->setText("Usa e salva");
  root->addWidget(buttons);

  connect(add, &QPushButton::clicked, this, [this]() { addItem(); });
  connect(duplicate, &QPushButton::clicked, this, [this]() { duplicateItem(); });
  connect(remove, &QPushButton::clicked, this, [this]() { removeItem(); });
  connect(m_items, &QListWidget::currentRowChanged, this, [this](int) {
    loadSelectedItem();
  });
  connect(m_resolution, &QComboBox::currentIndexChanged, this, [this]() {
    const QSize preset = m_resolution->currentData().toSize();
    if (preset.width() > 0 && preset.height() > 0)
    {
      m_canvasWidth->setValue(preset.width());
      m_canvasHeight->setValue(preset.height());
    }
  });
  connect(m_canvasWidth, &QSpinBox::valueChanged, this, [this](int) {
    syncResolutionCombo();
  });
  connect(m_canvasHeight, &QSpinBox::valueChanged, this, [this](int) {
    syncResolutionCombo();
  });
  connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
    storeSelectedItem();
    accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  const QJsonArray items = campaign.value("items").toArray();
  for (const QJsonValue& item : items)
  {
    addItem(item.toObject());
  }
  if (m_items->count() == 0)
  {
    addItem();
  }
  m_items->setCurrentRow(0);
}

void CampaignEditorDialog::addItem(const QJsonObject& item)
{
  storeSelectedItem();
  const QJsonObject value = item.isEmpty() ? defaultItem() : item;
  auto* listItem = new QListWidgetItem(m_items);
  listItem->setData(Qt::UserRole, value);
  updateItemLabel(m_items->count() - 1);
  m_items->setCurrentRow(m_items->count() - 1);
}

void CampaignEditorDialog::removeItem()
{
  const int row = m_items->currentRow();
  if (row < 0) return;
  delete m_items->takeItem(row);
  m_loadedRow = -1;
  if (m_items->count() == 0) addItem();
  else m_items->setCurrentRow(qMin(row, m_items->count() - 1));
}

void CampaignEditorDialog::duplicateItem()
{
  storeSelectedItem();
  const int row = m_items->currentRow();
  addItem(row >= 0 ? m_items->item(row)->data(Qt::UserRole).toJsonObject() : defaultItem());
}

void CampaignEditorDialog::loadSelectedItem()
{
  storeSelectedItem();
  m_loadedRow = m_items->currentRow();
  if (m_loadedRow >= 0)
  {
    setFormItem(m_items->item(m_loadedRow)->data(Qt::UserRole).toJsonObject());
  }
}

void CampaignEditorDialog::storeSelectedItem()
{
  if (m_loadedRow < 0 || m_loadedRow >= m_items->count()) return;
  m_items->item(m_loadedRow)->setData(Qt::UserRole, formItem());
  updateItemLabel(m_loadedRow);
}

void CampaignEditorDialog::updateItemLabel(int row)
{
  if (row < 0 || row >= m_items->count()) return;
  const QJsonObject item = m_items->item(row)->data(Qt::UserRole).toJsonObject();
  m_items->item(row)->setText(QString("%1. %2 | %3 | %4 | %5")
    .arg(row + 1)
    .arg(item.value("cameraId").toString("CAM01"))
    .arg(item.value("recipeId").toString("<ricetta>"))
    .arg(item.value("strategyId").toString())
    .arg(item.value("shapeId").toString()));
}

QJsonObject CampaignEditorDialog::formItem() const
{
  QJsonObject item;
  item["cameraId"] = m_camera->currentData().toString();
  item["recipeId"] = m_recipe->currentText();
  item["strategyId"] = m_strategy->currentData().toString();
  item["shapeId"] = m_shape->currentData().toString();
  item["passes"] = m_passes->value();
  item["intervalMs"] = m_intervalMs->value();
  item["xMinMm"] = m_xMin->value(); item["xMaxMm"] = m_xMax->value(); item["xStepMm"] = m_xStep->value();
  item["yMinMm"] = m_yMin->value(); item["yMaxMm"] = m_yMax->value(); item["yStepMm"] = m_yStep->value();
  item["angleMinDeg"] = m_angleMin->value(); item["angleMaxDeg"] = m_angleMax->value(); item["angleStepDeg"] = m_angleStep->value();
  item["pixelSizeMm"] = m_pixelSize->value();
  item["canvasWidth"] = m_canvasWidth->value();
  item["canvasHeight"] = m_canvasHeight->value();
  item["canvasBackground"] = 240;
  item["frameDeliveryMode"] = m_sendOnly->isChecked() ? "sendOnly" : "collectResults";
  item["sendOnly"] = m_sendOnly->isChecked();
  item["limits"] = QJsonObject{
    {"centerMeanMaxPx", m_centerMean->value()},
    {"centerMaxPx", m_centerMax->value()},
    {"angleMeanMaxDeg", m_angleMean->value()},
    {"angleMaxDeg", m_angleErrorMax->value()},
    {"processingMeanMaxMs", m_timeMean->value()},
    {"processingMaxMs", m_timeMax->value()}
  };
  return item;
}

void CampaignEditorDialog::setFormItem(const QJsonObject& source)
{
  QJsonObject item = defaultItem();
  for (auto it = source.begin(); it != source.end(); ++it) item[it.key()] = it.value();
  auto select = [](QComboBox* box, const QString& value, bool data) {
    const int index = data ? box->findData(value) : box->findText(value);
    if (index >= 0) box->setCurrentIndex(index);
  };
  select(m_recipe, item.value("recipeId").toString(), false);
  select(m_camera, item.value("cameraId").toString("CAM01"), true);
  select(m_strategy, item.value("strategyId").toString(), true);
  select(m_shape, item.value("shapeId").toString(), true);
  m_passes->setValue(item.value("passes").toInt(2));
  m_intervalMs->setValue(item.value("intervalMs").toInt(100));
  m_xMin->setValue(item.value("xMinMm").toDouble()); m_xMax->setValue(item.value("xMaxMm").toDouble()); m_xStep->setValue(item.value("xStepMm").toDouble(1));
  m_yMin->setValue(item.value("yMinMm").toDouble()); m_yMax->setValue(item.value("yMaxMm").toDouble()); m_yStep->setValue(item.value("yStepMm").toDouble(1));
  m_angleMin->setValue(item.value("angleMinDeg").toDouble()); m_angleMax->setValue(item.value("angleMaxDeg").toDouble(355)); m_angleStep->setValue(item.value("angleStepDeg").toDouble(5));
  m_pixelSize->setValue(item.value("pixelSizeMm").toDouble(0.0654));
  m_canvasWidth->setValue(item.value("canvasWidth").toInt(640));
  m_canvasHeight->setValue(item.value("canvasHeight").toInt(480));
  syncResolutionCombo();
  m_sendOnly->setChecked(
    testVisionFrameDeliveryModeFromString(item.value("frameDeliveryMode").toString()) ==
      TestVisionFrameDeliveryMode::SendOnly ||
    item.value("sendOnly").toBool(false));
  const QJsonObject limits = item.value("limits").toObject();
  m_centerMean->setValue(limits.value("centerMeanMaxPx").toDouble(5));
  m_centerMax->setValue(limits.value("centerMaxPx").toDouble(10));
  m_angleMean->setValue(limits.value("angleMeanMaxDeg").toDouble(5));
  m_angleErrorMax->setValue(limits.value("angleMaxDeg").toDouble(15));
  m_timeMean->setValue(limits.value("processingMeanMaxMs").toDouble(250));
  m_timeMax->setValue(limits.value("processingMaxMs").toDouble(2000));
}

QJsonObject CampaignEditorDialog::campaign() const
{
  QJsonArray items;
  for (int row = 0; row < m_items->count(); ++row)
  {
    items.append(m_items->item(row)->data(Qt::UserRole).toJsonObject());
  }
  return {
    {"name", m_name->text().trimmed()},
    {"cycles", m_cycles->value()},
    {"parallel", m_parallel->isChecked()},
    {"items", items}
  };
}

void CampaignEditorDialog::syncResolutionCombo()
{
  if (!m_resolution || !m_canvasWidth || !m_canvasHeight)
  {
    return;
  }

  const QSize current(m_canvasWidth->value(), m_canvasHeight->value());
  for (int index = 0; index < m_resolution->count() - 1; ++index)
  {
    if (m_resolution->itemData(index).toSize() == current)
    {
      m_resolution->setCurrentIndex(index);
      return;
    }
  }

  m_resolution->setCurrentIndex(m_resolution->count() - 1);
}
