#pragma once
#include <functional>
#include <memory>
#include <opencv2/opencv.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "blackboard.hpp"
#include "module_metadata.hpp"
#include "utils/logger.hpp"

class BaseModule {
 public:
  virtual ~BaseModule() = default;

  // The unique ID assigned to this node instance during pipeline assembly
  std::string id;
  std::string name;

  // Maps the Module's local input pin name to the global Blackboard key
  // e.g., input_mapping["Gray_In"] = "ColorConverter.Gray_Out"
  std::unordered_map<std::string, std::string> input_mapping;

  // Global UI callback injected from main.cpp, takes (pin_key, cv::Mat)
  static std::function<void(std::string, cv::Mat)> visualization_callback;

  // Stores runtime algorithm parameters set by UI or JSON
  std::unordered_map<std::string, std::any> parameters;

  // Parameter setter (primarily used by UI properties panel)
  template <typename T>
  void set_parameter(const std::string& key, T value) {
    parameters[key] = value;
  }

 protected:
  // Safe parameter retrieval helper
  template <typename T>
  T get_parameter(const std::string& key) {
    if (parameters.count(key)) {
      try {
        return std::any_cast<T>(parameters[key]);
      } catch (const std::bad_any_cast&) {
        // Fallback to default if type mismatch occurs
        LOG_ERROR("[BaseModule] Parameter type mismatch for key: " + key);
        throw std::runtime_error("Parameter type mismatch");
      }
    }
    // should never reach here
    return T();
  }

  // with default value
  template <typename T>
  T get_parameter(const std::string& key, T default_value) {
    if (parameters.count(key)) {
      try {
        return std::any_cast<T>(parameters[key]);
      } catch (const std::bad_any_cast&) {
        // Fallback to default if type mismatch occurs
        LOG_WARN("[BaseModule] Parameter type mismatch for key: " + key);
        return default_value;
      }
    }
    // should never reach here
    return default_value;
  }

  // Fetches a mapped input from the Blackboard
  template <typename T>
  T get_input(Blackboard& db, const std::string& local_pin_name) {
    if (input_mapping.count(local_pin_name)) {
      std::string global_key = input_mapping[local_pin_name];
      return db.read<T>(global_key);
    }
    return T();  // Return default constructed if not wired
  }

  // Publishes an output to the Blackboard
  template <typename T>
  void set_output(Blackboard& db, const std::string& local_pin_name, T value) {
    std::string global_key = id + "." + local_pin_name;
    db.write(global_key, value);
  }

  // Publishes data to the UI visualization callback
  template <typename T>
  void publish_visualization(const std::string& local_pin_name, T data) {
    if (visualization_callback) {
      visualization_callback(id + "." + local_pin_name, data);
    }
  }

  virtual bool inner_execute(Blackboard& db) = 0;

 public:
  void execute(Blackboard& db) {
    bool ret = false;

    try {
      ret = inner_execute(db);
    } catch (const std::exception& e) {
      LOG_ERROR("[BaseModule] Exception in " << id << ": " << e.what());
    }

    db.write(id + ".__result", ret);
  }
};

class ModuleFactory {
 public:
  using CreatorFunc = std::function<std::unique_ptr<BaseModule>()>;

  static std::unique_ptr<BaseModule> create_module(const std::string& type);

  // Get global registry (uses local static to avoid Static Initialization Order
  // Fiasco)
  static std::unordered_map<std::string, CreatorFunc>& get_registry();
  static std::unordered_map<std::string, ModuleMetadata>&
  get_metadata_registry();

  static bool register_creator(const std::string& type, CreatorFunc func,
                               const ModuleMetadata& meta);
};

// Helper struct for auto-registering modules
class AutoRegisterModule {
 public:
  AutoRegisterModule(const std::string& type, ModuleFactory::CreatorFunc func,
                     const ModuleMetadata& meta) {
    ModuleFactory::register_creator(type, std::move(func), meta);
  }
};

// Helper macro for registration (place at the end of each module cpp file)
#define REGISTER_MODULE(TypeName, ClassName, Meta)  \
  static AutoRegisterModule g_register_##ClassName( \
      TypeName, []() { return std::make_unique<ClassName>(); }, Meta);
