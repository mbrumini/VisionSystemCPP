#include "gui/MainWindow.h"

#include "gui/IconCatalog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>

void MainWindow::buildMenu()
{
  menuBar()->clear();

  QMenu* recipesMenu = menuBar()->addMenu(trText("menu.recipes"));
  recipesMenu->addAction(trText("menu.selectRecipe"), this, [this]() { m_recipes.selectRecipe(); });
  recipesMenu->addAction(trText("menu.newRecipe"), this, [this]() { m_recipes.createRecipe(); });
  recipesMenu->addAction(trText("menu.duplicateRecipe"), this, [this]() { m_recipes.duplicateRecipe(); });
  recipesMenu->addAction(trText("menu.deleteRecipe"), this, [this]() { m_recipes.deleteRecipe(); });
  recipesMenu->addAction(trText("menu.importRecipe"), this, [this]() { m_recipes.importRecipe(); });
  recipesMenu->addAction(trText("menu.exportRecipe"), this, [this]() { m_recipes.exportRecipe(); });

  QMenu* accessMenu = menuBar()->addMenu(trText("menu.access"));
  accessMenu->addAction(trText("access.loginWithPassword"), this, [this]() { showAccessLogin(); });
  accessMenu->addAction(trText("access.manageAccess"), this, [this]() { showAccessManagement(); });
  if (m_accessSession.role() == AccessRole::Guru)
  {
    accessMenu->addSeparator();
    QSettings settings;
    QAction* startAsGuruAction = accessMenu->addAction(trText("access.startAsGuru"));
    startAsGuruAction->setCheckable(true);
    startAsGuruAction->setChecked(settings.value("access/startAsGuru", true).toBool());
    connect(startAsGuruAction, &QAction::toggled, this, [this](bool enabled) {
      QSettings settings;
      settings.setValue("access/startAsGuru", enabled);
      appendLog(enabled ? trText("access.startAsGuruEnabled") : trText("access.startAsGuruDisabled"));
    });
  }

  QMenu* configMenu = menuBar()->addMenu(trText("menu.configurations"));
  configMenu->addAction(trText("menu.paths"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.paths"));
  });
  configMenu->addAction(trText("menu.diagnostics"), this, [this]() {
    appendLog(trText("log.placeholder") + ": " + trText("menu.diagnostics"));
  });

  QMenu* languageMenu = menuBar()->addMenu(trText("menu.language"));
  languageMenu->addAction("Italiano", this, [this]() { changeLanguage("it"); });
  languageMenu->addAction("English", this, [this]() { changeLanguage("en"); });

  QMenu* helpMenu = menuBar()->addMenu(trText("menu.help"));
  helpMenu->addAction(trText("commands.help"), this, [this]() { showHelp(); });

  QMenu* systemMenu = menuBar()->addMenu(trText("menu.system"));
  systemMenu->addAction(trText("commands.start"), this, [this]() {
    startMachine();
  });
  systemMenu->addAction(trText("commands.stop"), this, [this]() {
    stopMachine();
  });
  systemMenu->addAction(trText("commands.gridView"), this, [this]() { showGridView(); });

  QMenu* systemCamerasMenu = systemMenu->addMenu(trText("menu.cameras"));
  auto defaultCameraSlot = [this]() {
    return m_selectedCameraId.isEmpty() ? 1 : qMax(1, m_selectedCamera.slot);
  };
  systemCamerasMenu->addAction(trText("menu.configureCameras"), this, [this]() { showCameraSystemSettings(); });
  systemCamerasMenu->addAction(trText("menu.calibrateCheckerboard"), this, [this]() { showCheckerboardCalibrationDialog(); });
  systemCamerasMenu->addSeparator();
  systemCamerasMenu->addAction(trText("actions.assignVimbaCamera"), this, [this, defaultCameraSlot]() {
    m_cameraConfig.configureVimbaCameraSlot(defaultCameraSlot(), m_selectedCameraId);
  });
  systemCamerasMenu->addAction(trText("actions.assignUsbCamera"), this, [this, defaultCameraSlot]() {
    m_cameraConfig.configureUsbCameraSlot(defaultCameraSlot(), m_selectedCameraId);
  });
  systemCamerasMenu->addSeparator();
  for (const CameraConfig& camera : m_config.activeCameras())
  {
    systemCamerasMenu->addAction(QString("%1 | %2").arg(camera.id, camera.displayName), this, [this, camera]() {
      m_cameraConfig.configureCameraSource(camera);
    });
  }
  systemMenu->addAction(trText("commands.reloadConfig"), this, [this]() { loadConfiguration(); });
  systemMenu->addAction(trText("commands.toggleFullScreen"), this, [this]() { toggleFullScreen(); });
  systemMenu->addAction(trText("commands.setMaxThreads"), this, [this]() { setThreadLimitPrompt(); });
  QAction* setupDetailsAction = systemMenu->addAction(trText("commands.showSetupDetails"));
  setupDetailsAction->setCheckable(true);
  setupDetailsAction->setChecked(m_setupDetailsVisible);
  connect(setupDetailsAction, &QAction::toggled, this, [this](bool checked) {
    setSetupDetailsVisible(checked);
  });
  QAction* detailedLogAction = systemMenu->addAction(trText("commands.enableDetailedLog"));
  detailedLogAction->setCheckable(true);
  detailedLogAction->setChecked(m_detailedLogger.enabled());
  connect(detailedLogAction, &QAction::toggled, this, [this](bool checked) {
    setDetailedLogEnabled(checked);
  });
  QMenu* iconSetMenu = systemMenu->addMenu("Serie icone");
  QActionGroup* iconSetGroup = new QActionGroup(iconSetMenu);
  iconSetGroup->setExclusive(true);
  const QString currentIconSet = IconCatalog::iconSet();
  for (const QString& iconSetId : IconCatalog::iconSetIds())
  {
    QAction* action = iconSetMenu->addAction(IconCatalog::iconSetLabel(iconSetId));
    action->setCheckable(true);
    action->setData(iconSetId);
    action->setChecked(currentIconSet == iconSetId);
    iconSetGroup->addAction(action);
  }
  connect(iconSetGroup, &QActionGroup::triggered, this, [this](QAction* action) {
    const QString iconSetId = action->data().toString();
    IconCatalog::setIconSet(iconSetId);
    appendLog(QString("Serie icone salvata: %1. Riavvia l'app per applicarla.")
      .arg(IconCatalog::iconSetLabel(iconSetId)), true);
  });
  QMenu* simulatorResultsMenu = systemMenu->addMenu("Risultati simulatore");
  QActionGroup* simulatorResultsGroup = new QActionGroup(simulatorResultsMenu);
  simulatorResultsGroup->setExclusive(true);
  QSettings simulatorSettings;
  const QString simulatorResultMode =
    simulatorSettings.value("simulator/resultMode", "full").toString();
  const auto addSimulatorResultMode = [&](const QString& label, const QString& mode) {
    QAction* action = simulatorResultsMenu->addAction(label);
    action->setCheckable(true);
    action->setData(mode);
    action->setChecked(simulatorResultMode == mode);
    simulatorResultsGroup->addAction(action);
    return action;
  };
  addSimulatorResultMode("Invia risultati completi", "full");
  addSimulatorResultMode("Invia risultati leggeri", "summary");
  addSimulatorResultMode("Non inviare risultati", "none");
  connect(simulatorResultsGroup, &QActionGroup::triggered, this, [this](QAction* action) {
    QSettings settings;
    const QString mode = action->data().toString();
    settings.setValue("simulator/resultMode", mode);
    appendLog(QString("Risultati simulatore: %1").arg(action->text()), true);
  });
  systemMenu->addSeparator();
  systemMenu->addAction(trText("commands.exit"), qApp, &QApplication::quit);
}

