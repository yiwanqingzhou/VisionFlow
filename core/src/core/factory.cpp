#include "core/factory.hpp"

#include <any>
#include <iostream>
#include <thread>
#include <typeindex>
#include <vector>

#include "utils/logger.hpp"

// Define the static visualization callback
std::function<void(std::string, cv::Mat)> BaseModule::visualization_callback;

// Global registry implementation
std::unordered_map<std::string, ModuleFactory::CreatorFunc>&
ModuleFactory::get_registry() {
  static std::unordered_map<std::string, CreatorFunc> registry;
  return registry;
}

std::unordered_map<std::string, ModuleMetadata>&
ModuleFactory::get_metadata_registry() {
  static std::unordered_map<std::string, ModuleMetadata> registry;
  return registry;
}

std::unordered_map<std::string, std::vector<PropertyMetadata>>&
TypeRegistry::get_property_metadata() {
  static std::unordered_map<std::string, std::vector<PropertyMetadata>>
      property_metadata;
  return property_metadata;
}

std::unordered_map<std::type_index, TypeRegistry::PropertyExtractorFunc>&
TypeRegistry::get_property_extractors() {
  static std::unordered_map<std::type_index, PropertyExtractorFunc> extractors;
  return extractors;
}

std::unordered_map<std::string, TypeRegistry::ParamConverter>&
TypeRegistry::get_converters() {
  static std::unordered_map<std::string, ParamConverter> converters;
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    TypeRegistry::register_parameter_type<int>(TypeSystem::INT);
    TypeRegistry::register_parameter_type<float>(TypeSystem::FLOAT);
    TypeRegistry::register_parameter_type<double>(TypeSystem::DOUBLE);
    TypeRegistry::register_parameter_type<bool>(TypeSystem::BOOLEAN);

    TypeRegistry::register_parameter_type<std::string>(
        std::string(TypeSystem::STRING),
        [](const std::any& val, const std::string& prop) -> std::any {
          if (prop == "length" || prop == "length()")
            return (int)std::any_cast<std::string>(val).length();
          if (prop == "empty" || prop == "empty()")
            return std::any_cast<std::string>(val).empty();
          return std::any();
        },
        std::vector<PropertyMetadata>{
            {"length", std::string(TypeSystem::INT), "String length"},
            {"empty", std::string(TypeSystem::BOOLEAN), "Is string empty?"}});
    TypeRegistry::register_parameter_type<long long>(std::string("long long"));

    // Register Array Types
    TypeRegistry::register_parameter_type<std::vector<int>>(
        std::string(TypeSystem::INT_ARRAY),
        [](const std::any& val, const std::string& prop) -> std::any {
          if (prop == "size" || prop == "size()")
            return (int)std::any_cast<std::vector<int>>(val).size();
          if (prop == "empty" || prop == "empty()")
            return std::any_cast<std::vector<int>>(val).empty();
          return std::any();
        },
        std::vector<PropertyMetadata>{
            {"size", std::string(TypeSystem::INT), "Array size"},
            {"empty", std::string(TypeSystem::BOOLEAN), "Is array empty?"}});

    TypeRegistry::register_parameter_type<std::vector<float>>(
        std::string(TypeSystem::FLOAT_ARRAY),
        [](const std::any& val, const std::string& prop) -> std::any {
          if (prop == "size" || prop == "size()")
            return (int)std::any_cast<std::vector<float>>(val).size();
          if (prop == "empty" || prop == "empty()")
            return std::any_cast<std::vector<float>>(val).empty();
          return std::any();
        },
        std::vector<PropertyMetadata>{
            {"size", std::string(TypeSystem::INT), "Array size"},
            {"empty", std::string(TypeSystem::BOOLEAN), "Is array empty?"}});

    TypeRegistry::register_parameter_type<std::vector<double>>(
        std::string(TypeSystem::DOUBLE_ARRAY),
        [](const std::any& val, const std::string& prop) -> std::any {
          if (prop == "size" || prop == "size()")
            return (int)std::any_cast<std::vector<double>>(val).size();
          if (prop == "empty" || prop == "empty()")
            return std::any_cast<std::vector<double>>(val).empty();
          return std::any();
        },
        std::vector<PropertyMetadata>{
            {"size", std::string(TypeSystem::INT), "Array size"},
            {"empty", std::string(TypeSystem::BOOLEAN), "Is array empty?"}});

    TypeRegistry::register_parameter_type<std::vector<std::string>>(
        std::string(TypeSystem::STRING_ARRAY),
        [](const std::any& val, const std::string& prop) -> std::any {
          if (prop == "size" || prop == "size()")
            return (int)std::any_cast<std::vector<std::string>>(val).size();
          if (prop == "empty" || prop == "empty()")
            return std::any_cast<std::vector<std::string>>(val).empty();
          return std::any();
        },
        std::vector<PropertyMetadata>{
            {"size", std::string(TypeSystem::INT), "Array size"},
            {"empty", std::string(TypeSystem::BOOLEAN), "Is array empty?"}});

    // Register flow-only types
    TypeRegistry::register_flow_type<cv::Mat>(
        std::string(TypeSystem::IMAGE),
        [](const std::any& val, const std::string& prop) -> std::any {
          const auto& mat = std::any_cast<cv::Mat>(val);
          if (prop == "empty" || prop == "empty()") return mat.empty();
          if (prop == "cols") return mat.cols;
          if (prop == "rows") return mat.rows;
          if (prop == "channels" || prop == "channels()") return mat.channels();
          return std::any();
        },
        std::vector<PropertyMetadata>{
            {"empty", std::string(TypeSystem::BOOLEAN), "Is Image empty?"},
            {"cols", std::string(TypeSystem::INT), "Image width (pixels)"},
            {"rows", std::string(TypeSystem::INT), "Image height (pixels)"},
            {"channels", std::string(TypeSystem::INT), "Number of channels"}});
  }
  return converters;
}

bool ModuleFactory::register_creator(const std::string& type, CreatorFunc func,
                                     const ModuleMetadata& meta) {
  LOG_DEBUG("Registering Module: " << type << " (Parameters: "
                                   << meta.parameters.size() << ")");
  get_registry()[type] = func;
  get_metadata_registry()[type] = meta;
  return true;
}

std::unique_ptr<BaseModule> ModuleFactory::create_module(
    const std::string& type) {
  auto& registry = get_registry();
  auto it = registry.find(type);
  if (it != registry.end()) {
    return it->second();
  }

  LOG_WARN("Unrecognized module type: " << type);
  return nullptr;
}
