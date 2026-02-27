#pragma once
#include "blackboard.hpp"
#include <string>

class WorkflowEngine {
public:
  // 解析 JSON 并通过 Taskflow 执行
  static void RunFromJson(const std::string &jsonPath, Blackboard &db);

  // 之前 Demo 用的硬编码运行方式（可选保留）
  static void RunDemo(Blackboard &db);
};
