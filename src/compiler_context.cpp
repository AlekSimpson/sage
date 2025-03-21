#include <stdlib.h>
#include <stack.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <memory>


CompilerContext::CompilerContext() {
    SageSymbolTable top_level_scope;
    this->scope.push(top_level_scope);

    current_function_has_returned = false;
}

void CompilerContext::push_scope() {
    // do this so that the nested scope always can see the scopes above it
    SageSymbolTable scope_copy = scope.top();
    scope.push(scope_copy);
}

void CompilerContext::pop_scope() {
    scope.pop();
}

bool CompilerContext::declare_symbol(const std::string& name, llvm::Value* value) {
    auto current_scope = scope.top();
    
    auto search = current_scope.find(name);
    if (search == current_scope.end()) {
        // symbol already exists
        return false;
    }

    current_scope[name] = value;
    return true;
}

llvm::Value* CompilerContext::lookup_symbol(const std::string& name) {
    auto current_scope = scope.top();

    auto search = current_scope.find(name);
    if (search == current_scope.end()) {
        return nullptr;
    }

    return search;
}
