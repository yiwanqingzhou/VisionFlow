#pragma once

#include "core/factory.hpp"

class ColorConverter : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override;
};
