#include "gui/MainWindow.h"

#include "gui/help/HelpDialog.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

namespace
{
QStringList normalizedHelpList(const QStringList& values)
{
  QStringList result;
  for (const QString& value : values)
  {
    result.push_back(value.toLower());
  }
  return result;
}

QString helpCameraRoleSummary(const CameraConfig& camera)
{
  const QString imageMode = camera.profile.imageMode.toLower();
  const QStringList inspections = normalizedHelpList(camera.profile.inspectionTypes);
  const QStringList tools = normalizedHelpList(camera.profile.guiTools);

  QStringList roles;
  if (imageMode == QStringLiteral("bw") || inspections.contains(QStringLiteral("dimensional")))
  {
    roles.push_back(QStringLiteral("adatta a controlli BN/profilo esterno, edge dimensionali e quote in silhouette/profilo"));
    roles.push_back(QStringLiteral("non considerarla automaticamente adatta a dettagli sulla faccia del pezzo"));
  }
  if (imageMode == QStringLiteral("grayscale") &&
      (inspections.contains(QStringLiteral("surface")) ||
       inspections.contains(QStringLiteral("measurement")) ||
       tools.contains(QStringLiteral("surfacelocalization"))))
  {
    roles.push_back(QStringLiteral("candidata per misure superficiali in grigi e riferimenti sulla faccia del pezzo"));
  }
  if (inspections.contains(QStringLiteral("ai")) || tools.contains(QStringLiteral("aimodel")))
  {
    roles.push_back(QStringLiteral("candidata per controlli AI se il modello e il dataset sono configurati"));
  }
  if (tools.contains(QStringLiteral("surfacedefects")))
  {
    roles.push_back(QStringLiteral("candidata per difetti superficie"));
  }
  if (roles.isEmpty())
  {
    roles.push_back(QStringLiteral("capacita' specifiche non deducibili dal profilo"));
  }
  return roles.join(QStringLiteral("; "));
}
}

void MainWindow::showHelp()
{
  const QString language = m_translations.languageCode().isEmpty() ? QStringLiteral("it") : m_translations.languageCode();
  const QString helpPath = QDir(QString::fromUtf8(PROJECT_SOURCE_DIR)).filePath(QString("docs/help/%1/index.txt").arg(language));
  QFile file(helpPath);
  QString text;
  if (file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QTextStream stream(&file);
    text = stream.readAll();
  }
  else
  {
    text = trText("messages.helpPlaceholder");
  }

  HelpDialog::Labels labels;
  labels.manualTab = trText("help.manualTab");
  labels.chatTab = trText("help.chatTab");
  labels.questionPlaceholder = trText("help.questionPlaceholder");
  labels.askButton = trText("help.askButton");
  labels.waiting = trText("help.waiting");
  labels.assistantUnavailable = trText("help.assistantUnavailable");

  auto machineContextProvider = [this]() {
    QStringList lines;
    lines.push_back(QStringLiteral("Configurazione letta live dall'app al momento della domanda."));
    lines.push_back(QStringLiteral("Nota: la configurazione dichiara profilo, imageMode e tool software; non dichiara ancora l'hardware illuminatore fisico. Non inventare illuminatori non dichiarati."));
    if (!m_selectedCameraId.isEmpty())
    {
      lines.push_back(QStringLiteral("Camera selezionata: %1").arg(m_selectedCameraId));
    }
    else
    {
      lines.push_back(QStringLiteral("Camera selezionata: nessuna."));
    }

    const QVector<CameraConfig> cameras = m_config.activeCameras();
    if (cameras.isEmpty())
    {
      lines.push_back(QStringLiteral("Nessuna camera attiva nella configurazione corrente."));
      return lines.join('\n');
    }

    lines.push_back(QStringLiteral("Camere attive e capacita' dedotte:"));
    for (const CameraConfig& camera : cameras)
    {
      lines.push_back(QStringLiteral("- %1 slot %2, nome %3, profilo %4, imageMode %5, controlli %6, tool %7. Uso dedotto: %8.")
                        .arg(camera.id)
                        .arg(camera.slot)
                        .arg(camera.displayName)
                        .arg(camera.processingProfileId)
                        .arg(camera.profile.imageMode)
                        .arg(camera.profile.inspectionTypes.join(QStringLiteral(",")))
                        .arg(camera.profile.guiTools.join(QStringLiteral(",")))
                        .arg(helpCameraRoleSummary(camera)));
    }

    lines.push_back(QStringLiteral("Regola risposta: per richieste su misure superficiali, faccia del pezzo, fori ciechi, sedi, tasche o incisioni indica prima le camere candidate con profilo grayscale/surface/measurement. Non proporre camere BN dimensionali come CAM01/CAM02 per dettagli sulla superficie. Per profili esterni BN, silhouette o fori passanti visti in silhouette indica camere dimensionali. Se serve un illuminatore specifico non dichiarato, chiedi di verificarlo sulla macchina."));
    return lines.join('\n');
  };

  auto* dialog = new HelpDialog(text, language, labels, machineContextProvider, this);
  dialog->setWindowTitle(trText("commands.help"));
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
}
