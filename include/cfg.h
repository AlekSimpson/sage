#pragma once

#include <unordered_map>
#include <limits>
#include "node_manager.h"
#include <atomic>

#define INF numeric_limits<int>::infinity()

enum InstructionType {
  REFERENCE = 0,
  INIT, 
  IF,
  WHILE,
  FOR,
  INERT,
  START_NODE
};

struct VariableLifetime {
  uint64_t id;
  string name;
  int start_line;
  int end_line;
  bool high_priority;
  int register_assignment;
  int spill_offset;
  bool spilled;

  VariableLifetime();
  VariableLifetime(uint64_t id, string name, int start, bool priority);
  VariableLifetime(string name, int start, bool priority);

  void print_self() const;
};


class IDGenerator {
    static std::atomic<uint64_t> next_id;
public:
    static uint64_t generate() {
        return next_id.fetch_add(1, std::memory_order_relaxed);
    }
};

struct CFGNode {
  uint64_t id;
  int line_number;
  NodeIndex node;
  vector<string> variable_references;
  InstructionType inst_type;

  CFGNode();
  CFGNode(NodeIndex, vector<string>, InstructionType);
  CFGNode(NodeIndex, vector<string>, InstructionType, int);
  CFGNode(uint64_t, NodeIndex, vector<string>, InstructionType);
  CFGNode(const CFGNode&);
};

// Control Flow Graph data structure
class SageControlFlow {
public:
  uint64_t cursor;
  uint64_t start_node;
  uint64_t id_last_added;
  unordered_map<uint64_t, CFGNode> nodes;
  unordered_map<uint64_t, vector<uint64_t>> connections;
  int greatest_line_number;

  SageControlFlow();

  void add_new_node_connected(CFGNode child);
  void add_connection(uint64_t parent, uint64_t child);
  void append_nested_flow(SageControlFlow* nested_cfg);
  vector<uint64_t> get_dangling_cursors();
};
