#pragma once

#include <stdlib.h>
#include <stack>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <map>

#include "../include/parser.h"
#include "../include/node_manager.h"
#include "../include/symbols.h"

using namespace llvm;

class SageCodeGenVisitor {
private:
  std::shared_ptr<llvm::LLVMContext> llvm_context;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unique_ptr<llvm::Module> main_module;
  SageSymbolTable symbol_table;
  NodeManager* node_manager;

public:
  SageCodeGenVisitor();
  SageCodeGenVisitor(NodeManager*, std::shared_ptr<llvm::LLVMContext>);
    
  llvm::Module* get_module();
  void visitor_create_function_return(llvm::Value*);
  llvm::Value* process_expression(NodeIndex);
  void process_assignment(NodeIndex);

  llvm::Value* visit_program(NodeIndex);
  llvm::Value* visit_variable_decl(NodeIndex);
  llvm::Value* visit_function_declaration(NodeIndex);
  llvm::Value* visit_function_definition(NodeIndex);
  llvm::Value* visit_function_call(NodeIndex);
  llvm::Value* visit_codeblock(NodeIndex);
  llvm::Value* visit_unary_expr(NodeIndex);
  llvm::Value* visit_trinary_expr(NodeIndex);
  llvm::Value* visit_binary_expr(NodeIndex);
  llvm::Value* visit_expression(NodeIndex);
};

class SageCompiler {
private:
  NodeIndex ast;

public:
  NodeManager* node_manager;
  SageParser parser;
  SageCodeGenVisitor visitor;

  SageCompiler();
  SageCompiler(string mainfile, std::shared_ptr<llvm::LLVMContext> context);
  ~SageCompiler();

  llvm::Module* compile();
  llvm::Module* compile_module(NodeIndex ast);
  NodeIndex parse_codefile(string target_file);
  void optimize(llvm::Module* module, int level);
  successful generate_output(llvm::Module* module, const std::string& output_file);
  void compiler_exit(string message, int error_code);
  void compiler_exit(int error_code);
  bool check_filename_valid(string filename);
};
