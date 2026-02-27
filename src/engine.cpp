#include "engine.hpp"
#include "factory.hpp"
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <taskflow/taskflow.hpp>
#include <thread>
#include <unordered_map>

using json = nlohmann::json;

// 实现 RunFromJson：数据驱动的核心逻辑
void WorkflowEngine::RunFromJson(const std::string &jsonPath, Blackboard &db) {
  std::ifstream file(jsonPath);
  if (!file.is_open()) {
    std::cerr << "[Engine Error] 找不到配置文件: " << jsonPath << std::endl;
    return;
  }

  json config;
  try {
    file >> config;
  } catch (json::parse_error &e) {
    std::cerr << "[Engine Error] JSON 解析语法错误: " << e.what() << std::endl;
    return;
  }

  tf::Taskflow taskflow("TaskNodeFlow_Dynamic");
  tf::Executor executor;

  // 存储 ID 与 Taskflow 任务的对应关系
  std::unordered_map<std::string, tf::Task> id_to_task;
  // 存储 数据Key 与 产生该数据的节点ID 的映射（用于自动依赖分析）
  std::unordered_map<std::string, std::string> data_producer_map;

  // --- 阶段 1: 创建所有任务节点 ---
  if (config.contains("children") && config["children"].is_array()) {
    for (auto &node_cfg : config["children"]) {
      std::string id = node_cfg.at("id").get<std::string>();
      std::string type = node_cfg.value("type", "Default");

      // 创建任务逻辑
      auto task =
          taskflow
              .emplace([id, type, &db]() {
                auto start = std::chrono::high_resolution_clock::now();

                std::cout << "\n[EXECUTING] " << id << " (" << type
                          << ") 开始运行..." << std::endl;

                auto module = ModuleFactory::createModule(type);
                if (module) {
                  module->execute(db);
                } else {
                  std::cerr << "[Engine Error] 未识别的组件类型: " << type
                            << std::endl;
                }

                auto end = std::chrono::high_resolution_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              end - start)
                              .count();

                // 将执行时间存入 Blackboard
                db.record_metric(id, ms);
              })
              .name(id);

      id_to_task[id] = task;

      // 记录该节点产出的数据 Key
      if (node_cfg.contains("outputs")) {
        for (auto &out_key : node_cfg["outputs"]) {
          data_producer_map[out_key.get<std::string>()] = id;
        }
      }
    }

    // --- 阶段 2: 自动建立任务间的拓扑连接 ---
    for (auto &node_cfg : config["children"]) {
      std::string current_id = node_cfg.at("id").get<std::string>();

      if (node_cfg.contains("inputs")) {
        for (auto &in_key : node_cfg["inputs"]) {
          std::string key_str = in_key.get<std::string>();

          // 如果当前输入在生产者名单里，说明存在依赖
          if (data_producer_map.count(key_str)) {
            std::string producer_id = data_producer_map[key_str];

            // 设置顺序：生产者 precede 消费者
            id_to_task[producer_id].precede(id_to_task[current_id]);
            std::cout << "[BINDING] 数据 '" << key_str << "': " << producer_id
                      << " -> " << current_id << std::endl;
          }
        }
      }
    }
  }

  // --- 阶段 3: 并行执行 ---
  std::cout << "\n>>> Taskflow 调度引擎启动 (并行度: "
            << std::thread::hardware_concurrency() << ") <<<" << std::endl;
  executor.run(taskflow).wait();
  std::cout << ">>> 流程执行完毕 <<<\n" << std::endl;
}

// 实现 RunDemo：用于快速测试的硬编码逻辑
void WorkflowEngine::RunDemo(Blackboard &db) {
  tf::Taskflow tf;
  tf::Executor executor;

  auto [A, B, C, D] = tf.emplace([]() { std::cout << "Task A (Load)\n"; },
                                 []() { std::cout << "Task B (Process 1)\n"; },
                                 []() { std::cout << "Task C (Process 2)\n"; },
                                 []() { std::cout << "Task D (Save)\n"; });

  A.precede(B, C); // A 跑完后 B 和 C 并行
  B.precede(D);    // B 跑完
  C.precede(D);    // C 跑完后 D 汇总

  executor.run(tf).wait();
}
