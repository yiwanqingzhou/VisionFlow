#pragma once

#include "core/factory.hpp"

class ConditionNode : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override;
};
