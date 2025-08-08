#pragma once

#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <memory>
#include <stack>

#include "node_manager.h"
#include "sage_types.h"

struct SageSymbol {
  SageValue value;
  SageType* type;
  string identifier;
  bool is_variable;
  bool is_parameter;
  int volatile_register;

  SageSymbol();
  SageSymbol(SageValue, string);
};

typedef set<string> SageScope;
typedef bool successful;

class SageSymbolTable {
public:
  stack<SageScope> symbol_stack;
  SageScope type_scope;
  vector<SageSymbol*> symbol_table;
  map<string, uint32_t> symbol_map;
  bool current_function_has_returned;

  SageSymbolTable();
  ~SageSymbolTable();

  void push_scope();
  void pop_scope();

  bool declare_type_symbol(const string& name, SageType* type);
  bool declare_symbol(const string& name, SageValue value);
  bool declare_symbol(const string& name, SageType* valuetype);
  uint32_t declare_internal_symbol(int register_value); // sets is_variable to false

  SageType* derive_sage_type(NodeManager*, NodeIndex);
  SageType* resolve_sage_type(NodeManager*, NodeIndex);

  uint32_t lookup_id(string name); // lookup identifier
  SageSymbol* lookup(const string& name);
  SageSymbol* lookup(uint32_t id);

  void initialize();
  int size();
};


