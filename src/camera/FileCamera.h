#pragma once

#include "ICamera.h"

#include <filesystem>
#include <string>
#include <vector>

class FileCamera : public ICamera
{
public:
  explicit FileCamera(const std::string& folderPath, bool loop = true);

  bool open() override;
  bool getFrame(cv::Mat& frame) override;
  void close() override;

private:
  bool isImageFile(const std::filesystem::path& path) const;

  std::string m_folderPath;
  bool m_loop;
  size_t m_currentIndex = 0;
  std::vector<std::filesystem::path> m_images;
};
