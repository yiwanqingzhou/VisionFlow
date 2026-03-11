#pragma once

#include "core/factory.hpp"

class EdgeDetector : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override;
};
