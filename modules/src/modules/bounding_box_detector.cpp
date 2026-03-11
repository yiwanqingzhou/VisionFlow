#include "modules/bounding_box_detector.hpp"

#include <chrono>
#include <thread>

#include "core/type_system.hpp"
#include "utils/logger.hpp"

bool BoundingBoxDetector::inner_execute(Blackboard& db) {
  LOG_INFO("[BoundingBoxDetector] [Fake] Running YOLO inference on HSV...");
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  std::string hsv = db.read<std::string>("HSV_In");
  db.write("BoundingBoxes", std::string("[ {x:10, y:20, w:50, h:50} ]"));
  db.write("ObjectCount", 1);
  return true;
}

REGISTER_MODULE("BoundingBoxDetector", BoundingBoxDetector,
                (ModuleMetadata{
                    "BoundingBoxDetector",
                    "AI",
                    {{"HSV_In", TypeSystem::IMAGE}},  // inputs
                    {{"BoundingBoxes", TypeSystem::RECT},
                     {"ObjectCount", TypeSystem::INT}}  // outputs
                }))
