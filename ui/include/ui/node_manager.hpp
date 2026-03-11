#pragma once

#include <imgui.h>
#include <imgui_node_editor.h>

#include <any>
#include <deque>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/blackboard.hpp"
#include "core/type_system.hpp"

namespace ed = ax::NodeEditor;

// Internal structure: Describes a UI pin
struct Pin {
  ed::PinId id;
  std::string name;
  std::string type;
  ed::PinKind kind;
  bool is_flow = false;  // true for flow/control pins

  // to save collection path for loop output, e.g. "$InternalNode.ResultPin"
  std::string source_path = "";
};

// Internal structure: Describes a UI node
struct NodeObject {
  ed::NodeId id;
  std::string name;
  std::string type;  // Stores the module type
  std::vector<Pin> inputs;
  std::vector<Pin> outputs;
  ImVec2 pos;
  ImVec2 size;  // Used specifically for Groups to store their bounds
  std::unordered_map<std::string, std::any> parameters;

  // Grouping / Subflow features
  bool is_group = false;
  ed::NodeId parent_id = 0;  // The Group ID this node belongs to

  int dynamic_pin_counter =
      0;  // for dynamic pins naming; only plus; never minus
};

// Internal structure: Describes a link
struct LinkObject {
  ed::LinkId id;
  ed::PinId start_pin;
  ed::PinId end_pin;
  bool is_flow_link = false;  // true for flow/control connections
};

class NodeManager {
 public:
  NodeManager();
  void show_asset_panel();
  void draw_property_inspector();
  void draw();
  bool save(const std::string& path, bool is_auto_save = false);
  void load(const std::string& path);

  bool is_dirty() const { return dirty; }
  void mark_dirty() { dirty = true; }
  void clear_dirty() { dirty = false; }

  std::string get_static_type_by_path(const std::string& path);

 private:
  bool validate_pipeline_before_save(std::string& out_error);

  void ensure_loop_proxy_nodes(NodeObject& parent_loop);
  void sync_loop_exit_pins(NodeObject& proxy_node);

  std::vector<NodeObject> nodes;
  std::vector<LinkObject> links;
  int next_id = 1;
  bool dirty = false;
  ed::NodeId selected_node_for_properties = 0;
  ed::NodeId current_view_parent_id =
      0;  // Tracks which sub-graph the UI is currently inside
  std::unordered_set<uintptr_t>
      visible_pins;  // Tracks output pins toggled for image preview
  std::unordered_set<uintptr_t>
      initialized_nodes;              // Tracks nodes forced to editor position
  bool is_dragging_flow_pin = false;  // Show flow pins only

  // Renaming state
  ed::NodeId renaming_node_id = 0;
  char renaming_name_buf[128] = "";

  ed::PinId renaming_pin_id = 0;
  char renaming_pin_buf[64] = "";

  void reassign_duplicate_ids();

  // Grouping / Subflow / Loop features
  void group_selected_nodes(const std::string& group_type = "Subflow");
  void ungroup_selected_node();

  // Undo / Redo snapshot system
  struct Snapshot {
    std::vector<NodeObject> nodes;
    std::vector<LinkObject> links;
    int next_id;
  };
  std::deque<Snapshot> undo_stack;
  std::deque<Snapshot> redo_stack;
  static constexpr size_t MAX_UNDO_HISTORY = 50;
  void push_undo_snapshot();
  void undo();
  void redo();

  int get_next_id() { return next_id++; }
  uintptr_t get_hash_id(const std::string& str) {
    return std::hash<std::string>{}(str);
  }
  Pin* find_pin(ed::PinId id, ed::NodeId* out_node = nullptr);
  NodeObject* find_node(ed::NodeId id);

  // UI Display Constants
  static constexpr const char* UI_BTN_EXPAND = " [+] Expand";
  static constexpr const char* UI_BTN_SHOW_IMG = " (o) ";
  static constexpr const char* UI_BTN_HIDE_IMG = " (x) ";
  static constexpr const char* UI_PIN_ARROW = "->";
  static constexpr std::string_view UI_IMG_PIN_TYPE = TypeSystem::IMAGE;
  static constexpr const char* UI_FLOW_IN_PIN_NAME = "__flow_in";
  static constexpr const char* UI_FLOW_OUT_PIN_NAME = "__flow_out";

  std::string get_unique_node_name(const std::string& base_type);
  void generate_pins_for_node(NodeObject& node);

  // Toast notification state
  std::string toast_message;
  float toast_timer = 0.0f;
  void show_toast(const std::string& msg, float duration = 3.0f);

  static std::string expanded_image_key;
  static bool show_expanded_image;
};
