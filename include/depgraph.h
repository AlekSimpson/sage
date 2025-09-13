#pragma once

#include <map>
#include <string>
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

struct IdentNode;

// root of this tree represents the runtime portion of the code
struct DependencyGraph {
  NodeManager* man;
  set<string>* parent_scope;
  set<string> local_scope;
  set<int> comptime_nodes;
  map<string, int> nodename_map;
  map<int, IdentNode> nodes;
  map<int, set<int>> connections;
  string scopename;
  int scope_level;
  bool errors_exist; // used to flip when errors are reported during the graph build process

  DependencyGraph(NodeManager*, string, int, set<string>*);
  DependencyGraph();
  ~DependencyGraph();

  int add_scope_node(string ident_name, dep_type type, NodeIndex ast_pos, DependencyGraph* owned_scope);
  int add_node(string ident_name, dep_type type, NodeIndex ast_pos);
  int add_param_node(string ident_name, dep_type type, NodeIndex ast_pos);
  void add_connection(const string& dependency, const string& dependent, NodeIndex ast_pos);
  vector<int> get_exec_order();
  set<int>& get_dependents(int);
  bool dependencies_are_valid();
  void load_fringe(stack<int>& fringe);
  void merge_with(DependencyGraph& related_graph); // used to merge graphs that depend on each other for identifiers
  void quicksort(vector<int>*, int, int);
  void quicksort(vector<int>*);
  int partition(vector<int>*, int, int);
};

struct IdentNode {
  string name;
  dep_type type;
  NodeIndex ast_pos = -1;
  int in_degree = 0;
  DependencyGraph* owned_scope = nullptr;
  int reference_count = 1;
  int sort_value = -1;
  int instruction_freq = 0;
  bool high_priority = false;
  NodeManager* man = nullptr;
  bool is_parameter = false;

  void merge(IdentNode& node);
  int get_sort_value();
};
