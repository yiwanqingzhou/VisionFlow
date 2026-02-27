#pragma once
#include <imgui_node_editor.h>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ed = ax::NodeEditor;

// 内部结构：描述一个 UI 节点
struct NodeObject {
  ed::NodeId id;
  std::string name;
  std::string type; // Added to store the module type
  ed::PinId inputPin;
  ed::PinId outputPin;
  ImVec2 pos;
};

// 内部结构：描述一条连线
struct LinkObject {
  ed::LinkId id;
  ed::PinId startPin;
  ed::PinId endPin;
};

class NodeManager {
public:
  NodeManager();
  void ShowAssetPanel();
  void Draw();
  void Save(const std::string &path);
  void Load(const std::string &path);

private:
  std::vector<NodeObject> nodes;
  std::vector<LinkObject> links;
  int nextId = 1;

  int GetNextId() { return nextId++; }
};
