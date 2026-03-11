#pragma once
#include <string>

struct GlobalConfigData {
  std::string pipeline_config = "../assets/active_pipeline.json";
  std::string pipeline_layout = "../assets/active_pipeline.layout.json";
  std::string editor_file = "../assets/ui_layout.json";
  std::string imgui_ini_file = "../assets/imgui.ini";
  std::string pipeline_tmp = "/tmp/tasknodeflow_pipeline.json";
  std::string pipeline_layout_tmp = "/tmp/tasknodeflow_layout.json";
  std::string editor_tmp = "/tmp/tasknodeflow_editor.json";
  std::string imgui_ini_tmp = "/tmp/tasknodeflow_imgui.ini";
  std::string global_log_level = "DEBUG";
  bool auto_load = false;
};

class GlobalConfig {
 public:
  static GlobalConfig& get();

  void load();
  void save();

  GlobalConfigData& get_data() { return data_; }

 private:
  GlobalConfig();
  ~GlobalConfig() = default;

  GlobalConfigData data_;
  std::string config_path_ = "../assets/config.json";
};
