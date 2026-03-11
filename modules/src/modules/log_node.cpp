#include <sstream>
#include <vector>

#include "core/blackboard.hpp"
#include "core/factory.hpp"
#include "core/type_system.hpp"
#include "utils/logger.hpp"

std::string any_to_string(const std::any& val);

std::string any_to_string(const std::any& val) {
  if (!val.has_value()) return "";

  if (val.type() == typeid(int)) return std::to_string(std::any_cast<int>(val));
  if (val.type() == typeid(float))
    return std::to_string(std::any_cast<float>(val));
  if (val.type() == typeid(double))
    return std::to_string(std::any_cast<double>(val));
  if (val.type() == typeid(bool))
    return std::any_cast<bool>(val) ? "true" : "false";
  if (val.type() == typeid(std::string)) return std::any_cast<std::string>(val);

  // Helper to format a vector of any scalar type as "[n]{v0, v1, ...}"
  auto fmt_vec = [](std::string body, size_t n) {
    return "[" + std::to_string(n) + "]{" + body + "}";
  };

  if (val.type() == typeid(std::vector<int>)) {
    auto& vec = std::any_cast<const std::vector<int>&>(val);
    std::string s;
    for (size_t i = 0; i < vec.size(); ++i)
      s += std::to_string(vec[i]) + (i + 1 < vec.size() ? ", " : "");
    return fmt_vec(s, vec.size());
  }
  if (val.type() == typeid(std::vector<float>)) {
    auto& vec = std::any_cast<const std::vector<float>&>(val);
    std::string s;
    for (size_t i = 0; i < vec.size(); ++i)
      s += std::to_string(vec[i]) + (i + 1 < vec.size() ? ", " : "");
    return fmt_vec(s, vec.size());
  }
  if (val.type() == typeid(std::vector<double>)) {
    auto& vec = std::any_cast<const std::vector<double>&>(val);
    std::string s;
    for (size_t i = 0; i < vec.size(); ++i)
      s += std::to_string(vec[i]) + (i + 1 < vec.size() ? ", " : "");
    return fmt_vec(s, vec.size());
  }
  if (val.type() == typeid(std::vector<bool>)) {
    auto vec =
        std::any_cast<std::vector<bool>>(val);  // no const-ref for vector<bool>
    std::string s;
    for (size_t i = 0; i < vec.size(); ++i)
      s += std::string(vec[i] ? "true" : "false") +
           (i + 1 < vec.size() ? ", " : "");
    return fmt_vec(s, vec.size());
  }
  if (val.type() == typeid(std::vector<std::string>)) {
    auto& vec = std::any_cast<const std::vector<std::string>&>(val);
    std::string s;
    for (size_t i = 0; i < vec.size(); ++i)
      s += "\"" + vec[i] + "\"" + (i + 1 < vec.size() ? ", " : "");
    return fmt_vec(s, vec.size());
  }
  if (val.type() == typeid(std::vector<std::any>)) {
    auto& vec = std::any_cast<const std::vector<std::any>&>(val);
    std::string s;
    for (size_t i = 0; i < vec.size(); ++i)
      s += any_to_string(vec[i]) + (i + 1 < vec.size() ? ", " : "");
    return fmt_vec(s, vec.size());
  }

  return "<" + std::string(val.type().name()) + ">";
}

class LogNode : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override {
    std::string log_level = "INFO";
    std::string prefix = "";
    std::string postfix = "";

    if (parameters.count("log_level")) {
      try {
        log_level = std::any_cast<std::string>(parameters.at("log_level"));
      } catch (...) {
      }
    }
    if (parameters.count("prefix")) {
      try {
        prefix = std::any_cast<std::string>(parameters.at("prefix"));
      } catch (...) {
      }
    }
    if (parameters.count("postfix")) {
      try {
        postfix = std::any_cast<std::string>(parameters.at("postfix"));
      } catch (...) {
      }
    }

    std::any data_val;
    if (input_mapping.count("data_in")) {
      data_val = db.read_any(input_mapping.at("data_in"));
    }

    std::string msg = prefix + any_to_string(data_val) + postfix;

    if (log_level == "INFO") {
      LOG_INFO("[Logged] " << msg);
    } else if (log_level == "WARN") {
      LOG_WARN("[Logged] " << msg);
    } else if (log_level == "ERROR") {
      LOG_ERROR("[Logged] " << msg);
    } else {
      LOG_INFO("[Logged] " << msg);
    }

    return true;
  }
};

REGISTER_MODULE(
    "Log", LogNode,
    (ModuleMetadata{
        "Log",
        "Utility",
        {{"data_in", TypeSystem::ANY}},
        {},
        {make_param<std::string>("log_level", TypeSystem::STRING, "INFO")
             .set_options({"INFO", "WARN", "ERROR"}),
         make_param<std::string>("prefix", TypeSystem::STRING, ""),
         make_param<std::string>("postfix", TypeSystem::STRING, "")}}));
