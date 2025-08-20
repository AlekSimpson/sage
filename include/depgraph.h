#pragma once

#include <map>
#include <string>
#include "cfg.h"
#include "node_manager.h"
#include <set>
#include <stack>

/*
 * Dependency mapping strategy idea:
 * What references need to be established for each statement in a scope to run?
 */

typedef uintptr_t u64;

enum dep_type {
  INI = 0,
  REF
};

struct IdentNode {
  string name;
  dep_type type;
  NodeIndex ast_pos = -1;
  int degree = 0;
  DependencyGraph* owned_scope = nullptr;
  int reference_count = 1;
  int sort_value = -1;
  int instruction_freq = 0;

  u64 get_id();
  void merge(IdentNode& node);
  int get_sort_value();
};

// root of this tree represents the runtime portion of the code
struct DependencyGraph {
  set<string>* parent_scope;
  set<string> local_scope;
  set<u64> comptime_nodes;
  map<string, u64> nodename_map;
  map<u64, IdentNode> nodes;
  map<u64, set<u64>> connections;
  string scopename;
  int scope_level;
  bool errors_exist; // used to flip when errors are reported during the graph build process

  DependencyGraph(string, int, set<string>*);
  DependencyGraph();
  ~DependencyGraph();

  u64 add_scope_node(string ident_name, dep_type type, NodeIndex ast_pos, DependencyGraph* owned_scope);
  u64 add_node(string ident_name, dep_type type, NodeIndex ast_pos);
  void add_connection(const string& dependency, const string& dependent);
  vector<u64> get_exec_order();
  vector<DependencyGraph*> get_comptime_exec_order();
  set<u64>& get_dependents(u64);
  bool dependencies_are_valid();
  int get_in_degree(u64);
  void load_fringe(stack<u64>& fringe);

  void merge_with(DependencyGraph& related_graph); // used to merge graphs that depend on each other for identifiers

};

