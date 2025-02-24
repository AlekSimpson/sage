#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <memory>

#include "../include/codegen.h"

using namespace llvm;

SageCodeGenVisitor::SageCodeGenVisitor() {
  context = std::make_unique<LLVMContext>();
  builder = std::unique_ptr<llvm::IRBuilder<>>(new llvm::IRBuilder<>(*context));
  module = std::make_unique<llvm::Module>("Module", *context);
}

void SageCodeGenVisitor::visit(AbstractParseNode &node) {
  printf("abstract parse node called\n");
}

void SageCodeGenVisitor::visit(BinaryParseNode &node) {
  printf("binary parse node called\n");
}

void SageCodeGenVisitor::visit(TrinaryParseNode &node) {
  printf("trinary parse node called\n");
}

void SageCodeGenVisitor::visit(UnaryParseNode &node) {
  printf("unary parse node called\n");
}

void SageCodeGenVisitor::visit(BlockParseNode &node) {
  printf("block parse node called\n");
}
