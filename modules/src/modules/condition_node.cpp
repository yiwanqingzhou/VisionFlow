#include "modules/condition_node.hpp"

#include <sstream>

#include "core/blackboard.hpp"
#include "core/condition_evaluator.hpp"
#include "core/type_system.hpp"
#include "utils/logger.hpp"

bool ConditionNode::inner_execute(Blackboard& db) {
  std::string mode = get_parameter<std::string>("condition_mode", "expression");
  std::string expr = get_parameter<std::string>("expression", "");
  std::string target_node_id = get_parameter<std::string>("target_node_id", "");

  bool result = false;

  if (mode == "node_success") {
    // Check whether the target node's last execution succeeded
    std::string status_key = target_node_id + ".__result";
    bool has_key = db.has(status_key);
    bool val = has_key ? db.read<bool>(status_key) : false;
    result = val;
    LOG_DEBUG("[Condition " << id << "] NodeSuccess check for '"
                            << target_node_id
                            << "': has_key: " << (has_key ? "TRUE" : "FALSE")
                            << "  val=" << (val ? "TRUE" : "FALSE") << " => "
                            << (result ? "PASSED" : "FAILED"));
  } else if (mode == "expression") {
    auto eval_res = ConditionEvaluator::evaluate_expression(db, expr);
    result = (eval_res == ConditionEvaluator::EvalResult::TRUE);
    if (eval_res == ConditionEvaluator::EvalResult::ERROR) {
      LOG_WARN("[Condition "
               << id << "] Expression evaluation encountered an error.");
    }
    LOG_DEBUG("[Condition "
              << id << "] Expression '" << expr << "' => "
              << ConditionEvaluator::eval_result_to_str.at(eval_res));
  }

  // Write result for engine to read back
  db.write(id + ".__result", result);
  return result;
}

REGISTER_MODULE(
    "Condition", ConditionNode,
    (ModuleMetadata{
        "Condition",
        "Control",
        {},  // No data inputs
        {{"True", TypeSystem::TRIGGER},
         {"False", TypeSystem::TRIGGER}},  // Outputs
        {make_param<std::string>("condition_mode", TypeSystem::STRING,
                                 "node_success")
             .set_options({"node_success", "expression"}),
         make_param<std::string>("expression", TypeSystem::STRING, "")
             .set_visibility([](const auto& params) {
               return get_param_val<std::string>(params, "condition_mode") ==
                      "expression";
             }),
         make_param<std::string>("target_node_id", TypeSystem::STRING, "")
             .set_visibility([](const auto& params) {
               return get_param_val<std::string>(params, "condition_mode") ==
                      "node_success";
             })}}))
