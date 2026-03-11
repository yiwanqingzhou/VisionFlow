#pragma once

#include "core/factory.hpp"

// This module is never executed natively via inner_execute() because
// WorkflowEngine intercepts "Loop" nodes and handles their execution directly
// using subflows. However, registering this dummy class allows the UI Property
// Inspector to automatically generate the parameter editing UI (combo boxes,
// input text).
class LoopNode : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override;
};
