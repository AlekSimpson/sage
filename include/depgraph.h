#pragma once


// root of this tree represents the runtime portion of the code
struct DependencyTree {
  int tree_size;
  int* procedure_tag;
  
  void add_leaf(int parent, int proc, );
  vector<int> comptime_exec_order(); // linear order of leftmost leaf to the root

};
