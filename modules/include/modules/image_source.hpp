#pragma once

#include "core/factory.hpp"

class ImageSource : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override;
};
