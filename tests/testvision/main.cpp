#include "TestVisionWindow.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

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
      campaignPath = QFileInfo(QString::fromLocal8Bit(argv[++index])).absoluteFilePath();
    }
    else if (argument == "--broadcast" && index + 1 < argc)
    {
      broadcastCameras = QString::fromLocal8Bit(argv[++index]).split(',', Qt::SkipEmptyParts);
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
