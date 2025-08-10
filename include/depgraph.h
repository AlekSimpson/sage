#pragma once

#include <map>
#include <string>
#include "cfg.h"
#include "node_manager.h"
#include "arena.h"

/*
 * Dependency mapping strategy idea:
 * What references need to be established for each statement in a scope to run?
 */

typedef u64 uintptr_t;

enum dep_type {
  INIT = 0,
  REF
};

struct IdentNode {
  string name;
  dep_type type;
  NodeIndex ast_pos = -1;
  int degree = 0;

  u64 get_id();
};

// root of this tree represents the runtime portion of the code
struct DependencyGraph {
  map<string, u64> nodename_map;
  map<u64, IdentNode> nodes;
  map<u64, set<u64>> connections;
  string scopename;
  int scope_level;
  bool errors_exist; // used to flip when errors are reported during the graph build process

  DependencyGraph(string, int);
  DependencyGraph();

  u64 add_node(string ident_name, dep_type type, NodeIndex ast_pos);
  void add_connection(string dependency, string dependent);
  vector<u64> comptime_exec_order(); // linear order of leftmost leaf to the root
  set<u64>& get_dependents(u64);
  bool dependencies_are_valid();
  int get_in_degree(u64);
  void load_fringe();

  void merge_with(DependencyGraph& related_graph); // used to merge graphs that depend on each other for identifiers

};

