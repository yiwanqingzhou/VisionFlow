#include <string>
#include <vector>

#include "core/blackboard.hpp"
#include "core/factory.hpp"
#include "core/type_system.hpp"
#include "utils/logger.hpp"

class GenerateNode : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override {
    std::string data_type = std::string(TypeSystem::INT);

    if (parameters.count("data_type")) {
      try {
        data_type = std::any_cast<std::string>(parameters.at("data_type"));
      } catch (...) {
      }
    }

    std::any parsed_val;
    try {
      if (data_type == std::string(TypeSystem::INT) &&
          parameters.count("int_value")) {
        parsed_val = parameters.at("int_value");
      } else if (data_type == std::string(TypeSystem::FLOAT) &&
                 parameters.count("float_value")) {
        parsed_val = parameters.at("float_value");
      } else if (data_type == std::string(TypeSystem::DOUBLE) &&
                 parameters.count("double_value")) {
        parsed_val = parameters.at("double_value");
      } else if (data_type == std::string(TypeSystem::STRING) &&
                 parameters.count("string_value")) {
        parsed_val = parameters.at("string_value");
      } else if (data_type == std::string(TypeSystem::INT_ARRAY) &&
                 parameters.count("int_array_value")) {
        parsed_val = parameters.at("int_array_value");
      } else if (data_type == std::string(TypeSystem::FLOAT_ARRAY) &&
                 parameters.count("float_array_value")) {
        parsed_val = parameters.at("float_array_value");
      } else if (data_type == std::string(TypeSystem::DOUBLE_ARRAY) &&
                 parameters.count("double_array_value")) {
        parsed_val = parameters.at("double_array_value");
      } else if (data_type == std::string(TypeSystem::STRING_ARRAY) &&
                 parameters.count("string_array_value")) {
        parsed_val = parameters.at("string_array_value");
      } else {
        LOG_ERROR("[Generate "
                  << id
                  << "] Unknown data_type or missing parameter: " << data_type);
        return false;
      }
    } catch (const std::exception& e) {
      LOG_ERROR("[Generate " << id
                             << "] Failed to extract value: " << e.what());
      return false;
    }

    db.write<std::any>(id + ".data_out", parsed_val);
    return true;
  }
};

REGISTER_MODULE(
    "Generate", GenerateNode,
    (ModuleMetadata{
        "Generate",
        "Utility",
        {},
        {{"data_out", TypeSystem::ANY}},
        {make_param<std::string>("data_type", TypeSystem::STRING,
                                 std::string(TypeSystem::INT))
             .set_options({std::string(TypeSystem::INT),
                           std::string(TypeSystem::FLOAT),
                           std::string(TypeSystem::DOUBLE),
                           std::string(TypeSystem::STRING),
                           std::string(TypeSystem::INT_ARRAY),
                           std::string(TypeSystem::FLOAT_ARRAY),
                           std::string(TypeSystem::DOUBLE_ARRAY),
                           std::string(TypeSystem::STRING_ARRAY)}),
         make_param<int>("int_value", TypeSystem::INT, 0)
             .set_visibility([](const auto& p) {
               return get_param_val<std::string>(
                          p, "data_type", std::string(TypeSystem::INT)) ==
                      std::string(TypeSystem::INT);
             }),
         make_param<float>("float_value", TypeSystem::FLOAT, 0.0f)
             .set_visibility([](const auto& p) {
               return get_param_val<std::string>(
                          p, "data_type", std::string(TypeSystem::INT)) ==
                      std::string(TypeSystem::FLOAT);
             }),
         make_param<double>("double_value", std::string(TypeSystem::DOUBLE),
                            0.0)
             .set_visibility([](const auto& p) {
               return get_param_val<std::string>(
                          p, "data_type", std::string(TypeSystem::INT)) ==
                      std::string(TypeSystem::DOUBLE);
             }),
         make_param<std::string>("string_value", TypeSystem::STRING, "")
             .set_visibility([](const auto& p) {
               return get_param_val<std::string>(
                          p, "data_type", std::string(TypeSystem::INT)) ==
                      std::string(TypeSystem::STRING);
             }),
         make_param<std::vector<int>>("int_array_value", TypeSystem::INT_ARRAY,
                                      {0})
             .set_visibility([](const auto& p) {
               return get_param_val<std::string>(
                          p, "data_type", std::string(TypeSystem::INT)) ==
                      std::string(TypeSystem::INT_ARRAY);
             }),
         make_param<std::vector<float>>("float_array_value",
                                        TypeSystem::FLOAT_ARRAY, {0.0f})
             .set_visibility([](const auto& p) {
               return get_param_val<std::string>(
                          p, "data_type", std::string(TypeSystem::INT)) ==
                      std::string(TypeSystem::FLOAT_ARRAY);
             }),
         make_param<std::vector<double>>("double_array_value",
                                         TypeSystem::DOUBLE_ARRAY, {0.0})
             .set_visibility([](const auto& p) {
               return get_param_val<std::string>(
                          p, "data_type", std::string(TypeSystem::INT)) ==
                      std::string(TypeSystem::DOUBLE_ARRAY);
             }),
         make_param<std::vector<std::string>>("string_array_value",
                                              TypeSystem::STRING_ARRAY, {""})
             .set_visibility([](const auto& p) {
               return get_param_val<std::string>(
                          p, "data_type", std::string(TypeSystem::INT)) ==
                      std::string(TypeSystem::STRING_ARRAY);
             })}}));
