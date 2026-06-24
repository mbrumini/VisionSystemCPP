#include "simulator/SimulatorProtocol.h"

#include <opencv2/imgcodecs.hpp>

namespace SimulatorProtocol
{
QString encodeImageBase64(const cv::Mat& image)
{
  if (image.empty())
  {
    return {};
  }

  std::vector<uchar> bytes;
  if (!cv::imencode(".bmp", image, bytes))
  {
    return {};
  }

  return QString::fromLatin1(
    QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()))
      .toBase64());
}

DecodeImageResult decodeImageFromBase64(const QByteArray& encodedImage)
{
  DecodeImageResult result;
  if (encodedImage.isEmpty())
  {
    result.errorCode = QStringLiteral("invalid_image");
    result.errorMessage = QStringLiteral("Payload immagine mancante");
    return result;
  }

  const QByteArray imageBytes = QByteArray::fromBase64(encodedImage);
  if (imageBytes.isEmpty())
  {
    result.errorCode = QStringLiteral("invalid_image");
    result.errorMessage = QStringLiteral("Base64 immagine non valido");
    return result;
  }

  if (imageBytes.size() > kMaximumImagePayloadBytes)
  {
    result.errorCode = QStringLiteral("image_too_large");
    result.errorMessage = QStringLiteral("Payload immagine oltre il limite di 12 MB");
    return result;
  }

  const std::vector<uchar> bytes(imageBytes.begin(), imageBytes.end());
  result.image = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
  if (result.image.empty())
  {
    result.errorCode = QStringLiteral("invalid_image");
    result.errorMessage = QStringLiteral("Immagine non decodificabile");
    return result;
  }

  const qint64 decodedBytes = static_cast<qint64>(result.image.total() * result.image.elemSize());
  if (decodedBytes > kMaximumDecodedImageBytes)
  {
    result.image.release();
    result.errorCode = QStringLiteral("image_too_large");
    result.errorMessage = QStringLiteral("Immagine decodificata oltre il limite di 12 MB");
    return result;
  }

  result.ok = true;
  return result;
}
}
