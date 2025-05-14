#pragma once

#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <memory>
#include <stack>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include "../include/node_manager.h"

using namespace llvm;

struct LLVMSymbol {
  llvm::Value* value;
  llvm::Type* type;
  string identifier;
};

typedef unordered_set<string> SageScope;
typedef bool successful;

LLVMSymbol create_symbol(string, llvm::Value*, llvm::Type*);

class SageSymbolTable {
private:
  stack<SageScope> symbol_stack;
  SageScope type_scope;
  map<string, LLVMSymbol*> symbol_table;
  llvm::LLVMContext* context;

public:

  bool current_function_has_returned;

  SageSymbolTable();
  ~SageSymbolTable();

  void push_scope();
  void pop_scope();
  bool declare_symbol(const string& name, LLVMSymbol value, bool is_type_symbol);
  llvm::Type* derive_sage_type(const NodeManager*, NodeIndex node);
  LLVMSymbol* lookup_symbol(const string& name);
  llvm::Type* resolve_sage_type(const NodeManager*, NodeIndex type_node);
  void initialize(llvm::Module* main_module, llvm::LLVMContext& llvm_context);

};


