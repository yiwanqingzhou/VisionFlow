#include "modules/loop_node.hpp"

#include "core/factory.hpp"
#include "core/type_system.hpp"

bool LoopNode::inner_execute(Blackboard& /*db*/) {
  // Loop execution is entirely handled by WorkflowEngine::build_flow
  // (engine.cpp). This inner_execute() should never be called.
  return true;
}

REGISTER_MODULE(
    "Loop", LoopNode,
    (ModuleMetadata{
        "Loop",
        "Control",
        {{"collection", TypeSystem::ANY}},
        {{"item", TypeSystem::ANY}, {"index", TypeSystem::INT}},
        {make_param<std::string>("loop_mode", TypeSystem::STRING, "for_count")
             .set_options({"for_count", "for_each", "while_expr"}),
         make_param<int>("min_iterations", TypeSystem::INT, 0)
             .set_visibility([](const auto& params) {
               return get_param_val<std::string>(params, "loop_mode",
                                                 "for_count") == "for_count";
             }),
         make_param<int>("max_iterations", TypeSystem::INT, 5)
             .set_visibility([](const auto& params) {
               return get_param_val<std::string>(params, "loop_mode",
                                                 "for_count") == "for_count";
             }),
         make_param<int>("step", TypeSystem::INT, 1)
             .set_visibility([](const auto& params) {
               return get_param_val<std::string>(params, "loop_mode",
                                                 "for_count") == "for_count";
             }),
         make_param<std::string>("expression", TypeSystem::STRING, "")
             .set_visibility([](const auto& params) {
               return get_param_val<std::string>(params, "loop_mode",
                                                 "for_count") == "while_expr";
             })}}));
