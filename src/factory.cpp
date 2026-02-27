#include "factory.hpp"
#include <iostream>
#include <thread>

// 示例积木 A：模拟传感器加载
class SensorModule : public BaseModule {
public:
  void execute(Blackboard &db) override {
    std::cout << "[SensorModule] 采集传感器数据..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    db.write("raw_data", 100);
  }
};

class FilterModule : public BaseModule {
public:
  void execute(Blackboard &db) override {
    std::cout << "[FilterModule] 处理滤波..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int raw = db.read<int>("raw_data");
    db.write("filtered_data", raw * 2);
  }
};

class LoggerModule : public BaseModule {
public:
  void execute(Blackboard &db) override {
    std::cout << "[LoggerModule] 记录日志..." << std::endl;
    int raw = db.read<int>("raw_data");
    int filtered = db.read<int>("filtered_data");
    std::cout << "  > 原始数据 = " << raw << ", 滤波后 = " << filtered << "\n";
  }
};

class CustomAIModule : public BaseModule {
public:
  void execute(Blackboard &db) override {
    std::cout << "[CustomAIModule] 运行推理模型..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    db.write("ai_result", std::string("Detected Object"));
  }
};

REGISTER_MODULE("Sensor", SensorModule)
REGISTER_MODULE("Filter", FilterModule)
REGISTER_MODULE("Logger", LoggerModule)
REGISTER_MODULE("CustomAI", CustomAIModule)

// 全局注册表实现
std::unordered_map<std::string, ModuleFactory::CreatorFunc> &
ModuleFactory::getRegistry() {
  static std::unordered_map<std::string, CreatorFunc> registry;
  return registry;
}

bool ModuleFactory::registerCreator(const std::string &type, CreatorFunc func) {
  getRegistry()[type] = func;
  return true;
}

std::unique_ptr<BaseModule>
ModuleFactory::createModule(const std::string &type) {
  auto &registry = getRegistry();
  auto it = registry.find(type);
  if (it != registry.end()) {
    return it->second();
  }

  std::cerr << "[Warning] Unrecognized module type: " << type << std::endl;
  return nullptr;
}
