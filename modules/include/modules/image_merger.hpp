#pragma once

#include "core/factory.hpp"

class ImageMerger : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override;
};
