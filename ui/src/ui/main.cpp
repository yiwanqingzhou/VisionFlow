#include <GLFW/glfw3.h>
#include <imgui.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "core/blackboard.hpp"
#include "core/engine.hpp"
#include "core/factory.hpp"
#include "ui/node_manager.hpp"
#include "ui/texture_manager.hpp"
#include "ui/ui_manager.hpp"
#include "utils/global_config.hpp"
#include "utils/logger.hpp"

int main() {
  // 1. Load Global Settings first so we know where to store layouts
  GlobalConfig& config = GlobalConfig::get();

  // Apply log level from config
  vf::Logger::instance().setLevel(config.get_data().global_log_level);

  // 2. Environment initialization
  GLFWwindow* window = UIManager::init();
  if (!window) return -1;

  // 1.5 Sync persistent UI setups into temporary proxies before launching
  std::error_code ec;
  if (std::filesystem::exists(config.get_data().imgui_ini_file)) {
    std::filesystem::copy_file(
        config.get_data().imgui_ini_file, config.get_data().imgui_ini_tmp,
        std::filesystem::copy_options::overwrite_existing, ec);
  }
  if (std::filesystem::exists(config.get_data().editor_file)) {
    std::filesystem::copy_file(
        config.get_data().editor_file, config.get_data().editor_tmp,
        std::filesystem::copy_options::overwrite_existing, ec);
  }
  if (std::filesystem::exists(config.get_data().pipeline_layout)) {
    std::filesystem::copy_file(
        config.get_data().pipeline_layout,
        config.get_data().pipeline_layout_tmp,
        std::filesystem::copy_options::overwrite_existing, ec);
  }

  // Set ImGui Ini file path *after* context creation in init()
  ImGui::GetIO().IniFilename = config.get_data().imgui_ini_tmp.c_str();

  // 3. Node editor configuration
  ax::NodeEditor::Config ed_config;
  ed_config.SettingsFile =
      config.get_data().editor_tmp.c_str();  // Auto-save UI layout to tmp
  ax::NodeEditor::EditorContext* editor_ctx =
      ax::NodeEditor::CreateEditor(&ed_config);

  // 4. Core objects
  NodeManager node_mgr;
  Blackboard blackboard;
  WorkflowEngine workflow_engine;

  GLuint graph_texture = 0;
  int graph_w = 0;
  int graph_h = 0;
  bool show_graph_modal = false;
  bool show_graph_details = true;

  if (config.get_data().auto_load) {
    ax::NodeEditor::SetCurrentEditor(editor_ctx);
    node_mgr.load(config.get_data().pipeline_config);
    ax::NodeEditor::SetCurrentEditor(nullptr);
  }

  bool should_close = false;

  // Inject UI visualization dependency into the pure logic layer
  BaseModule::visualization_callback = [](std::string key, cv::Mat img) {
    if (!img.empty()) {
      TextureManager::get_instance().submit_image(key, img);
    }
  };

  while (!should_close) {
    UIManager::start_frame();

    // Process any CV Mats pushed by background modules into OpenGL textures
    TextureManager::get_instance().process_pending();

    // --- UI Panel A: Console ---
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1420, 140), ImGuiCond_FirstUseEver);
    ImGui::Begin("Pipeline Settings");

    // Intercept Window Close *after* polling events
    if (glfwWindowShouldClose(window)) {
      if (node_mgr.is_dirty()) {
        glfwSetWindowShouldClose(window, GLFW_FALSE);
        ImGui::OpenPopup("Unsaved Changes (Exit)");
      } else {
        should_close = true;
      }
    }

    // Workspace Actions
    if (ImGui::Button("Reload (from JSON)", ImVec2(180, 40))) {
      ImGui::OpenPopup("Reload JSON");
    }

    ImGui::SameLine();

    if (ImGui::Button("Show Flow", ImVec2(120, 40))) {
      LOG_INFO("Generating Taskflow Graph...");

      // Save to a temporary file for checking, preserving dirty state
      std::string temp_path = "/tmp/tasknodeflow_tmp.json";
      ax::NodeEditor::SetCurrentEditor(editor_ctx);
      node_mgr.save(temp_path, true);
      ax::NodeEditor::SetCurrentEditor(nullptr);

      // Build graph explicitly from the temporary file
      if (workflow_engine.build_graph_from_json(temp_path, blackboard,
                                                show_graph_details)) {
        // Render PNG synchronously
        int ret = system("dot -Tpng taskflow_graph.dot -o taskflow_graph.png");
        if (ret == 0) {
          if (graph_texture) {
            glDeleteTextures(1, &graph_texture);  // Delete old texture if any
          }
          if (UIManager::LoadTextureFromFile(
                  "taskflow_graph.png", &graph_texture, &graph_w, &graph_h)) {
            show_graph_modal = true;
            ImGui::OpenPopup("Taskflow Graph Viewer");
          } else {
            LOG_ERROR("Failed to load taskflow_graph.png into OpenGL texture.");
          }
        } else {
          LOG_ERROR(
              "Failed to generate taskflow graph. Make sure graphviz is "
              "installed.");
        }
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Save (to JSON)", ImVec2(180, 40))) {
      // 1. Save Logical Pipeline
      ax::NodeEditor::SetCurrentEditor(editor_ctx);
      bool save_success = node_mgr.save(config.get_data().pipeline_config);
      ax::NodeEditor::SetCurrentEditor(nullptr);

      if (save_success) {
        // 2. Flush Temporary UI Layouts back to Permanent Directory
        std::error_code ec;
        if (std::filesystem::exists(config.get_data().imgui_ini_tmp)) {
          std::filesystem::copy_file(
              config.get_data().imgui_ini_tmp, config.get_data().imgui_ini_file,
              std::filesystem::copy_options::overwrite_existing, ec);
        }
        if (std::filesystem::exists(config.get_data().editor_tmp)) {
          std::ifstream tmp_file(config.get_data().editor_tmp);
          if (tmp_file.is_open()) {
            nlohmann::json ui_root;
            try {
              tmp_file >> ui_root;
              std::ofstream out_file(config.get_data().editor_file);
              out_file << ui_root.dump(4);
            } catch (...) {
              // Fallback to pure copy if parsing fails
              std::filesystem::copy_file(
                  config.get_data().editor_tmp, config.get_data().editor_file,
                  std::filesystem::copy_options::overwrite_existing, ec);
            }
          }
        }

        // 3. Flush Pipeline Layout
        if (std::filesystem::exists(config.get_data().pipeline_layout_tmp)) {
          std::filesystem::copy_file(
              config.get_data().pipeline_layout_tmp,
              config.get_data().pipeline_layout,
              std::filesystem::copy_options::overwrite_existing, ec);
        }

        config.save();  // Save global config just in case
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("RUN WORKFLOW", ImVec2(180, 40))) {
      // Check for unsaved changes before running
      if (node_mgr.is_dirty()) {
        ImGui::OpenPopup("Unsaved Changes (Run)");
      } else {
        std::thread([&workflow_engine, &blackboard, &config]() {
          if (workflow_engine.build_graph_from_json(
                  config.get_data().pipeline_config, blackboard)) {
            workflow_engine.execute_graph();
          }
        }).detach();
      }
    }

    // --- Popup Modal for Unsaved Changes (Run) ---
    ImGui::SetNextWindowSize(ImVec2(450, 150), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Unsaved Changes (Run)", NULL)) {
      ImGui::Text("You have unsaved changes. Save before running?");
      ImGui::Spacing();
      ImGui::Spacing();

      if (ImGui::Button("Save and Run", ImVec2(130, 0))) {
        ax::NodeEditor::SetCurrentEditor(editor_ctx);
        bool save_success = node_mgr.save(config.get_data().pipeline_config);
        ax::NodeEditor::SetCurrentEditor(nullptr);

        if (save_success) {
          config.save();

          std::thread([&workflow_engine, &blackboard, &config]() {
            if (workflow_engine.build_graph_from_json(
                    config.get_data().pipeline_config, blackboard)) {
              workflow_engine.execute_graph();
            }
          }).detach();

          ImGui::CloseCurrentPopup();
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Run Without Saving", ImVec2(160, 0))) {
        std::thread([&workflow_engine, &blackboard, &config]() {
          if (workflow_engine.build_graph_from_json(
                  config.get_data().pipeline_config, blackboard)) {
            workflow_engine.execute_graph();
          }
        }).detach();

        ImGui::CloseCurrentPopup();
      }

      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(100, 0))) {
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    // --- Popup Modal for Unsaved Changes ---
    ImGui::SetNextWindowSize(ImVec2(450, 150), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Unsaved Changes", NULL)) {
      ImGui::Text("You have unsaved changes. Save before running?");
      ImGui::Spacing();
      ImGui::Spacing();

      if (ImGui::Button("Save and Run", ImVec2(130, 0))) {
        ax::NodeEditor::SetCurrentEditor(editor_ctx);
        bool save_success = node_mgr.save(config.get_data().pipeline_config);
        ax::NodeEditor::SetCurrentEditor(nullptr);

        if (save_success) {
          config.save();

          std::thread([&workflow_engine, &blackboard, &config]() {
            if (workflow_engine.build_graph_from_json(
                    config.get_data().pipeline_config, blackboard)) {
              workflow_engine.execute_graph();
            }
          }).detach();

          ImGui::CloseCurrentPopup();
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Run Without Saving", ImVec2(160, 0))) {
        std::thread([&workflow_engine, &blackboard, &config]() {
          if (workflow_engine.build_graph_from_json(
                  config.get_data().pipeline_config, blackboard)) {
            workflow_engine.execute_graph();
          }
        }).detach();

        ImGui::CloseCurrentPopup();
      }

      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(100, 0))) {
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    // --- Popup Modal for Reload Path ---
    ImGui::SetNextWindowSize(ImVec2(600, 150), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Reload JSON", NULL)) {
      ImGui::Text("Enter Pipeline JSON Path to Load:");

      static char path_buf[256] = "";
      static bool init_buf = false;
      if (!init_buf) {
        snprintf(path_buf, sizeof(path_buf), "%s",
                 config.get_data().pipeline_config.c_str());
        init_buf = true;
      }

      ImGui::SetNextItemWidth(-FLT_MIN);
      ImGui::InputText("##Path", path_buf, sizeof(path_buf));

      if (ImGui::Button("Load", ImVec2(120, 0))) {
        config.get_data().pipeline_config = path_buf;
        config.save();
        ax::NodeEditor::SetCurrentEditor(editor_ctx);
        node_mgr.load(config.get_data().pipeline_config);
        ax::NodeEditor::SetCurrentEditor(nullptr);
        ImGui::CloseCurrentPopup();
        init_buf = false;  // Reset for next open
      }

      ImGui::SetItemDefaultFocus();
      ImGui::SameLine();

      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
        init_buf = false;  // Reset for next open
      }
      ImGui::EndPopup();
    }

    // --- Popup Modal for Unsaved Changes on Exit ---
    ImGui::SetNextWindowSize(ImVec2(450, 150), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Unsaved Changes (Exit)", NULL)) {
      ImGui::Text("You have unsaved changes. Save before exiting?");
      ImGui::Spacing();
      ImGui::Spacing();

      if (ImGui::Button("Save and Exit", ImVec2(130, 0))) {
        ax::NodeEditor::SetCurrentEditor(editor_ctx);
        if (node_mgr.save(config.get_data().pipeline_config)) {
          ax::NodeEditor::SetCurrentEditor(nullptr);
          config.save();
          should_close = true;
          ImGui::CloseCurrentPopup();
        } else {
          ax::NodeEditor::SetCurrentEditor(nullptr);
          // Validation failed, let user see error toast and keep window open
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Exit Without Saving", ImVec2(160, 0))) {
        should_close = true;
        ImGui::CloseCurrentPopup();
      }

      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(100, 0))) {
        glfwSetWindowShouldClose(window, GLFW_FALSE);
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    // --- Popup Modal for Graph Viewer ---
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Appearing);
    if (show_graph_modal) {
      ImGui::OpenPopup("Taskflow Graph Viewer");
    }
    if (ImGui::BeginPopupModal("Taskflow Graph Viewer", &show_graph_modal)) {
      if (ImGui::Checkbox("Show Details (Recursive)", &show_graph_details)) {
        // Trigger regeneration
        std::string temp_path = config.get_data().pipeline_tmp;
        if (workflow_engine.build_graph_from_json(temp_path, blackboard,
                                                  show_graph_details)) {
          system("dot -Tpng taskflow_graph.dot -o taskflow_graph.png");
          if (graph_texture) {
            glDeleteTextures(1, &graph_texture);
          }
          UIManager::LoadTextureFromFile("taskflow_graph.png", &graph_texture,
                                         &graph_w, &graph_h);
        }
      }
      ImGui::Separator();

      if (graph_texture) {
        // Calculate scale to fit window if too large
        ImVec2 avail_size = ImGui::GetContentRegionAvail();
        float scale = 1.0f;
        if (graph_w > avail_size.x) scale = avail_size.x / graph_w;
        if (graph_h * scale > avail_size.y - 40)
          scale =
              (avail_size.y - 40.0f) / graph_h;  // reserve space for close btn

        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - graph_w * scale) *
                             0.5f);
        ImGui::Image((void*)(intptr_t)graph_texture,
                     ImVec2(graph_w * scale, graph_h * scale));
      } else {
        ImGui::Text("Failed to load graph texture.");
      }

      ImGui::Spacing();
      if (ImGui::Button("Close", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
        show_graph_modal = false;
      }
      ImGui::EndPopup();
    }

    ImGui::Text("Status: Running on Taskflow v3.x");
    ImGui::End();

    // --- UI Panel B: Node Canvas ---
    ax::NodeEditor::SetCurrentEditor(editor_ctx);
    node_mgr.draw();  // Draw all nodes and links
    ax::NodeEditor::SetCurrentEditor(nullptr);

    UIManager::end_frame(window);
  }

  // 4. Resource cleanup
  ax::NodeEditor::DestroyEditor(editor_ctx);
  UIManager::cleanup(window);

  return 0;
}
