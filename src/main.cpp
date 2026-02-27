#include "blackboard.hpp"
#include "engine.hpp"
#include "node_manager.hpp"
#include "ui_manager.hpp"
#include <imgui.h>
#include <thread>

int main() {
  // 1. 环境初始化
  GLFWwindow *window = UIManager::Init();
  if (!window)
    return -1;

  // 2. 节点编辑器配置
  ax::NodeEditor::Config config;
  config.SettingsFile = "Simple.json"; // 自动保存 UI 布局位置
  ax::NodeEditor::EditorContext *editor_ctx =
      ax::NodeEditor::CreateEditor(&config);

  // 3. 核心对象
  NodeManager node_mgr;
  Blackboard blackboard;

  while (!glfwWindowShouldClose(window)) {
    UIManager::StartFrame();

    // --- UI 面板 A：控制台 ---
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::Begin("Workflow Control");
    if (ImGui::Button("RUN WORKFLOW (Save & Exec)", ImVec2(-1, 40))) {
      // A. 将当前的 UI 连线保存为 JSON 格式的 DAG
      node_mgr.Save("../assets/active_pipeline.json");

      // B. 异步启动 Taskflow 引擎，避免 UI 卡顿
      std::thread([&blackboard]() {
        WorkflowEngine::RunFromJson("../assets/active_pipeline.json",
                                    blackboard);
      }).detach();
    }
    ImGui::Text("Status: Running on Taskflow v3.x");
    ImGui::End();

    // --- UI 面板 B：节点画布 ---
    ax::NodeEditor::SetCurrentEditor(editor_ctx);
    node_mgr.Draw(); // 调用 NodeManager 绘制所有节点和连线
    ax::NodeEditor::SetCurrentEditor(nullptr);

    UIManager::EndFrame(window);
  }

  // 4. 清理资源
  ax::NodeEditor::DestroyEditor(editor_ctx);
  UIManager::Cleanup(window);

  return 0;
}
