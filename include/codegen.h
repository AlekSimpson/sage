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

struct VisitorValue {
  SageValue sage_ir;
  llvm::Value* llvm_ir;

  VisitorValue(SageValue, llvm::Value*);
  VisitorValue();
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
  VisitorValue process_expression(NodeIndex);

  VisitorValue build_store(VisitorValue rhs, LLVMSymbol* variable_symbol);

  VisitorValue visit_program(NodeIndex);
  VisitorValue visit_variable_decl(NodeIndex);
  VisitorValue visit_function_declaration(NodeIndex);
  VisitorValue visit_function_definition(NodeIndex);
  VisitorValue visit_function_call(NodeIndex);
  VisitorValue visit_codeblock(NodeIndex);
  VisitorValue visit_unary_expr(NodeIndex);
  VisitorValue visit_trinary_expr(NodeIndex);
  VisitorValue visit_binary_expr(NodeIndex);
  VisitorValue visit_expression(NodeIndex);
  VisitorValue visit_variable_assignment(NodeIndex);
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
