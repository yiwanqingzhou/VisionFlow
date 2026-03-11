#pragma once
#include <any>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/type_system.hpp"

struct PinMetadata {
  std::string name;
  std::string type;  // See TypeSystem::IMAGE, TypeSystem::INT, etc.

  PinMetadata(const char* in_name, const char* in_type)
      : name(in_name), type(in_type) {}
  PinMetadata(const std::string& in_name, const std::string& in_type)
      : name(in_name), type(in_type) {}
  PinMetadata(const std::string& in_name, const std::string_view in_type)
      : name(in_name), type(std::string(in_type)) {}
};

struct PropertyMetadata {
  std::string name;
  std::string return_type;  // e.g. TypeSystem::BOOLEAN
  std::string description;
};

#include <typeindex>

class TypeRegistry {
 public:
  // For parameters: JSON serialization
  using DeserializeFunc =
      std::function<std::any(const nlohmann::json&, const std::string&)>;
  using SerializeFunc =
      std::function<void(nlohmann::json&, const std::string&, const std::any&)>;

  struct ParamConverter {
    DeserializeFunc deserialize;
    SerializeFunc serialize;
  };

  static std::unordered_map<std::string, ParamConverter>& get_converters();

  // No longer needed: we do not serialize any arbitrary std::any to JSON string
  // for evaluation static std::unordered_map<std::type_index, SerializeFunc>&
  // get_serializers_by_type();

  // For flow/data evaluation: Property extraction directly from std::any
  using PropertyExtractorFunc =
      std::function<std::any(const std::any&, const std::string&)>;

  static std::unordered_map<std::type_index, PropertyExtractorFunc>&
  get_property_extractors();

  static std::unordered_map<std::string, std::vector<PropertyMetadata>>&
  get_property_metadata();

  // 1. Register a type that can be used as a node Parameter (needs JSON)
  template <typename T>
  static void register_parameter_type(
      const std::string& type_name, PropertyExtractorFunc extractor = nullptr,
      const std::vector<PropertyMetadata>& props = {}) {
    auto serialize_func = [](nlohmann::json& j, const std::string& key,
                             const std::any& val) {
      j[key] = std::any_cast<T>(val);
    };

    get_converters()[type_name] = {
        [](const nlohmann::json& j, const std::string& key) -> std::any {
          return j.at(key).get<T>();
        },
        serialize_func};

    if (extractor) {
      get_property_extractors()[std::type_index(typeid(T))] = extractor;
      get_property_metadata()[type_name] = props;
    }
  }

  template <typename T>
  static void register_parameter_type(std::string_view type_name) {
    register_parameter_type<T>(std::string(type_name));
  }

  // 2. Register a type that flows through pins (I/O only), no JSON needed.
  //    Provides properties for Condition evaluator and UI autocomplete.
  template <typename T>
  static void register_flow_type(const std::string& type_name,
                                 PropertyExtractorFunc extractor,
                                 const std::vector<PropertyMetadata>& props) {
    get_property_extractors()[std::type_index(typeid(T))] = extractor;
    get_property_metadata()[type_name] = props;
  }
};

struct ParameterMetadata {
  std::string name;
  std::string type_name;

  std::any min_val;
  std::any max_val;
  std::any default_val;

  // Optional: if non-empty, the string property inspector will show a Combo
  std::vector<std::string> options;

  // Optional: if set, this parameter is only shown when the predicate returns
  // true. Receives the full parameters map of the current node.
  std::function<bool(const std::unordered_map<std::string, std::any>&)>
      visible_when;

  TypeRegistry::DeserializeFunc deserialize;
  TypeRegistry::SerializeFunc serialize;

  // Fluent API helpers
  ParameterMetadata set_options(std::vector<std::string> opts) {
    options = std::move(opts);
    return std::move(*this);
  }

  ParameterMetadata set_visibility(
      std::function<bool(const std::unordered_map<std::string, std::any>&)>
          vis) {
    visible_when = std::move(vis);
    return std::move(*this);
  }

  ParameterMetadata visible_if(const std::string& other_param,
                               const std::string& value) {
    visible_when = [other_param, value](const auto& params) {
      auto it = params.find(other_param);
      if (it == params.end()) return false;
      try {
        return std::any_cast<std::string>(it->second) == value;
      } catch (...) {
        return false;
      }
    };
    return std::move(*this);
  }
};

template <typename T>
inline T get_param_val(const std::unordered_map<std::string, std::any>& params,
                       const std::string& name, T default_val = T()) {
  auto it = params.find(name);
  if (it == params.end()) return default_val;
  try {
    return std::any_cast<T>(it->second);
  } catch (...) {
    return default_val;
  }
}

template <typename T>
ParameterMetadata make_param(const std::string& name,
                             const std::string& type_name, T default_v, T min_v,
                             T max_v) {
  ParameterMetadata meta;
  meta.name = name;
  meta.type_name = type_name;
  meta.default_val = default_v;
  meta.min_val = min_v;
  meta.max_val = max_v;

  auto& converters = TypeRegistry::get_converters();
  if (converters.count(type_name)) {
    meta.deserialize = converters[type_name].deserialize;
    meta.serialize = converters[type_name].serialize;
  } else {
    meta.deserialize = [](const nlohmann::json& j,
                          const std::string& key) -> std::any {
      return j.at(key).get<T>();
    };
    meta.serialize = [](nlohmann::json& j, const std::string& key,
                        const std::any& val) {
      j[key] = std::any_cast<T>(val);
    };
  }
  return meta;
}

template <typename T>
ParameterMetadata make_param(const std::string& name,
                             std::string_view type_name, T default_v, T min_v,
                             T max_v) {
  return make_param<T>(name, std::string(type_name), default_v, min_v, max_v);
}

template <typename T>
ParameterMetadata make_param(const std::string& name,
                             const std::string& type_name, T default_v) {
  return make_param<T>(name, type_name, default_v, T(), T());
}

template <typename T>
ParameterMetadata make_param(const std::string& name,
                             std::string_view type_name, T default_v) {
  return make_param<T>(name, std::string(type_name), default_v);
}

struct ModuleMetadata {
  std::string type_name;  // E.g., "Sensor"
  std::string category;   // E.g., "Input", "Processing", "Output"
  std::vector<PinMetadata> inputs;
  std::vector<PinMetadata> outputs;
  std::vector<ParameterMetadata> parameters;
};
