#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>

#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"

typedef llvm::Value LLVMValue

LLVMValue* SageCodeGenVisitor::build_store(LLVMValue* rhs, LLVMValue* variable_symbol) {
    builder->CreateStore(rhs, variable_symbol);

    // Load instructions
    // TODO: sage symbol table and bytecode VM memory need to be connected somehow

}
