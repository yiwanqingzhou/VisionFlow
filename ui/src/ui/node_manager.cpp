#include "ui/node_manager.hpp"

#include <imgui.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <queue>
#include <unordered_map>

#include "core/condition_evaluator.hpp"
#include "core/factory.hpp"
#include "ui/node_manager.hpp"
#include "ui/texture_manager.hpp"
#include "utils/global_config.hpp"

namespace ed = ax::NodeEditor;

static const float MAX_UNDO_HISTORY = 50;

std::string NodeManager::expanded_image_key = "";
bool NodeManager::show_expanded_image = false;

NodeManager::NodeManager() {
  // Create an initial sample node
  std::string init_type = "ImageSource";
  std::string init_name = get_unique_node_name(init_type);
  NodeObject init_node = {
      ed::NodeId(get_next_id()), init_name,    init_type, {}, {},
      ImVec2(100, 100),          ImVec2(0, 0), {}};
  generate_pins_for_node(init_node);
  nodes.push_back(init_node);
}

std::string NodeManager::get_unique_node_name(const std::string& base_type) {
  int index = 1;
  while (true) {
    std::string proposed = base_type + "_" + std::to_string(index);
    uintptr_t proposed_id = get_hash_id(proposed);
    bool conflict = false;
    for (const auto& n : nodes) {
      if (n.name == proposed || n.id.Get() == proposed_id) {
        conflict = true;
        break;
      }
    }
    if (!conflict) return proposed;
    index++;
  }
}

Pin* NodeManager::find_pin(ed::PinId id, ed::NodeId* out_node) {
  for (auto& node : nodes) {
    for (auto& pin : node.inputs) {
      if (pin.id == id) {
        if (out_node) *out_node = node.id;
        return &pin;
      }
    }
    for (auto& pin : node.outputs) {
      if (pin.id == id) {
        if (out_node) *out_node = node.id;
        return &pin;
      }
    }
  }
  return nullptr;
}

NodeObject* NodeManager::find_node(ed::NodeId id) {
  for (auto& node : nodes) {
    if (node.id == id) return &node;
  }
  return nullptr;
}

// Helper to dynamically generate pins for a node from its ModuleMetadata
void NodeManager::generate_pins_for_node(NodeObject& node) {
  auto& registry = ModuleFactory::get_metadata_registry();

  if (registry.count(node.type)) {
    const auto& meta = registry.at(node.type);
    for (const auto& in_pin : meta.inputs) {
      bool flow = (in_pin.type == TypeSystem::TRIGGER);
      node.inputs.push_back(
          {ed::PinId(get_hash_id(node.name + "::" + in_pin.name)), in_pin.name,
           in_pin.type, ed::PinKind::Input, flow});
    }
    for (const auto& out_pin : meta.outputs) {
      bool flow = (out_pin.type == TypeSystem::TRIGGER);
      node.outputs.push_back(
          {ed::PinId(get_hash_id(node.name + "::" + out_pin.name)),
           out_pin.name, out_pin.type, ed::PinKind::Output, flow});
    }

    // Default initialization of parameters based on metadata
    for (const auto& param : meta.parameters) {
      node.parameters[param.name] = param.default_val;
    }
  } else if (node.type != "Subflow") {
    // Default fallback (but not for Subflow which shouldn't have any data pins
    // natively)
    node.inputs.push_back(
        {ed::PinId(get_next_id()), "In", "any", ed::PinKind::Input});
    node.outputs.push_back(
        {ed::PinId(get_next_id()), "Out", "any", ed::PinKind::Output});
  }

  // __flow_in always added LAST (renders below data pins)
  node.inputs.push_back(
      {ed::PinId(get_hash_id(node.name + "::" + UI_FLOW_IN_PIN_NAME)),
       UI_FLOW_IN_PIN_NAME, std::string(TypeSystem::TRIGGER),
       ed::PinKind::Input, true});

  // Every node gets a __flow_out pin, EXCEPT Condition (which uses True/False)
  if (node.type != "Condition") {
    node.outputs.push_back(
        {ed::PinId(get_hash_id(node.name + "::" + UI_FLOW_OUT_PIN_NAME)),
         UI_FLOW_OUT_PIN_NAME, std::string(TypeSystem::TRIGGER),
         ed::PinKind::Output, true});
  }
}

void NodeManager::show_asset_panel() {
  ImGui::SetNextWindowPos(ImVec2(10, 160), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(300, 730), ImGuiCond_FirstUseEver);

  ImGui::Begin("Asset Library");

  // Dynamically fetch available node component types from metadata registry
  auto& registry = ModuleFactory::get_metadata_registry();
  std::vector<std::string> categories;
  for (const auto& [type_name, meta] : registry) {
    categories.push_back(type_name);
  }
  std::sort(categories.begin(), categories.end());

  for (const auto& cat : categories) {
    // Create a draggable item
    ImGui::Selectable(cat.c_str());

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      // Pass the component type string as payload
      ImGui::SetDragDropPayload("DND_NODE_TYPE", cat.c_str(), cat.size() + 1);

      // Tooltip preview while dragging
      ImGui::Text("Creating %s...", cat.c_str());
      ImGui::EndDragDropSource();
    }
  }
  ImGui::End();
}

void NodeManager::show_toast(const std::string& msg, float duration) {
  LOG_INFO("Showing toast: " << msg);
  toast_message = msg;
  toast_timer = duration;
}

void NodeManager::push_undo_snapshot() {
  undo_stack.push_back({nodes, links, next_id});
  if (undo_stack.size() > MAX_UNDO_HISTORY) {
    undo_stack.pop_front();
  }
  redo_stack.clear();  // New action invalidates redo history
}

void NodeManager::undo() {
  if (undo_stack.empty()) {
    show_toast("Nothing to undo.", 1.5f);
    return;
  }
  // Save current state to redo stack
  redo_stack.push_back({nodes, links, next_id});

  // Restore from undo stack
  auto& snapshot = undo_stack.back();
  nodes = snapshot.nodes;
  links = snapshot.links;
  next_id = snapshot.next_id;
  undo_stack.pop_back();
  dirty = true;
}

void NodeManager::redo() {
  if (redo_stack.empty()) {
    show_toast("Nothing to redo.", 1.5f);
    return;
  }
  // Save current state to undo stack
  undo_stack.push_back({nodes, links, next_id});

  // Restore from redo stack
  auto& snapshot = redo_stack.back();
  nodes = snapshot.nodes;
  links = snapshot.links;
  next_id = snapshot.next_id;
  redo_stack.pop_back();
  dirty = true;
}

void NodeManager::group_selected_nodes(const std::string& group_type) {
  std::vector<ed::NodeId> selected_nodes;
  selected_nodes.resize(ed::GetSelectedObjectCount());
  int node_count =
      ed::GetSelectedNodes(selected_nodes.data(), selected_nodes.size());

  // --- Validation Rule 1: at least 2 nodes ---
  if (node_count < 2) {
    show_toast("Need at least 2 nodes to group.");
    return;
  }

  // Build a set for fast lookup
  std::unordered_set<uintptr_t> sel_set;
  for (int i = 0; i < node_count; ++i) {
    sel_set.insert(selected_nodes[i].Get());
  }

  // --- Validation Rule 2: all selected nodes must share the same parent_id ---
  ed::NodeId common_parent = ed::NodeId(0);
  bool first = true;
  for (auto sel_id : selected_nodes) {
    for (const auto& node : nodes) {
      if (node.id == sel_id) {
        if (first) {
          common_parent = node.parent_id;
          first = false;
        } else if (node.parent_id != common_parent) {
          show_toast("Cannot group nodes from different hierarchy levels.");
          return;
        }
        break;
      }
    }
  }

  // --- Validation Rule 3: no data-flow path between two selected nodes ---
  //     may pass through a non-selected node (topology continuity).
  // Strategy: for every link that exits the selected set (source is selected,
  //           destination is NOT selected), do a BFS from the destination.
  //           If BFS reaches another selected node, we have a "gap".
  // Build adjacency: node_id -> list of downstream node_ids via links
  std::unordered_map<uintptr_t, std::vector<uintptr_t>> adjacency;
  for (const auto& link : links) {
    ed::NodeId src_node_id, dst_node_id;
    if (find_pin(link.start_pin, &src_node_id) &&
        find_pin(link.end_pin, &dst_node_id)) {
      adjacency[src_node_id.Get()].push_back(dst_node_id.Get());
    }
  }

  for (const auto& link : links) {
    ed::NodeId src_node_id, dst_node_id;
    if (!find_pin(link.start_pin, &src_node_id) ||
        !find_pin(link.end_pin, &dst_node_id))
      continue;

    // Only care about edges that EXIT the selected set
    if (sel_set.count(src_node_id.Get()) && !sel_set.count(dst_node_id.Get())) {
      // BFS from dst_node_id through non-selected nodes
      std::queue<uintptr_t> bfs;
      std::unordered_set<uintptr_t> visited;
      bfs.push(dst_node_id.Get());
      visited.insert(dst_node_id.Get());

      while (!bfs.empty()) {
        uintptr_t cur = bfs.front();
        bfs.pop();

        if (adjacency.find(cur) != adjacency.end()) {
          for (uintptr_t next : adjacency[cur]) {
            if (sel_set.count(next)) {
              // Found a path: selected -> non-selected -> ... -> selected
              show_toast(
                  "Cannot group: data flow passes through\nnon-selected "
                  "intermediate nodes.");
              return;
            }
            if (!visited.count(next) && !sel_set.count(next)) {
              visited.insert(next);
              bfs.push(next);
            }
          }
        }
      }
    }
  }

  // --- All validations passed, proceed to group ---
  push_undo_snapshot();
  ImVec2 center(0, 0);
  for (auto id : selected_nodes) {
    ImVec2 pos = ed::GetNodePosition(id);
    center.x += pos.x;
    center.y += pos.y;
  }
  center.x /= node_count;
  center.y /= node_count;

  std::string node_name = get_unique_node_name(group_type);
  uintptr_t id = get_hash_id(node_name);

  NodeObject new_group = {ed::NodeId(id), node_name,        group_type, {}, {},
                          center,         ImVec2(300, 200), {}};
  new_group.is_group = true;
  new_group.parent_id = current_view_parent_id;

  generate_pins_for_node(new_group);

  nodes.push_back(new_group);
  ed::SetNodePosition(ed::NodeId(id), center);

  // Reparent selected nodes
  for (auto sel_id : selected_nodes) {
    for (auto& node : nodes) {
      if (node.id == sel_id && node.id != new_group.id) {
        node.parent_id = new_group.id;
      }
    }
  }

  ed::ClearSelection();
  dirty = true;
}

void NodeManager::ungroup_selected_node() {
  std::vector<ed::NodeId> selected_nodes;
  selected_nodes.resize(ed::GetSelectedObjectCount());
  int node_count =
      ed::GetSelectedNodes(selected_nodes.data(), selected_nodes.size());

  if (node_count == 1) {
    ed::NodeId sel_id = selected_nodes[0];
    auto it =
        std::find_if(nodes.begin(), nodes.end(),
                     [sel_id](const NodeObject& n) { return n.id == sel_id; });

    if (it != nodes.end()) {
      push_undo_snapshot();
      // If it's a group node, but NOT the context node of the current inner
      // view
      if (it->is_group && it->id != current_view_parent_id) {
        ed::NodeId group_id = it->id;
        ed::NodeId new_parent = it->parent_id;

        // Reparent children
        for (auto& node : nodes) {
          if (node.parent_id == group_id) {
            node.parent_id = new_parent;
          }
        }

        // Delete the group node - Modifed: We no longer delete the group node
        // so that Subflow and Loop nodes remain as empty containers.
        // nodes.erase(it);
        ed::ClearSelection();
        dirty = true;
      } else if (it->parent_id.Get() != 0) {
        // Normal node extraction from a group (including the context node
        // itself, which can't be deleted but can conceptually be "extracted"
        // although UI prevents this) If it's the context node, we shouldn't do
        // anything because that means you selected the group interface from
        // INSIDE the group and tried to extract it.
        if (it->id != current_view_parent_id) {
          ed::NodeId current_parent = it->parent_id;
          auto parent_it = std::find_if(nodes.begin(), nodes.end(),
                                        [current_parent](const NodeObject& n) {
                                          return n.id == current_parent;
                                        });
          if (parent_it != nodes.end()) {
            it->parent_id = parent_it->parent_id;
            ed::ClearSelection();
            dirty = true;
          }
        }
      }
    }
  }
}

void NodeManager::ensure_loop_proxy_nodes(NodeObject& parent_loop) {
  NodeObject* entry = nullptr;
  NodeObject* exit = nullptr;

  // 1. check if Loop_Entry & Loop_Exit are existed
  for (auto& n : nodes) {
    if (n.parent_id == parent_loop.id) {
      if (n.type == "Loop_Entry") entry = &n;
      if (n.type == "Loop_Exit") exit = &n;
    }
  }

  uintptr_t base_id = parent_loop.id.Get();

  // 2. create if not existed
  if (!entry) {
    NodeObject entry_node;
    entry_node.id = ed::NodeId(get_hash_id(parent_loop.name + "_EntryProxy"));
    entry_node.name = "Loop_Entry";
    entry_node.type = "Loop_Entry";
    entry_node.parent_id = parent_loop.id;
    entry_node.pos = ImVec2(-200, 0);  // left

    // Use stable hash-based IDs so pin IDs are consistent across save/load.
    // The load() function maps "Loop_X.item" links to these pins.
    entry_node.outputs.push_back(
        {ed::PinId(get_hash_id(parent_loop.name + "_Entry_item")), "item",
         "any", ed::PinKind::Output});
    entry_node.outputs.push_back(
        {ed::PinId(get_hash_id(parent_loop.name + "_Entry_index")), "index",
         "int", ed::PinKind::Output});

    nodes.push_back(entry_node);
    entry = &nodes.back();
    ed::SetNodePosition(entry_node.id, entry_node.pos);
  }

  if (!exit) {
    NodeObject exit_node;
    exit_node.id = ed::NodeId(get_hash_id(parent_loop.name + "_ExitProxy"));
    exit_node.name = "Loop_Exit";
    exit_node.type = "Loop_Exit";
    exit_node.parent_id = parent_loop.id;
    exit_node.pos = ImVec2(600, 0);  // right

    // the input pins of exit node will be dynamically refreshed in draw loop
    // via sync_loop_exit_pins
    nodes.push_back(exit_node);
    exit = &nodes.back();
    ed::SetNodePosition(exit_node.id, exit_node.pos);
  }

  // Ensure entry outputs are populated (e.g. if entry existed but had no pins)
  if (entry->outputs.empty()) {
    entry->outputs.push_back(
        {ed::PinId(get_hash_id(parent_loop.name + "_Entry_item")), "item",
         "any", ed::PinKind::Output});
    entry->outputs.push_back(
        {ed::PinId(get_hash_id(parent_loop.name + "_Entry_index")), "index",
         "int", ed::PinKind::Output});
  }
}

void NodeManager::sync_loop_exit_pins(NodeObject& proxy_node) {
  auto* parent = find_node(proxy_node.parent_id);
  if (!parent) return;

  // Only update when the output pins from parent changes
  size_t expected_size = 0;
  for (auto& p : parent->outputs)
    if (!p.is_flow && p.name != "item" && p.name != "index") expected_size++;

  if (proxy_node.inputs.size() == expected_size) return;

  // clear proxy_node.inputs and use outputs from parent node
  // TODO: try to sync pins instead of clearing and recreating
  proxy_node.inputs.clear();
  for (auto& out_pin : parent->outputs) {
    // exclude built-in item/index and flow pins
    if (out_pin.is_flow || out_pin.name == "item" || out_pin.name == "index")
      continue;

    Pin in_p;
    in_p.id = ed::PinId(get_hash_id(parent->name + "_ExitPin_" + out_pin.name));
    in_p.name = out_pin.name;
    in_p.type = out_pin.type;
    in_p.kind = ed::PinKind::Input;  // as Input in Exit node
    proxy_node.inputs.push_back(in_p);
  }
}

void NodeManager::draw() {
  show_asset_panel();

  ImGui::SetNextWindowPos(ImVec2(320, 160), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(1110, 730), ImGuiCond_FirstUseEver);

  std::string window_title = dirty
                                 ? "Pipeline [Unsaved Changes]###PipelineWindow"
                                 : "Pipeline###PipelineWindow";
  ImGui::Begin(window_title.c_str());

  ed::Begin("Pipeline Workspace");

  // Handle Breadcrumb navigation via an explicit back button on the canvas
  if (current_view_parent_id.Get() != 0) {
    ed::Suspend();
    ImGui::SetCursorScreenPos(
        ImVec2(ImGui::GetWindowPos().x + 20, ImGui::GetWindowPos().y + 40));
    if (ImGui::Button("^^ Back to Parent Group", ImVec2(200, 30))) {
      auto it = std::find_if(
          nodes.begin(), nodes.end(),
          [&](const NodeObject& n) { return n.id == current_view_parent_id; });
      if (it != nodes.end()) {
        current_view_parent_id = it->parent_id;
      } else {
        current_view_parent_id = 0;
      }
      ed::NavigateToContent();
    }
    ed::Resume();
  }

  // Keyboard Shortcuts
  if (ImGui::IsKeyPressed(ImGuiKey_G) && ImGui::GetIO().KeyCtrl) {
    if (ImGui::GetIO().KeyShift) {
      ungroup_selected_node();
    } else {
      group_selected_nodes("Subflow");  // Default or Subflow
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_U) && ImGui::GetIO().KeyCtrl) {
    ungroup_selected_node();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Z) && ImGui::GetIO().KeyCtrl) {
    if (ImGui::GetIO().KeyShift) {
      redo();
    } else {
      undo();
    }
  }

  // Keep track of visible pins to avoid drawing dangling links
  std::unordered_set<uintptr_t> current_view_pins;

  for (auto& node : nodes) {
    if (node.id == current_view_parent_id && current_view_parent_id.Get() != 0)
      continue;

    // filter nodes not in current view
    if (node.parent_id != current_view_parent_id &&
        node.id != current_view_parent_id)
      continue;

    if (node.type == "Loop_Exit") {
      sync_loop_exit_pins(node);  // sync exit node pins
    }

    bool is_context_node =
        (node.type == "Loop" && node.id == current_view_parent_id &&
         current_view_parent_id.Get() != 0);

    if (node.parent_id != current_view_parent_id && !is_context_node) {
      continue;
    }

    for (const auto& pin : node.inputs) current_view_pins.insert(pin.id.Get());
    for (const auto& pin : node.outputs) current_view_pins.insert(pin.id.Get());

    if (node.is_group && !is_context_node) {
      // Group Rendering Logic (Now looks like a regular node)
      ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(50, 50, 60, 200));
      ed::PushStyleColor(ed::StyleColor_NodeBorder,
                         ImColor(100, 200, 255, 200));

      ed::BeginNode(node.id);

      ImGui::Spacing();

      // Editable node name (Commit on Deactivate)
      char name_buf[128];
      const char* label =
          (renaming_node_id == node.id) ? renaming_name_buf : node.name.c_str();
      strncpy(name_buf, label, sizeof(name_buf));

      ImGui::PushItemWidth(150.0f);
      if (ImGui::InputText(("##name" + std::to_string(node.id.Get())).c_str(),
                           name_buf, sizeof(name_buf))) {
        if (renaming_node_id != node.id) {
          renaming_node_id = node.id;
          strncpy(renaming_name_buf, name_buf, sizeof(renaming_name_buf));
        }
      }

      if (ImGui::IsItemDeactivatedAfterEdit() && renaming_node_id == node.id) {
        if (node.name != renaming_name_buf) {
          // Check for duplicate names (use pointer check in case IDs already
          // collided)
          bool name_exists = false;
          for (const auto& other : nodes) {
            if (&other != &node && other.name == renaming_name_buf) {
              name_exists = true;
              break;
            }
          }

          if (name_exists) {
            show_toast("Node name '" + std::string(renaming_name_buf) +
                           "' already exists!",
                       3.0f);
          } else {
            node.name = renaming_name_buf;
            dirty = true;
          }
        }
        renaming_node_id = 0;
      }
      ImGui::PopItemWidth();

      if (node.type == "Subflow") {
        ImGui::TextColored(ImColor(150, 200, 255), "Subflow Group");
      } else if (node.type == "Loop") {
        ImGui::TextColored(ImColor(150, 255, 150), "Loop Group");
      }
      ImGui::TextDisabled("(Double-click to enter)");
      ImGui::Spacing();

      std::vector<ed::PinId> external_inputs;
      std::vector<ed::PinId> external_outputs;
      std::vector<std::string> ext_in_names;
      std::vector<std::string> ext_out_names;

      std::vector<ed::PinId> flow_inputs;
      std::vector<std::string> flow_in_names;
      std::vector<ed::PinId> flow_outputs;
      std::vector<std::string> flow_out_names;

      // Add actual registered module pins for this group (e.g. Loop's
      // collection/item/index)
      for (const auto& pin : node.inputs) {
        if (pin.name == UI_FLOW_IN_PIN_NAME || pin.is_flow) {
          if (node.type == "Subflow") {
            bool has_link = false;
            for (const auto& l : links) {
              if (l.end_pin == pin.id) {
                has_link = true;
                break;
              }
            }
            if (!is_dragging_flow_pin && !has_link) continue;
          }
          flow_inputs.push_back(pin.id);
          flow_in_names.push_back(pin.name);
        } else {
          external_inputs.push_back(pin.id);
          ext_in_names.push_back(pin.name);
        }
        current_view_pins.insert(pin.id.Get());
      }
      for (const auto& pin : node.outputs) {
        if (node.type == "Loop" &&
            (pin.name == "item" || pin.name == "index")) {
          // Internalized pins for the loop itself; exposed internally via
          // context node
          continue;
        }
        if (pin.name == UI_FLOW_OUT_PIN_NAME || pin.is_flow) {
          if (node.type == "Subflow") {
            bool has_link = false;
            for (const auto& l : links) {
              if (l.start_pin == pin.id) {
                has_link = true;
                break;
              }
            }
            if (!is_dragging_flow_pin && !has_link) continue;
          }
          flow_outputs.push_back(pin.id);
          flow_out_names.push_back(pin.name);
        } else {
          external_outputs.push_back(pin.id);
          ext_out_names.push_back(pin.name);
        }
        current_view_pins.insert(pin.id.Get());
      }

      for (const auto& child : nodes) {
        if (child.parent_id != node.id) continue;
        if (child.type == "Loop_Entry" || child.type == "Loop_Exit") continue;

        for (const auto& pin : child.inputs) {
          if (pin.is_flow || pin.name == UI_FLOW_IN_PIN_NAME ||
              pin.name == UI_FLOW_OUT_PIN_NAME)
            continue;

          // For INPUT pins, check links where this pin is the DESTINATION
          bool has_external_link = false;
          bool has_any_link = false;
          for (const auto& link : links) {
            if (link.end_pin == pin.id) {
              has_any_link = true;
              ed::NodeId source_node;
              if (find_pin(link.start_pin, &source_node)) {
                auto it = std::find_if(nodes.begin(), nodes.end(),
                                       [source_node](const NodeObject& n) {
                                         return n.id == source_node;
                                       });
                if (it != nodes.end() && it->parent_id != node.id) {
                  has_external_link = true;
                }
              }
            }
          }
          if (has_external_link || !has_any_link) {
            external_inputs.push_back(pin.id);
            ext_in_names.push_back(pin.name);
            current_view_pins.insert(pin.id.Get());
          }
        }

        for (const auto& pin : child.outputs) {
          if (pin.is_flow || pin.name == UI_FLOW_IN_PIN_NAME ||
              pin.name == UI_FLOW_OUT_PIN_NAME)
            continue;

          // For OUTPUT pins, check links where this pin is the SOURCE
          bool has_external_link = false;
          bool has_any_link = false;
          for (const auto& link : links) {
            if (link.start_pin == pin.id) {
              has_any_link = true;
              ed::NodeId target_node;
              if (find_pin(link.end_pin, &target_node)) {
                auto it = std::find_if(nodes.begin(), nodes.end(),
                                       [target_node](const NodeObject& n) {
                                         return n.id == target_node;
                                       });
                if (it != nodes.end() && it->parent_id != node.id) {
                  has_external_link = true;
                }
              }
            }
          }
          if (has_external_link || !has_any_link) {
            if (node.type == "Loop" &&
                (pin.name == "item" || pin.name == "index")) {
              continue;  // Do not expose these on the external loop surface.
            }
            external_outputs.push_back(pin.id);
            ext_out_names.push_back(pin.name);
            current_view_pins.insert(pin.id.Get());
          }
        }
      }

      // Append flow pins at the end of collection to ensure they render below
      // data pins
      external_inputs.insert(external_inputs.end(), flow_inputs.begin(),
                             flow_inputs.end());
      ext_in_names.insert(ext_in_names.end(), flow_in_names.begin(),
                          flow_in_names.end());
      external_outputs.insert(external_outputs.end(), flow_outputs.begin(),
                              flow_outputs.end());
      ext_out_names.insert(ext_out_names.end(), flow_out_names.begin(),
                           flow_out_names.end());

      // Draw Group Inputs
      ImGui::BeginGroup();
      for (size_t i = 0; i < external_inputs.size(); ++i) {
        ImGui::BeginGroup();

        // Find if this is a flow pin
        bool is_flow_pin = false;
        ed::NodeId pin_node_id;
        if (find_pin(external_inputs[i], &pin_node_id)) {
          auto parent_node_it = std::find_if(
              nodes.begin(), nodes.end(),
              [&](const NodeObject& n) { return n.id == pin_node_id; });
          if (parent_node_it != nodes.end()) {
            auto pin_it = std::find_if(
                parent_node_it->inputs.begin(), parent_node_it->inputs.end(),
                [&](const Pin& p) { return p.id == external_inputs[i]; });
            if (pin_it != parent_node_it->inputs.end()) {
              is_flow_pin = pin_it->is_flow ||
                            pin_it->name == UI_FLOW_IN_PIN_NAME ||
                            pin_it->name == UI_FLOW_OUT_PIN_NAME;
            }
          }
        }

        ed::BeginPin(external_inputs[i], ed::PinKind::Input);
        if (is_flow_pin) {
          ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), ">>");
        } else {
          ImGui::Text("%s", UI_PIN_ARROW);
        }
        ed::EndPin();
        ImGui::SameLine();
        if (ext_in_names[i] != "__flow_in" && ext_in_names[i] != "__flow_out") {
          ImGui::Text("%s", ext_in_names[i].c_str());
        } else {
          ImGui::Text(" ");
        }
        ImGui::EndGroup();
      }
      ImGui::EndGroup();

      ImGui::SameLine(0.0f, 20.0f);

      // Draw Group Outputs
      ImGui::BeginGroup();
      float max_out_w = 0.0f;
      for (size_t i = 0; i < external_outputs.size(); ++i) {
        float w = ImGui::CalcTextSize(ext_out_names[i].c_str()).x +
                  ImGui::CalcTextSize(UI_PIN_ARROW).x +
                  ImGui::GetStyle().ItemSpacing.x;
        max_out_w = std::max(max_out_w, w);
      }
      for (size_t i = 0; i < external_outputs.size(); ++i) {
        ImGui::BeginGroup();
        float w = ImGui::CalcTextSize(ext_out_names[i].c_str()).x +
                  ImGui::CalcTextSize(UI_PIN_ARROW).x +
                  ImGui::GetStyle().ItemSpacing.x;
        float offset = max_out_w - w;
        if (offset > 0.0f)
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

        if (ext_out_names[i] != "__flow_in" &&
            ext_out_names[i] != "__flow_out") {
          ImGui::Text("%s", ext_out_names[i].c_str());
        } else {
          ImGui::Text(" ");
        }
        ImGui::SameLine();

        // Find if this is a flow pin
        bool is_flow_pin = false;
        ed::NodeId pin_node_id;
        if (find_pin(external_outputs[i], &pin_node_id)) {
          auto parent_node_it = std::find_if(
              nodes.begin(), nodes.end(),
              [&](const NodeObject& n) { return n.id == pin_node_id; });
          if (parent_node_it != nodes.end()) {
            auto pin_it = std::find_if(
                parent_node_it->outputs.begin(), parent_node_it->outputs.end(),
                [&](const Pin& p) { return p.id == external_outputs[i]; });
            if (pin_it != parent_node_it->outputs.end()) {
              is_flow_pin = pin_it->is_flow ||
                            pin_it->name == UI_FLOW_IN_PIN_NAME ||
                            pin_it->name == UI_FLOW_OUT_PIN_NAME;
            }
          }
        }

        ed::BeginPin(external_outputs[i], ed::PinKind::Output);
        if (is_flow_pin) {
          ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), ">>");
        } else {
          ImGui::Text("%s", UI_PIN_ARROW);
        }
        ed::EndPin();
        ImGui::EndGroup();
      }
      ImGui::EndGroup();

      if (external_inputs.empty() && external_outputs.empty()) {
        ImGui::Dummy(ImVec2(150.0f, 20.0f));
      }

      if (node.type == "Loop") {
        ImGui::Spacing();
        ImGui::Separator();

        ImGui::PushID(node.id.AsPointer());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        if (ImGui::Button(" + Add Output ")) {
          std::string pin_name;
          bool exists = true;
          while (exists) {
            node.dynamic_pin_counter++;
            pin_name = "out_vec_" + std::to_string(node.dynamic_pin_counter);

            exists = false;
            for (auto& p : node.outputs) {
              if (p.name == pin_name) {
                exists = true;
                break;
              }
            }
          }
          ed::PinId pin_id =
              ed::PinId(get_hash_id(node.name + "::dyn_" + pin_name));
          node.outputs.push_back(
              Pin{pin_id, pin_name, "vector", ed::PinKind::Output, false, ""});
          dirty = true;
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
      }

      ed::EndNode();

      // Double-click to enter Subgraph
      if (ed::GetDoubleClickedNode() == node.id) {
        current_view_parent_id = node.id;
        if (node.type == "Loop") {
          ensure_loop_proxy_nodes(node);
        }

        ed::ClearSelection();
        ed::NavigateToContent();
      }

      ed::PopStyleColor(2);
      continue;
    }

    ed::BeginNode(node.id);

    if (is_context_node) {
      ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[GROUP INTERFACE]");
      ImGui::Spacing();
    }

    // Editable node name (Commit on Deactivate)
    float text_width = ImGui::CalcTextSize(node.name.c_str()).x;
    ImGui::PushItemWidth(std::clamp(text_width + 20.0f, 120.0f, 220.0f));

    char name_buf[128];
    const char* label =
        (renaming_node_id == node.id) ? renaming_name_buf : node.name.c_str();
    strncpy(name_buf, label, sizeof(name_buf));

    if (ImGui::InputText(("##name" + std::to_string(node.id.Get())).c_str(),
                         name_buf, sizeof(name_buf))) {
      if (renaming_node_id != node.id) {
        renaming_node_id = node.id;
        strncpy(renaming_name_buf, name_buf, sizeof(renaming_name_buf));
      }
    }

    if (ImGui::IsItemDeactivatedAfterEdit() && renaming_node_id == node.id) {
      if (node.name != renaming_name_buf) {
        // Check for duplicate names (use pointer check in case IDs already
        // collided)
        bool name_exists = false;
        for (const auto& other : nodes) {
          if (&other != &node && other.name == renaming_name_buf) {
            name_exists = true;
            break;
          }
        }

        if (name_exists) {
          show_toast("Node name '" + std::string(renaming_name_buf) +
                         "' already exists!",
                     3.0f);
        } else {
          node.name = renaming_name_buf;
          dirty = true;
        }
      }
      renaming_node_id = 0;
    }
    ImGui::PopItemWidth();

    ImGui::TextDisabled("%s", node.type.c_str());

    ImGui::Spacing();

    // Grouping pins correctly for minimal layout
    ImGui::BeginGroup();
    for (auto& pin : node.inputs) {
      // __flow_in: always show for Condition; show for others only when
      // dragging a flow link or already connected
      if (pin.name == UI_FLOW_IN_PIN_NAME && node.type != "Condition") {
        bool connected = false;
        for (auto& lk : links) {
          if (lk.end_pin == pin.id || lk.start_pin == pin.id) {
            connected = true;
            break;
          }
        }
        if (!is_dragging_flow_pin && !connected) {
          // Always register the pin with editor so it can receive drops,
          // but render it as invisible so node size stays fixed
          ImGui::BeginGroup();
          ed::BeginPin(pin.id, ed::PinKind::Input);
          ImGui::TextColored(ImVec4(0, 0, 0, 0), ">>");
          ed::EndPin();
          ImGui::EndGroup();
          continue;
        }
      }
      ImGui::BeginGroup();
      ed::BeginPin(pin.id, ed::PinKind::Input);
      if (pin.is_flow) {
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), ">>");
      } else {
        ImGui::Text("%s", UI_PIN_ARROW);
      }
      ed::EndPin();
      if (pin.name != UI_FLOW_IN_PIN_NAME) {
        ImGui::SameLine();
        if (pin.is_flow) {
          ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 0.8f), "%s",
                             pin.name.c_str());
        } else {
          ImGui::Text("%s", pin.name.c_str());
        }
      }
      ImGui::EndGroup();
    }
    ImGui::EndGroup();

    // Push right-side outputs
    // Add 20px minimum gap between inputs and outputs
    ImGui::SameLine(0.0f, 20.0f);

    ImGui::BeginGroup();
    float max_out_w = 0.0f;
    for (auto& pin : node.outputs) {
      if (pin.name == UI_FLOW_OUT_PIN_NAME) continue;  // rendered separately
      float w;
      if (pin.is_flow) {
        // Name + space + >> width
        w = ImGui::CalcTextSize(pin.name.c_str()).x + 2.0f +
            ImGui::CalcTextSize(">>").x;
      } else {
        w = ImGui::CalcTextSize(pin.name.c_str()).x +
            ImGui::CalcTextSize(UI_PIN_ARROW).x +
            ImGui::GetStyle().ItemSpacing.x;
        if (pin.type == UI_IMG_PIN_TYPE)
          w += ImGui::CalcTextSize(UI_BTN_SHOW_IMG).x;
      }
      max_out_w = std::max(max_out_w, w);
    }

    for (auto& pin : node.outputs) {
      // __flow_out is rendered separately at the bottom right
      if (pin.name == UI_FLOW_OUT_PIN_NAME) continue;
      float w = ImGui::CalcTextSize(pin.name.c_str()).x +
                ImGui::CalcTextSize(UI_PIN_ARROW).x +
                ImGui::GetStyle().ItemSpacing.x;
      if (pin.type == UI_IMG_PIN_TYPE) {
        // Must exactly match the max_out_w calculation logic
        w += ImGui::CalcTextSize(UI_BTN_SHOW_IMG).x;
      }
      float offset = max_out_w - w;
      if (offset > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
      }

      // display name for True/False keeps as-is (already clean)
      std::string disp_name = pin.name;
      if (pin.is_flow) {
        // Width of this flow pin = name + 2px gap + >>
        float this_w = ImGui::CalcTextSize(disp_name.c_str()).x + 2.0f +
                       ImGui::CalcTextSize(">>").x;
        float flow_off = max_out_w - this_w;
        if (flow_off > 0.0f)
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + flow_off);
        ImGui::BeginGroup();
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 0.8f), "%s",
                           disp_name.c_str());
        ImGui::SameLine(0.0f, 2.0f);
        ed::BeginPin(pin.id, ed::PinKind::Output);
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), ">>");
        ed::EndPin();
        ImGui::EndGroup();
      } else {
        ImGui::BeginGroup();
        ImGui::Text("%s", disp_name.c_str());
        // Inline visualizer toggle button for image outputs
        if (pin.type == UI_IMG_PIN_TYPE) {
          bool is_visible = visible_pins.count(pin.id.Get());
          ImGui::SameLine(0.0f, 2.0f);
          ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                              ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
          if (ImGui::SmallButton(
                  (std::string(is_visible ? UI_BTN_HIDE_IMG : UI_BTN_SHOW_IMG) +
                   "##" + std::to_string(pin.id.Get()))
                      .c_str())) {
            if (is_visible)
              visible_pins.erase(pin.id.Get());
            else
              visible_pins.insert(pin.id.Get());
          }
          ImGui::PopStyleVar();
        }
        ImGui::SameLine();
        ed::BeginPin(pin.id, ed::PinKind::Output);
        ImGui::Text("%s", UI_PIN_ARROW);
        ed::EndPin();
        ImGui::EndGroup();
      }
    }

    // Render __flow_out at the bottom-right (for non-Condition nodes)
    // Always rendered (even if invisible) to keep node size stable
    for (auto& pin : node.outputs) {
      if (pin.name == UI_FLOW_OUT_PIN_NAME) {
        bool connected = false;
        for (auto& lk : links) {
          if (lk.start_pin == pin.id) {
            connected = true;
            break;
          }
        }
        bool visible = (is_dragging_flow_pin || connected);
        float fw = ImGui::CalcTextSize(">>").x;
        float flow_off = max_out_w - fw;
        if (flow_off > 0.0f)
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + flow_off);
        ImGui::BeginGroup();
        ed::BeginPin(pin.id, ed::PinKind::Output);
        if (visible) {
          ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), ">>");
        } else {
          // Invisible placeholder to keep layout stable
          ImGui::TextColored(ImVec4(0, 0, 0, 0), ">>");
        }
        ed::EndPin();
        ImGui::EndGroup();
        break;
      }
    }
    ImGui::EndGroup();  // End outer output column group

    // Post-pin rendering of visuals
    for (auto& pin : node.outputs) {
      if (visible_pins.count(pin.id.Get())) {
        std::string pin_key = node.name + "." + pin.name;
        GLuint tex_id = TextureManager::get_instance().get_texture(pin_key);
        if (tex_id > 0) {
          ImGui::Spacing();
          // Add a small button above the image to expand it
          if (ImGui::Button((std::string(UI_BTN_EXPAND) + "##" +
                             std::to_string(pin.id.Get()))
                                .c_str())) {
            expanded_image_key = pin_key;
            show_expanded_image = true;
          }
          ImGui::Image((void*)(intptr_t)tex_id, ImVec2(240, 180));
        } else {
          ImGui::TextDisabled("(No Data For %s)", pin.name.c_str());
        }
      }
    }

    ed::EndNode();

    // Sync node position with the editor
    uintptr_t nid = node.id.Get();
    if (initialized_nodes.find(nid) == initialized_nodes.end()) {
      // First time this node is rendered in this session/view
      ed::SetNodePosition(node.id, node.pos);
      initialized_nodes.insert(nid);
    } else {
      ImVec2 current_pos = ed::GetNodePosition(node.id);
      // Guard: Ensure editor returns valid coordinates before syncing back to
      // persistent data
      if (current_pos.x < 1e10f && current_pos.y < 1e10f) {
        if (current_pos.x != node.pos.x || current_pos.y != node.pos.y) {
          // Do not sync position back to the node if it's the context node
          if (!is_context_node) {
            node.pos = current_pos;
            dirty = true;
          }
        }

        // Hierarchy Drag & Drop: When a node moves, check if it was dropped
        // inside a Group
        if (!node.is_group) {
          ed::NodeId new_parent = current_view_parent_id;
          ImVec2 node_size = ed::GetNodeSize(node.id);

          for (const auto& group : nodes) {
            if (group.is_group && group.parent_id == current_view_parent_id) {
              ImVec2 g_pos = ed::GetNodePosition(group.id);
              ImVec2 g_size = group.size;

              if (current_pos.x > g_pos.x && current_pos.y > g_pos.y &&
                  (current_pos.x + node_size.x) < (g_pos.x + g_size.x) &&
                  (current_pos.y + node_size.y) < (g_pos.y + g_size.y)) {
                new_parent = group.id;
                break;
              }
            }
          }

          if (node.parent_id != new_parent) {
            node.parent_id = new_parent;
            dirty = true;
          }
        }
      }
    }
  }

  for (auto& link : links) {
    if (current_view_pins.count(link.start_pin.Get()) &&
        current_view_pins.count(link.end_pin.Get())) {
      if (link.is_flow_link) {
        // Cyan-blue color for flow/control links to distinguish from data links
        ed::Link(link.id, link.start_pin, link.end_pin,
                 ImColor(0.3f, 0.8f, 1.0f, 1.0f), 2.5f);
      } else {
        ed::Link(link.id, link.start_pin, link.end_pin);
      }
    }
  }

  // Handle link creation interaction
  is_dragging_flow_pin = false;  // Reset each frame
  if (ed::BeginCreate(ImColor(255, 255, 255), 2.0f)) {
    ed::PinId start_pin, end_pin;
    if (ed::QueryNewLink(&start_pin, &end_pin)) {
      ed::NodeId start_node, end_node;
      Pin* startPin = find_pin(start_pin, &start_node);
      Pin* endPin = find_pin(end_pin, &end_node);

      // Set flow drag flag so other nodes show their flow pins
      if (startPin && startPin->is_flow) is_dragging_flow_pin = true;
      if (endPin && endPin->is_flow) is_dragging_flow_pin = true;

      if (startPin && endPin) {
        NodeObject* s_node = find_node(start_node);
        NodeObject* e_node = find_node(end_node);

        bool s_is_exit = (s_node && s_node->type == "Loop_Exit");
        bool e_is_exit = (e_node && e_node->type == "Loop_Exit");

        if (s_is_exit || e_is_exit) {
          Pin* proxy_pin = s_is_exit ? startPin : endPin;
          Pin* source_pin = s_is_exit ? endPin : startPin;
          NodeObject* proxy_node = s_is_exit ? s_node : e_node;
          NodeObject* source_node = s_is_exit ? e_node : s_node;

          if (source_pin->kind == ed::PinKind::Output &&
              proxy_pin->kind == ed::PinKind::Input) {
            if (ed::AcceptNewItem(ImColor(100, 200, 255), 2.5f)) {
              push_undo_snapshot();
              // make sure start is Output and end is Input
              links.push_back({ed::LinkId(get_next_id()), source_pin->id,
                               proxy_pin->id, false});

              // sync config to parent loop
              auto* parentLoop = find_node(proxy_node->parent_id);
              if (parentLoop) {
                for (auto& p : parentLoop->outputs) {
                  if (p.name == proxy_pin->name) {
                    p.source_path = source_node->name + "." + source_pin->name;
                    break;
                  }
                }
              }
              dirty = true;
            }
          } else {
            ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
            ImGui::SetTooltip("x Incompatible Direction for Loop Exit");
          }
        }

        else {
          bool canConnect = true;
          std::string rejectReason = "";

          if (startPin == endPin) {
            canConnect = false;
            rejectReason = "x Cannot connect pin to itself";
          } else if (start_node == end_node) {
            canConnect = false;
            rejectReason = "x Cannot connect to the same node";
          } else if (startPin->kind == endPin->kind) {
            canConnect = false;
            rejectReason = "x Incompatible Pin Kinds";
          } else if (startPin->type != endPin->type &&
                     startPin->type != TypeSystem::ANY &&
                     endPin->type != TypeSystem::ANY) {
            canConnect = false;
            rejectReason =
                "x Type Mismatch: " + startPin->type + " != " + endPin->type;
          } else {
            // Find which one is the input pin to check if it's already
            // occupied
            Pin* inputPin =
                (startPin->kind == ed::PinKind::Input) ? startPin : endPin;
            for (const auto& link : links) {
              if (link.start_pin == inputPin->id ||
                  link.end_pin == inputPin->id) {
                canConnect = false;
                rejectReason = "x Input already occupied";
                break;
              }
            }
          }

          if (!canConnect) {
            ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
            ImGui::SetTooltip("%s", rejectReason.c_str());
          } else {
            if (ed::AcceptNewItem(ImColor(0, 255, 0), 2.0f)) {
              push_undo_snapshot();
              ed::PinId actual_start =
                  (startPin->kind == ed::PinKind::Output) ? start_pin : end_pin;
              ed::PinId actual_end =
                  (startPin->kind == ed::PinKind::Input) ? start_pin : end_pin;
              bool flow = (startPin->is_flow || endPin->is_flow);
              links.push_back(
                  {ed::LinkId(get_next_id()), actual_start, actual_end, flow});
              dirty = true;
            }
          }
        }
      }
    } else {
      // Still in create mode (hovering) — check if any flow pin is the source
      ed::PinId hovered_pin;
      if (ed::QueryNewNode(&hovered_pin)) {
        Pin* hp = find_pin(hovered_pin, nullptr);
        if (hp && hp->is_flow) is_dragging_flow_pin = true;
      }
    }
    ed::EndCreate();
  }

  // --- Handle deletion logic ---
  if (ed::BeginDelete()) {
    // 1. Handle link deletion
    ed::LinkId linkId;
    while (ed::QueryDeletedLink(&linkId)) {
      if (ed::AcceptDeletedItem()) {
        push_undo_snapshot();

        auto it =
            std::find_if(links.begin(), links.end(),
                         [&](const LinkObject& l) { return l.id == linkId; });
        if (it != links.end()) {
          // Check if this link is connected to Loop_Exit
          ed::NodeId end_node_id;
          Pin* endP = find_pin(it->end_pin, &end_node_id);
          NodeObject* endNode = find_node(end_node_id);
          if (endP && endNode && endNode->type == "Loop_Exit") {
            // Find the parent Loop and clear the mapping path
            NodeObject* parent = find_node(endNode->parent_id);
            if (parent) {
              for (auto& out_p : parent->outputs) {
                if (out_p.name == endP->name) {
                  out_p.source_path = "";  // clear source path
                  break;
                }
              }
            }
          }

          links.erase(it);
        }

        dirty = true;
      }
    }

    // 2. Handle node deletion
    ed::NodeId node_id;
    while (ed::QueryDeletedNode(&node_id)) {
      if (ed::AcceptDeletedItem()) {
        push_undo_snapshot();

        std::vector<ed::NodeId> children_to_delete;
        for (const auto& n : nodes) {
          if (n.parent_id == node_id) {
            children_to_delete.push_back(n.id);
          }
        }

        auto cleanup_node_data = [&](ed::NodeId target_node_id) {
          // Collect all pins belonging to the deleted node
          std::vector<ed::PinId> pins;  // pins to remove
          for (const auto& node : nodes) {
            if (node.id == target_node_id) {
              for (auto& pin : node.inputs) pins.push_back(pin.id);
              for (auto& pin : node.outputs) pins.push_back(pin.id);
              break;
            }
          }

          // Clean up any links attached to the deleted pins
          links.erase(
              std::remove_if(links.begin(), links.end(),
                             [&pins](const LinkObject& l) {
                               return std::find(pins.begin(), pins.end(),
                                                l.start_pin) != pins.end() ||
                                      std::find(pins.begin(), pins.end(),
                                                l.end_pin) != pins.end();
                             }),
              links.end());
        };

        for (auto childId : children_to_delete) {
          cleanup_node_data(childId);
        }
        cleanup_node_data(node_id);

        // Remove the node from the collection
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                                   [node_id](const NodeObject& n) {
                                     return n.id == node_id ||
                                            n.parent_id == node_id;
                                   }),
                    nodes.end());
        dirty = true;
      }
    }
    ed::EndDelete();
  }

  // Check for node selection to open inspector
  std::vector<ed::NodeId> selected_nodes;
  selected_nodes.resize(ed::GetSelectedObjectCount());
  int node_count =
      ed::GetSelectedNodes(selected_nodes.data(), selected_nodes.size());

  if (node_count == 1) {
    selected_node_for_properties = selected_nodes[0];
  } else {
    // Click background or select multiple nodes to deselect inspector
    selected_node_for_properties = 0;
  }

  // Right-click context menus
  ed::Suspend();
  ed::NodeId contextNodeId;

  if (ed::ShowNodeContextMenu(&contextNodeId)) {
    // If the user right clicks a node, ensure it is selected for ungrouping
    ed::SelectNode(contextNodeId, true);
    ImGui::OpenPopup("NodeContextMenu");
  } else if (ed::ShowBackgroundContextMenu()) {
    ImGui::OpenPopup("BackgroundContextMenu");
  }

  if (ImGui::BeginPopup("NodeContextMenu")) {
    if (ImGui::MenuItem("Extract out of Group / Ungroup", "Ctrl+Shift+G")) {
      ungroup_selected_node();
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopup("BackgroundContextMenu")) {
    if (ImGui::MenuItem("Group selected into Loop", "Ctrl+G")) {
      group_selected_nodes("Loop");
    }
    if (ImGui::MenuItem("Group selected into Subflow")) {
      group_selected_nodes("Subflow");
    }
    ImGui::EndPopup();
  }
  ed::Resume();

  // End node editor canvas
  ed::End();

  // Render Popout Image Viewer
  if (show_expanded_image && !expanded_image_key.empty()) {
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Image Viewer", &show_expanded_image)) {
      GLuint tex_id =
          TextureManager::get_instance().get_texture(expanded_image_key);
      if (tex_id > 0) {
        // Get available size to scale image preserving aspect ratio or fill
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::Image((void*)(intptr_t)tex_id, avail);
      } else {
        ImGui::TextDisabled("No texture data available.");
      }
    }
    ImGui::End();
  }

  // Process drag & drop from the asset panel
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("DND_NODE_TYPE")) {
      const char* typeStr = (const char*)payload->Data;

      // Map screen coordinates to the node editor's internal canvas space
      ImVec2 mousePos = ImGui::GetMousePos();

      // We must map it using the editor's screen to canvas conversion.
      // Since ed::End() was already called, we temporarily resume context
      // to do the math, or we can just do it while editor was active.
      ImVec2 canvasPos = ed::ScreenToCanvas(mousePos);

      // Instantiate a new node
      std::string node_name = get_unique_node_name(typeStr);
      uintptr_t id = get_hash_id(node_name);

      NodeObject new_node = {
          ed::NodeId(id), node_name, std::string(typeStr), {}, {}, canvasPos,
          ImVec2(0, 0),   {}};
      // Ensure new nodes match current scope
      new_node.parent_id = current_view_parent_id;

      if (std::string(typeStr) == "Subflow" || std::string(typeStr) == "Loop") {
        new_node.is_group = true;
        new_node.size = ImVec2(300, 200);  // Default group size
      }
      // Note: We always generate pins based on module metadata now, even for
      // Groups.
      generate_pins_for_node(new_node);

      push_undo_snapshot();
      nodes.push_back(new_node);

      // Apply position
      ed::SetNodePosition(ed::NodeId(id), canvasPos);
      dirty = true;
    }
    ImGui::EndDragDropTarget();
  }

  ImGui::End();  // End the "Pipeline" window

  // Actually render the property inspector panel!
  draw_property_inspector();

  // --- Toast notification overlay (Global scope) ---
  if (toast_timer > 0.0f) {
    toast_timer -= ImGui::GetIO().DeltaTime;
    float alpha = std::min(toast_timer, 1.0f);

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 120.0f),
        ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.85f * alpha);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0.9f, 0.2f, 0.2f, 0.85f * alpha));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,
                          ImVec4(0.9f, 0.2f, 0.2f, 0.85f * alpha));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 10));

    if (ImGui::Begin("##Toast", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_Tooltip)) {
      ImGui::Text("%s", toast_message.c_str());
      ImGui::End();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
  }
}

std::string NodeManager::get_static_type_by_path(const std::string& path) {
  std::vector<std::string> parts;
  std::stringstream ss(path);
  std::string segment;
  while (std::getline(ss, segment, '.')) {
    parts.push_back(segment);
  }

  if (parts.empty()) return "";

  // 1. Handle NodeName
  std::string node_name = parts[0];
  const NodeObject* target_node = nullptr;
  for (const auto& n : nodes) {
    if (n.name == node_name) {
      target_node = &n;
      break;
    }
  }
  if (!target_node) return "";
  if (parts.size() == 1) return target_node->type;

  // 2. Handle Pin
  std::string pin_name = parts[1];
  std::string current_type = "";
  if (target_node->is_group) {
    if (pin_name == "in") current_type = std::string(TypeSystem::TRIGGER);
    if (pin_name == "out") current_type = std::string(TypeSystem::TRIGGER);
  }

  for (const auto& pin : target_node->inputs) {
    if (pin.name == pin_name) current_type = pin.type;
  }
  for (const auto& pin : target_node->outputs) {
    if (pin.name == pin_name) current_type = pin.type;
  }

  if (current_type.empty()) return "";
  if (parts.size() == 2) return current_type;

  // 3. Handle Property
  auto& all_props = TypeRegistry::get_property_metadata();

  for (size_t i = 2; i < parts.size(); ++i) {
    std::string prop_name = parts[i];

    if (prop_name.size() > 2 &&
        prop_name.substr(prop_name.size() - 2) == "()") {
      prop_name = prop_name.substr(0, prop_name.size() - 2);
    }

    if (!all_props.count(current_type)) return "";

    bool found = false;
    for (const auto& prop_meta : all_props.at(current_type)) {
      if (prop_meta.name == prop_name) {
        current_type = prop_meta.return_type;
        found = true;
        break;
      }
    }
    if (!found) return "";
  }

  return current_type;
}

bool NodeManager::validate_pipeline_before_save(std::string& out_error) {
  std::unordered_map<std::string, ed::NodeId> name_to_id;
  std::vector<NodeObject*> condition_nodes;

  for (auto& n : nodes) {
    if (name_to_id.count(n.name)) {
      out_error = "Duplicate node name detected: " + n.name;
      return false;
    }
    name_to_id[n.name] = n.id;
    if (n.type == "Condition") {
      condition_nodes.push_back(&n);
    }
  }

  std::unordered_map<uintptr_t, std::unordered_set<uintptr_t>> adj;
  for (auto& link : links) {
    ed::NodeId src_node, dst_node;
    if (find_pin(link.start_pin, &src_node) &&
        find_pin(link.end_pin, &dst_node)) {
      adj[src_node.Get()].insert(dst_node.Get());
    }
  }

  auto is_reachable = [&](uintptr_t from, uintptr_t to) {
    if (from == to) return true;
    std::unordered_set<uintptr_t> visited;
    std::vector<uintptr_t> stack = {from};
    while (!stack.empty()) {
      auto curr = stack.back();
      stack.pop_back();
      if (curr == to) return true;
      if (visited.count(curr)) continue;
      visited.insert(curr);
      for (auto neighbor : adj[curr]) {
        stack.push_back(neighbor);
      }
    }
    return false;
  };

  // Helper: check if `inner_node` is a (direct or indirect) child of a Loop
  // node whose name is `loop_name`.
  auto is_inside_loop = [&](const NodeObject* inner_node,
                            const std::string& loop_name) -> bool {
    if (!name_to_id.count(loop_name)) return false;
    ed::NodeId loop_id = name_to_id[loop_name];

    // Walk up the parent chain of inner_node
    ed::NodeId cur = inner_node->parent_id;
    while (cur.Get() != 0) {
      if (cur == loop_id) return true;
      // Find the node with id == cur to get its parent
      bool found = false;
      for (const auto& n : nodes) {
        if (n.id == cur) {
          cur = n.parent_id;
          found = true;
          break;
        }
      }
      if (!found) break;
    }
    return false;
  };

  for (auto* cond : condition_nodes) {
    if (!cond->parameters.count("condition_mode") ||
        std::any_cast<std::string>(cond->parameters.at("condition_mode")) !=
            "expression") {
      continue;
    }

    std::string expr =
        std::any_cast<std::string>(cond->parameters.at("expression"));

    std::vector<std::string> vars;
    std::string syntax_err;
    if (!ConditionEvaluator::validate_expression_syntax_and_vars(expr, vars,
                                                                 syntax_err)) {
      out_error = "Condition Node '" + cond->name +
                  "' expression syntax error:\n" + syntax_err;
      return false;
    }

    std::vector<std::string> var_types;
    for (const auto& var : vars) {
      size_t dot_pos = var.find('.');
      std::string node_name =
          (dot_pos != std::string::npos) ? var.substr(0, dot_pos) : var;
      std::string pin_name =
          (dot_pos != std::string::npos) ? var.substr(dot_pos + 1) : "";

      if (!name_to_id.count(node_name)) {
        out_error = "Condition Node '" + cond->name +
                    "' references non-existent node: " + node_name;
        return false;
      }

      // --- Loop-aware reachability check ---
      // If the referenced node is a Loop AND this Condition lives inside that
      // Loop, then Loop's item/index/dynamic outputs are always available
      // before any child node runs — no adj-graph check needed.
      bool reachable = false;
      NodeObject* ref_node = nullptr;
      for (auto& n : nodes)
        if (n.name == node_name) {
          ref_node = &n;
          break;
        }

      if (ref_node && ref_node->type == "Loop" &&
          is_inside_loop(cond, node_name)) {
        // The Loop that contains this Condition always executes before it.
        reachable = true;
      } else {
        ed::NodeId target_id = name_to_id[node_name];
        reachable = is_reachable(target_id.Get(), cond->id.Get());
      }

      if (!reachable) {
        out_error = "Condition '" + cond->name + "' uses variable from '" +
                    node_name + "', but '" + node_name +
                    "' does not execute before it.\n"
                    "Please connect them with a flow or data link.";
        return false;
      }

      // --- Type resolution ---
      // For Loop built-in outputs (item / index), derive the type directly
      // from the Loop node's pin metadata rather than the type registry path,
      // because the path "Loop_X.item" may not be registered globally.
      std::string actual_type;
      if (ref_node && ref_node->type == "Loop") {
        if (pin_name == "index") {
          actual_type = TypeSystem::INT;
        } else if (pin_name == "item") {
          // item type depends on loop mode; treat as ANY for validation
          actual_type = TypeSystem::ANY;
        } else {
          // dynamic output (out_vec_N) — treat as ANY
          actual_type = TypeSystem::ANY;
        }
      } else {
        actual_type = get_static_type_by_path(var);
        if (actual_type.empty()) {
          out_error = "Condition Node '" + cond->name +
                      "' references invalid variable or property: $" + var;
          return false;
        }
      }

      if (expr == "$" + var || expr == "!$" + var) {
        if (actual_type != TypeSystem::BOOLEAN &&
            actual_type != TypeSystem::INT && actual_type != TypeSystem::ANY) {
          out_error = "Condition Node '" + cond->name + "' uses '" + var +
                      "' (Type: " + actual_type +
                      ") as a standalone condition. " +
                      "Only bool or int types are allowed here.";
          return false;
        }
      }
      var_types.push_back(actual_type);
    }

    // check if all the var types are same (skip if any is ANY — it's wildcard)
    bool has_any = false;
    for (const auto& t : var_types)
      if (t == TypeSystem::ANY) {
        has_any = true;
        break;
      }

    if (!has_any) {
      for (size_t i = 1; i < var_types.size(); ++i) {
        if (var_types[i] != var_types[0]) {
          out_error = "Condition Node '" + cond->name +
                      "' uses variables of different types. " +
                      "All variables in an expression must have the same type.";
          return false;
        }
      }
    }
  }

  return true;
}

bool NodeManager::save(const std::string& path, bool is_auto_save) {
  std::string validation_error;
  if (!validate_pipeline_before_save(validation_error)) {
    std::cerr << "Pipeline Validation Failed prior to save!\n"
              << validation_error << std::endl;
    if (!is_auto_save) {
      show_toast("Save Failed! Check console. Error: " + validation_error,
                 5.0f);
    }
    return false;
  }

  nlohmann::json root;
  nlohmann::json layout_root = nlohmann::json::object();
  root["children"] = nlohmann::json::array();

  // Map each pin ID to its parent node and pin name for precise lookup
  std::map<unsigned long long, std::pair<std::string, std::string>> pinInfo;
  for (auto& n : nodes) {
    for (auto& pin : n.inputs) {
      pinInfo[pin.id.Get()] = {n.name, pin.name};
    }
    for (auto& pin : n.outputs) {
      pinInfo[pin.id.Get()] = {n.name, pin.name};
    }
  }

  // Recursive lambda to collect nodes into a json array
  std::function<nlohmann::json(ed::NodeId)> collect_children =
      [&](ed::NodeId parent) {
        nlohmann::json arr = nlohmann::json::array();
        for (auto& node : nodes) {
          if (node.parent_id == parent) {
            // Loop_Entry and Loop_Exit are runtime proxy nodes; they are always
            // recreated by ensure_loop_proxy_nodes() at load time. Skip them.
            if (node.type == "Loop_Entry" || node.type == "Loop_Exit") continue;

            nlohmann::json n_json;
            n_json["id"] = node.name;
            n_json["type"] = node.type;

            n_json["inputs"] = nlohmann::json::object();
            n_json["outputs"] = nlohmann::json::array();
            n_json["parameters"] = nlohmann::json::object();

            auto& registry = ModuleFactory::get_metadata_registry();
            if (registry.count(node.type)) {
              const auto& meta = registry.at(node.type);
              for (const auto& param_meta : meta.parameters) {
                if (node.parameters.count(param_meta.name) &&
                    param_meta.serialize) {
                  try {
                    param_meta.serialize(n_json["parameters"], param_meta.name,
                                         node.parameters.at(param_meta.name));
                  } catch (...) {
                  }
                }
              }
            }

            // Record all non-flow outputs this node provides
            for (auto& pin : node.outputs) {
              if (!pin.is_flow) {
                n_json["outputs"].push_back(pin.name);
              }
            }

            // Find which nodes output to this node's specific inputs
            nlohmann::json flow_links_json = nlohmann::json::array();
            for (auto& link : links) {
              for (auto& pin : node.inputs) {
                if (pin.id == link.end_pin) {
                  auto sourceInfo = pinInfo[link.start_pin.Get()];
                  std::string source_node_name = sourceInfo.first;
                  if (source_node_name == "Loop_Entry") {
                    if (NodeObject* parent_loop = find_node(node.parent_id);
                        parent_loop) {
                      source_node_name = parent_loop->name;
                    }
                  }

                  if (link.is_flow_link || pin.is_flow) {
                    flow_links_json.push_back(source_node_name + "." +
                                              sourceInfo.second);
                  } else {
                    n_json["inputs"][pin.name] =
                        source_node_name + "." + sourceInfo.second;
                  }
                  break;
                }
              }
            }
            if (!flow_links_json.empty()) {
              n_json["flow_links"] = flow_links_json;
            }

            // Save position and size to separate layout JSON
            // (We update node.pos from editor first to ensure it's fresh)
            if (ed::GetCurrentEditor()) {
              ImVec2 editor_pos = ed::GetNodePosition(node.id);
              // Only update if the editor provides valid coordinates.
              // This prevents overwriting with FLT_MAX for non-rendered nodes.
              if (editor_pos.x != FLT_MAX && editor_pos.y != FLT_MAX) {
                node.pos = editor_pos;
              }
            }
            layout_root[node.name]["pos"] = {node.pos.x, node.pos.y};
            if (node.is_group) {
              layout_root[node.name]["size"] = {node.size.x, node.size.y};
            }

            // If it's a group, recurse
            if (node.is_group) {
              n_json["children"] = collect_children(node.id);
            }

            if (node.type == "Loop") {
              nlohmann::json dyn_out = nlohmann::json::array();
              for (auto& p : node.outputs) {
                if (p.name == "item" || p.name == "index" ||
                    p.name == UI_FLOW_OUT_PIN_NAME)
                  continue;

                std::string source_path = p.source_path;

                if (source_path.find("Loop_Entry") != std::string::npos) {
                  if (size_t dot_pos = source_path.find('.');
                      dot_pos != std::string::npos) {
                    source_path = node.name + source_path.substr(dot_pos);
                  }
                }

                dyn_out.push_back({{"name", p.name}, {"source", source_path}});
              }
              n_json["dynamic_outputs"] = dyn_out;
            }

            arr.push_back(n_json);
          }
        }
        return arr;
      };

  // Start collection from root (parent_id == 0)
  root["children"] = collect_children(0);

  // Determine layout path via GlobalConfig
  std::string layout_path = GlobalConfig::get().get_data().pipeline_layout_tmp;

  // Save logic JSON
  std::ofstream o(path);
  o << root.dump(4);

  // Save layout JSON
  std::ofstream lo(layout_path);
  lo << layout_root.dump(4);

  if (is_auto_save) {
    std::cout << "Pipeline auto-saved to temporary file: " << path << std::endl;
  } else {
    std::cout << "Saved to " << path << std::endl;
    clear_dirty();
  }
  return true;
}

void NodeManager::load(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) return;

  nlohmann::json root;
  try {
    f >> root;
  } catch (...) {
    return;
  }

  nodes.clear();
  links.clear();
  initialized_nodes.clear();
  next_id = 1;

  // Determine layout path via GlobalConfig
  std::string layout_path = GlobalConfig::get().get_data().pipeline_layout_tmp;

  nlohmann::json layout_root = nlohmann::json::object();
  std::ifstream lf(layout_path);
  if (lf.is_open()) {
    try {
      lf >> layout_root;
    } catch (...) {
    }
  }

  if (!root.contains("children")) return;

  std::map<std::string, NodeObject*> name_to_node;
  std::vector<nlohmann::json> all_nodes_json;

  // 1. Reconstruct all nodes recursively
  std::function<void(const nlohmann::json&, ed::NodeId)> parse_nodes =
      [&](const nlohmann::json& children_arr, ed::NodeId parent) {
        for (const auto& node_cfg : children_arr) {
          all_nodes_json.push_back(node_cfg);  // Save flat for link pass later

          std::string name = node_cfg.value("id", "Unknown");
          std::string type = node_cfg.value("type", "Default");

          uintptr_t id = get_hash_id(name);
          NodeObject new_node = {ed::NodeId(id), name,         type, {}, {},
                                 ImVec2(0, 0),   ImVec2(0, 0), {}};
          new_node.parent_id = parent;

          // Load position and size for all nodes
          if (layout_root.contains(name)) {
            auto l_cfg = layout_root[name];
            if (l_cfg.contains("pos") && l_cfg["pos"].is_array() &&
                l_cfg["pos"].size() == 2) {
              new_node.pos = ImVec2(l_cfg["pos"][0].get<float>(),
                                    l_cfg["pos"][1].get<float>());
            }
            if (l_cfg.contains("size") && l_cfg["size"].is_array() &&
                l_cfg["size"].size() == 2) {
              new_node.size = ImVec2(l_cfg["size"][0].get<float>(),
                                     l_cfg["size"][1].get<float>());
            }
          } else {
            // Fallback to logic JSON (backward compatibility)
            if (node_cfg.contains("pos") && node_cfg["pos"].is_array() &&
                node_cfg["pos"].size() == 2) {
              new_node.pos = ImVec2(node_cfg["pos"][0].get<float>(),
                                    node_cfg["pos"][1].get<float>());
            }
            if (node_cfg.contains("size") && node_cfg["size"].is_array() &&
                node_cfg["size"].size() == 2) {
              new_node.size = ImVec2(node_cfg["size"][0].get<float>(),
                                     node_cfg["size"][1].get<float>());
            }
          }
          if (new_node.size.x == 0 && new_node.size.y == 0) {
            if (type == "Subflow" || type == "Loop") {
              new_node.size = ImVec2(300, 200);  // default fallback for groups
            }
          }

          if (type == "Subflow" || type == "Loop") {
            new_node.is_group = true;
          }
          // Note: Generate pins unconditionally to recover metadata pins for
          // groups loaded from disk
          generate_pins_for_node(new_node);

          if (node_cfg.contains("parameters") &&
              node_cfg["parameters"].is_object()) {
            auto param_cfg = node_cfg["parameters"];
            auto& registry = ModuleFactory::get_metadata_registry();
            if (registry.count(type)) {
              for (const auto& param_meta : registry.at(type).parameters) {
                if (param_cfg.contains(param_meta.name) &&
                    param_meta.deserialize) {
                  try {
                    new_node.parameters[param_meta.name] =
                        param_meta.deserialize(param_cfg, param_meta.name);
                  } catch (...) {
                  }
                }
              }
            }
          }

          // Restore dynamic output pins for Loop nodes (user-added via
          // "Add Output"). generate_pins_for_node only adds static metadata
          // pins (item, index); dynamic ones are stored separately.
          if (type == "Loop" && node_cfg.contains("dynamic_outputs") &&
              node_cfg["dynamic_outputs"].is_array()) {
            for (const auto& dyn : node_cfg["dynamic_outputs"]) {
              std::string pin_name = dyn.value("name", "");
              std::string source = dyn.value("source", "");

              // Skip built-in pins that generate_pins_for_node already added
              if (pin_name == "item" || pin_name == "index" ||
                  pin_name == "__flow_out" || pin_name.empty())
                continue;

              // Avoid duplicates (shouldn't happen, but guard anyway)
              bool already = false;
              for (const auto& p : new_node.outputs)
                if (p.name == pin_name) {
                  already = true;
                  break;
                }
              if (already) continue;

              Pin dyn_pin;
              dyn_pin.id = ed::PinId(get_hash_id(
                  name + "::dyn_" + std::to_string(new_node.outputs.size())));
              dyn_pin.name = pin_name;
              dyn_pin.type = "vector";
              dyn_pin.kind = ed::PinKind::Output;
              dyn_pin.is_flow = false;
              dyn_pin.source_path = source;
              new_node.outputs.push_back(dyn_pin);
            }
          }

          nodes.push_back(new_node);

          if (node_cfg.contains("children") &&
              node_cfg["children"].is_array()) {
            parse_nodes(node_cfg["children"], ed::NodeId(id));
          }
        }
      };

  parse_nodes(root["children"], 0);

  // Fix any legacy ID collisions before building links
  reassign_duplicate_ids();

  // Build name dictionary to find pin pairs
  for (auto& n : nodes) {
    name_to_node[n.name] = &n;
  }

  // 2. Reconstruct data links (inputs) and flow links
  for (const auto& node_cfg : all_nodes_json) {
    std::string consumer_name = node_cfg.value("id", "");
    if (!name_to_node.count(consumer_name)) continue;

    NodeObject* consumer = name_to_node[consumer_name];

    // --- Data links ---
    if (node_cfg.contains("inputs") && node_cfg["inputs"].is_object()) {
      for (auto& el : node_cfg["inputs"].items()) {
        std::string current_pin_name = el.key();
        std::string source_full = el.value().get<std::string>();

        size_t dot_pos = source_full.find('.');
        if (dot_pos == std::string::npos) continue;

        std::string producer_name = source_full.substr(0, dot_pos);
        std::string producer_pin_name = source_full.substr(dot_pos + 1);

        if (!name_to_node.count(producer_name)) continue;
        NodeObject* producer = name_to_node[producer_name];

        ed::PinId start_pin = 0;

        // If the producer is a Loop node and the pin is item/index, redirect
        // to the Loop_Entry proxy node's output pin. These pins use stable
        // hash IDs produced by ensure_loop_proxy_nodes(), which is called
        // after loading to recreate proxy nodes.
        if (producer->type == "Loop" &&
            (producer_pin_name == "item" || producer_pin_name == "index")) {
          // Derive the stable hash ID that ensure_loop_proxy_nodes() assigns
          start_pin = ed::PinId(
              get_hash_id(producer_name + "_Entry_" + producer_pin_name));
        } else {
          for (auto& pin : producer->outputs) {
            if (pin.name == producer_pin_name) {
              start_pin = pin.id;
              break;
            }
          }
        }

        ed::PinId end_pin = 0;
        for (auto& pin : consumer->inputs) {
          if (pin.name == current_pin_name) {
            end_pin = pin.id;
            break;
          }
        }

        if (start_pin.Get() != 0 && end_pin.Get() != 0) {
          links.push_back(
              {ed::LinkId(get_next_id()), start_pin, end_pin, false});
        }
      }
    }

    // --- Flow links ---
    if (node_cfg.contains("flow_links") && node_cfg["flow_links"].is_array()) {
      for (const auto& fl : node_cfg["flow_links"]) {
        std::string source_full = fl.get<std::string>();

        size_t dot_pos = source_full.find('.');
        if (dot_pos == std::string::npos) continue;

        std::string producer_name = source_full.substr(0, dot_pos);
        std::string producer_pin_name = source_full.substr(dot_pos + 1);

        if (!name_to_node.count(producer_name)) continue;
        NodeObject* producer = name_to_node[producer_name];

        // Source is an output pin (e.g. __flow_out / True / False)
        ed::PinId start_pin = 0;
        for (auto& pin : producer->outputs) {
          if (pin.name == producer_pin_name) {
            start_pin = pin.id;
            break;
          }
        }
        // Destination is this node's __flow_in input pin
        ed::PinId end_pin = 0;
        for (auto& pin : consumer->inputs) {
          if (pin.name == UI_FLOW_IN_PIN_NAME) {
            end_pin = pin.id;
            break;
          }
        }

        if (start_pin.Get() != 0 && end_pin.Get() != 0) {
          links.push_back(
              {ed::LinkId(get_next_id()), start_pin, end_pin, true});
        }
      }
    }
  }

  // Reconstruct Loop proxy nodes (Loop_Entry / Loop_Exit) for every Loop that
  // was loaded. They are not serialised, so we must recreate them here so that
  // internal links (which reference Loop_Entry's stable hash pin IDs) resolve
  // correctly when the user enters the loop context.
  for (auto& node : nodes) {
    if (node.type == "Loop") {
      ensure_loop_proxy_nodes(node);
    }
  }

  // After proxy nodes are rebuilt, restore the visual links from internal
  // source nodes to Loop_Exit input pins.  The source paths are stored in
  // each Loop's dynamic_outputs[].source field.
  // Re-build name_to_node map since ensure_loop_proxy_nodes may have added
  // Loop_Entry / Loop_Exit nodes.
  for (auto& n : nodes) {
    name_to_node[n.name] = &n;
  }

  // sync_loop_exit_pins populates Loop_Exit input pins from the parent Loop's
  // output list. Call it now so the pin IDs exist before we rebuild links.
  for (auto& n : nodes) {
    if (n.type == "Loop_Exit") {
      sync_loop_exit_pins(n);
    }
  }

  // fix links for Loop_Entry and LoopExit
  for (auto& node : nodes) {
    if (node.type == "Loop") {
      NodeObject *entry_proxy = nullptr, *exit_proxy = nullptr;
      for (auto& n : nodes) {
        if (n.parent_id == node.id) {
          if (n.type == "Loop_Entry") entry_proxy = &n;
          if (n.type == "Loop_Exit") exit_proxy = &n;
        }
      }
      if (!entry_proxy || !exit_proxy) continue;

      for (auto& op : node.outputs) {
        if (op.source_path.empty()) continue;

        // If the source_path is node name itself, e.g. Loop_1.item
        // the link should started from LoopEntry
        if (op.source_path.find(node.name + ".") == 0) {
          std::string attr = op.source_path.substr(node.name.length() + 1);

          Pin *srcPin = nullptr, *dstPin = nullptr;
          for (auto& p : entry_proxy->outputs) {
            if (p.name == attr) {
              srcPin = &p;
              break;
            }
          }
          for (auto& p : exit_proxy->inputs) {
            if (p.name == op.name) {
              dstPin = &p;
              break;
            }
          }

          if (srcPin && dstPin) {
            links.push_back(
                {ed::LinkId(get_next_id()), srcPin->id, dstPin->id, false});
          }
        }
      }
    }
  }

  for (const auto& node_cfg_outer : all_nodes_json) {
    if (node_cfg_outer.value("type", "") != "Loop") continue;
    if (!node_cfg_outer.contains("dynamic_outputs")) continue;

    std::string loop_name = node_cfg_outer.value("id", "");
    if (!name_to_node.count(loop_name)) continue;
    NodeObject* loop_node = name_to_node[loop_name];

    // Find Loop_Exit proxy for this loop
    NodeObject* loop_exit = nullptr;
    for (auto& n : nodes) {
      if (n.parent_id == loop_node->id && n.type == "Loop_Exit") {
        loop_exit = &n;
        break;
      }
    }
    if (!loop_exit) continue;

    for (const auto& dyn : node_cfg_outer["dynamic_outputs"]) {
      std::string pin_name = dyn.value("name", "");
      std::string source = dyn.value("source", "");
      if (pin_name.empty() || source.empty()) continue;

      // Parse "SourceNode.pin_name"
      size_t dot = source.find('.');
      if (dot == std::string::npos) continue;
      std::string src_node_name = source.substr(0, dot);
      std::string src_pin_name = source.substr(dot + 1);

      if (!name_to_node.count(src_node_name)) continue;
      NodeObject* src_node = name_to_node[src_node_name];

      // Find source output pin
      ed::PinId start_pin = 0;
      for (auto& p : src_node->outputs) {
        if (p.name == src_pin_name) {
          start_pin = p.id;
          break;
        }
      }
      // Find Loop_Exit input pin with matching name
      ed::PinId end_pin = 0;
      for (auto& p : loop_exit->inputs) {
        if (p.name == pin_name) {
          end_pin = p.id;
          break;
        }
      }

      if (start_pin.Get() != 0 && end_pin.Get() != 0) {
        // Avoid duplicate links
        bool dup = false;
        for (const auto& lk : links)
          if (lk.start_pin == start_pin && lk.end_pin == end_pin) {
            dup = true;
            break;
          }
        if (!dup)
          links.push_back(
              {ed::LinkId(get_next_id()), start_pin, end_pin, false});
      }
    }
  }

  clear_dirty();
}

using PropertyDrawFunc = std::function<bool(const std::string&, std::any&,
                                            const ParameterMetadata&)>;
static std::unordered_map<std::string, PropertyDrawFunc>&
get_property_drawers() {
  static std::unordered_map<std::string, PropertyDrawFunc> drawers;
  static bool init = false;
  if (!init) {
    init = true;
    drawers["int"] = [](const std::string& name, std::any& val,
                        const ParameterMetadata& meta) {
      int v = std::any_cast<int>(val);
      if (ImGui::InputInt(name.c_str(), &v)) {
        val = v;
        return true;
      }
      return false;
    };
    drawers["float"] = [](const std::string& name, std::any& val,
                          const ParameterMetadata& meta) {
      float v = std::any_cast<float>(val);
      if (ImGui::SliderFloat(name.c_str(), &v,
                             std::any_cast<float>(meta.min_val),
                             std::any_cast<float>(meta.max_val))) {
        val = v;
        return true;
      }
      return false;
    };
    drawers["bool"] = [](const std::string& name, std::any& val,
                         const ParameterMetadata& meta) {
      bool v = std::any_cast<bool>(val);
      if (ImGui::Checkbox(name.c_str(), &v)) {
        val = v;
        return true;
      }
      return false;
    };
    drawers["string"] = [](const std::string& name, std::any& val,
                           const ParameterMetadata& meta) {
      std::string v = std::any_cast<std::string>(val);
      if (!meta.options.empty()) {
        // Show as dropdown
        int current_idx = 0;
        for (int i = 0; i < (int)meta.options.size(); ++i) {
          if (meta.options[i] == v) {
            current_idx = i;
            break;
          }
        }
        // Build null-terminated list for ImGui::Combo
        std::string items_str;
        for (const auto& opt : meta.options) {
          items_str += opt;
          items_str += '\0';
        }
        items_str += '\0';
        if (ImGui::Combo(name.c_str(), &current_idx, items_str.c_str())) {
          val = meta.options[current_idx];
          return true;
        }
        return false;
      }
      // Fallback: text input
      char buf[256];
      strncpy(buf, v.c_str(), sizeof(buf));
      buf[sizeof(buf) - 1] = 0;
      if (ImGui::InputText(name.c_str(), buf, sizeof(buf))) {
        val = std::string(buf);
        return true;
      }
      return false;
    };
    drawers["double"] = [](const std::string& name, std::any& val,
                           const ParameterMetadata& meta) {
      double v = std::any_cast<double>(val);
      if (ImGui::InputDouble(name.c_str(), &v)) {
        val = v;
        return true;
      }
      return false;
    };
    drawers[std::string(TypeSystem::INT_ARRAY)] =
        [](const std::string& name, std::any& val,
           const ParameterMetadata& meta) {
          auto vec = std::any_cast<std::vector<int>>(val);
          bool changed = false;
          if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            int count = vec.size();
            if (ImGui::InputInt(("Size##" + name).c_str(), &count)) {
              if (count < 0) count = 0;
              vec.resize(count);
              changed = true;
            }
            for (size_t i = 0; i < vec.size(); ++i) {
              if (ImGui::InputInt(
                      ("[" + std::to_string(i) + "]##" + name).c_str(),
                      &vec[i])) {
                changed = true;
              }
            }
            ImGui::TreePop();
          }
          if (changed) val = vec;
          return changed;
        };
    drawers[std::string(TypeSystem::FLOAT_ARRAY)] =
        [](const std::string& name, std::any& val,
           const ParameterMetadata& meta) {
          auto vec = std::any_cast<std::vector<float>>(val);
          bool changed = false;
          if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            int count = vec.size();
            if (ImGui::InputInt(("Size##" + name).c_str(), &count)) {
              if (count < 0) count = 0;
              vec.resize(count);
              changed = true;
            }
            for (size_t i = 0; i < vec.size(); ++i) {
              if (ImGui::InputFloat(
                      ("[" + std::to_string(i) + "]##" + name).c_str(),
                      &vec[i])) {
                changed = true;
              }
            }
            ImGui::TreePop();
          }
          if (changed) val = vec;
          return changed;
        };
    drawers[std::string(TypeSystem::DOUBLE_ARRAY)] =
        [](const std::string& name, std::any& val,
           const ParameterMetadata& meta) {
          auto vec = std::any_cast<std::vector<double>>(val);
          bool changed = false;
          if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            int count = vec.size();
            if (ImGui::InputInt(("Size##" + name).c_str(), &count)) {
              if (count < 0) count = 0;
              vec.resize(count);
              changed = true;
            }
            for (size_t i = 0; i < vec.size(); ++i) {
              if (ImGui::InputDouble(
                      ("[" + std::to_string(i) + "]##" + name).c_str(),
                      &vec[i])) {
                changed = true;
              }
            }
            ImGui::TreePop();
          }
          if (changed) val = vec;
          return changed;
        };
    drawers[std::string(TypeSystem::STRING_ARRAY)] =
        [](const std::string& name, std::any& val,
           const ParameterMetadata& meta) {
          auto vec = std::any_cast<std::vector<std::string>>(val);
          bool changed = false;
          if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            int count = vec.size();
            if (ImGui::InputInt(("Size##" + name).c_str(), &count)) {
              if (count < 0) count = 0;
              vec.resize(count);
              changed = true;
            }
            for (size_t i = 0; i < vec.size(); ++i) {
              char buf[256];
              strncpy(buf, vec[i].c_str(), sizeof(buf));
              buf[sizeof(buf) - 1] = 0;
              if (ImGui::InputText(
                      ("[" + std::to_string(i) + "]##" + name).c_str(), buf,
                      sizeof(buf))) {
                vec[i] = buf;
                changed = true;
              }
            }
            ImGui::TreePop();
          }
          if (changed) val = vec;
          return changed;
        };
  }
  return drawers;
}

void NodeManager::draw_property_inspector() {
  ImGui::SetNextWindowPos(ImVec2(1130, 160), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(300, 730), ImGuiCond_FirstUseEver);

  // Force inspector to front if we can, to ensure it doesn't get hidden behind
  // anything
  ImGui::Begin("Node Inspector");

  bool node_selected = (selected_node_for_properties.Get() != 0);
  NodeObject* selected_node = nullptr;

  if (node_selected) {
    for (auto& n : nodes) {
      if (n.id == selected_node_for_properties) {
        selected_node = &n;
        break;
      }
    }
    if (!selected_node) {
      selected_node_for_properties = 0;  // Node no longer exists
      node_selected = false;
    }
  }

  if (!node_selected) {
    ImGui::TextDisabled("Select a node to edit its properties.");
  }

  if (node_selected) {
    ImGui::Text("Node: %s", selected_node->name.c_str());
    ImGui::TextDisabled("Type: %s", selected_node->type.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    auto& registry = ModuleFactory::get_metadata_registry();
    if (registry.count(selected_node->type)) {
      const auto& meta = registry.at(selected_node->type);
      if (meta.parameters.empty()) {
        ImGui::TextDisabled("No properties available for this module.");
      } else {
        for (const auto& param : meta.parameters) {
          // Ensure parameter is instantiated in the node
          if (!selected_node->parameters.count(param.name)) {
            selected_node->parameters[param.name] = param.default_val;
          }

          // Skip parameter if its visibility predicate returns false
          if (param.visible_when &&
              !param.visible_when(selected_node->parameters)) {
            continue;
          }

          ImGui::PushID(param.name.c_str());
          bool changed = false;

          auto& drawers = get_property_drawers();
          if (drawers.count(param.type_name)) {
            if (drawers.at(param.type_name)(
                    param.name, selected_node->parameters[param.name], param)) {
              changed = true;
            }
          } else {
            ImGui::TextDisabled("Unsupported parameter type: %s",
                                param.type_name.c_str());
          }

          if (changed) dirty = true;
          ImGui::PopID();
        }
      }
    }

    if (selected_node->type == "Loop") {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "Loop Output Mappings:");
      ImGui::Indent();

      for (size_t i = 0; i < selected_node->outputs.size(); ++i) {
        auto& pin = selected_node->outputs[i];

        if (pin.is_flow) continue;

        ImGui::PushID(pin.id.AsPointer());

        // item & index is fixedly generated
        if (pin.name == "item" || pin.name == "index") {
          ImGui::BulletText("%s", pin.name.c_str());
          ImGui::TextDisabled("  Value generated by loop context.");
          ImGui::Separator();
          ImGui::PopID();
          continue;
        }

        // --- for dynamic outputs (Commit on Deactivate)
        char name_buf[64];
        const char* p_label =
            (renaming_pin_id == pin.id) ? renaming_pin_buf : pin.name.c_str();
        strncpy(name_buf, p_label, sizeof(name_buf));

        if (ImGui::InputText("Pin Name", name_buf, sizeof(name_buf))) {
          if (renaming_pin_id != pin.id) {
            renaming_pin_id = pin.id;
          }
          strncpy(renaming_pin_buf, name_buf, sizeof(renaming_pin_buf));
        }

        if (ImGui::IsItemDeactivatedAfterEdit() && renaming_pin_id == pin.id) {
          if (pin.name != renaming_pin_buf) {
            // Check for duplicate pin names in this node
            bool pin_exists = false;
            for (const auto& other_p : selected_node->outputs) {
              if (&other_p != &pin && other_p.name == renaming_pin_buf) {
                pin_exists = true;
                break;
              }
            }

            if (pin_exists) {
              show_toast("Pin name '" + std::string(renaming_pin_buf) +
                             "' already exists in this node!",
                         3.0f);
            } else {
              pin.name = renaming_pin_buf;
              dirty = true;
            }
          }
          renaming_pin_id = 0;
        }

        // collect source path (e.g., "$Process.out")
        char src_buf[128];
        strncpy(src_buf, pin.source_path.c_str(), sizeof(src_buf));
        if (ImGui::InputText("Collect From", src_buf, sizeof(src_buf))) {
          pin.source_path = src_buf;
          dirty = true;
        }

        if (ImGui::Button("Remove Pin")) {
          ed::PinId pin_id_to_remove = pin.id;
          std::string pin_name_to_remove = pin.name;

          links.erase(std::remove_if(links.begin(), links.end(),
                                     [&](const LinkObject& l) {
                                       return l.start_pin == pin_id_to_remove ||
                                              l.end_pin == pin_id_to_remove;
                                     }),
                      links.end());

          selected_node->outputs.erase(selected_node->outputs.begin() + i);
          pin.source_path = "";

          if (selected_node->type == "Loop") {
            for (auto& n : nodes) {
              if (n.parent_id == selected_node->id && n.type == "Loop_Exit") {
                ed::PinId exit_input_pin_id = 0;
                for (auto& exit_p : n.inputs) {
                  if (exit_p.name == pin_name_to_remove) {
                    exit_input_pin_id = exit_p.id;
                    break;
                  }
                }

                if (exit_input_pin_id.Get() != 0) {
                  links.erase(std::remove_if(links.begin(), links.end(),
                                             [&](const LinkObject& l) {
                                               return l.end_pin ==
                                                      exit_input_pin_id;
                                             }),
                              links.end());
                }

                sync_loop_exit_pins(n);
                break;
              }
            }
          }

          dirty = true;
          i--;
        }

        ImGui::Separator();
        ImGui::PopID();
      }
    }
  }

  ImGui::End();
}

void NodeManager::reassign_duplicate_ids() {
  std::unordered_set<uintptr_t> taken_ids;
  int fixed_count = 0;
  for (auto& node : nodes) {
    uintptr_t id = node.id.Get();
    if (taken_ids.count(id)) {
      // Collision! Find next available hash-like ID
      while (taken_ids.count(id)) {
        id++;
      }
      LOG_WARN("Fixing duplicate Node ID for node '"
               << node.name << "': " << node.id.Get() << " -> " << id);
      node.id = ed::NodeId(id);
      fixed_count++;

      // Update node ID and all its pins' IDs to maintain consistency
      for (auto& pin : node.inputs) {
        pin.id = ed::PinId(get_hash_id(node.name + "::" + pin.name));
      }
      for (auto& pin : node.outputs) {
        pin.id = ed::PinId(get_hash_id(node.name + "::" + pin.name));
      }
      dirty = true;
    }
    taken_ids.insert(id);
  }
  if (fixed_count > 0) {
    LOG_INFO("Successfully fixed " << fixed_count << " duplicate node IDs.");
  }
}
