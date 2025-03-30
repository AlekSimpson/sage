#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <memory>
#include <stack>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

#include "symbols.h"
#include "../include/parse_node.h"

using namespace llvm;

LLVMSymbol create_symbol(string identifier, llvm::Value* value, llvm::Type* type) {
    return {value, type, identifier};
}

SageSymbolTable::SageSymbolTable(llvm::LLVMContext& llvm_context) {
    symbol_stack = stack<SageScope>();
    symbol_table = map<string, LLVMSymbol*>();
    current_function_has_returned = false;
    context = llvm_context;

    // initialize root stack with sage built in datatypes and methods
    SageScope root_scope = {"bool", "char", "int", "i8", "i32", "i64", "float", "f32", "f64", "void"};
    symbol_stack.push(root_scope);
    type_scope = root_scope;
    
    declare_symbol("bool", create_symbol("bool", nullptr, llvm::Type::getInt1Ty(*context)));
    declare_symbol("success_t", create_symbol("success_t", nullptr, llvm::Type::getInt1Ty(*context)));
    declare_symbol("char", create_symbol("char", nullptr, llvm::Type::getInt8Ty(*context)));
    declare_symbol("int", create_symbol("int", nullptr, llvm::Type::getInt64Ty(*context)));
    declare_symbol("i8", create_symbol("i8", nullptr, llvm::Type::getInt8Ty(*context)));
    declare_symbol("i32", create_symbol("i32", nullptr, llvm::Type::getInt32Ty(*context)));
    declare_symbol("i64", create_symbol("i64", nullptr, llvm::Type::getInt64Ty(*context)));
    declare_symbol("float", create_symbol("float", nullptr, llvm::Type::getFloatTy(*context)));
    declare_symbol("f32", create_symbol("f32", nullptr, llvm::Type::getFloatTy(*context)));
    declare_symbol("f64", create_symbol("f64", nullptr, llvm::Type::getDoubleTy(*context)));
    declare_symbol("void", create_symbol("void", nullptr, llvm::Type::getVoidTy(*context)));
}

SageSymbolTable::~SageSymbolTable() {
    LLVMSymbol* temp;
    for (const auto& pair : symbol_table) {
        temp = pair.second; // second position holds the LLVMSymbol pointers
        delete temp;
    }
}

successful SageSymbolTable::declare_symbol(const string& name, LLVMSymbol value) {
    auto current_scope = symbol_stack.top();

    auto search = current_scope.find(name);
    if (search != current_scope.end()) {
        return false;
    }


    current_scope.insert(name);
    LLVMSymbol* new_symbol = new LLVMSymbol;
    new_symbol->value = value.value;
    new_symbol->type = value.type;
    new_symbol->identifier = value.identifier;

    symbol_table[name] = new_symbol;
    return true;
}

void SageSymbolTable::push_scope() {
    auto topscope = symbol_stack.top();
    symbol_stack.push(scope_copy);
}

void SageSymbolTable::pop_scope() {
    symbol_stack.pop();
}

LLVMSymbol* lookup_symbol(const string& name) {
    auto current_scope = symbol_stack.top();

    auto search = current_scope.find(name);
    if (search != current_scope.end()) {
        return nullptr;
    }

    return symbol_table[name];
}

llvm::Type* resolve_sage_type(UnaryParseNode* type_node) {
    string typename = type_node->get_token().lexeme;
    auto search_name = type_scope.find(typename);
    if (search_name == type_scope.end()) {
        return nullptr;
    }

    return symbol_table[typename]->type;
}



