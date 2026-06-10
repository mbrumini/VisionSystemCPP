#include "FileCamera.h"

#include <opencv2/imgcodecs.hpp>
#include <algorithm>

FileCamera::FileCamera(const std::string& folderPath, bool loop)
  : m_folderPath(folderPath), m_loop(loop)
{
}

bool FileCamera::open()
{
  m_images.clear();
  m_currentIndex = 0;

  if (!std::filesystem::exists(m_folderPath))
  {
    return false;
  }

  for (const auto& entry : std::filesystem::directory_iterator(m_folderPath))
  {
    if (entry.is_regular_file() && isImageFile(entry.path()))
    {
      m_images.push_back(entry.path());
    }
  }

  std::sort(m_images.begin(), m_images.end());

  if (m_images.empty())
  {
    return false;
  }

  return true;
}

bool FileCamera::getFrame(cv::Mat& frame)
{
  if (m_images.empty())
  {
    return false;
  }

  if (m_currentIndex >= m_images.size())
  {
    if (!m_loop)
    {
      return false;
    }

    m_currentIndex = 0;
  }

  const auto& imagePath = m_images[m_currentIndex];

  frame = cv::imread(imagePath.string(), cv::IMREAD_COLOR);

  if (frame.empty())
  {
    return false;
  }

  m_currentIndex++;
  return true;
}

void FileCamera::close()
{
  m_images.clear();
  m_currentIndex = 0;
}

bool FileCamera::isImageFile(const std::filesystem::path& path) const
{
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  return ext == ".png" ||
    ext == ".jpg" ||
    ext == ".jpeg" ||
    ext == ".bmp" ||
    ext == ".tif" ||
    ext == ".tiff";
}
