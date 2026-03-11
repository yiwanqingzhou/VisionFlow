#include "modules/ui_display.hpp"

#include "utils/logger.hpp"

bool UI_Display::inner_execute(Blackboard& db) {
  LOG_INFO("[UI_Display] [Fake] Updating Dashboard...");
  std::string final_img = db.read<std::string>("Display_Image");
  int count = db.read<int>("Total_Count");
  LOG_INFO("[DASHBOARD] Frame ready. Detected Objects: " << count);
  return true;
}

REGISTER_MODULE("UI_Display", UI_Display,
                (ModuleMetadata{
                    "UI_Display",
                    "Output",
                    {{"Display_Image", TypeSystem::IMAGE},
                     {"Total_Count", TypeSystem::INT}},  // inputs
                    {}                                   // outputs
                }))
