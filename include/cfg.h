#pragma once

#include <uuid/uuid.h>
#include <limits>
#include "node_manager.h"

#define INF numeric_limits<int>::infinity();

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
  string name;
  int start_line;
  int end_line;
  int register_assignment;
  int spill_offset;
  bool high_priority;
  bool spilled;

  VariableLifetime(string, int, bool);
};

struct CFGNode {
  uuid_t id;
  int line_number;
  NodeIndex node;
  vector<string> variable_references;
  InstructionType inst_type;

  CFGNode(NodeIndex, vector<string>, InstructionType);
  CFGNode(NodeIndex, vector<string>, InstructionType, int);
  CFGNode(uuid_t, NodeIndex, vector<string>, InstructionType);
  CFGNode(CFGNode);
};

// Control Flow Graph data structure
class SageControlFlow {
public:
  uuid_t cursor;
  uuid_t start_node;
  uuid_t id_last_added;
  unordered_map<uuid_t, CFGNode> nodes;
  unordered_map<uuit_t, vector<uuid_t>> connections;
  int greatest_line_number;

  SageControlFlow();

  void add_connection(uuid_t parent, CFGNode child);
  void add_connection(uuid_t parent, uuid_t child);
  void append_nested_flow(SageControlFlow* nested_cfg);
  vector<uuid_t> get_dangling_cursors();
};
