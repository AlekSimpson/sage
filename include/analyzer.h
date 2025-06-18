#pragma once

#include "cfg.h"
#include "node_manager.h"

class SageAnalyzer {
public:
  NodeManager* manager;
  int register_range_begin;
  int register_range_end;

  unordered_map<string, VariableLifetime> variable_lifetimes;

  SageAnalyzer(NodeManager* manager);
  SageAnalyzer();

  void generate_control_node(
    vector<string>*, InstructionType, NodeIndex, vector<uuid_t*>,
    SageControlFlow*
  );
  vector<string> expression_contains_references(NodeIndex);
  SageControlFlow* generate_control_flow(NodeIndex ast);
  unordered_map<string, VariableLifetime> extract_lifetimes(SageControlFlow* controlflow);
  void linear_scan_register_allocation(unordered_map<string, VariableLifetime>* intervals);

  void perform_static_analysis(NodeIndex root);
};
