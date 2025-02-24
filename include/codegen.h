#pragma once

#include "../include/parse_node.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace llvm;

class SageCodeGenVisitor {
private:
  std::unique_ptr<LLVMContext> context;
  std::unique_ptr<IRBuilder<>> builder;
  std::unique_ptr<Module> module;

public:
  SageCodeGenVisitor();
    
  void visit(BinaryParseNode& node);
  void visit(TrinaryParseNode& node);
  void visit(UnaryParseNode& node);
  void visit(BlockParseNode& node);
  void visit(AbstractParseNode& node);
};
