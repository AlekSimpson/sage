#pragma once

#include "../include/parse_node.h"

#include <stdlib.h>
#include <stack.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <memory>

using namespace llvm;
typedef SageSymbolTable std::map<std::string, llvm::Value*>

class SageCodeGenVisitor {
private:
  std::unique_ptr<llvm::LLVMContext> llvm_context;
  SageSymbolTable global_table;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unique_ptr<llvm::Module> main_module;
  CompilerContext symbol_table;

public:
  SageCodeGenVisitor();
    
  void visitor_create_function_return();
  llvm::Value* visit_binary_expr(BinaryParseNode* node);
  llvm::Value* visit_function_definition(TrinaryParseNode* node);
  // Other visit methods for AST nodes
};

class CompilerContext {
private:
  std::stack<SageSymbolTable> scope;

  // this exists to tell the compiler wether the function that we are compiling has generated the return statement or not yet... this is useful so that we can derive whether or not we should automatically add a "return void" statement.
  bool current_function_has_returned;

public:
  CompilerContext();

  void push_scope(); // pushes a copy of the top element onto the stack
  void pop_scope();
  bool declare_symbol(const std::string& name, llvm::Value* value);
  llvm::Value* lookup_symbol(const std::string& name);
};

class SageCompiler {
private:
  std::unique_ptr<AST> ast;
  std::unique_ptr<CompilerContext> compiler_context;
    
public:
  SageCompiler(std::unique_ptr<AST> ast);
    
  std::unique_ptr<llvm::Module> compile() {
    SageCodeGenVisitor visitor;
    ast->accept(&visitor);
    return visitor.getModule();
  }
    
  void optimize(llvm::Module* module, int optLevel);
  // {
  //   // Set up optimization passes
  //   llvm::legacy::PassManager PM;
  //   if (optLevel > 0) {
  //     // TODO Add optimization passes based on opt level
  //   }
  //   PM.run(*module);
  // }
    
  void generate_output(llvm::Module* module, const std::string& output_file);
};
