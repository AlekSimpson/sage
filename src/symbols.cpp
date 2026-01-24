#include <string>
#include <set>
#include <stack>
#include <numeric>
#include <algorithm>
#include <cassert>

#include "../include/codegen.h"
#include "../include/symbols.h"

#include "../include/node_manager.h"
#include "../include/sage_types.h"

using namespace std;

bool symbol_entry::type_is_resolved() {
    return type != nullptr;
}

void symbol_entry::spill(int offset) {
    spilled = true;
    spill_offset = offset;
}

symbol_entry::symbol_entry() : value(SageValue()),
                               type(nullptr),
                               identifier(""),
                               assigned_register(-1),
                               spill_offset(0),
                               scope_id(-1),
                               spilled(false) {
}

symbol_entry::symbol_entry(SageValue sval, string ident) : value(sval),
                                                           type(sval.valuetype),
                                                           identifier(ident),
                                                           assigned_register(-1),
                                                           spill_offset(0),
                                                           scope_id(-1),
                                                           spilled(false) {
}

SageSymbolTable::SageSymbolTable() {
    this->function_visitor_state = stack<function_visit>();
    this->size = 0;
    this->capacity = 0;
    this->scope_manager = nullptr;
}

SageSymbolTable::SageSymbolTable(ScopeManager *scopeman, NodeManager *nm, int initial_size)
    : nm(nm), scope_manager(scopeman) {
    this->function_visitor_state = stack<function_visit>();
    this->size = initial_size;
    this->capacity = 0;
    this->scope_manager = scopeman;

    entries.reserve(initial_size);

    declare_builtin_symbol("__SAGE__NULL__SYMBOL__", TypeRegistery::get_byte_type(VOID));
}

SageSymbolTable::~SageSymbolTable() {
}

void SageSymbolTable::declare_builtin_type_symbol(const string &name, SageType *type) {
    symbol_entry entry;
    entry.type = type;
    entry.identifier = name;
    entry.definition_ast_index = -1;
    entry.scope_id = 0;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    types.insert(capacity);
    builtins.insert(capacity);
    scope_symbol_map[{0, name}] = capacity;
    scope_manager->register_symbol_in_current_scope(capacity);

    capacity++;
}

void SageSymbolTable::declare_builtin_symbol(const string &name, SageType *type) {
    symbol_entry entry;
    entry.type = type;
    entry.assigned_register = -1;
    entry.identifier = name;
    entry.scope_id = 0;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    builtins.insert(capacity);
    scope_symbol_map[{0, name}] = capacity;
    scope_manager->register_symbol_in_current_scope(capacity);

    capacity++;
}

table_index SageSymbolTable::declare_literal(NodeIndex ast_id, SageValue value) {
    int current_scope = nm->get_scope_id(ast_id);
    string name = nm->get_identifier(ast_id);
    for (table_index literal: literals) {
        if (entries[literal].value.equals(value)) {
            return literal;
        }
    }

    symbol_entry entry;
    entry.value = value;
    entry.type = value.valuetype;
    entry.identifier = name;
    entry.scope_id = current_scope;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    literals.insert(capacity);
    scope_symbol_map[{current_scope, name}] = capacity;
    scope_manager->register_symbol_in_current_scope(capacity);

    capacity++;
    return capacity - 1;
}

table_index SageSymbolTable::declare_constant(NodeIndex ast_id, SageValue value) {
    int current_scope = nm->get_scope_id(ast_id);
    string name = nm->get_identifier(ast_id);
    for (table_index constant: constants) {
        if (entries[constant].identifier == name && entries[constant].value.equals(value)) {
            return constant;
        }
    }

    symbol_entry entry;
    entry.value = value;
    entry.type = value.valuetype;
    entry.identifier = name;
    entry.scope_id = current_scope;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    constants.insert(capacity);

    scope_symbol_map[{current_scope, name}] = capacity;
    scope_manager->register_symbol_in_current_scope(capacity);

    capacity++;
    return capacity - 1;
}

table_index SageSymbolTable::declare_temporary(int register_alloc) {
    auto name = str("temp", temporary_counter_gen);
    temporary_counter_gen++;

    symbol_entry entry;
    entry.type = nullptr;
    entry.assigned_register = register_alloc;
    entry.identifier = name;
    entry.scope_id = -1;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    scope_symbol_map[{-1, name}] = capacity;
    scope_manager->register_symbol_in_current_scope(capacity);

    capacity++;
    return capacity - 1;
}

table_index SageSymbolTable::declare_type_symbol(NodeIndex ast_id, SageType *type) {
    string name = nm->get_identifier(ast_id);
    int scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_id;

    symbol_entry entry;
    entry.type = type;
    entry.identifier = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    types.insert(capacity);
    scope_symbol_map[{scope_id, name}] = capacity;
    scope_manager->register_symbol_in_current_scope(capacity);

    capacity++;
    return capacity - 1;
}

table_index SageSymbolTable::declare_symbol(NodeIndex ast_id, SageType *valuetype) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_id;

    symbol_entry entry;
    entry.type = valuetype;
    entry.identifier = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    scope_symbol_map[{scope_id, name}] = capacity;
    scope_manager->register_symbol_in_scope(scope_id, capacity);

    capacity++;
    return capacity - 1;
}

table_index SageSymbolTable::declare_variable(NodeIndex ast_id, SageType *valuetype) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_id;

    symbol_entry entry;
    entry.type = valuetype;
    entry.identifier = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    variables.insert(capacity);
    scope_symbol_map[{scope_id, name}] = capacity;
    scope_manager->register_symbol_in_scope(scope_id, capacity);

    capacity++;
    return capacity - 1;
}

table_index SageSymbolTable::declare_parameter(NodeIndex ast_id, SageType *valuetype, int parameter_register_assignment) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_id;

    // TODO: handle auto function spilling when there are more than 6 parameters

    symbol_entry entry;
    entry.type = valuetype;
    entry.identifier = name;
    entry.scope_id = scope_id;
    entry.assigned_register = parameter_register_assignment;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    parameters.insert(capacity);
    scope_symbol_map[{scope_id, name}] = capacity;
    scope_manager->register_symbol_in_scope(scope_id, capacity);

    capacity++;
    return capacity - 1;
}

const symbol_entry *SageSymbolTable::global_lookup(const string &name) {
    int ptr_b = entries.size() - 1;

    for (int ptr_a = 0; ptr_a < capacity; ++ptr_a) {
        if (entries[ptr_a].identifier == name) {
            return &entries[ptr_a];
        }

        if (entries[ptr_b].identifier == name) {
            return &entries[ptr_b];
        }
        ptr_b--;
    }

    return nullptr;
}

table_index SageSymbolTable::lookup_table_idx(const string &name, int scope_id) {
    // search starting from the given scope, then chain through parent scopes
    if (!scope_manager) return SAGE_NULL_SYMBOL;

    int current_scope = scope_id;

    while (current_scope != -1) {
        auto it = scope_symbol_map.find({current_scope, name});
        if (it != scope_symbol_map.end()) {
            return it->second;
        }

        current_scope = scope_manager->get_parent_scope(current_scope);
    }

    return SAGE_NULL_SYMBOL;
}

symbol_entry *SageSymbolTable::lookup_by_index(table_index entry_index) {
    if (entry_index < (table_index) capacity) return &entries[entry_index];

    return nullptr;
}

symbol_entry *SageSymbolTable::lookup(const string &name, int scope_id) {
    // search starting from the given scope, then chain through parent scopes
    if (!scope_manager) return nullptr;

    int current_scope = scope_id;

    while (current_scope != -1) {
        auto it = scope_symbol_map.find({current_scope, name});
        if (it != scope_symbol_map.end()) return &entries[it->second];

        current_scope = scope_manager->get_parent_scope(current_scope);
    }

    return nullptr;
}

bool SageSymbolTable::is_visible(table_index symbol_index, int from_scope_id) {
    if (symbol_index >= (table_index) capacity || scope_manager == nullptr) return false;

    int symbol_scope = entries[symbol_index].scope_id;
    return scope_manager->is_ancestor_of(symbol_scope, from_scope_id);
}

void SageSymbolTable::initialize() {
    // setup builtin primitives
    declare_builtin_type_symbol("bool", TypeRegistery::get_byte_type(BOOL));
    declare_builtin_type_symbol("char", TypeRegistery::get_byte_type(CHAR));
    declare_builtin_type_symbol("int", TypeRegistery::get_integer_type(8));
    declare_builtin_type_symbol("i8", TypeRegistery::get_integer_type(1));
    declare_builtin_type_symbol("i32", TypeRegistery::get_integer_type(4));
    declare_builtin_type_symbol("i64", TypeRegistery::get_integer_type(8));
    declare_builtin_type_symbol("float", TypeRegistery::get_float_type(8));
    declare_builtin_type_symbol("f32", TypeRegistery::get_float_type(4));
    declare_builtin_type_symbol("f64", TypeRegistery::get_float_type(8));
    declare_builtin_type_symbol("void", TypeRegistery::get_byte_type(VOID));

    // setup builtin functions
    vector<SageType *> puti_params = {
        TypeRegistery::get_integer_type(8),
        TypeRegistery::get_integer_type(8)
    };
    vector<SageType *> puts_params = {
        TypeRegistery::get_pointer_type(TypeRegistery::get_byte_type(CHAR)),
        TypeRegistery::get_integer_type(8)
    };
    declare_builtin_symbol("puti", TypeRegistery::get_function_type(puti_params, vector<SageType *>()));
    declare_builtin_symbol("puts", TypeRegistery::get_function_type(puts_params, vector<SageType *>()));
}

SageType *SageSymbolTable::resolve_variable_type(table_index entry_index) {
    auto entry = entries[entry_index];
    if (entry.type_is_resolved()) {
        return entry.type;
    }

    auto hosttype = nm->get_host_nodetype(entry.definition_ast_index);
    NodeIndex type_ast_id = nm->get_right(entry.definition_ast_index);
    if (hosttype == PN_TRINARY) {
        type_ast_id = nm->get_middle(entry.definition_ast_index);
    }

    auto scope_id = nm->get_scope_id(type_ast_id);
    auto identifier = nm->get_identifier(type_ast_id);
    auto type_symbol = lookup(identifier, scope_id);
    assert(type_symbol != nullptr);

    if (!type_symbol->type_is_resolved()) {
        return resolve_struct_type(type_symbol->symbol_id);
    }

    return type_symbol->type;
}

SageType *SageSymbolTable::resolve_struct_type(table_index entry_index) {
    vector<SageType *> member_types;
    symbol_entry struct_entry = entries[entry_index];
    if (struct_entry.type_is_resolved()) {
        return struct_entry.type;
    }

    NodeIndex struct_signature = nm->get_right(struct_entry.definition_ast_index);

    int scope_id = nm->get_scope_id(struct_entry.definition_ast_index);
    for (auto member_expression: nm->get_children(struct_signature)) {
        auto type_expression = nm->get_right(member_expression);
        if (nm->get_host_nodetype(member_expression) == PN_TRINARY) {
            type_expression = nm->get_middle(member_expression);
        }
        auto identifier = nm->get_identifier(type_expression);
        auto type_symbol = lookup(identifier, scope_id);
        assert(type_symbol != nullptr);

        member_types.push_back(resolve_struct_type(type_symbol->symbol_id));
    }

    return TypeRegistery::get_struct_type(struct_entry.identifier, member_types);
}

SageType *SageSymbolTable::resolve_function_type(table_index entry_index) {
    vector<SageType *> parameter_types;
    vector<SageType *> return_types;
    symbol_entry function_entry = entries[entry_index];
    if (function_entry.type_is_resolved()) {
        return function_entry.type;
    }

    NodeIndex function_signature = nm->get_right(function_entry.definition_ast_index);

    int scope_id = nm->get_scope_id(function_entry.definition_ast_index);
    for (auto parameter_expression: nm->get_children(nm->get_left(function_signature))) {
        auto type_expression = nm->get_right(parameter_expression);
        if (nm->get_host_nodetype(parameter_expression) == PN_TRINARY) {
            type_expression = nm->get_middle(parameter_expression);
        }
        auto identifier = nm->get_identifier(type_expression);
        auto type_symbol = lookup(identifier, scope_id);
        assert(type_symbol != nullptr);

        parameter_types.push_back(resolve_struct_type(type_symbol->symbol_id));
    }
    for (auto return_type_expression: nm->get_children(nm->get_middle(function_signature))) {
        auto identifier = nm->get_identifier(return_type_expression);
        auto type_symbol = lookup(identifier, scope_id);
        assert(type_symbol != nullptr);

        return_types.push_back(resolve_struct_type(type_symbol->symbol_id));
    }

    if (return_types.empty()) {
        return_types.push_back(TypeRegistery::get_byte_type(VOID));
    }

    return TypeRegistery::get_function_type(return_types, parameter_types);
}
