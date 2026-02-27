#pragma once
#include "blackboard.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class BaseModule {
public:
  virtual ~BaseModule() = default;
  virtual void execute(Blackboard &db) = 0;
};

class ModuleFactory {
public:
  using CreatorFunc = std::function<std::unique_ptr<BaseModule>()>;

  static std::unique_ptr<BaseModule> createModule(const std::string &type);

  // 获取全局注册表 (使用 local static 避免 Static Initialization Order Fiasco)
  static std::unordered_map<std::string, CreatorFunc> &getRegistry();

  static bool registerCreator(const std::string &type, CreatorFunc func);
};

// 辅助注册类
class AutoRegisterModule {
public:
  AutoRegisterModule(const std::string &type, ModuleFactory::CreatorFunc func) {
    ModuleFactory::registerCreator(type, func);
  }
};

// 辅助注册宏：写在每个模块实现文件(cpp)的末尾
#define REGISTER_MODULE(TypeName, ClassName)                                   \
  static AutoRegisterModule g_register_##ClassName(                            \
      TypeName, []() { return std::make_unique<ClassName>(); });
