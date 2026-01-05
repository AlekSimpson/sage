#include <string>
#include <set>
#include <stack>

#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/sage_types.h"

symbol_entry::symbol_entry() :
    value(SageValue()),
    type(nullptr),
    identifier(""),
    is_variable(false),
    is_parameter(false),
    spilled(false),
    in_constant_pool(false),
    assigned_register(-1),
    spill_offset(0),
    constant_pool_index(-1),
    scope_id(-1),
    symbol_id(-1) {}

symbol_entry::symbol_entry(SageValue sval, string ident) :
    value(sval),
    type(sval.valuetype),
    identifier(ident),
    is_variable(false),
    is_parameter(false),
    spilled(false),
    in_constant_pool(false),
    assigned_register(-1),
    spill_offset(0),
    constant_pool_index(-1),
    scope_id(-1),
    symbol_id(-1) {}

SageSymbolTable::SageSymbolTable() {
    this->function_visitor_state = stack<function_visit>();
    this->size = 0;
    this->capacity = 0;
    this->scope_manager = nullptr;
}

SageSymbolTable::SageSymbolTable(ScopeManager* scopeman, int initial_size) {
    this->function_visitor_state = stack<function_visit>();
    this->size = initial_size;
    this->capacity = 0;
    this->scope_manager = scopeman;
    
    // Reserve space in the vector
    entries.reserve(initial_size);
}

SageSymbolTable::~SageSymbolTable() {}

int SageSymbolTable::declare_symbol(const string& name, SageValue value) {
    int current_scope = scope_manager ? scope_manager->get_current_scope() : 0;
    
    symbol_entry entry;
    entry.value = value;
    entry.type = value.valuetype;
    entry.identifier = name;
    entry.is_variable = true;
    entry.is_parameter = false;
    entry.scope_id = current_scope;
    entry.symbol_id = capacity;
    
    entries.push_back(entry);
    
    // Register in scope-symbol map for fast lookup
    scope_symbol_map[{current_scope, name}] = capacity;
    
    // Register symbol in scope manager
    if (scope_manager) {
        scope_manager->register_symbol_in_current_scope(capacity);
    }
    
    capacity++;
    return capacity - 1;  // Return the index of the newly added entry
}

int SageSymbolTable::declare_symbol(const string& name, SageType* valuetype) {
    int current_scope = scope_manager ? scope_manager->get_current_scope() : 0;
    
    symbol_entry entry;
    entry.type = valuetype;
    entry.identifier = name;
    entry.is_variable = true;
    entry.is_parameter = false;
    entry.scope_id = current_scope;
    entry.symbol_id = capacity;
    
    entries.push_back(entry);
    
    // Register in scope-symbol map for fast lookup
    scope_symbol_map[{current_scope, name}] = capacity;
    
    // Register symbol in scope manager
    if (scope_manager) {
        scope_manager->register_symbol_in_current_scope(capacity);
    }
    
    capacity++;
    return capacity - 1;
}

int SageSymbolTable::declare_symbol(const string& name, int register_alloc) {
    int current_scope = scope_manager ? scope_manager->get_current_scope() : 0;
    
    symbol_entry entry;
    entry.type = nullptr;
    entry.assigned_register = register_alloc;
    entry.identifier = name;
    entry.is_variable = true;
    entry.is_parameter = false;
    entry.scope_id = current_scope;
    entry.symbol_id = capacity;
    
    entries.push_back(entry);
    
    // Register in scope-symbol map for fast lookup
    scope_symbol_map[{current_scope, name}] = capacity;
    
    // Register symbol in scope manager
    if (scope_manager) {
        scope_manager->register_symbol_in_current_scope(capacity);
    }
    
    capacity++;
    return capacity - 1;
}

int SageSymbolTable::declare_symbol_in_scope(const string& name, SageType* valuetype, int scope_id) {
    symbol_entry entry;
    entry.type = valuetype;
    entry.identifier = name;
    entry.is_variable = true;
    entry.is_parameter = false;
    entry.scope_id = scope_id;
    entry.symbol_id = capacity;
    
    entries.push_back(entry);
    
    // Register in scope-symbol map for fast lookup
    scope_symbol_map[{scope_id, name}] = capacity;
    
    // Register symbol in scope manager
    if (scope_manager) {
        scope_manager->register_symbol_in_scope(scope_id, capacity);
    }
    
    capacity++;
    return capacity - 1;
}

int SageSymbolTable::lookup_idx(const string& name) {
    // Legacy lookup - searches from current scope up through parents
    return lookup_from_scope(name, scope_manager->get_current_scope());
}

symbol_entry* SageSymbolTable::lookup(const string& name) {
    int idx = lookup_idx(name);
    if (idx >= 0 && idx < capacity) {
        return &entries[idx];
    }
    return nullptr;
}

symbol_entry* SageSymbolTable::lookup_by_index(int entry_index) {
    if (entry_index >= 0 && entry_index < capacity) {
        return &entries[entry_index];
    }
    return nullptr;
}

int SageSymbolTable::lookup_from_scope(const string& name, int scope_id) {
    // Search starting from the given scope, then chain through parent scopes
    int current_scope = scope_id;
    
    while (current_scope != -1) {
        // Check if symbol exists in this scope
        auto it = scope_symbol_map.find({current_scope, name});
        if (it != scope_symbol_map.end()) {
            return it->second;
        }
        
        // Move to parent scope
        if (scope_manager) {
            current_scope = scope_manager->get_parent_scope(current_scope);
        } else {
            break;
        }
    }
    
    return -1;  // Symbol not found
}

bool SageSymbolTable::is_visible(int symbol_index, int from_scope_id) {
    if (symbol_index < 0 || symbol_index >= capacity) {
        return false;
    }
    
    int symbol_scope = entries[symbol_index].scope_id;
    
    // Symbol is visible if its scope is an ancestor of from_scope_id
    if (scope_manager) {
        return scope_manager->is_ancestor_of(symbol_scope, from_scope_id);
    }
    
    // Without scope manager, assume all symbols are visible
    return true;
}

SageType* SageSymbolTable::resolve_sage_type(NodeManager* man, NodeIndex typenode) {
    string name = man->get_lexeme(typenode);
    int found_entry = lookup_from_scope(name, scope_manager->get_current_scope());
    if (found_entry == -1) {
        return nullptr;
    }

    return entries[found_entry].type;
}

void SageSymbolTable::initialize() {
    // Initialize root stack with sage built-in datatypes and methods
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
}

void SageSymbolTable::declare_type_symbol(const string& name, SageType* type) {
    int current_scope = scope_manager ? scope_manager->get_current_scope() : 0;
    
    types.insert(name);
    
    symbol_entry entry;
    entry.type = type;
    entry.identifier = name;
    entry.is_variable = false;
    entry.is_parameter = false;
    entry.scope_id = current_scope;
    entry.symbol_id = capacity;
    
    entries.push_back(entry);
    
    // Register in scope-symbol map
    scope_symbol_map[{current_scope, name}] = capacity;
    
    if (scope_manager) {
        scope_manager->register_symbol_in_current_scope(capacity);
    }
    
    capacity++;
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
