#pragma once

#include <string>
#include <map>
#include <vector>
#include <stack>
#include <set>

#include "node_manager.h"
#include "sage_types.h"

struct SageSymbol {
  SageValue value;
  SageType* type;
  string identifier;
  bool is_variable;
  bool is_parameter;
  int assigned_register;
  bool spilled;
  int spill_offset;

  SageSymbol();
  SageSymbol(SageValue, string);
};

struct function_visit {
  string function_name = "";
  int return_statement_count = 0;

  function_visit(string name) : function_name(name) {};
  bool has_returned() const { return return_statement_count > 0; }
};

class SageSymbolTable {
public:
  set<string> types;
  vector<SageSymbol*> symbol_table;
  map<string, uint32_t> symbol_map;
  stack<function_visit> function_visitor_state;
  //bool current_function_has_returned;

  SageSymbolTable();
  ~SageSymbolTable();

  void declare_type_symbol(const string& name, SageType* type);
  void declare_symbol(const string& name, SageValue value);
  void declare_symbol(const string& name, SageType* valuetype);
  void declare_symbol(const string& name, int register_alloc);
  void declare_parameter_symbol(const string& name, int register_alloc);
  uint32_t declare_internal_symbol(int register_value); // sets is_variable to false
  uint32_t declare_internal_symbol(const string& name, SageValue value);

  SageType* derive_sage_type(NodeManager*, NodeIndex);
  SageType* resolve_sage_type(NodeManager*, NodeIndex);

  uint32_t lookup_id(string name); // lookup identifier
  SageSymbol* lookup(const string& name);
  SageSymbol* lookup(uint32_t id);

  void initialize();
  int size();
};


