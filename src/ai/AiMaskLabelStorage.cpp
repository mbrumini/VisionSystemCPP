#include "ai/AiMaskLabelStorage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSize>
#include <QTextStream>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

QString AiMaskLabelStorage::yoloSegmentationLine(
  const QVector<QPoint>& polygon,
  const QSize& imageSize,
  int classId)
{
  if (polygon.size() < 3 || imageSize.width() <= 0 || imageSize.height() <= 0)
  {
    return {};
  }

  QStringList values;
  values.reserve(1 + polygon.size() * 2);
  values.append(QString::number(qMax(0, classId)));
  for (const QPoint& point : polygon)
  {
    const double x = qBound(0.0, point.x() / static_cast<double>(imageSize.width()), 1.0);
    const double y = qBound(0.0, point.y() / static_cast<double>(imageSize.height()), 1.0);
    values.append(QString::number(x, 'f', 6));
    values.append(QString::number(y, 'f', 6));
  }
  return values.join(' ');
}

bool AiMaskLabelStorage::savePolygon(
  const QString& sourceImagePath,
  const QString& masksDirectory,
  const QString& labelsDirectory,
  const QVector<QPoint>& polygon,
  AiMaskLabelPaths* savedPaths,
  QString* errorMessage)
{
  return savePolygons(
    sourceImagePath,
    masksDirectory,
    labelsDirectory,
    {{0, polygon}},
    savedPaths,
    errorMessage);
}

bool AiMaskLabelStorage::savePolygons(
  const QString& sourceImagePath,
  const QString& masksDirectory,
  const QString& labelsDirectory,
  const QVector<AiSegmentationPolygon>& polygons,
  AiMaskLabelPaths* savedPaths,
  QString* errorMessage)
{
  const QImage source(sourceImagePath);
  if (source.isNull())
  {
    if (errorMessage)
    {
      *errorMessage = "Immagine raw non valida: " + sourceImagePath;
    }
    return false;
  }
  bool hasPiece = false;
  for (const AiSegmentationPolygon& polygon : polygons)
  {
    if (polygon.classId == 0 && polygon.points.size() >= 3)
    {
      hasPiece = true;
      break;
    }
  }
  if (!hasPiece)
  {
    if (errorMessage)
    {
      *errorMessage = "La maschera Pezzo richiede almeno 3 punti.";
    }
    return false;
  }
  if (!QDir().mkpath(masksDirectory) || !QDir().mkpath(labelsDirectory))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile creare le cartelle mask/label.";
    }
    return false;
  }

  const QString stem = QFileInfo(sourceImagePath).completeBaseName();
  const QString maskPath = QDir(masksDirectory).filePath(stem + ".png");
  const QString labelPath = QDir(labelsDirectory).filePath(stem + ".txt");

  cv::Mat mask = cv::Mat::zeros(source.height(), source.width(), CV_8UC1);
  for (const AiSegmentationPolygon& polygon : polygons)
  {
    if (polygon.points.size() < 3)
    {
      continue;
    }
    std::vector<cv::Point> cvPolygon;
    cvPolygon.reserve(static_cast<size_t>(polygon.points.size()));
    for (const QPoint& point : polygon.points)
    {
      cvPolygon.emplace_back(
        qBound(0, point.x(), source.width() - 1),
        qBound(0, point.y(), source.height() - 1));
    }
    const int value = polygon.classId == 0 ? 255 : 128;
    cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{cvPolygon}, cv::Scalar(value));
  }
  if (!cv::imwrite(maskPath.toStdString(), mask))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile salvare la maschera: " + maskPath;
    }
    return false;
  }

  QFile labelFile(labelPath);
  if (!labelFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile salvare la label: " + labelPath;
    }
    return false;
  }
  QTextStream stream(&labelFile);
  for (const AiSegmentationPolygon& polygon : polygons)
  {
    const QString line = yoloSegmentationLine(
      polygon.points, source.size(), polygon.classId);
    if (!line.isEmpty())
    {
      stream << line << '\n';
    }
  }

  if (savedPaths)
  {
    savedPaths->maskPath = maskPath;
    savedPaths->labelPath = labelPath;
  }
  return true;
}

QVector<QPoint> AiMaskLabelStorage::loadPolygon(
  const QString& sourceImagePath,
  const QString& labelsDirectory,
  QString* errorMessage)
{
  const QVector<AiSegmentationPolygon> polygons =
    loadPolygons(sourceImagePath, labelsDirectory, errorMessage);
  for (const AiSegmentationPolygon& polygon : polygons)
  {
    if (polygon.classId == 0)
    {
      return polygon.points;
    }
  }
  return {};
}

QVector<AiSegmentationPolygon> AiMaskLabelStorage::loadPolygons(
  const QString& sourceImagePath,
  const QString& labelsDirectory,
  QString* errorMessage)
{
  const QImage source(sourceImagePath);
  if (source.isNull())
  {
    return {};
  }

  const QString labelPath = QDir(labelsDirectory).filePath(
    QFileInfo(sourceImagePath).completeBaseName() + ".txt");
  QFile labelFile(labelPath);
  if (!labelFile.exists())
  {
    return {};
  }
  if (!labelFile.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile leggere la label: " + labelPath;
    }
    return {};
  }

  QVector<AiSegmentationPolygon> polygons;
  while (!labelFile.atEnd())
  {
    const QString text = QString::fromUtf8(labelFile.readLine()).simplified();
    if (text.isEmpty())
    {
      continue;
    }
    const QStringList values = text.split(' ');
    bool classOk = false;
    const int classId = values.value(0).toInt(&classOk);
    if (!classOk || values.size() < 7 || (values.size() - 1) % 2 != 0)
    {
      if (errorMessage)
      {
        *errorMessage = "Label YOLO segmentation non valida: " + labelPath;
      }
      return {};
    }
    AiSegmentationPolygon polygon;
    polygon.classId = qMax(0, classId);
    for (int i = 1; i + 1 < values.size(); i += 2)
    {
      bool xOk = false;
      bool yOk = false;
      const double x = values[i].toDouble(&xOk);
      const double y = values[i + 1].toDouble(&yOk);
      if (!xOk || !yOk)
      {
        if (errorMessage)
        {
          *errorMessage = "Coordinate label non valide: " + labelPath;
        }
        return {};
      }
      polygon.points.append({
        qBound(0, qRound(x * source.width()), source.width() - 1),
        qBound(0, qRound(y * source.height()), source.height() - 1)
      });
    }
    polygons.append(polygon);
  }
  return polygons;
}
