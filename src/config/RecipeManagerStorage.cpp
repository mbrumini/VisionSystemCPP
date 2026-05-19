#include "RecipeManager.h"

#include "RecipeJsonUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace RecipeJsonUtils;

QString RecipeManager::cameraSampleImagesPath(const QString& cameraId) const
{
  return cameraImagesPath(cameraId, "sample");
}

QString RecipeManager::cameraTestImagesPath(const QString& cameraId) const
{
  return cameraImagesPath(cameraId, "test");
}

QString RecipeManager::firstCameraSampleImagePath(const QString& cameraId) const
{
  return firstImageInDirectory(cameraSampleImagesPath(cameraId));
}

QString RecipeManager::firstCameraTestImagePath(const QString& cameraId) const
{
  return firstImageInDirectory(cameraTestImagesPath(cameraId));
}

bool RecipeManager::ensureCameraImageFolders(const QString& cameraId, QString* errorMessage) const
{
  QDir directory;
  for (const QString& path : {cameraSampleImagesPath(cameraId), cameraTestImagesPath(cameraId)})
  {
    if (!directory.mkpath(path))
    {
      if (errorMessage)
      {
        *errorMessage = "Impossibile creare cartella immagini ricetta: " + path;
      }
      return false;
    }
  }

  return true;
}

bool RecipeManager::importCameraSampleImage(const QString& cameraId, const QString& sourceFilePath, QString* errorMessage) const
{
  if (!isSupportedImageFile(sourceFilePath))
  {
    if (errorMessage)
    {
      *errorMessage = "Immagine campione non valida: " + sourceFilePath;
    }
    return false;
  }

  const QString destinationDirectory = cameraSampleImagesPath(cameraId);
  if (!QDir().mkpath(destinationDirectory))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare cartella sample: " + destinationDirectory;
    }
    return false;
  }

  QDir sampleDir(destinationDirectory);
  const QFileInfo source(sourceFilePath);
  const QString destinationPath = sampleDir.filePath(source.fileName());
  const QString sourceAbsolutePath = source.absoluteFilePath();
  const QString destinationAbsolutePath = QFileInfo(destinationPath).absoluteFilePath();
  for (const QFileInfo& file : sampleDir.entryInfoList(imageNameFilters(), QDir::Files))
  {
    if (file.absoluteFilePath() == sourceAbsolutePath)
    {
      continue;
    }
    sampleDir.remove(file.fileName());
  }

  if (sourceAbsolutePath == destinationAbsolutePath)
  {
    return true;
  }

  if (!QFile::copy(sourceFilePath, destinationPath))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile copiare immagine campione: " + sourceFilePath;
    }
    return false;
  }

  return true;
}

bool RecipeManager::importCameraTestImages(const QString& cameraId, const QString& sourceDirectory, QString* errorMessage) const
{
  QDir source(sourceDirectory);
  if (!source.exists())
  {
    if (errorMessage)
    {
      *errorMessage = "Cartella immagini test non trovata: " + sourceDirectory;
    }
    return false;
  }

  const QString destinationDirectory = cameraTestImagesPath(cameraId);
  if (!QDir().mkpath(destinationDirectory))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare cartella test: " + destinationDirectory;
    }
    return false;
  }

  QDir destination(destinationDirectory);
  int copied = 0;
  for (const QFileInfo& file : source.entryInfoList(imageNameFilters(), QDir::Files, QDir::Name))
  {
    const QString destinationPath = destination.filePath(file.fileName());
    if (file.absoluteFilePath() == QFileInfo(destinationPath).absoluteFilePath())
    {
      copied += 1;
      continue;
    }
    if (QFile::exists(destinationPath))
    {
      QFile::remove(destinationPath);
    }
    if (QFile::copy(file.absoluteFilePath(), destinationPath))
    {
      copied += 1;
    }
  }

  if (copied == 0)
  {
    if (errorMessage)
    {
      *errorMessage = "Nessuna immagine test copiata da: " + sourceDirectory;
    }
    return false;
  }

  return true;
}

QString RecipeManager::cameraRecipePath(const QString& cameraId) const
{
  return QDir(recipesRoot()).filePath(m_recipeId + "/cameras/" + cameraId + ".json");
}

QString RecipeManager::cameraImagesPath(const QString& cameraId, const QString& kind) const
{
  return QDir(recipesRoot()).filePath(m_recipeId + "/images/" + cameraId + "/" + kind);
}

bool RecipeManager::copyDirectory(const QString& sourceDirectory, const QString& destinationDirectory, QString* errorMessage)
{
  QDir source(sourceDirectory);

  if (!source.exists())
  {
    if (errorMessage)
    {
      *errorMessage = "Directory sorgente non trovata: " + sourceDirectory;
    }

    return false;
  }

  QDir destination;

  if (!destination.mkpath(destinationDirectory))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare directory destinazione: " + destinationDirectory;
    }

    return false;
  }

  const QFileInfoList entries = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

  for (const QFileInfo& entry : entries)
  {
    const QString targetPath = QDir(destinationDirectory).filePath(entry.fileName());

    if (entry.isDir())
    {
      if (!copyDirectory(entry.absoluteFilePath(), targetPath, errorMessage))
      {
        return false;
      }
    }
    else if (!QFile::copy(entry.absoluteFilePath(), targetPath))
    {
      if (errorMessage)
      {
        *errorMessage = "Impossibile copiare file: " + entry.absoluteFilePath();
      }

      return false;
    }
  }

  return true;
}
