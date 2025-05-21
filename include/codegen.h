#pragma once

#include <stdlib.h>
#include <stack>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <map>

#include "../include/parser.h"
#include "../include/interpreter.h"
#include "../include/node_manager.h"
#include "../include/symbols.h"

using namespace llvm;

struct SageIR {
  instruction sage_ir;
  llvm::Value* llvm_ir;
};

class SageCodeGenVisitor {
private:
  std::shared_ptr<llvm::LLVMContext> llvm_context;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unique_ptr<llvm::Module> main_module;
  SageSymbolTable symbol_table;
  NodeManager* node_manager;
  vector<instruction> sage_bytecode; // builder functions write to this
  SageCompiler* compiler;

public:
  SageCodeGenVisitor();
  SageCodeGenVisitor(SageComiler*, NodeManager*, std::shared_ptr<llvm::LLVMContext>);
    
  SageInterpreter* interpreter(); // NOTE: might not need?
  llvm::Module* get_module();
  void visitor_create_function_return(SageIR);
  SageIR process_expression(NodeIndex);

  SageIR build_store(SageIR rhs, SageIR variable_symbol);

  SageIR visit_program(NodeIndex);
  SageIR visit_variable_decl(NodeIndex);
  SageIR visit_function_declaration(NodeIndex);
  SageIR visit_function_definition(NodeIndex);
  SageIR visit_function_call(NodeIndex);
  SageIR visit_codeblock(NodeIndex);
  SageIR visit_unary_expr(NodeIndex);
  SageIR visit_trinary_expr(NodeIndex);
  SageIR visit_binary_expr(NodeIndex);
  SageIR visit_expression(NodeIndex);
  SageIR visit_variable_assignment(NodeIndex);
};

class SageCompiler {
private:
  NodeIndex ast;

public:
  NodeManager* node_manager;
  SageParser parser;
  SageCodeGenVisitor visitor;
  SageInterpreter* interpreter;

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
