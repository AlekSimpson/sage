#pragma once

#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <memory>
#include <uuid/uuid.h>
#include <stack>
#include "node_manager.h"
#include "codegen.h"
#include "sage_types.h"

using namespace llvm;

struct SageSymbol {
  SageValue value;
  SageType type;
  string identifier;
  bool is_variable;
  int volatile_register;

  SageSymbol(SageValue, string);
};

typedef unordered_set<string> SageScope;
typedef bool successful;

class SageSymbolTable {
public:
  stack<SageScope> symbol_stack;
  SageScope type_scope;
  vector<SageSymbol*> symbol_table;
  map<string, uint32_t> symbol_map;
  // map<string, SageSymbol*> symbol_table;
  bool current_function_has_returned;

  SageSymbolTable();
  ~SageSymbolTable();

  void push_scope();
  void pop_scope();

  bool declare_type_symbol(const string& name, SageType type);
  bool declare_symbol(const string& name, SageValue value);
  bool declare_symbol(uuid_t id, SageValue value);
  bool declare_symbol(const string& name, SageType valuetype);
  bool declare_internal_symbol(uuid_t id, SageValue value); // sets is_variable to false

  SageType derive_sage_type(NodeManager*, NodeIndex);
  SageType resolve_sage_type(NodeManager*, NodeIndex);

  SageSymbol* lookup_symbol(const string& name);
  SageSymbol* lookup_symbol(uuid_t id);

  void initialize();
};


