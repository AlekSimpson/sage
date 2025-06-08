#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <memory>
#include <stack>
#include <uuid/uuid.h>
#include <format>

#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"
#include "../include/sage_types.h"

using namespace llvm;

SageSymbol::SageSymbol(SageValue rawvalue, string identifier) {
    value = rawvalue;

    identifier = identifier;
}

SageSymbolTable::SageSymbolTable() {
    symbol_stack = stack<SageScope>();
    // symbol_table = map<string, SageSymbol*>();
    current_function_has_returned = false;
}

SageSymbolTable::~SageSymbolTable() {
    for (auto symbol : symbol_table) {
        if (symbol != nullptr) {
            delete symbol;
        }
    }
}

void SageSymbolTable::initialize() {
    // initialize root stack with sage built in datatypes and methods
    symbol_stack.push({});

    // we'll rely on cpp printf for now but we can recreate print in bytecode i think
    declare_symbol("printf", function_type, llvmvalue);

    declare_type_symbol("bool", SageBuiltinType(BOOL));
    declare_type_symbol("char",  SageBuiltinType(CHAR));
    declare_type_symbol("int", SageBuiltinType(I64));
    declare_type_symbol("i8", SageBuiltinType(I8));
    declare_type_symbol("i32", SageBuiltinType(I32));
    declare_type_symbol("i64", SageBuiltinType(I64));
    declare_type_symbol("float", SageBuiltinType(F64));
    declare_type_symbol("f32", SageBuiltinType(F32));
    declare_type_symbol("f64", SageBuiltinType(F64));
    declare_type_symbol("void", SageBuiltinType(VOID));

    // TODO FIX: this interpretation of pointer syntax does not account for the fact that there could be double or triple pointers
    declare_type_symbol("char*", SagePointerType(SageBuiltinType(CHAR)));

    SageScope root_scope = {
        "bool", "char", "int", "i8", 
        "i32", "i64", "float", "f32", "f64", "void",
        "char*"
    };
    type_scope = root_scope;
}

successful SageSymbolTable::declare_symbol(const string& name, SageValue value) {
    auto current_scope = symbol_stack.top();

    auto search = current_scope.find(name);
    if (search != current_scope.end()) {
        return false;
    }

    current_scope.insert(name);
    SageSymbol* new_symbol = new SageSymbol;
    new_symbol->value = value;
    new_symbol->identifier = name;

    // symbol_table[name] = new_symbol;
    symbol_table.push_back(new_symbol);
    symbol_map[name] = static_cast<uint32_t>(symbol_table.size() - 1);

    return true;
}

successful SageSymbolTable::declare_symbol(uuid_t id, SageValue value) {
    string name = std::format("{}", id);

    return declare_symbol(name, value);
}

successful SageSymbolTable::declare_symbol(const string& name, SageType valuetype) {
    auto current_scope = symbol_stack.top();

    auto search = current_scope.find(name);
    if (search != current_scope.end()) {
        return false;
    }

    current_scope.insert(name);
    SageSymbol* new_symbol = new SageSymbol;
    new_symbol->type = valuetype;
    new_symbol->identifier = name;

    symbol_table.push_back(new_symbol);
    symbol_map[name] = symbol_table.size() - 1;

    return true;
}

successful SageSymbolTable::declare_type_symbol(const string& name, SageType type) {
    auto current_scope = symbol_stack.top();

    auto search = current_scope.find(name);
    if (search != current_scope.end()) {
        return false;
    }

    type_scope.insert(name);

    current_scope.insert(name);
    SageSymbol* new_symbol = new SageSymbol;
    new_symbol->type = type;
    new_symbol->identifier = name;

    symbol_table.push_back(new_symbol);
    symbol_map[name] = static_cast<uint32_t>(symbol_table.size() - 1);
    
    return true;
}

void SageSymbolTable::push_scope() {
    auto topscope = symbol_stack.top();
    symbol_stack.push(topscope);
}

void SageSymbolTable::pop_scope() {
    auto scope = symbol_stack.top();
    symbol_stack.pop();

    auto previous_scope = symbol_stack.top();

    // any symbols declared in the scope to pop should be removed from the symbol table
    for (string symbol: scope) {
        if (!previous_scope.contains(symbol)) {
            delete symbol_table[symbol_map[symbol]];
            symbol_map.erase(symbol);
        }
    }
}

SageSymbol* SageSymbolTable::lookup_symbol(const string& name) {
    auto current_scope = symbol_stack.top();

    auto search = current_scope.find(name);
    if (search != current_scope.end()) {
        return nullptr;
    }

    return symbol_table[symbol_map[name]];
}

SageSymbol* SageSymbolTable::lookup_symbol(uuid_t id) {
    string name = std::format("{}", id);
    return lookup_symbol(name);
}

SageType SageSymbolTable::derive_sage_type(NodeManager* manager, NodeIndex typenode) {
    switch (manager->get_nodetype(typenode)) {
        case PN_NUMBER:
            return SageBuiltinType(I64);
        case PN_FLOAT:
            return SageBuiltinType(F64);
        case PN_STRING:
            return SagePointerType(SageBuiltinType(CHAR));
        case PN_VAR_REF:
            // TODO
            break;
        default:
            break;
    }
    return SageBuiltinType(VOID);
}

SageType SageSymbolTable::resolve_sage_type(NodeManager* manager, NodeIndex typenode) {
    auto current_scope = symbol_stack.top();
    string type_name = manager->get_lexeme(typenode);
    auto search_name = type_scope.find(type_name);
    if (search_name == type_scope.end()) {
        return SageBuiltinType(VOID);
    }

    auto it = symbol_table.find(type_name);
    if (it == symbol_table.end() || it->second == nullptr) {
        return SageBuiltinType(VOID);
    }

    return symbol_table[symbol_map[type_name]]->type;
}

