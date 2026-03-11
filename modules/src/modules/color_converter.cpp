#include "modules/color_converter.hpp"

#include <chrono>
#include <opencv2/opencv.hpp>
#include <thread>

#include "utils/logger.hpp"

bool ColorConverter::inner_execute(Blackboard& db) {
  LOG_INFO("[ColorConverter] Converting color spaces...");

  // Fetch input dynamically based on topological connection
  cv::Mat img = get_input<cv::Mat>(db, "Image_In");
  if (img.empty()) {
    LOG_ERROR("[ColorConverter] Empty input image received!");
    return false;
  }

  cv::Mat gray, hsv;
  cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
  cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

  set_output(db, "Gray_Out", gray);
  set_output(db, "HSV_Out", hsv);

  publish_visualization("Gray_Out", gray);
  publish_visualization("HSV_Out", hsv);
  return true;
}

REGISTER_MODULE("ColorConverter", ColorConverter,
                (ModuleMetadata{
                    "ColorConverter",
                    "Processing",
                    {{"Image_In", TypeSystem::IMAGE}},  // inputs
                    {{"Gray_Out", TypeSystem::IMAGE},
                     {"HSV_Out", TypeSystem::IMAGE}}  // outputs
                }))
