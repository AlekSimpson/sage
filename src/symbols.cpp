#include <string>
#include <set>
#include <stack>
#include <algorithm>
#include <cassert>

#include "../include/codegen.h"
#include "../include/symbols.h"

#include <boost/function/function_template.hpp>

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

bool symbol_entry::needs_comptime_resolution() {
    return type->match(TypeRegistery::get_pending_comptime_type());
}

symbol_entry::symbol_entry() : function_info(FunctionVisit()),
                               value(SageValue()),
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
    this->function_visitor_state = stack<FunctionVisit *>();
    this->scope_manager = nullptr;
}

SageSymbolTable::SageSymbolTable(ScopeManager *scopeman, NodeManager *nm, int initial_size)
    : entries(SymbolArena(initial_size + 1)), nm(nm), scope_manager(scopeman) {
    this->function_visitor_state = stack<FunctionVisit *>();
    this->scope_manager = scopeman;

    declare_builtin_symbol("__SAGE__NULL__SYMBOL__", TypeRegistery::get_byte_type(VOID));
}

void SageSymbolTable::register_comptime_value(ComptimeManager &comptime_manager, NodeIndex ast_node, table_index i) {
    comptime_values.insert(i);
    auto &entry = entries.get(i);
    entry.is_comptime_constant = true;
    entry.task_id = comptime_manager.add_task(ast_node);
    entry.type = TypeRegistery::get_pending_comptime_type();
    comptime_task_id_to_symbol_id[entry.task_id] = i;
}

void SageSymbolTable::declare_builtin_type_symbol(const string &name, SageType *type) {
    table_index new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.type = type == nullptr ? TypeRegistery::get_pointer_type(TypeRegistery::get_builtin_type(VOID, 0)) : type;
    entry.identifier = name;
    entry.definition_ast_index = -1;
    entry.scope_id = 0;
    entry.symbol_id = new_index;

    types.insert(new_index);
    builtins.insert(new_index);
    scope_symbol_map[{0, name}] = new_index;
    scope_manager->register_symbol_in_current_scope(new_index);
}

table_index SageSymbolTable::declare_builtin_symbol(const string &name, SageType *type) {
    table_index new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.type = type == nullptr ? TypeRegistery::get_pointer_type(TypeRegistery::get_builtin_type(VOID, 0)) : type;
    entry.assigned_register = -1;
    entry.identifier = name;
    entry.scope_id = 0;
    entry.symbol_id = new_index;

    builtins.insert(new_index);
    scope_symbol_map[{0, name}] = new_index;
    scope_manager->register_symbol_in_current_scope(new_index);
    return new_index;
}

table_index SageSymbolTable::declare_immediate(SageValue value, string lexeme) {
    auto search = lookup(lexeme, 0);
    if (search != nullptr) return search->symbol_id;

    table_index new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.value = value;
    entry.type = value.valuetype;
    entry.identifier = lexeme;
    entry.scope_id = 0;
    entry.definition_ast_index = -1;
    entry.symbol_id = new_index;

    return new_index;
}

table_index SageSymbolTable::declare_literal(NodeIndex ast_id, SageValue value) {
    int current_scope = nm->get_scope_id(ast_id);
    string name = nm->get_identifier(ast_id);

    auto it = scope_symbol_map.find({current_scope, name});
    if (it == scope_symbol_map.end()) {
        table_index new_index = entries.allocate_symbol();
        auto &entry = entries.get(new_index);
        entry.value = value;
        entry.type = value.valuetype;
        entry.identifier = name;
        entry.scope_id = current_scope;
        entry.definition_ast_index = ast_id;
        entry.symbol_id = new_index;

        literals.insert(new_index);
        scope_symbol_map[{current_scope, name}] = new_index;
        scope_manager->register_symbol_in_current_scope(new_index);

        return new_index;
    }

    return (table_index) it->second;
}

table_index SageSymbolTable::declare_constant(NodeIndex ast_id, SageValue value) {
    int current_scope = nm->get_scope_id(ast_id);
    string name = nm->get_identifier(ast_id);

    auto it = scope_symbol_map.find({current_scope, name});
    if (it == scope_symbol_map.end()) {
        table_index new_index = entries.allocate_symbol();
        auto &entry = entries.get(new_index);
        entry.value = value;
        entry.type = value.valuetype;
        entry.identifier = name;
        entry.scope_id = current_scope;
        entry.definition_ast_index = ast_id;
        entry.symbol_id = new_index;

        constants.insert(new_index);

        scope_symbol_map[{current_scope, name}] = new_index;
        scope_manager->register_symbol_in_current_scope(new_index);

        return new_index;
    }

    return (table_index) it->second;
}

table_index SageSymbolTable::declare_temporary(int register_alloc) {
    auto name = str("temp", temporary_counter_gen);
    temporary_counter_gen++;

    table_index new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.type = TypeRegistery::get_pointer_type(TypeRegistery::get_builtin_type(VOID, 0));
    entry.assigned_register = register_alloc;
    entry.identifier = name;
    entry.scope_id = -1;
    entry.symbol_id = new_index;

    scope_symbol_map[{-1, name}] = new_index;
    scope_manager->register_symbol_in_current_scope(new_index);

    return new_index;
}

table_index SageSymbolTable::declare_type_symbol(NodeIndex ast_id, SageType *type) {
    string name = nm->get_identifier(ast_id);
    int scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_id;

    table_index new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.type = type;
    entry.identifier = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = new_index;

    types.insert(new_index);
    scope_symbol_map[{scope_id, name}] = new_index;
    scope_manager->register_symbol_in_current_scope(new_index);

    return new_index;
}

table_index SageSymbolTable::declare_symbol(NodeIndex ast_id, SageType *valuetype) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_id;

    table_index new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.type = valuetype;
    entry.identifier = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = new_index;

    if (identifiers_that_must_be_spilled.find(name) != identifiers_that_must_be_spilled.end()) {
        entry.spilled = true;
    }

    scope_symbol_map[{scope_id, name}] = new_index;
    scope_manager->register_symbol_in_scope(scope_id, new_index);

    return new_index;
}

table_index SageSymbolTable::declare_function(NodeIndex ast_id, SageType *function_type) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_id;

    table_index new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.function_info = FunctionVisit(new_index);
    entry.type = function_type;
    entry.identifier = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = new_index;

    if (name == "main") {
        program_uses_main_function = true;
    }

    functions.insert(new_index);
    scope_symbol_map[{scope_id, name}] = new_index;
    scope_manager->register_symbol_in_scope(scope_id, new_index);

    return new_index;
}

table_index SageSymbolTable::declare_variable(NodeIndex ast_id, SageType *valuetype) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_id;

    table_index new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.type = valuetype;
    entry.identifier = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = new_index;

    if (identifiers_that_must_be_spilled.find(name) != identifiers_that_must_be_spilled.end()) {
        entry.spilled = true;
    }

    variables.insert(new_index);
    scope_symbol_map[{scope_id, name}] = new_index;
    scope_manager->register_symbol_in_scope(scope_id, new_index);

    return new_index;
}

table_index SageSymbolTable::declare_parameter(NodeIndex ast_id, SageType *valuetype,
                                               int parameter_register_assignment) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_id;

    table_index new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.type = valuetype;
    entry.identifier = name;
    entry.scope_id = scope_id;
    entry.assigned_register = parameter_register_assignment;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = new_index;

    // TODO: handle auto function spilling when there are more than 6 parameters

    if (identifiers_that_must_be_spilled.find(name) != identifiers_that_must_be_spilled.end()) {
        entry.spilled = true;
    }

    parameters.insert(new_index);
    scope_symbol_map[{scope_id, name}] = new_index;
    scope_manager->register_symbol_in_scope(scope_id, new_index);

    return new_index;
}

const symbol_entry *SageSymbolTable::global_lookup(const string &name) {
    int ptr_b = entries.size - 1;

    for (int ptr_a = 0; ptr_a < entries.size - 1; ++ptr_a) {
        if (entries.data[ptr_a].identifier == name) {
            return entries.get_pointer(ptr_a);
        }

        if (entries.data[ptr_b].identifier == name) {
            return entries.get_pointer(ptr_b);
        }
        ptr_b--;
    }

    return nullptr;
}

table_index SageSymbolTable::lookup_table_index(const string &name, int scope_id) {
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
    if (entry_index < (table_index) entries.CAPACITY) return entries.get_pointer(entry_index);

    return nullptr;
}

symbol_entry *SageSymbolTable::lookup(const string &name, int scope_id) {
    // search starting from the given scope, then chain through parent scopes
    if (!scope_manager) return nullptr;

    int current_scope = scope_id;

    while (current_scope != -1) {
        auto it = scope_symbol_map.find({current_scope, name});
        if (it != scope_symbol_map.end()) return entries.get_pointer(it->second);

        current_scope = scope_manager->get_parent_scope(current_scope);
    }

    return nullptr;
}

bool SageSymbolTable::is_visible(table_index symbol_index, int from_scope_id) {
    if (symbol_index >= (table_index) entries.CAPACITY || scope_manager == nullptr) return false;

    int symbol_scope = entries.get(symbol_index).scope_id;
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

    declare_builtin_type_symbol("string", TypeRegistery::get_struct_type("string", {
                                                                             TypeRegistery::get_pointer_type(
                                                                                 TypeRegistery::get_byte_type(CHAR)),
                                                                             TypeRegistery::get_integer_type(8)
                                                                         }));

    // setup builtin functions
    vector<SageType *> puti_params = {
        TypeRegistery::get_integer_type(8),
        TypeRegistery::get_integer_type(8)
    };
    vector<SageType *> puts_params = {
        TypeRegistery::get_pointer_type(TypeRegistery::get_byte_type(CHAR)),
        TypeRegistery::get_integer_type(8)
    };
    vector<SageType *> return_type = {
        TypeRegistery::get_byte_type(VOID),
    };
    declare_builtin_symbol("puti", TypeRegistery::get_function_type(puti_params, return_type));
    declare_builtin_symbol("puts", TypeRegistery::get_function_type(puts_params, return_type));
    int index = declare_builtin_symbol("global", TR::get_function_type(return_type, return_type));
    auto *entry = lookup_by_index(index);
    entry->function_info = FunctionVisit(index);
    function_visitor_state.push(&entry->function_info);
}

SageType *SageSymbolTable::resolve_type_identifier(string type_identifier, int scope_id) {
    if (cached_type_identifiers.find(type_identifier) != cached_type_identifiers.end()) {
        return cached_type_identifiers.at(type_identifier);
    }

    auto type_search = lookup(type_identifier, scope_id);
    if (type_search != nullptr) return type_search->type;

    /*
     * get identifier type keys
     * type keys:
     * - "*": pointer
     * - "[]": array reference
     * - "[..]": dynamic array
     * - "[<digits>]": normal array
     */

    string base_type_identifier = "";
    bool started_processing_type_modifiers = false;
    SageType *current_type = nullptr;
    auto get_current_type = [&]() -> SageType * {
        if (current_type == nullptr) {
            current_type = lookup(base_type_identifier, scope_id)->type;
        }
        return current_type;
    };

    bool processing_array_type = false;
    int skip_until = -1;
    string current_array_length_lexeme = "";
    for (int i = 0; i < (int)type_identifier.size(); ++i) {
        skip_until = i == skip_until ? -1 : skip_until;
        if (i < skip_until) continue;

        started_processing_type_modifiers = (type_identifier[i] == '*' || type_identifier[i] == '[') || started_processing_type_modifiers;
        if (!started_processing_type_modifiers) {
            base_type_identifier += type_identifier[i];
            continue;
        }

        if (type_identifier[i] == '*') {
            current_type = TR::get_pointer_type(get_current_type());
        }else if (type_identifier[i] == '[') {
            processing_array_type = true;
            continue;
        }

        if (!processing_array_type) continue;

        if (type_identifier[i] == '.') {
            // dynamic array
            current_type = TR::get_dyn_array_type(get_current_type());
            skip_until = i + 3;
            processing_array_type = false;
        }else if (type_identifier[i] == ']' && current_array_length_lexeme.empty()) {
            // array reference
            current_type = TR::get_reference_type(get_current_type());
            processing_array_type = false;
        }else if (type_identifier[i] == ']' && !current_array_length_lexeme.empty()) {
            current_type = TR::get_array_type(get_current_type(), stoi(current_array_length_lexeme));
            current_array_length_lexeme.clear();
            processing_array_type = false;
        }else {
            // digits, normal array
            current_array_length_lexeme += type_identifier[i];
        }
    }

    return current_type;
}

SageType *SageSymbolTable::resolve_variable_type(table_index entry_index) {
    auto entry = entries.get(entry_index);
    if (entry.type_is_resolved()) {
        return entry.type;
    }

    auto hosttype = nm->get_host_nodetype(entry.definition_ast_index);
    NodeIndex type_ast_id = nm->get_right(entry.definition_ast_index);
    if (hosttype == PN_TRINARY) {
        type_ast_id = nm->get_middle(entry.definition_ast_index);
    }

    auto scope_id = entry.scope_id;
    auto identifier = nm->get_identifier(type_ast_id);


    return resolve_type_identifier(identifier, scope_id);
}

SageType *SageSymbolTable::resolve_struct_type(table_index entry_index) {
    vector<SageType *> member_types;
    auto struct_entry = entries.get(entry_index);
    if (struct_entry.type_is_resolved()) {
        return struct_entry.type;
    }

    NodeIndex struct_signature = nm->get_right(struct_entry.definition_ast_index);

    int scope_id = struct_entry.scope_id;
    for (auto member_expression: nm->get_children(struct_signature)) {
        auto type_expression = nm->get_right(member_expression);
        if (nm->get_host_nodetype(member_expression) == PN_TRINARY) {
            type_expression = nm->get_middle(member_expression);
        }
        auto identifier = nm->get_identifier(type_expression);
        member_types.push_back(resolve_type_identifier(identifier, scope_id));
    }

    return TypeRegistery::get_struct_type(struct_entry.identifier, member_types);
}

SageType *SageSymbolTable::resolve_function_type(table_index entry_index) {
    vector<SageType *> parameter_types;
    vector<SageType *> return_types;
    auto function_entry = entries.get(entry_index);
    if (function_entry.type_is_resolved()) {
        return function_entry.type;
    }

    NodeIndex function_signature = nm->get_right(function_entry.definition_ast_index);

    int scope_id = function_entry.scope_id;
    string current_identifier;
    for (auto parameter_expression: nm->get_children(nm->get_left(function_signature))) {
        auto type_expression = nm->get_right(parameter_expression);
        if (nm->get_host_nodetype(parameter_expression) == PN_TRINARY) {
            type_expression = nm->get_middle(parameter_expression);
        }
        current_identifier = nm->get_identifier(type_expression);
        parameter_types.push_back(resolve_type_identifier(current_identifier, scope_id));
    }
    for (auto return_type_expression: nm->get_children(nm->get_middle(function_signature))) {
        current_identifier = nm->get_identifier(return_type_expression);
        return_types.push_back(resolve_type_identifier(current_identifier, scope_id));
    }

    if (return_types.empty()) {
        return_types.push_back(TypeRegistery::get_byte_type(VOID));
    }

    return TypeRegistery::get_function_type(parameter_types, return_types);
}


table_index SymbolArena::allocate_symbol() {
    assertm(size < CAPACITY && size + 1 < CAPACITY, "ATTEMPTED TO ALLOCATE WHEN ARENA IS FULL.");

    int new_id = size;
    size++;
    return new_id;
}

symbol_entry &SymbolArena::get(table_index index) {
    assertm((int)index < CAPACITY, "Out of bounds arena access.");
    return data[index];
}

symbol_entry *SymbolArena::get_pointer(table_index index) {
    assertm((int)index < CAPACITY, "Out of bounds arena access.");
    return &data[index];
}


int SageSymbolTable::get_result_total_byte_size(table_index symbol_index) {
    auto *entry = lookup_by_index(symbol_index);
    auto *function_type = static_cast<SageFunctionType *>(entry->type);
    int bytesize = 0;
    for (auto *type: function_type->return_type) {
        bytesize += type->size;
    }
    return bytesize;
}

int SageSymbolTable::get_amount_of_elements_being_returned(table_index symbol_index) {
    auto *entry = lookup_by_index(symbol_index);
    auto *function_type = static_cast<SageFunctionType *>(entry->type);
    return function_type->return_type.size();
}

bool SageSymbolTable::needs_return_stack_pointer(table_index index) {
    int bytesize = get_result_total_byte_size(index);
    return bytesize > 8;
}
