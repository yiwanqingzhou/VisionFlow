#include "core/engine.hpp"

#include <bit>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <taskflow/taskflow.hpp>
#include <thread>
#include <unordered_map>

#include "core/blackboard.hpp"
#include "core/condition_evaluator.hpp"
#include "core/factory.hpp"
#include "utils/logger.hpp"

using json = nlohmann::json;

// Parse JSON and build the computation graph
bool WorkflowEngine::build_graph_from_json(const std::string& json_path,
                                           Blackboard& db, bool show_details) {
  taskflow.clear();           // Clear any previous graph
  data_producer_map.clear();  // Clear global data map
  sub_taskflows.clear();      // Release previously built sub-graphs
  node_to_container.clear();  // Clear hierarchy tracking

  std::ifstream file(json_path);
  if (!file.is_open()) {
    LOG_ERROR("[Engine Error] Could not find config file: " << json_path);
    return false;
  }

  nlohmann::json config;
  try {
    file >> config;
  } catch (nlohmann::json::parse_error& e) {
    LOG_ERROR("[Engine Error] JSON parse syntax error: " << e.what());
    return false;
  }

  // --- Execution pass ---
  taskflow.name("TaskNodeFlow_Dynamic");
  build_flow(taskflow, config, db, GraphPass::EXECUTION);

  // --- Visualization / DOT dump ---
  if (!show_details) {
    LOG_INFO(">>> Dumping High-level Taskflow graph to taskflow_graph.dot <<<");
    tf::Taskflow high_tf;
    high_tf.name("TaskNodeFlow_Dynamic");
    data_producer_map.clear();
    build_flow(high_tf, config, db, GraphPass::HIGH_LEVEL_VIZ);

    std::ofstream out("taskflow_graph.dot");
    if (out.is_open()) {
      high_tf.dump(out);
    }
  } else {
    // Build a separate, temporary Taskflow where both Subflow and Loop nodes
    // use composed_of so their internals appear in the DOT graph.
    viz_sub_taskflows.clear();
    data_producer_map.clear();  // rebuild cleanly for the viz pass
    {
      tf::Taskflow viz_tf;
      viz_tf.name("TaskNodeFlow_Dynamic");
      build_flow(viz_tf, config, db, GraphPass::DETAILED_VIZ);

      LOG_INFO(">>> Dumping Detailed Taskflow graph to taskflow_graph.dot <<<");
      std::ofstream out("taskflow_graph.dot");
      if (out.is_open()) {
        viz_tf.dump(out);
      }
    }
  }
  // Rebuild data_producer_map for actual execution
  data_producer_map.clear();
  if (config.contains("children") && config["children"].is_array()) {
    for (auto& node_cfg : config["children"]) {
      std::string id = node_cfg.at("id").get<std::string>();
      if (node_cfg.contains("outputs")) {
        for (auto& out_key : node_cfg["outputs"]) {
          data_producer_map[id + "." + out_key.get<std::string>()] = id;
        }
      }
    }
  }

  return true;
}

template <typename FlowBuilder>
void WorkflowEngine::build_flow(FlowBuilder& flow, const nlohmann::json& config,
                                Blackboard& db,
                                WorkflowEngine::GraphPass pass) {
  // Store mapping between ID and Taskflow task
  std::unordered_map<std::string, tf::Task> id_to_task;

  // --- Phase 1: Create all task nodes ---
  if (config.contains("children") && config["children"].is_array()) {
    for (auto& node_cfg : config["children"]) {
      std::string id = node_cfg.at("id").get<std::string>();
      std::string type = node_cfg.value("type", "Default");

      // Track parent-child relationship (if we are in a sub-flow)
      if (config.contains("id")) {
        node_to_container[id] = config["id"].get<std::string>();
      }

      std::unordered_map<std::string, std::string> input_mapping;
      if (node_cfg.contains("inputs") && node_cfg["inputs"].is_object()) {
        for (auto& el : node_cfg["inputs"].items()) {
          input_mapping[el.key()] = el.value().get<std::string>();
        }
      }

      // Record data Key output by this node
      if (node_cfg.contains("outputs")) {
        for (auto& out_key : node_cfg["outputs"]) {
          std::string full_key = id + "." + out_key.get<std::string>();
          data_producer_map[full_key] = id;
        }
      }

      tf::Task task;

      // Parse parameters (shared by Condition and standard tasks)
      std::unordered_map<std::string, std::any> parameters;
      if (node_cfg.contains("parameters") &&
          node_cfg["parameters"].is_object()) {
        auto param_cfg = node_cfg["parameters"];
        auto& registry = ModuleFactory::get_metadata_registry();
        if (registry.count(type)) {
          for (const auto& param_meta : registry.at(type).parameters) {
            if (param_cfg.contains(param_meta.name) && param_meta.deserialize) {
              try {
                parameters[param_meta.name] =
                    param_meta.deserialize(param_cfg, param_meta.name);
              } catch (const std::exception& e) {
                LOG_ERROR("Failed to deserialize parameter: " +
                          param_meta.name);
              }
            }
          }
        }
      }

      if (type == "Loop_Entry" || type == "Loop_Exit") {
        // empty task
        task = flow.emplace([]() {}).name(id);
      } else if (type == "Subflow") {
        if (pass == GraphPass::HIGH_LEVEL_VIZ) {
          // In high-level viz, show as an opaque task
          task = flow.placeholder().name(id);
        } else {
          // Build the sub-graph statically (composed_of) so it appears in the
          // DOT visualization with its internal structure visible.
          // Use viz_sub_taskflows during the viz pass so the execution
          // sub_taskflows are not disturbed.
          auto& storage = (pass == GraphPass::DETAILED_VIZ) ? viz_sub_taskflows
                                                            : sub_taskflows;
          storage.emplace_back();
          tf::Taskflow& sub_tf = storage.back();
          sub_tf.name(id);
          build_flow(sub_tf, node_cfg, db, pass);
          task = flow.composed_of(sub_tf).name(id);
        }
      } else if (type == "Loop") {
        if (pass == GraphPass::DETAILED_VIZ) {
          // Visualization: show one iteration's structure as a static subgraph
          viz_sub_taskflows.emplace_back();
          tf::Taskflow& loop_viz_tf = viz_sub_taskflows.back();
          loop_viz_tf.name(id);
          build_flow(loop_viz_tf, node_cfg, db, pass);
          task = flow.composed_of(loop_viz_tf).name(id);
        } else if (pass == GraphPass::HIGH_LEVEL_VIZ) {
          // High-level viz: just a placeholder
          task = flow.placeholder().name(id);
        } else {
          // Loop node: iterates children sequentially
          task =
              flow.emplace([this, node_cfg, id, parameters, input_mapping,
                            &db]() {
                    std::string mode = "for_count";
                    int min_iter = 0;
                    int max_iter = 5;
                    int step = 1;
                    std::string expr;

                    // Extract parameters
                    if (parameters.count("loop_mode")) {
                      try {
                        mode = std::any_cast<std::string>(
                            parameters.at("loop_mode"));
                      } catch (...) {
                      }
                    }
                    if (parameters.count("min_iterations")) {
                      try {
                        min_iter =
                            std::any_cast<int>(parameters.at("min_iterations"));
                      } catch (...) {
                      }
                    }
                    if (parameters.count("max_iterations")) {
                      try {
                        max_iter =
                            std::any_cast<int>(parameters.at("max_iterations"));
                      } catch (...) {
                      }
                    }
                    if (parameters.count("step")) {
                      try {
                        step = std::any_cast<int>(parameters.at("step"));
                      } catch (...) {
                      }
                    }
                    if (parameters.count("expression")) {
                      try {
                        expr = std::any_cast<std::string>(
                            parameters.at("expression"));
                      } catch (...) {
                      }
                    }

                    LOG_DEBUG("[LOOP] " << id << " mode=" << mode);

                    // Dynamic output collection
                    std::map<std::string, std::vector<std::any>> aggregators;
                    auto run_iteration = [&](int idx, std::any current_item =
                                                          std::any()) {
                      LOG_DEBUG("[LOOP] " << id << " " << mode << " iteration "
                                          << idx);
                      db.write(id + ".index", idx);
                      if (current_item.has_value()) {
                        db.write(id + ".item", current_item);
                      }

                      tf::Taskflow iter_tf;
                      this->build_flow(iter_tf, node_cfg, db);
                      executor.run(iter_tf).wait();

                      if (node_cfg.contains("dynamic_outputs")) {
                        for (auto& out_p : node_cfg["dynamic_outputs"]) {
                          std::string pin_name =
                              out_p.contains("name") ? out_p["name"] : "";
                          // "source" matches the key used during save
                          std::string src_path =
                              out_p.contains("source") ? out_p["source"] : "";

                          if (!src_path.empty()) {
                            // remove the $ prefix
                            if (src_path[0] == '$')
                              src_path = src_path.substr(1);

                            if (db.has(src_path)) {
                              aggregators[pin_name].push_back(
                                  db.read_any(src_path));
                            }
                          }
                        }
                      }
                    };

                    // Handle Input collection
                    std::any collection;
                    if (input_mapping.count("collection")) {
                      collection = db.read_any(input_mapping.at("collection"));
                    }

                    auto try_iterate_collection = [&](const std::any& coll,
                                                      auto type_ptr) -> bool {
                      using T = decltype(type_ptr);
                      if (coll.type() == typeid(std::vector<T>)) {
                        const auto& vec =
                            std::any_cast<const std::vector<T>&>(coll);
                        for (size_t i = 0; i < vec.size(); ++i) {
                          run_iteration(i, std::any(vec[i]));
                        }
                        return true;
                      }
                      return false;
                    };

                    if (mode == "for_count") {
                      if (step == 0) step = 1;
                      for (int i = min_iter;
                           (step > 0) ? (i < max_iter) : (i > max_iter);
                           i += step) {
                        run_iteration(i);
                      }
                    } else if (mode == "for_each") {
                      if (collection.type() == typeid(std::vector<std::any>)) {
                        const auto& vec =
                            std::any_cast<const std::vector<std::any>&>(
                                collection);
                        for (size_t i = 0; i < vec.size(); ++i)
                          run_iteration(i, vec[i]);
                      } else {
                        bool handled = false;
                        handled |= try_iterate_collection(collection, int{});
                        handled |= try_iterate_collection(collection, float{});
                        handled |= try_iterate_collection(collection, double{});
                        handled |=
                            try_iterate_collection(collection, std::string{});
                        handled |=
                            try_iterate_collection(collection, cv::Mat{});

                        if (!handled) {
                          LOG_ERROR("[LOOP] "
                                    << id << " Unsupported collection type: "
                                    << collection.type().name()
                                    << " Skipping loop.");
                        }
                      }
                    } else if (mode == "while_expr") {
                      int iter = 0;
                      const int HARD_LIMIT = 10000;
                      while (iter < HARD_LIMIT) {
                        if (!expr.empty() &&
                            ConditionEvaluator::evaluate_expression(db, expr) !=
                                ConditionEvaluator::EvalResult::TRUE) {
                          LOG_INFO(
                              "[LOOP] "
                              << id
                              << " while condition false or error, exiting");
                          break;
                        }
                        run_iteration(iter);
                        ++iter;
                      }
                    }
                    // Write aggregated outputs
                    for (auto& [pin_name, vec] : aggregators) {
                      db.write(id + "." + pin_name, std::any(vec));
                    }
                  })
                  .name(id);
        }
      } else if (type == "Condition") {
        // Conditional tasking: returns 0 for True branch, 1 for False branch
        task =
            flow.emplace([this, id, input_mapping, parameters, &db]() -> int {
                  bool result = this->execute_node_module(
                      "Condition", id, input_mapping, parameters, db);
                  LOG_DEBUG("[Condition "
                            << id << "] => "
                            << (result ? "TRUE (0)" : "FALSE (1)"));
                  return result ? 0 : 1;
                })
                .name(id);
      } else {
        // Create standard task logic
        task = flow.emplace([this, id, type, input_mapping, parameters, &db]() {
                     auto start = std::chrono::high_resolution_clock::now();
                     this->execute_node_module(type, id, input_mapping,
                                               parameters, db);
                     auto end = std::chrono::high_resolution_clock::now();
                     auto ms =
                         std::chrono::duration_cast<std::chrono::milliseconds>(
                             end - start)
                             .count();
                     db.record_metric(id, ms);
                   })
                   .name(id);
      }

      id_to_task[id] = task;
    }

    // --- Phase 2: Automatically establish topological connections between
    // tasks ---

    // First pass: Handle Condition nodes specially.
    // Conditional tasking requires successors to be added in order:
    //   precede index 0 = True branch, index 1 = False branch
    for (auto& node_cfg : config["children"]) {
      std::string node_id = node_cfg.at("id").get<std::string>();
      std::string node_type = node_cfg.value("type", "Default");

      if (node_type == "Condition" && id_to_task.count(node_id)) {
        // Find which downstream nodes connect to True vs False
        std::string true_key = node_id + ".True";
        std::string false_key = node_id + ".False";

        std::vector<tf::Task> true_successors;
        std::vector<tf::Task> false_successors;

        for (auto& other_cfg : config["children"]) {
          std::string other_id = other_cfg.at("id").get<std::string>();
          if (other_id == node_id || !id_to_task.count(other_id)) continue;

          // Check flow_links for Condition True/False connections
          if (other_cfg.contains("flow_links")) {
            for (auto& fl : other_cfg["flow_links"]) {
              std::string val = fl.get<std::string>();
              if (val == true_key || val == node_id + ".True_Out") {
                true_successors.push_back(id_to_task[other_id]);
              } else if (val == false_key || val == node_id + ".False_Out") {
                false_successors.push_back(id_to_task[other_id]);
              }
            }
          }
          // Also check inputs (backward compatibility)
          if (other_cfg.contains("inputs")) {
            for (auto& el : other_cfg["inputs"].items()) {
              std::string val = el.value().get<std::string>();
              if (val == true_key || val == node_id + ".True_Out") {
                true_successors.push_back(id_to_task[other_id]);
              } else if (val == false_key || val == node_id + ".False_Out") {
                false_successors.push_back(id_to_task[other_id]);
              }
            }
          }
        }

        // Create True and False router tasks to enforce strict indexing
        // (0=True, 1=False)
        auto true_router = flow.emplace([]() {}).name(node_id + "_True_Router");
        auto false_router =
            flow.emplace([]() {}).name(node_id + "_False_Router");

        // The condition Task MUST precede exactly these two routers in this
        // order: Index 0 -> True_Router, Index 1 -> False_Router
        id_to_task[node_id].precede(true_router, false_router);

        // Attach actual successors to the respective routers
        for (auto& t : true_successors) {
          true_router.precede(t);
          LOG_VERBOSE("[BINDING] Condition '" << node_id << "' True -> "
                                              << t.name());
        }
        for (auto& f : false_successors) {
          false_router.precede(f);
          LOG_VERBOSE("[BINDING] Condition '" << node_id << "' False -> "
                                              << f.name());
        }
      }
    }

    // Recursive helper to Link nodes and their descendants' dependencies
    // to their siblings in THIS builder's context
    std::function<void(const nlohmann::json&)> link_data_deps =
        [&](const nlohmann::json& c) {
          if (c.contains("children") && c["children"].is_array()) {
            for (auto& child : c["children"]) link_data_deps(child);
          }

          std::string current_id = c.at("id").get<std::string>();
          if (c.contains("inputs") && c["inputs"].is_object()) {
            for (auto& el : c["inputs"].items()) {
              std::string key_str = el.value().get<std::string>();

              // Skip links already handled by Condition special pass
              if (key_str.find(".True") != std::string::npos ||
                  key_str.find(".False") != std::string::npos) {
                continue;
              }

              // Resolve producer to its sibling ancestor
              if (data_producer_map.count(key_str)) {
                std::string producer_id = data_producer_map[key_str];
                std::string res_producer_id = producer_id;
                while (!id_to_task.count(res_producer_id) &&
                       node_to_container.count(res_producer_id)) {
                  res_producer_id = node_to_container[res_producer_id];
                }

                // Resolve consumer to its sibling ancestor
                std::string res_consumer_id = current_id;
                while (!id_to_task.count(res_consumer_id) &&
                       node_to_container.count(res_consumer_id)) {
                  res_consumer_id = node_to_container[res_consumer_id];
                }

                // Only link if both resolved nodes are in THIS builder's
                // context AND they are different tasks
                if (id_to_task.count(res_producer_id) &&
                    id_to_task.count(res_consumer_id) &&
                    res_producer_id != res_consumer_id) {
                  id_to_task[res_producer_id].precede(
                      id_to_task[res_consumer_id]);
                  LOG_VERBOSE("[BINDING DATA] " << key_str << ": "
                                                << res_producer_id << " -> "
                                                << res_consumer_id);
                }
              }
            }
          }
        };

    if (config.contains("children") && config["children"].is_array()) {
      for (auto& node_cfg : config["children"]) link_data_deps(node_cfg);
    }

    // Third pass: Flow links
    std::function<void(const nlohmann::json&)> link_flow_deps =
        [&](const nlohmann::json& c) {
          if (c.contains("children") && c["children"].is_array()) {
            for (auto& child : c["children"]) link_flow_deps(child);
          }

          std::string current_id = c.at("id").get<std::string>();
          if (c.contains("flow_links") && c["flow_links"].is_array()) {
            for (auto& fl : c["flow_links"]) {
              std::string source_full = fl.get<std::string>();

              if (source_full.find(".True") != std::string::npos ||
                  source_full.find(".False") != std::string::npos) {
                continue;
              }

              size_t dot = source_full.find('.');
              if (dot == std::string::npos) continue;
              std::string producer_id = source_full.substr(0, dot);

              // Resolve producer to its sibling ancestor
              std::string res_producer_id = producer_id;
              while (!id_to_task.count(res_producer_id) &&
                     node_to_container.count(res_producer_id)) {
                res_producer_id = node_to_container[res_producer_id];
              }

              // Resolve consumer to its sibling ancestor
              std::string res_consumer_id = current_id;
              while (!id_to_task.count(res_consumer_id) &&
                     node_to_container.count(res_consumer_id)) {
                res_consumer_id = node_to_container[res_consumer_id];
              }

              if (id_to_task.count(res_producer_id) &&
                  id_to_task.count(res_consumer_id) &&
                  res_producer_id != res_consumer_id) {
                id_to_task[res_producer_id].precede(
                    id_to_task[res_consumer_id]);
                LOG_VERBOSE("[BINDING FLOW] " << producer_id << " -> "
                                              << current_id << " (promoted to "
                                              << res_producer_id << " -> "
                                              << res_consumer_id << ")");
              }
            }
          }
        };

    if (config.contains("children") && config["children"].is_array()) {
      for (auto& node_cfg : config["children"]) link_flow_deps(node_cfg);
    }
  }
}

// Explicit template instantiations
template void WorkflowEngine::build_flow<tf::Taskflow>(
    tf::Taskflow&, const nlohmann::json&, Blackboard&,
    WorkflowEngine::GraphPass);

void WorkflowEngine::execute_graph() {
  if (taskflow.empty()) {
    LOG_ERROR("[Engine Error] Taskflow graph is empty. Cannot execute.");
    return;
  }

  LOG_INFO(">>> Taskflow scheduling engine started (Parallelism: "
           << std::thread::hardware_concurrency() << ") <<<\n");
  executor.run(taskflow).wait();
  LOG_INFO(">>> Workflow execution finished <<<");
}

// Runs a hardcoded Taskflow graph for basic validation
void WorkflowEngine::run_demo(Blackboard& db) {
  tf::Taskflow tf;
  tf::Executor executor;

  auto [A, B, C, D] = tf.emplace([]() { LOG_INFO("Task A (Load)"); },
                                 []() { LOG_INFO("Task B (Process 1)"); },
                                 []() { LOG_INFO("Task C (Process 2)"); },
                                 []() { LOG_INFO("Task D (Save)"); });

  A.precede(B, C);  // After A, B and C run in parallel
  B.precede(D);
  C.precede(D);

  executor.run(tf).wait();
}

bool WorkflowEngine::execute_node_module(
    const std::string& type, const std::string& id,
    const std::unordered_map<std::string, std::string>& input_mapping,
    const std::unordered_map<std::string, std::any>& parameters,
    Blackboard& db) {
  LOG_DEBUG("[EXECUTING] " << id << " (" << type << ") started running...");

  try {
    auto module = ModuleFactory::create_module(type);
    if (module) {
      module->id = id;
      module->input_mapping = input_mapping;
      for (auto& [k, v] : parameters) {
        module->set_parameter(k, v);
      }
      module->execute(db);

    } else {
      LOG_ERROR("[Engine Error] Unrecognized module type: " << type);
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[Engine Error] Exception in " << id << ": " << e.what());
  }

  bool ret = false;
  if (!db.has(id + ".__result")) {
    db.write(id + ".__result", ret);
  } else
    ret = db.read<bool>(id + ".__result");

  return ret;
}
