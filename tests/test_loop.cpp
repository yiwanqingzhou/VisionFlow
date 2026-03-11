#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>

#include "core/blackboard.hpp"
#include "core/engine.hpp"
#include "core/factory.hpp"
#include "modules/loop_node.hpp"

// We create a minimal mock node just to capture loop execution details.
// It will be executed inside the loop.

class MockIncrementor : public BaseModule {
 public:
  bool inner_execute(Blackboard& db) override {
    // Read previous, increment, write back
    int count = db.has("Mock.Count") ? db.read<int>("Mock.Count") : 0;
    count++;
    db.write("Mock.Count", count);

    // Test that the loop writes the iteration parameter correctly
    if (db.has("Loop_1.index")) {
      int iter = db.read<int>("Loop_1.index");
      std::cout << "MockIncrementor executed at Loop index: " << iter << "\n";
    }

    // Capture items for for_each loop
    if (db.has("Loop_1.item")) {
      std::any item = db.read_any("Loop_1.item");
      if (item.type() == typeid(std::string)) {
        std::string s =
            db.has("Mock.Concat") ? db.read<std::string>("Mock.Concat") : "";
        s += std::any_cast<std::string>(item);
        db.write("Mock.Concat", s);
      }
    }

    return true;
  }
};

REGISTER_MODULE("MockIncrementor", MockIncrementor,
                (ModuleMetadata{"MockIncrementor", "Test", {}, {}, {}}));

void test_for_count() {
  std::cout << "[Test] Running 'for_count' loop test..." << std::endl;

  std::string pipeline_json = R"({
    "children": [
      {
        "id": "Loop_1",
        "type": "Loop",
        "parameters": {
          "loop_mode": "for_count",
          "min_iterations": 0,
          "max_iterations": 3,
          "step": 1
        },
        "children": [
          {
            "id": "Inc_1",
            "type": "MockIncrementor",
            "parameters": {}
          }
        ]
      }
    ]
  })";

  Blackboard db;
  WorkflowEngine engine;

  std::ofstream out("test_for_count.json");
  out << pipeline_json;
  out.close();

  bool build_ok = engine.build_graph_from_json("test_for_count.json", db);
  assert(build_ok);

  engine.execute_graph();

  assert(db.has("Mock.Count"));
  int final_count = db.read<int>("Mock.Count");
  std::cout << "Final count: " << final_count << "\n";
  assert(final_count == 3);

  std::cout << "[Test] 'for_count' loop test PASSED\n\n";
}

void test_for_each() {
  std::cout << "[Test] Running 'for_each' loop test..." << std::endl;

  std::string pipeline_json = R"({
    "children": [
      {
        "id": "Generate_1",
        "type": "Generate",
        "parameters": {
          "data_type": "string_array",
          "string_array_value": ["Hello", "_", "World"]
        }
      },
      {
        "id": "Loop_1",
        "type": "Loop",
        "flow_links": ["Generate_1.__flow_out"],
        "inputs": {
          "collection": "Generate_1.data_out"
        },
        "parameters": {
          "loop_mode": "for_each"
        },
        "children": [
          {
            "id": "Log_1",
            "type": "Log",
            "inputs": {
              "data_in": "Loop_1.item"
            },
            "parameters": {
              "log_level": "INFO",
              "prefix": "TestLoopItem: "
            }
          },
          {
            "id": "Inc_1",
            "type": "MockIncrementor",
            "flow_links": ["Log_1.__flow_out"],
            "parameters": {}
          }
        ]
      }
    ]
  })";

  Blackboard db;
  // Initialize mock count
  db.write("Mock.Count", 0);
  db.write("Mock.Concat", std::string(""));

  WorkflowEngine engine;

  std::ofstream out("test_for_each.json");
  out << pipeline_json;
  out.close();

  bool build_ok = engine.build_graph_from_json("test_for_each.json", db);
  if (!build_ok) {
    std::cerr
        << "Failed to build graph. Are Generate/Log modules linked properly?"
        << std::endl;
  }
  assert(build_ok);

  engine.execute_graph();

  assert(db.has("Mock.Count"));
  int final_count = db.read<int>("Mock.Count");
  std::cout << "Final count: " << final_count << "\n";
  assert(final_count == 3);  // 3 items in vector

  std::string final_concat = db.read<std::string>("Mock.Concat");
  std::cout << "Final string concat: " << final_concat << "\n";
  assert(final_concat == "Hello_World");

  std::cout << "[Test] 'for_each' loop test PASSED\n\n";
}

void test_while_expr() {
  std::cout << "[Test] Running 'while_expr' loop test..." << std::endl;

  std::string pipeline_json = R"({
    "children": [
      {
        "id": "Loop_1",
        "type": "Loop",
        "parameters": {
          "loop_mode": "while_expr",
          "expression": "$Mock.Count < 5"
        },
        "children": [
          {
            "id": "Inc_1",
            "type": "MockIncrementor",
            "parameters": {}
          }
        ]
      }
    ]
  })";

  Blackboard db;
  // Initialize condition variable
  db.write("Mock.Count", 0);

  WorkflowEngine engine;

  std::ofstream out("test_while_expr.json");
  out << pipeline_json;
  out.close();

  bool build_ok = engine.build_graph_from_json("test_while_expr.json", db);
  assert(build_ok);

  engine.execute_graph();

  assert(db.has("Mock.Count"));
  int final_count = db.read<int>("Mock.Count");
  std::cout << "Final count: " << final_count << "\n";
  assert(final_count == 5);

  std::cout << "[Test] 'while_expr' loop test PASSED\n\n";
}

void test_loop_output() {
  std::cout << "[Test] Running 'loop_output' aggregation test..." << std::endl;

  std::string pipeline_json = R"({
    "children": [
      {
        "id": "Loop_1",
        "type": "Loop",
        "parameters": {
          "loop_mode": "for_count",
          "min_iterations": 0,
          "max_iterations": 3,
          "step": 1
        },
        "dynamic_outputs": [
          {
            "name": "calculated_values",
            "source": "$Mock.Count"
          }
        ],
        "children": [
          {
            "id": "Inc_1",
            "type": "MockIncrementor",
            "parameters": {}
          }
        ]
      }
    ]
  })";

  Blackboard db;
  // Initialize mock count
  db.write("Mock.Count", 0);

  WorkflowEngine engine;

  std::ofstream out("test_loop_output.json");
  out << pipeline_json;
  out.close();

  bool build_ok = engine.build_graph_from_json("test_loop_output.json", db);
  assert(build_ok);

  engine.execute_graph();

  assert(db.has("Loop_1.calculated_values"));
  auto results = db.read<std::vector<std::any>>("Loop_1.calculated_values");
  assert(results.size() == 3);

  assert(std::any_cast<int>(results[0]) == 1);
  assert(std::any_cast<int>(results[1]) == 2);
  assert(std::any_cast<int>(results[2]) == 3);

  std::cout << "[Test] 'loop_output' aggregation test PASSED\n\n";
}

int main() {
  // Force initialization of static type converters
  TypeRegistry::get_converters();

  // Force linkage of LoopNode so REGISTER_MODULE runs
  LoopNode _force_link_loop;

  try {
    test_for_count();
    test_for_each();
    test_while_expr();
    test_loop_output();
    std::cout << "ALL LOOP TESTS PASSED SUCCESSFULLY!" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Test failed with exception: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}
