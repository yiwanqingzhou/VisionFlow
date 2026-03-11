#include "modules/image_source.hpp"

#include <chrono>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <thread>

#include "core/type_system.hpp"
#include "utils/logger.hpp"

bool ImageSource::inner_execute(Blackboard& db) {
  LOG_INFO("[ImageSource] Generating synthetic moving frame...");

  // Simulate camera delay
  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  static int t = 0;
  t++;

  cv::Mat img(480, 640, CV_8UC3, cv::Scalar(40, 40, 40));
  int x = (int)(320 + 200 * std::sin(t * 0.1));
  int y = (int)(240 + 150 * std::cos(t * 0.08));
  cv::circle(img, cv::Point(x, y), 50, cv::Scalar(0, 255, 255), -1);
  cv::putText(img, "Synthetic OpenCV Source", cv::Point(20, 40),
              cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);

  // Use inherited BaseModule method to safely export topology-aware keys
  set_output(db, "Image", img);
  publish_visualization("Image", img);
  return true;
}

REGISTER_MODULE("ImageSource", ImageSource,
                (ModuleMetadata{
                    "ImageSource",
                    "Input",
                    {},                             // inputs
                    {{"Image", TypeSystem::IMAGE}}  // outputs
                }))
