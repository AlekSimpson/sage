#pragma once

#include <stdlib.h>
#include <stack>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <map>

#include "../include/symbols.h"
#include "../include/parse_node.h"

using namespace llvm;

class SageCodeGenVisitor {
private:
  std::shared_ptr<llvm::LLVMContext> llvm_context;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unique_ptr<llvm::Module> main_module;
  SageSymbolTable symbol_table;

public:
  SageCodeGenVisitor(std::shared_ptr<llvm::LLVMContext>);
    
  llvm::Module* get_module();
  void visitor_create_function_return(llvm::Value*);
  llvm::Value* process_expression(BinaryParseNode*);
  void process_assignment(BinaryParseNode*);

  llvm::Value* visit_program(BlockParseNode*);
  llvm::Value* visit_variable_decl(AbstractParseNode*);
  llvm::Value* visit_function_declaration(BinaryParseNode*);
  llvm::Value* visit_function_definition(TrinaryParseNode*);
  llvm::Value* visit_codeblock(BlockParseNode*);
  llvm::Value* visit_unary_expr(UnaryParseNode*);
  llvm::Value* visit_trinary_expr(TrinaryParseNode*);
  llvm::Value* visit_binary_expr(BinaryParseNode*);
  llvm::Value* visit_expression(AbstractParseNode*);
};

typedef AbstractParseNode SageAST;

class SageCompiler {
private:
  std::unique_ptr<SageAST> ast;

public:
  SageCompiler(std::unique_ptr<SageAST> ast);

  llvm::Module* compile(); //{
    // SageCodeGenVisitor visitor = SageCodeGenVisitor();
    // ast->accept(&visitor);
    // return visitor.get_module();
  // }
    
  void optimize(llvm::Module* module, int optLevel);
  void generate_output(llvm::Module* module, const std::string& output_file);
};
