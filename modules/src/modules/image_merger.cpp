#include "modules/image_merger.hpp"

#include <chrono>
#include <thread>

#include "core/type_system.hpp"
#include "utils/logger.hpp"

bool ImageMerger::inner_execute(Blackboard& db) {
  LOG_INFO(
      "[ImageMerger] [Fake] Drawing boxes and edges onto original image...");
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  std::string orig = db.read<std::string>("Original");
  std::string edges = db.read<std::string>("Edges");
  std::string boxes = db.read<std::string>("BoundingBoxes");
  db.write("Composite_Image", std::string("<Final Rendered Frame>"));
  return true;
}

REGISTER_MODULE("ImageMerger", ImageMerger,
                (ModuleMetadata{
                    "ImageMerger",
                    "Processing",
                    {{"Original", TypeSystem::IMAGE},
                     {"Edges", TypeSystem::IMAGE},
                     {"BoundingBoxes", TypeSystem::RECT}},    // inputs
                    {{"Composite_Image", TypeSystem::IMAGE}}  // outputs
                }))
