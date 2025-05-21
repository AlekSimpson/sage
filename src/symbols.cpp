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
#include "../include/node_manager.h"
#include "../include/codegen.h"

using namespace llvm;

LLVMSymbol create_symbol(string identifier, VisitorValue value, llvm::Type* type) {
    return LLVMSymbol{value, type, NULL_INDEX, NULL_INDEX, identifier};
}

SageSymbolTable::SageSymbolTable() {
    symbol_stack = stack<SageScope>();
    symbol_table = map<string, LLVMSymbol*>();
    current_function_has_returned = false;
}

SageSymbolTable::~SageSymbolTable() {
    LLVMSymbol* temp;
    for (const auto& pair : symbol_table) {
        temp = pair.second; // second position holds the LLVMSymbol pointers
        if (temp != nullptr) {
            delete temp;
        }
    }
}

void SageSymbolTable::initialize(llvm::Module* main_module, llvm::LLVMContext& llvm_context) {
    context = &llvm_context;

    // initialize root stack with sage built in datatypes and methods
    symbol_stack.push({});
 
    // add printf by default
    vector<llvm::Type*> param_types;
    param_types.push_back(llvm::Type::getInt8PtrTy(*context));
    llvm::FunctionType* function_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), 
        param_types, 
        true
    );
    llvm::Value* llvmvalue = llvm::Function::Create(
        function_type,
        llvm::Function::ExternalLinkage,
        "printf",
        main_module
    );
    declare_symbol("printf", create_symbol("printf", llvmvalue, function_type), false);

    declare_symbol("bool", create_symbol("bool", nullptr, llvm::Type::getInt1Ty(*context)), true);
    declare_symbol("success_t", create_symbol("success_t", nullptr, llvm::Type::getInt1Ty(*context)), true);
    declare_symbol("char", create_symbol("char", nullptr, llvm::Type::getInt8Ty(*context)), true);
    declare_symbol("int", create_symbol("int", nullptr, llvm::Type::getInt64Ty(*context)), true);
    declare_symbol("i8", create_symbol("i8", nullptr, llvm::Type::getInt8Ty(*context)), true);
    declare_symbol("i32", create_symbol("i32", nullptr, llvm::Type::getInt32Ty(*context)), true);
    declare_symbol("i64", create_symbol("i64", nullptr, llvm::Type::getInt64Ty(*context)), true);
    declare_symbol("float", create_symbol("float", nullptr, llvm::Type::getFloatTy(*context)), true);
    declare_symbol("f32", create_symbol("f32", nullptr, llvm::Type::getFloatTy(*context)), true);
    declare_symbol("f64", create_symbol("f64", nullptr, llvm::Type::getDoubleTy(*context)), true);
    declare_symbol("void", create_symbol("void", nullptr, llvm::Type::getVoidTy(*context)), true);

    // FIX: this interpretation of pointer syntax does not account for the fact that there could be double or triple pointers
    declare_symbol("char*", create_symbol("char*", nullptr, llvm::Type::getInt8PtrTy(*context)), true);

    SageScope root_scope = {
        "bool", "success_t", "char", "int", "i8", 
        "i32", "i64", "float", "f32", "f64", "void",
        "char*"
    };
    type_scope = root_scope;
}

successful SageSymbolTable::declare_symbol(const string& name, LLVMSymbol value, bool is_type_symbol) {
    auto current_scope = symbol_stack.top();

    auto search = current_scope.find(name);
    if (search != current_scope.end()) {
        return false;
    }

    if (is_type_symbol) {
        type_scope.insert(name);
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
    symbol_stack.push(topscope);
}

void SageSymbolTable::pop_scope() {
    symbol_stack.pop();
}

LLVMSymbol* SageSymbolTable::lookup_symbol(const string& name) {
    auto current_scope = symbol_stack.top();

    auto search = current_scope.find(name);
    if (search != current_scope.end()) {
        return nullptr;
    }

    return symbol_table[name];
}

llvm::Type* SageSymbolTable::derive_sage_type(NodeManager* manager, NodeIndex node) {
    switch (manager->get_nodetype(node)) {
        case PN_NUMBER:
            return llvm::Type::getInt64Ty(*context);
        case PN_FLOAT:
            return llvm::Type::getFloatTy(*context);
        case PN_STRING:
            return llvm::Type::getInt8Ty(*context);
        case PN_VAR_REF:
            // TODO
            break;
        default:
            return nullptr;
    }
    return nullptr;
}

llvm::Type* SageSymbolTable::resolve_sage_type(NodeManager* manager, NodeIndex type_node) {
    auto current_scope = symbol_stack.top();
    string type_name = manager->get_lexeme(type_node);
    auto search_name = type_scope.find(type_name);
    if (search_name == type_scope.end()) {
        return nullptr;
    }

    auto it = symbol_table.find(type_name);
    if (it == symbol_table.end() || it->second == nullptr) {
        return nullptr;
    }

    return symbol_table[type_name]->type;
}



