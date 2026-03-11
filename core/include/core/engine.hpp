#pragma once
#include <list>
#include <nlohmann/json.hpp>
#include <string>
#include <taskflow/taskflow.hpp>
#include <unordered_map>

#include "blackboard.hpp"

class WorkflowEngine {
 public:
  enum class GraphPass { EXECUTION, DETAILED_VIZ, HIGH_LEVEL_VIZ };

  // Load a pipeline configuration from JSON and build the Taskflow graph
  bool build_graph_from_json(const std::string& json_path, Blackboard& db,
                             bool show_details = true);

  // Execute the previously built Taskflow graph
  void execute_graph();

  // Hardcoded demo execution (optional)
  void run_demo(Blackboard& db);

  tf::Taskflow taskflow;
  tf::Executor executor;

  // Store mapping between Data Key and Producer Node ID (for auto dependency
  // analysis) Kept as a member to be shared across recursive subflow builds
  std::unordered_map<std::string, std::string> data_producer_map;

  // Owning storage for statically-built sub-Taskflows (Subflow / Loop groups).
  // Must outlive the parent taskflow. std::list gives pointer stability so
  // composed_of references remain valid after additional insertions.
  std::list<tf::Taskflow> sub_taskflows;

  // Separate storage for the visualization-only pass (DOT dump). Only used
  // inside build_graph_from_json and must outlive the local viz_tf.
  std::list<tf::Taskflow> viz_sub_taskflows;

  // Track parent-child relationships for dependency promotion (ID -> Container
  // ID)
  std::unordered_map<std::string, std::string> node_to_container;

 private:
  template <typename FlowBuilder>
  void build_flow(FlowBuilder& flow, const nlohmann::json& config,
                  Blackboard& db, GraphPass pass = GraphPass::EXECUTION);

  // Helper to execute a single module (used by standard nodes and Condition
  // nodes)
  bool execute_node_module(
      const std::string& type, const std::string& id,
      const std::unordered_map<std::string, std::string>& input_mapping,
      const std::unordered_map<std::string, std::any>& parameters,
      Blackboard& db);
};
