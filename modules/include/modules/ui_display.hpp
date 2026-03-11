#pragma once

#include "core/factory.hpp"

class UI_Display : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override;
};
