#pragma once

#include "cfg.h"
#include "node_manager.h"
#include "depgraph.h"

class SageAnalyzer {
public:
  NodeManager* manager;
  int register_range_begin;
  int register_range_end;

  unordered_map<string, VariableLifetime> variable_lifetimes;

  SageAnalyzer(NodeManager* manager);
  SageAnalyzer();

  vector<string> expression_contains_references(NodeIndex);
  SageControlFlow* generate_control_flow(NodeIndex ast);
  map<string, VariableLifetime> extract_lifetimes(SageControlFlow* controlflow);
  void linear_scan_register_allocation(int register_count);

  void perform_static_analysis(NodeIndex root);

  map<string, DependencyGraph*> detect_identifier_dependencies(NodeIndex);
};
