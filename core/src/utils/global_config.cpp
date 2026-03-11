#include "utils/global_config.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

#include "utils/logger.hpp"

GlobalConfig& GlobalConfig::get() {
  static GlobalConfig instance;
  return instance;
}

GlobalConfig::GlobalConfig() {
  load();
}

void GlobalConfig::load() {
  std::ifstream file(config_path_);
  if (!file.is_open()) {
    LOG_INFO("[GlobalConfig] Custom config not found, using defaults.");
    return;
  }

  try {
    nlohmann::json j;
    file >> j;
    if (j.contains("pipeline_config"))
      data_.pipeline_config = j["pipeline_config"].get<std::string>();
    if (j.contains("pipeline_layout"))
      data_.pipeline_layout = j["pipeline_layout"].get<std::string>();
    if (j.contains("editor_file"))
      data_.editor_file = j["editor_file"].get<std::string>();
    if (j.contains("imgui_ini_file"))
      data_.imgui_ini_file = j["imgui_ini_file"].get<std::string>();
    if (j.contains("global_log_level"))
      data_.global_log_level = j["global_log_level"].get<std::string>();
    if (j.contains("auto_load")) data_.auto_load = j["auto_load"].get<bool>();

    LOG_INFO("[GlobalConfig] Configuration loaded.");
  } catch (const std::exception& e) {
    LOG_ERROR("[GlobalConfig] Failed to parse config.json: " << e.what());
  }
}

void GlobalConfig::save() {
  nlohmann::json j;
  j["pipeline_config"] = data_.pipeline_config;
  j["pipeline_layout"] = data_.pipeline_layout;
  j["editor_file"] = data_.editor_file;
  j["imgui_ini_file"] = data_.imgui_ini_file;
  j["global_log_level"] = data_.global_log_level;
  j["auto_load"] = data_.auto_load;

  std::ofstream file(config_path_);
  if (file.is_open()) {
    file << j.dump(4);
    LOG_INFO("[GlobalConfig] Configuration saved.");
  } else {
    LOG_ERROR("[GlobalConfig] Failed to open config.json for writing.");
  }
}
