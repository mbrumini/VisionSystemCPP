#include "gui/modules/MainWindowThreadModule.h"

#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/TouchIconButton.h"
#include "processing/thread/ThreadInspectionUtils.h"
#include "processing/thread/ThreadProfileExtractor.h"
#include "runtime/PartPose.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

MainWindowThreadModule::MainWindowThreadModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

void MainWindowThreadModule::syncExtractionRoiOverlay(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid)
  {
    QString migrateError;
    if (recipes().migrateThreadInspectionRoiToPartSpace(camera.id, pose, &migrateError))
    {
      log(tr("log.threadRoiMigratedToPart"));
    }
    else if (!migrateError.isEmpty())
    {
      log(migrateError);
    }
  }

  ThreadInspectionSettings settings = recipes().loadThreadInspectionSettings(camera.id);
  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  const QSize imageSize = input.empty()
    ? QSize(selectedPreview().width(), selectedPreview().height())
    : QSize(input.cols, input.rows);
  const QRect imageRoi = effectiveThreadExtractionImageRect(settings, pose, imageSize);

  if (imageRoi.isValid())
  {
    largeImage()->setRoi(imageRoi);
  }
  else
  {
    largeImage()->clearRoi();
  }
}

void MainWindowThreadModule::activateThreadExtractionRoiDrawing(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (!pose.valid && context().surface)
  {
    context().surface->localizePoseOnSample(camera);
  }
  if (!cameraRuntime()[camera.id].currentPose().valid)
  {
    log(tr("log.threadRoiNeedsPose") + ": " + camera.id);
    return;
  }

  context().imaging->ensureReferenceImageVisible(camera);
  syncExtractionRoiOverlay(camera);
  largeImage()->setRoiDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::ThreadInspection;
  log(tr("log.threadRoiDrawing") + ": " + camera.id);
}

void MainWindowThreadModule::clearThreadExtractionRoi(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  QString error;
  if (!recipes().clearThreadInspectionExtractionRoi(camera.id, &error))
  {
    log(error);
    return;
  }

  context().deactivateImageDrawingTools();
  largeImage()->clearRoi();
  context().imaging->ensureReferenceImageVisible(camera);
  syncExtractionRoiOverlay(camera);
  log(tr("log.threadRoiCleared") + ": " + camera.id);
  activateThreadExtractionRoiDrawing(camera);
}

void MainWindowThreadModule::testThreadExtraction(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  const ThreadInspectionSettings settings = recipes().loadThreadInspectionSettings(camera.id);
  if (!settings.hasExtractionRoi)
  {
    log(tr("log.threadRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError.isEmpty() ? tr("log.threadExtractionFailed") : imageError);
    return;
  }

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  const QRect imageRoi = effectiveThreadExtractionImageRect(settings, pose, QSize(input.cols, input.rows));
  if (!imageRoi.isValid())
  {
    log(tr("log.threadRoiMissing") + ": " + camera.id);
    return;
  }

  const cv::Rect cvRoi(imageRoi.x(), imageRoi.y(), imageRoi.width(), imageRoi.height());
  const ThreadProfileResult result = ThreadProfileExtractor().extract(input, cvRoi, settings);
  if (!result.valid)
  {
    log(tr("log.threadExtractionFailed") + ": " + camera.id + " (" + result.error + ")");
    if (!result.diagnosticImage.empty())
    {
      selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
      largeImage()->setImage(selectedPreview());
      syncExtractionRoiOverlay(camera);
    }
    return;
  }

  selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
  largeImage()->setImage(selectedPreview());
  syncExtractionRoiOverlay(camera);
  log(QString("%1: %2 cols=%3/%4")
        .arg(tr("log.threadExtractionOk"))
        .arg(camera.id)
        .arg(result.validColumns)
        .arg(result.columns.size()));
}

void MainWindowThreadModule::showThreadInspectionPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  const ThreadInspectionSettings settings = recipes().loadThreadInspectionSettings(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  context().imaging->ensureReferenceImageVisible(camera);

  auto* panel = new QWidget(toolsContainer());
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(QString("%1 | %2").arg(tr("tools.threadInspection"), camera.id), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* note = new QLabel(tr("labels.threadInspectionNote"), panel);
  note->setObjectName("toolPanelNote");
  note->setWordWrap(true);
  layout->addWidget(note);

  auto* poseLabel = new QLabel(pose.valid ? tr("labels.partPose") : tr("log.partPoseMissing"), panel);
  poseLabel->setObjectName("toolPanelNote");
  poseLabel->setWordWrap(true);
  layout->addWidget(poseLabel);

  auto* buttons = new QWidget(panel);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setSpacing(6);

  auto* roiButton = createTouchIconButton("threadExtractionRoi", tr("actions.threadExtractionRoi"), buttons);
  auto* clearRoiButton = createTouchIconButton("clear", tr("actions.clearThreadRoi"), buttons);
  auto* testButton = createTouchIconButton("testThreadExtraction", tr("actions.testThreadExtraction"), buttons);
  QObject::connect(roiButton, &QPushButton::clicked, window(), [this, camera]() {
    activateThreadExtractionRoiDrawing(camera);
  });
  QObject::connect(clearRoiButton, &QPushButton::clicked, window(), [this, camera]() {
    clearThreadExtractionRoi(camera);
  });
  QObject::connect(testButton, &QPushButton::clicked, window(), [this, camera]() {
    testThreadExtraction(camera);
  });
  buttonsLayout->addWidget(roiButton, 0, 0, 1, 2);
  buttonsLayout->addWidget(clearRoiButton, 1, 0);
  buttonsLayout->addWidget(testButton, 1, 1);
  layout->addWidget(buttons);

  auto* filterBox = new QGroupBox(tr("labels.threadDirtFilter"), panel);
  auto* filterLayout = new QGridLayout(filterBox);
  filterLayout->setContentsMargins(8, 10, 8, 8);

  auto working = std::make_shared<ThreadInspectionSettings>(settings);
  auto filterTimer = new QTimer(panel);
  filterTimer->setSingleShot(true);
  filterTimer->setInterval(180);

  auto persistFilter = [this, camera, working]() {
    if (working->thresholdMin > working->thresholdMax)
    {
      std::swap(working->thresholdMin, working->thresholdMax);
    }

    QString error;
    if (!recipes().saveThreadInspectionSettings(camera.id, *working, &error))
    {
      log(error);
      return;
    }

    if (recipes().loadThreadInspectionSettings(camera.id).hasExtractionRoi)
    {
      testThreadExtraction(camera);
    }
  };

  QObject::connect(filterTimer, &QTimer::timeout, window(), [persistFilter]() {
    persistFilter();
  });

  auto queueFilterSave = [filterTimer]() {
    filterTimer->start();
  };

  auto addSlider = [&](int row,
                       const QString& labelText,
                       int minValue,
                       int maxValue,
                       int value,
                       const std::function<void(int)>& applyValue) {
    auto* label = new QLabel(labelText, filterBox);
    auto* valueLabel = new QLabel(QString::number(value), filterBox);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* slider = new QSlider(Qt::Horizontal, filterBox);
    slider->setRange(minValue, maxValue);
    slider->setValue(value);
    slider->setTracking(true);
    QObject::connect(slider, &QSlider::valueChanged, panel, [valueLabel, applyValue, queueFilterSave](int newValue) {
      valueLabel->setText(QString::number(newValue));
      applyValue(newValue);
      queueFilterSave();
    });
    filterLayout->addWidget(label, row, 0);
    filterLayout->addWidget(valueLabel, row, 1);
    filterLayout->addWidget(slider, row + 1, 0, 1, 2);
  };

  addSlider(0, tr("labels.thresholdMin"), 0, 255, working->thresholdMin, [working](int value) {
    working->thresholdMin = value;
  });
  addSlider(2, tr("labels.thresholdMax"), 0, 255, working->thresholdMax, [working](int value) {
    working->thresholdMax = value;
  });
  addSlider(4, tr("labels.threadMorphOpenRadius"), 0, 15, working->morphOpenRadius, [working](int value) {
    working->morphOpenRadius = value;
  });
  addSlider(6, tr("labels.threadMinSpeckArea"), 0, 200, working->minSpeckAreaPx, [working](int value) {
    working->minSpeckAreaPx = value;
  });
  addSlider(8, tr("labels.threadProfileSmoothRadius"), 0, 15, working->profileSmoothRadius, [working](int value) {
    working->profileSmoothRadius = value;
  });

  auto* outlierLabel = new QLabel(tr("labels.threadOutlierSigma"), filterBox);
  auto* outlierSpin = new QDoubleSpinBox(filterBox);
  outlierSpin->setRange(0.0, 8.0);
  outlierSpin->setSingleStep(0.1);
  outlierSpin->setDecimals(1);
  outlierSpin->setValue(working->outlierRejectSigma);
  QObject::connect(outlierSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), panel, [working, queueFilterSave](double value) {
    working->outlierRejectSigma = value;
    queueFilterSave();
  });
  filterLayout->addWidget(outlierLabel, 10, 0);
  filterLayout->addWidget(outlierSpin, 10, 1);

  layout->addWidget(filterBox);

  auto* backButton = createTouchIconButton("back", tr("commands.backToCameraTools"), panel);
  QObject::connect(backButton, &QPushButton::clicked, window(), [this, camera]() {
    context().showCameraToolList(camera);
    log(tr("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tr("tools.threadInspection"));
  syncExtractionRoiOverlay(camera);
}
