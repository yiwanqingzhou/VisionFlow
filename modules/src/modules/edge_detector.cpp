#include "modules/edge_detector.hpp"

#include <chrono>
#include <opencv2/opencv.hpp>
#include <thread>

#include "core/type_system.hpp"
#include "utils/logger.hpp"

bool EdgeDetector::inner_execute(Blackboard& db) {
  LOG_INFO("[EdgeDetector] Applying Canny edge detection...");

  cv::Mat gray = get_input<cv::Mat>(db, "Gray_In");
  if (gray.empty()) {
    LOG_ERROR("[EdgeDetector] Empty input gray image received!");
    return false;
  }

  cv::Mat edges;
  int t1 = get_parameter<int>("Threshold1", 100);
  int t2 = get_parameter<int>("Threshold2", 200);
  cv::Canny(gray, edges, t1, t2);

  set_output(db, "Edges", edges);
  publish_visualization("Edges", edges);
  return true;
}

REGISTER_MODULE("EdgeDetector", EdgeDetector,
                (ModuleMetadata{
                    "EdgeDetector",
                    "Vision",
                    {{"Gray_In", TypeSystem::IMAGE}},  // inputs
                    {{"Edges", TypeSystem::IMAGE}},    // outputs
                    {make_param<int>("Threshold1", TypeSystem::INT, 100, 0,
                                     255),
                     make_param<int>("Threshold2", TypeSystem::INT, 200, 0,
                                     255)}  // parameters
                }))
