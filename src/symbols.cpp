#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <memory>
#include <stack>
#include <uuid/uuid.h>
#include <format>
#include <algorithm>

#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"
#include "../include/sage_types.h"

SageSymbol::SageSymbol() {}

SageSymbol::SageSymbol(SageValue rawvalue, string _identifier) 
    : value(rawvalue), identifier(_identifier) {}

SageSymbolTable::SageSymbolTable() {
    symbol_stack = stack<SageScope>();
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

    auto chartype = TypeRegistery::get_builtin_type(CHAR);
    declare_type_symbol("bool", TypeRegistery::get_builtin_type(BOOL));
    declare_type_symbol("char",  chartype);
    declare_type_symbol("int", TypeRegistery::get_builtin_type(I64));
    declare_type_symbol("i8", TypeRegistery::get_builtin_type(I8));
    declare_type_symbol("i32", TypeRegistery::get_builtin_type(I32));
    declare_type_symbol("i64", TypeRegistery::get_builtin_type(I64));
    declare_type_symbol("float", TypeRegistery::get_builtin_type(F64));
    declare_type_symbol("f32", TypeRegistery::get_builtin_type(F32));
    declare_type_symbol("f64", TypeRegistery::get_builtin_type(F64));
    declare_type_symbol("void", TypeRegistery::get_builtin_type(VOID));

    // TODO FIX: this interpretation of pointer syntax does not account for the fact that there could be double or triple pointers
    declare_type_symbol("char*", TypeRegistery::get_pointer_type(chartype));

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

successful SageSymbolTable::declare_symbol(const string& name, SageType* valuetype) {
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

successful SageSymbolTable::declare_type_symbol(const string& name, SageType* type) {
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

uint32_t SageSymbolTable::declare_internal_symbol(int register_value) {
    SageSymbol* new_symbol = new SageSymbol;
    new_symbol->is_variable = false;
    new_symbol->volatile_register = register_value;
    new_symbol->identifier = "temp_" + to_string(symbol_table.size());
    
    symbol_table.push_back(new_symbol);
    uint32_t symbol_id = static_cast<uint32_t>(symbol_table.size() - 1);
    symbol_map[new_symbol->identifier] = symbol_id;
    
    return symbol_id;
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
        if (previous_scope.find(symbol) != previous_scope.end()) {
            delete symbol_table[symbol_map[symbol]];
            symbol_map.erase(symbol);
        }
    }
}

uint32_t SageSymbolTable::lookup_id(string name) {
    auto current_scope = symbol_stack.top();

    auto search = current_scope.find(name);
    if (search != current_scope.end()) {
        return 0;
    }

    return symbol_map[name];
}

SageSymbol* SageSymbolTable::lookup(const string& name) {
    auto current_scope = symbol_stack.top();

    auto search = current_scope.find(name);
    if (search != current_scope.end()) {
        return nullptr;
    }

    return symbol_table[symbol_map[name]];
}

SageSymbol* SageSymbolTable::lookup(uint32_t id) {
    if (id < 0 || id >= symbol_table.size()) {
        return nullptr;
    }

    return symbol_table[id];
}

SageType* SageSymbolTable::derive_sage_type(NodeManager* manager, NodeIndex typenode) {
    switch (manager->get_nodetype(typenode)) {
        case PN_NUMBER:
            return TypeRegistery::get_builtin_type(I64);
        case PN_FLOAT:
            return TypeRegistery::get_builtin_type(F64);
        case PN_STRING:
            return TypeRegistery::get_pointer_type(TypeRegistery::get_builtin_type(CHAR));
        case PN_VAR_REF:
            // TODO
            break;
        default:
            break;
    }
    return TypeRegistery::get_builtin_type(VOID);
}

SageType* SageSymbolTable::resolve_sage_type(NodeManager* manager, NodeIndex typenode) {
    auto current_scope = symbol_stack.top();
    string type_name = manager->get_lexeme(typenode);
    auto search_name = type_scope.find(type_name);
    if (search_name == type_scope.end()) {
        return TypeRegistery::get_builtin_type(VOID);
    }

    auto it = symbol_map.find(type_name);
    if (it == symbol_map.end()) {
        return TypeRegistery::get_builtin_type(VOID);
    }

    return symbol_table[symbol_map[type_name]]->type;
}

int SageSymbolTable::size() {
    return symbol_table.size();
}
