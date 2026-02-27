#include <fstream>
#include <imgui.h>
#include <imgui_node_editor.h>
#include <map>
#include <nlohmann/json.hpp>
#include <vector>
#include <iostream>

namespace ed = ax::NodeEditor;
using json = nlohmann::json;

// 假设我们有一个简单的结构来记录当前的 UI 状态
struct NodeInfo {
  std::string id;
  std::vector<std::string> inputs;
  std::vector<std::string> outputs;
};

void SaveUiToJson(const char *filename) {
  json root;
  root["children"] = json::array();

  // 1. 获取所有节点的 ID
  int nodeCount = ed::GetNodeCount();
  std::vector<ed::NodeId> nodeIds(nodeCount);
  ed::GetNodes(nodeIds.data(), nodeCount);

  // 2. 建立 Pin 到 Node 的映射（用于通过连线找依赖）
  // 在实际开发中，你通常会有个 Map 维护 PinID -> NodeID
  // 这里演示核心逻辑：
  for (auto nodeId : nodeIds) {
    json nodeJson;
    // 实际应用中，你需要从你的内存业务对象中获取真实的字符串 ID
    std::string stringId = "Node_" + std::to_string(nodeId.Get());

    nodeJson["id"] = stringId;
    nodeJson["type"] = "Atom";
    nodeJson["inputs"] = json::array();
    nodeJson["outputs"] = json::array();

    // 3. 查找连线：如果某个 Pin 是输入且有连线，则记录依赖
    // 提示：这部分通常配合你的 Link 数据结构实现

    root["children"].push_back(nodeJson);
  }

  // 4. 写入文件
  std::ofstream o(filename);
  o << std::setw(4) << root << std::endl;
  std::cout << "[UI] 已保存流程至: " << filename << std::endl;
}
