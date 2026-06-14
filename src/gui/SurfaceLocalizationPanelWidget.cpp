#include "SurfaceLocalizationPanelWidget.h"

#include "gui/TouchIconButton.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QPair>
#include <QSizePolicy>
#include <QSlider>
#include <QVBoxLayout>
#include <QVector>

SurfaceLocalizationPanelWidget::SurfaceLocalizationPanelWidget(
  const QString& titleText,
  const SurfaceAnnulusLocalizationConfig& annulus,
  const TranslationManager& translations,
  Handlers handlers,
  QWidget* parent)
  : QWidget(parent)
{
  auto trText = [&translations](const QString& key) {
    return translations.text(key);
  };

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  auto* title = new QLabel(titleText, this);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* drawingMode = new QWidget(this);
  auto* drawingLayout = new QHBoxLayout(drawingMode);
  drawingLayout->setContentsMargins(0, 0, 0, 0);
  drawingLayout->setSpacing(6);

  auto* centerRadiusButton = createTouchIconButton("circleGeometry", trText("labels.circleDrawingCenterRadius"), drawingMode);
  centerRadiusButton->setCheckable(true);
  centerRadiusButton->setChecked(true);
  auto* threePointButton = createTouchIconButton("arcGeometry", trText("labels.circleDrawingThreePoints"), drawingMode);
  threePointButton->setCheckable(true);

  connect(centerRadiusButton, &QPushButton::clicked, this, [centerRadiusButton, threePointButton]() {
    centerRadiusButton->setChecked(true);
    threePointButton->setChecked(false);
  });
  connect(threePointButton, &QPushButton::clicked, this, [centerRadiusButton, threePointButton, handlers]() {
    centerRadiusButton->setChecked(false);
    threePointButton->setChecked(true);
    if (handlers.drawThreePointCircle)
    {
      handlers.drawThreePointCircle();
    }
  });

  drawingLayout->addWidget(centerRadiusButton);
  drawingLayout->addWidget(threePointButton);
  layout->addWidget(drawingMode);

  auto* buttons = new QWidget(this);
  auto* buttonsLayout = new QGridLayout(buttons);
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setSpacing(6);

  struct Action
  {
    QString label;
    QString iconId;
    std::function<void()> handler;
  };
  const QVector<Action> actions = {
    {trText("actions.surfaceOuterCircle"), "surfaceOuterCircle", [handlers, threePointButton]() {
      if (threePointButton->isChecked())
      {
        if (handlers.drawThreePointCircle)
        {
          handlers.drawThreePointCircle();
        }
        return;
      }
      if (handlers.drawOuterCircle)
      {
        handlers.drawOuterCircle();
      }
    }},
    {trText("actions.surfaceInnerCircle"), "surfaceInnerCircle", [handlers, threePointButton]() {
      if (threePointButton->isChecked())
      {
        if (handlers.drawThreePointCircle)
        {
          handlers.drawThreePointCircle();
        }
        return;
      }
      if (handlers.drawInnerCircle)
      {
        handlers.drawInnerCircle();
      }
    }},
    {trText("actions.surfaceEdgeCircle"), "surfaceEdgeCircle", [handlers, threePointButton]() {
      if (threePointButton->isChecked())
      {
        if (handlers.drawThreePointCircle)
        {
          handlers.drawThreePointCircle();
        }
        return;
      }
      if (handlers.drawEdgeCircle)
      {
        handlers.drawEdgeCircle();
      }
    }},
    {trText("actions.surfaceAddExclusion"), "surfaceAddExclusion", [handlers]() {
      if (handlers.addExclusion)
      {
        handlers.addExclusion();
      }
    }},
    {trText("actions.surfaceClearExclusions"), "surfaceClearExclusions", [handlers]() {
      if (handlers.clearExclusions)
      {
        handlers.clearExclusions();
      }
    }},
    {trText("actions.clearLocalization"), "clear", [handlers]() {
      if (handlers.clearLocalization)
      {
        handlers.clearLocalization();
      }
    }}
  };

  for (int i = 0; i < actions.size(); ++i)
  {
    auto* button = createTouchIconButton(actions[i].iconId, actions[i].label, buttons);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(button, &QPushButton::clicked, this, actions[i].handler);
    buttonsLayout->addWidget(button, i / 3, i % 3);
  }

  layout->addWidget(buttons);
  layout->addSpacing(2);

  auto* methodBox = new QGroupBox(trText("labels.localizationMethod"), this);
  auto* methodLayout = new QGridLayout(methodBox);
  methodLayout->setContentsMargins(8, 10, 8, 8);
  methodLayout->setHorizontalSpacing(8);
  methodLayout->setVerticalSpacing(4);

  auto* methodCombo = new QComboBox(methodBox);
  const QString thresholdLabel = trText("labels.threshold");
  const QString edgeSensitivityLabel = trText("labels.edgeSensitivity");
  methodCombo->addItem(trText("labels.methodThreshold"), "threshold");
  methodCombo->addItem(trText("labels.methodEdge"), "edge");
  const int methodIndex = methodCombo->findData(annulus.method);
  methodCombo->setCurrentIndex(methodIndex >= 0 ? methodIndex : 0);
  methodLayout->addWidget(methodCombo, 0, 0, 1, 3);

  auto* parameterTitle = new QLabel(methodCombo->currentData().toString() == "edge" ? trText("labels.edgeSensitivity") : trText("labels.threshold"), methodBox);
  parameterTitle->setObjectName("toolPanelNote");

  auto* parameterLabel = new QLabel(QString::number(methodCombo->currentData().toString() == "edge" ? annulus.edgeSensitivity : annulus.thresholdMax), methodBox);
  parameterLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  parameterLabel->setObjectName("toolPanelNote");

  auto* parameterSlider = new QSlider(Qt::Horizontal, methodBox);
  parameterSlider->setRange(0, 255);
  parameterSlider->setValue(methodCombo->currentData().toString() == "edge" ? annulus.edgeSensitivity : annulus.thresholdMax);
  parameterSlider->setMinimumHeight(22);
  methodLayout->addWidget(parameterTitle, 1, 0, 1, 2);
  methodLayout->addWidget(parameterLabel, 1, 2);
  methodLayout->addWidget(parameterSlider, 2, 0, 1, 3);

  auto* edgeBandBox = new QWidget(methodBox);
  auto* edgeBandLayout = new QGridLayout(edgeBandBox);
  edgeBandLayout->setContentsMargins(0, 2, 0, 0);
  edgeBandLayout->setHorizontalSpacing(8);
  edgeBandLayout->setVerticalSpacing(2);

  auto* innerBandTitle = new QLabel(trText("labels.edgeBandInner"), edgeBandBox);
  auto* innerBandValue = new QLabel(QString::number(annulus.edgeBandInner), edgeBandBox);
  innerBandValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  auto* innerBandSlider = new QSlider(Qt::Horizontal, edgeBandBox);
  innerBandSlider->setRange(1, 200);
  innerBandSlider->setValue(annulus.edgeBandInner);

  auto* outerBandTitle = new QLabel(trText("labels.edgeBandOuter"), edgeBandBox);
  auto* outerBandValue = new QLabel(QString::number(annulus.edgeBandOuter), edgeBandBox);
  outerBandValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  auto* outerBandSlider = new QSlider(Qt::Horizontal, edgeBandBox);
  outerBandSlider->setRange(1, 200);
  outerBandSlider->setValue(annulus.edgeBandOuter);

  auto* fitErrorTitle = new QLabel(trText("labels.edgeFitMaxError"), edgeBandBox);
  auto* fitErrorValue = new QLabel(QString::number(annulus.edgeFitMaxError), edgeBandBox);
  fitErrorValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  auto* fitErrorSlider = new QSlider(Qt::Horizontal, edgeBandBox);
  fitErrorSlider->setRange(1, 50);
  fitErrorSlider->setValue(annulus.edgeFitMaxError);

  edgeBandLayout->addWidget(innerBandTitle, 0, 0);
  edgeBandLayout->addWidget(innerBandValue, 0, 1);
  edgeBandLayout->addWidget(innerBandSlider, 1, 0, 1, 2);
  edgeBandLayout->addWidget(outerBandTitle, 2, 0);
  edgeBandLayout->addWidget(outerBandValue, 2, 1);
  edgeBandLayout->addWidget(outerBandSlider, 3, 0, 1, 2);
  edgeBandLayout->addWidget(fitErrorTitle, 4, 0);
  edgeBandLayout->addWidget(fitErrorValue, 4, 1);
  edgeBandLayout->addWidget(fitErrorSlider, 5, 0, 1, 2);
  edgeBandBox->setVisible(methodCombo->currentData().toString() == "edge");
  methodLayout->addWidget(edgeBandBox, 3, 0, 1, 3);

  connect(methodCombo, &QComboBox::currentIndexChanged, this, [=](int) {
    const QString method = methodCombo->currentData().toString();
    const int value = method == "edge" ? annulus.edgeSensitivity : annulus.thresholdMax;
    parameterTitle->setText(method == "edge" ? edgeSensitivityLabel : thresholdLabel);
    parameterLabel->setText(QString::number(value));
    parameterSlider->blockSignals(true);
    parameterSlider->setValue(value);
    parameterSlider->blockSignals(false);
    edgeBandBox->setVisible(method == "edge");
    if (handlers.methodChanged)
    {
      handlers.methodChanged(method);
    }
  });
  connect(parameterSlider, &QSlider::valueChanged, this, [=](int value) {
    parameterLabel->setText(QString::number(value));
    if (methodCombo->currentData().toString() == "edge")
    {
      if (handlers.edgeSensitivityChanged)
      {
        handlers.edgeSensitivityChanged(value);
      }
      return;
    }

    if (handlers.thresholdChanged)
    {
      handlers.thresholdChanged(value);
    }
  });
  connect(innerBandSlider, &QSlider::valueChanged, this, [=](int value) {
    innerBandValue->setText(QString::number(value));
    if (handlers.edgeBandChanged)
    {
      handlers.edgeBandChanged(value, outerBandSlider->value());
    }
  });
  connect(outerBandSlider, &QSlider::valueChanged, this, [=](int value) {
    outerBandValue->setText(QString::number(value));
    if (handlers.edgeBandChanged)
    {
      handlers.edgeBandChanged(innerBandSlider->value(), value);
    }
  });
  connect(fitErrorSlider, &QSlider::valueChanged, this, [=](int value) {
    fitErrorValue->setText(QString::number(value));
    if (handlers.edgeFitMaxErrorChanged)
    {
      handlers.edgeFitMaxErrorChanged(value);
    }
  });
  layout->addWidget(methodBox);

  auto* backButton = createTouchIconButton("back", trText("commands.backToCameraTools"), this);
  connect(backButton, &QPushButton::clicked, this, [handlers]() {
    if (handlers.back)
    {
      handlers.back();
    }
  });
  layout->addWidget(backButton);
  layout->addStretch(1);
}
