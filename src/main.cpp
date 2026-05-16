#include "camera/FileCamera.h"
#include "processing/ImageProcessor.h"
#include "utils/Timer.h"

#include <opencv2/opencv.hpp>
#include <iomanip>
#include <iostream>
#include <string>

int main()
{
  std::cout << "OpenCV version: " << CV_VERSION << std::endl;

  std::string imageFolder = std::string(PROJECT_SOURCE_DIR) + "/data/images";

  FileCamera camera(imageFolder, true);

  if (!camera.open())
  {
    std::cerr << "Errore apertura FileCamera" << std::endl;
    return 1;
  }

  cv::Mat frame;
  cv::Mat processedFrame;
  ImageProcessor processor;

  while (true)
  {
    Timer cycleTimer;

    if (!camera.getFrame(frame))
    {
      std::cerr << "Errore lettura frame" << std::endl;
      break;
    }

    if (!processor.process(frame, processedFrame))
    {
      std::cerr << "Errore processing frame" << std::endl;
      break;
    }

    cv::imshow("FileCamera - simulazione camera", processedFrame);

    const double cycleMs = cycleTimer.elapsedMilliseconds();
    std::cout << "Tempo ciclo: " << std::fixed << std::setprecision(3)
              << cycleMs << " ms" << std::endl;

    int key = cv::waitKey(0);

    if (key == 27) // ESC
    {
      break;
    }
  }

  camera.close();
  cv::destroyAllWindows();

  return 0;
}
