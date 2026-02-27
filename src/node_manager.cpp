#include "node_manager.hpp"
#include <fstream>
#include <imgui.h>
#include <iostream>

NodeManager::NodeManager() {
  // 初始创建一个示例节点
  nodes.push_back({ed::NodeId(GetNextId()), "Camera_Source", "Sensor",
                   ed::PinId(GetNextId()), ed::PinId(GetNextId())});
}

void NodeManager::ShowAssetPanel() {
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(250, 600), ImGuiCond_FirstUseEver);

  ImGui::Begin("Asset Library");

  // 定义我们所有的积木类型
  std::vector<std::string> categories = {"Sensor", "Filter", "Logger",
                                         "CustomAI"};

  for (const auto &cat : categories) {
    // 创建一个可拖动的项目
    ImGui::Selectable(cat.c_str());

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      // 传递积木类型字符串作为负载
      ImGui::SetDragDropPayload("DND_NODE_TYPE", cat.c_str(), cat.size() + 1);

      // 拖动时的预览
      ImGui::Text("Creating %s...", cat.c_str());
      ImGui::EndDragDropSource();
    }
  }
  ImGui::End();
}

void NodeManager::Draw() {
  ShowAssetPanel();

  ed::Begin("Node Editor Canvas");

  for (auto &node : nodes) {
    ed::BeginNode(node.id);
    ImGui::Text("%s", node.name.c_str());

    // 左侧输入点
    ed::BeginPin(node.inputPin, ed::PinKind::Input);
    ImGui::Text("-> In");
    ed::EndPin();

    ImGui::SameLine();

    // 右侧输出点
    ed::BeginPin(node.outputPin, ed::PinKind::Output);
    ImGui::Text("Out ->");
    ed::EndPin();
    ed::EndNode();
  }

  for (auto &link : links) {
    ed::Link(link.id, link.startPin, link.endPin);
  }

  // 处理连线交互
  if (ed::BeginCreate()) { // 只有返回 true 才能继续
    ed::PinId startPin, endPin;
    if (ed::QueryNewLink(&startPin, &endPin)) {
      if (ed::AcceptNewItem()) {
        links.push_back({ed::LinkId(GetNextId()), startPin, endPin});
      }
    }
    ed::EndCreate();
  }

  // --- 处理删除逻辑 ---
  if (ed::BeginDelete()) {
    // 1. 处理删除连线
    ed::LinkId linkId;
    while (ed::QueryDeletedLink(&linkId)) {
      if (ed::AcceptDeletedItem()) {
        links.erase(std::remove_if(links.begin(), links.end(),
                                   [linkId](const LinkObject &l) {
                                     return l.id == linkId;
                                   }),
                    links.end());
      }
    }

    // 2. 处理删除节点
    ed::NodeId nodeId;
    while (ed::QueryDeletedNode(&nodeId)) {
      if (ed::AcceptDeletedItem()) {
        // 先找出被删节点的所有引脚
        std::vector<ed::PinId> pinsToRemove;
        for (const auto &node : nodes) {
          if (node.id == nodeId) {
            pinsToRemove.push_back(node.inputPin);
            pinsToRemove.push_back(node.outputPin);
            break;
          }
        }

        // 自动清理所有相关的连线
        links.erase(
            std::remove_if(links.begin(), links.end(),
                           [&pinsToRemove](const LinkObject &l) {
                             return std::find(pinsToRemove.begin(),
                                              pinsToRemove.end(), l.startPin) !=
                                        pinsToRemove.end() ||
                                    std::find(pinsToRemove.begin(),
                                              pinsToRemove.end(),
                                              l.endPin) != pinsToRemove.end();
                           }),
            links.end());

        // 从 nodes 向量中移除
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                                   [nodeId](const NodeObject &n) {
                                     return n.id == nodeId;
                                   }),
                    nodes.end());
      }
    }
    ed::EndDelete();
  }

  ed::End();

  // 处理拖拽新增节点
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload("DND_NODE_TYPE")) {
      const char *typeStr = (const char *)payload->Data;

      // 1. 获取当前屏幕鼠标位置
      ImVec2 mousePos = ImGui::GetMousePos();

      // 2. 将屏幕坐标转换为节点编辑器的画布坐标
      ImVec2 canvasPos = ed::ScreenToCanvas(mousePos);

      // 创建新节点逻辑
      int id = GetNextId();
      nodes.push_back({ed::NodeId(id),
                       std::string(typeStr) + "_" + std::to_string(id),
                       std::string(typeStr), ed::PinId(GetNextId()),
                       ed::PinId(GetNextId())});

      // 3. 设置位置
      ed::SetNodePosition(ed::NodeId(id), canvasPos);
    }
    ImGui::EndDragDropTarget();
  }
}

void NodeManager::Save(const std::string &path) {
  nlohmann::json root;
  root["children"] = nlohmann::json::array();

  // 建立 PinID 到 NodeName 的反向查找表
  std::map<unsigned long long, std::string> pinToNode;
  for (auto &n : nodes) {
    pinToNode[n.inputPin.Get()] = n.name;
    pinToNode[n.outputPin.Get()] = n.name;
  }

  for (auto &node : nodes) {
    nlohmann::json n_json;
    n_json["id"] = node.name;
    n_json["type"] = node.type;
    n_json["inputs"] = nlohmann::json::array();
    n_json["outputs"] = {node.name +
                         "_out"}; // 简单起见，输出 Key 设为 节点名_out

    // 查找谁连到了我的输入 Pin
    for (auto &link : links) {
      if (link.endPin == node.inputPin) {
        std::string sourceNode = pinToNode[link.startPin.Get()];
        n_json["inputs"].push_back(sourceNode + "_out");
      }
    }
    root["children"].push_back(n_json);
  }

  std::ofstream o(path);
  o << root.dump(4);
  std::cout << "Saved to " << path << std::endl;
}
