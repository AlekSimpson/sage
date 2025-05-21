#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>

#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"

VisitorValue::VisitorValue(SageValue sage_value, llvm::Value* ll_value) 
    : sage_ir(sage_value), llvm_ir(ll_value) {}

SageInterpreter* SageCodeGenVisitor::interpreter() {
    return compiler->interpreter;
}

VisitorValue SageCodeGenVisitor::build_store(VisitorValue rhs, LLVMSymbol* variable_symbol) {
    llvm::Value* llvm_value = builder->CreateStore(rhs.llvm_ir, variable_symbol->value);

    int vm_pointer = 0;
    if (variable_symbol->vm_stack_pointer != NULL_INDEX) {
        vm_pointer = variable_symbol->vm_stack_pointer;
    }else if (variable_symbol->vm_heap_pointer != NULL_INDEX) {
        vm_pointer = variable_symbol->vm_heap_pointer;
    }else {
        // something went wrong
        printf("builders.cpp:build_store: something went wrong, couldn't find vm memory.\n");
    }

    int store_value = rhs.sage_ir.get_value();

    // NOTE: SAGE LOAD IR:
    // STORE vm_pointer, store_value
    sage_bytecode.push_back(double_op(OP_STORE, vm_pointer, store_value));

    return VisitorValue(llvm_value, SageIntValue(vm_pointer, 64));
}






