#pragma once

#include "core/factory.hpp"

class BoundingBoxDetector : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override;
};
