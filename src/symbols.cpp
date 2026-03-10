#include <string>
#include <set>
#include <stack>
#include <algorithm>
#include <cassert>

#include "../include/codegen.h"
#include "../include/symbols.h"

//#include <boost/function/function_template.hpp>

#include "../include/node_manager.h"
#include "../include/sage_types.h"

using namespace std;

bool SymbolEntry::type_is_resolved() {
    return datatype != nullptr;
}

void SymbolEntry::spill(int offset) {
    spilled = true;
    stack_offset = offset;
}

bool SymbolEntry::needs_comptime_resolution() {
    return datatype->match(TypeRegistery::get_pending_comptime_type());
}

SageNamespace *SymbolEntry::get_namespace() {
    return type_namespace;
}

SageSymbolTable::SageSymbolTable() : scope_manager(nullptr), function_processing_context(stack<SymbolIndex>()) {
}

SageSymbolTable::SageSymbolTable(ScopeManager *scopeman, NodeManager *nm, int initial_size)
    : entries(SymbolArena(initial_size + 1)), nm(nm), scope_manager(scopeman) {
    this->function_processing_context = stack<SymbolIndex>();
    this->scope_manager = scopeman;

    declare_null_symbol();
}

bool SageSymbolTable::is_comptime_value(SymbolEntry *search_entry) {
    return comptime_values.find(search_entry->symbol_index) != comptime_values.end();
}

void SageSymbolTable::register_comptime_value(ComptimeManager &comptime_manager, NodeIndex ast_node,
                                              SymbolIndex index) {
    comptime_values.insert(index);
    auto &entry = entries.get(index);
    entry.task_id = comptime_manager.add_task(ast_node);
    entry.datatype = TypeRegistery::get_pending_comptime_type();
    comptime_task_id_to_symbol_id[entry.task_id] = index;
}

void SageSymbolTable::declare_null_symbol() {
    SymbolIndex new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.datatype = TypeRegistery::get_pointer_type(TypeRegistery::get_builtin_type(VOID, 0));
    entry.name = NULL_SYMBOL_NAME;
    entry.scope_id = -1;
    entry.symbol_index = SAGE_NULL_SYMBOL;

    builtins.insert(new_index);
    scope_symbol_map[{0, NULL_SYMBOL_NAME}] = new_index;
    scope_manager->register_symbol_in_current_scope(new_index);
}

SymbolIndex SageSymbolTable::declare_builtin_type_symbol(const string &name, SageType *type) {
    SymbolIndex new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.type_namespace = nullptr;
    entry.datatype = type;
    entry.name = name;
    entry.definition_ast_index = -1;
    entry.scope_id = 0;
    entry.symbol_index = new_index;

    builtins.insert(new_index);
    types.insert(new_index);
    scope_symbol_map[{0, name}] = new_index;
    scope_manager->register_symbol_in_current_scope(new_index);
    return new_index;
}

SymbolIndex SageSymbolTable::declare_literal(NodeIndex ast_id, SageValue value, int static_pointer) {
    int current_scope = nm->get_scope_id(ast_id);
    string name = nm->get_identifier(ast_id);

    auto it = scope_symbol_map.find({current_scope, name});
    if (it == scope_symbol_map.end()) {
        SymbolIndex new_index = entries.allocate_symbol();
        auto &entry = entries.get(new_index);
        entry.data = value;
        entry.datatype = value.type;
        entry.name = name;
        entry.scope_id = current_scope;
        entry.definition_ast_index = ast_id;
        entry.static_stack_pointer = static_pointer;
        entry.symbol_index = new_index;

        scope_symbol_map[{current_scope, name}] = new_index;
        scope_manager->register_symbol_in_current_scope(new_index);

        return new_index;
    }

    return it->second;
}

SymbolIndex SageSymbolTable::declare_type_symbol(NodeIndex ast_id, SageType *type) {
    string name = nm->get_identifier(ast_id);
    int scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_index;

    SymbolIndex new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.type_namespace = new SageNamespace();
    entry.datatype = type;
    entry.name = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_index = new_index;

    types.insert(new_index);
    scope_symbol_map[{scope_id, name}] = new_index;
    scope_manager->register_symbol_in_current_scope(new_index);

    return new_index;
}

SymbolIndex SageSymbolTable::declare_function(NodeIndex ast_id, SageType *function_type) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_index;

    SymbolIndex new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);

    entry.datatype = function_type;
    entry.name = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_index = new_index;

    if (name == "main") {
        program_uses_main_function = true;
    }

    functions.insert(new_index);
    scope_symbol_map[{scope_id, name}] = new_index;
    scope_manager->register_symbol_in_scope(scope_id, new_index);

    return new_index;
}

SymbolIndex SageSymbolTable::declare_builtin_function(const string &name, SageType *function_type) {
    SymbolIndex new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);

    entry.datatype = function_type;
    entry.name = name;
    entry.scope_id = 0;
    entry.definition_ast_index = -1;
    entry.symbol_index = new_index;

    if (name == "main") {
        program_uses_main_function = true;
    }

    functions.insert(new_index);
    builtins.insert(new_index);
    scope_symbol_map[{entry.scope_id, name}] = new_index;
    scope_manager->register_symbol_in_scope(entry.scope_id, new_index);

    return new_index;
}

SymbolIndex SageSymbolTable::declare_variable(NodeIndex ast_id, SageType *valuetype) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_index;

    SymbolIndex new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.datatype = valuetype;
    entry.name = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_index = new_index;

    if (identifiers_that_must_be_spilled.find(name) != identifiers_that_must_be_spilled.end()) {
        entry.spilled = true;
    }

    variables.insert(new_index);
    scope_symbol_map[{scope_id, name}] = new_index;
    scope_manager->register_symbol_in_scope(scope_id, new_index);

    return new_index;
}

SymbolIndex SageSymbolTable::declare_parameter(NodeIndex ast_id, SageType *valuetype,
                                               int parameter_register_assignment) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);
    auto symbol_check = lookup(name, scope_id);
    if (symbol_check != nullptr) return symbol_check->symbol_index;

    SymbolIndex new_index = entries.allocate_symbol();
    auto &entry = entries.get(new_index);
    entry.datatype = valuetype;
    entry.name = name;
    entry.scope_id = scope_id;
    entry.assigned_register = parameter_register_assignment;
    entry.definition_ast_index = ast_id;
    entry.symbol_index = new_index;

    // TODO: handle auto function spilling when there are more than 6 parameters

    if (identifiers_that_must_be_spilled.find(name) != identifiers_that_must_be_spilled.end()) {
        entry.spilled = true;
    }

    scope_symbol_map[{scope_id, name}] = new_index;
    scope_manager->register_symbol_in_scope(scope_id, new_index);

    return new_index;
}

const SymbolEntry *SageSymbolTable::global_lookup(const string &name) {
    int ptr_b = entries.size - 1;

    for (int ptr_a = 0; ptr_a < entries.size - 1; ++ptr_a) {
        if (entries.data[ptr_a].name == name) {
            return entries.get_pointer(ptr_a);
        }

        if (entries.data[ptr_b].name == name) {
            return entries.get_pointer(ptr_b);
        }
        ptr_b--;
    }

    return nullptr;
}

SymbolIndex SageSymbolTable::lookup_table_index(const string &name, int scope_id) {
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

SymbolEntry *SageSymbolTable::lookup_by_index(SymbolIndex entry_index) {
    if (entry_index < entries.CAPACITY) return entries.get_pointer(entry_index);
    return nullptr;
}

SymbolEntry *SageSymbolTable::lookup(const string &name, int scope_id) {
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

bool SageSymbolTable::is_visible(SymbolIndex symbol_index, int from_scope_id) {
    if (symbol_index >= entries.CAPACITY || scope_manager == nullptr) return false;

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

    auto type_symbol_index = declare_builtin_type_symbol("string", TypeRegistery::get_struct_type("string", {
                                                                             TypeRegistery::get_pointer_type(
                                                                                 TypeRegistery::get_byte_type(CHAR)),
                                                                             TypeRegistery::get_integer_type(8)
                                                                         }));
    auto *string_namespace = new BuiltinNamespace();
    entries.get_pointer(type_symbol_index)->type_namespace = string_namespace;
    string_namespace->add_field_member("bytes", TR::get_pointer_type(TR::get_byte_type(CHAR)));
    string_namespace->add_field_member("length", TR::get_integer_type(8));


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
    declare_builtin_function("puti", TypeRegistery::get_function_type(puti_params, return_type));
    declare_builtin_function("puts", TypeRegistery::get_function_type(puts_params, return_type));
    declare_builtin_function(GLOBAL_NAME, TR::get_function_type(return_type, return_type));
    //int index = declare_builtin_symbol("global", TR::get_function_type(return_type, return_type));
    //auto *entry = lookup_by_index(index);
    //entry->function_info = FunctionVisit(index);
    //function_visitor_state.push(&entry->function_info);
}

SageType *SageSymbolTable::resolve_unknown_type_node(NodeIndex node, int scope_id) {
    return nullptr;
}

SageType *SageSymbolTable::resolve_variable_type(SymbolIndex entry_index) {
    auto &entry = entries.get(entry_index);
    if (entry.type_is_resolved()) {
        return entry.datatype;
    }

    auto hosttype = nm->get_host_nodetype(entry.definition_ast_index);
    NodeIndex type_ast_id = nm->get_right(entry.definition_ast_index);
    if (hosttype == PN_TRINARY) {
        type_ast_id = nm->get_middle(entry.definition_ast_index);
    }

    return resolve_unknown_type_node(type_ast_id, entry.scope_id);
}

SageType *SageSymbolTable::resolve_struct_type(SymbolIndex entry_index) {
    vector<SageType *> member_types;
    auto &struct_entry = entries.get(entry_index);
    if (struct_entry.type_is_resolved()) {
        return struct_entry.datatype;
    }

    NodeIndex struct_body = nm->get_right(struct_entry.definition_ast_index);
    struct_body = nm->get_branch(struct_body);

    int scope_id = nm->get_scope_id(struct_body);
    for (auto member_expression: nm->get_children(struct_body)) {
        auto type_expression = nm->get_right(member_expression);
        if (nm->get_host_nodetype(member_expression) == PN_TRINARY) {
            type_expression = nm->get_middle(member_expression);
        }

        auto identifier = nm->get_identifier(type_expression);
        auto *member_type = resolve_unknown_type_node(type_expression, scope_id);
        member_types.push_back(member_type);

        auto member_symbol_node = nm->get_left(member_expression);
        auto member_name = nm->get_identifier(member_symbol_node);
        auto *member_symbol = lookup(member_name, scope_id);
        assert(member_symbol != nullptr);
        member_symbol->datatype = member_type;
        struct_entry.type_namespace->add_field_member(member_symbol);
    }

    return TypeRegistery::get_struct_type(struct_entry.name, member_types);
}

SageType *SageSymbolTable::resolve_function_type(SymbolIndex entry_index) {
    vector<SageType *> parameter_types;
    vector<SageType *> return_types;
    auto *function_entry = entries.get_pointer(entry_index);
    if (function_entry->type_is_resolved()) {
        return function_entry->datatype;
    }

    NodeIndex function_signature = nm->get_right(function_entry->definition_ast_index);

    int scope_id = function_entry->scope_id;
    string current_identifier;
    for (auto parameter_expression: nm->get_children(nm->get_left(function_signature))) {
        auto type_expression = nm->get_right(parameter_expression);
        if (nm->get_host_nodetype(parameter_expression) == PN_TRINARY) {
            type_expression = nm->get_middle(parameter_expression);
        }
        parameter_types.push_back(resolve_unknown_type_node(type_expression, scope_id));
    }
    for (auto return_type_expression: nm->get_children(nm->get_middle(function_signature))) {
        return_types.push_back(resolve_unknown_type_node(return_type_expression, scope_id));
    }

    if (return_types.empty()) {
        return_types.push_back(TypeRegistery::get_byte_type(VOID));
    }

    SageType *first_parameter = nullptr;
    if (!parameter_types.empty()) {
        first_parameter = parameter_types[0];
        if (first_parameter->identify() == FUNC || first_parameter->identify() == REFERENCE || TR::is_builtin_primitive(first_parameter)) {
            return TypeRegistery::get_function_type(parameter_types, return_types);
        }

        string base_type_name = first_parameter->get_base_type_string();
        auto *type_entry = lookup(base_type_name, function_entry->scope_id);
        assert(type_entry != nullptr);
        type_entry->type_namespace->add_method(function_entry);
    }

    return TypeRegistery::get_function_type(parameter_types, return_types);
}

int SageSymbolTable::get_result_total_byte_size(SymbolIndex symbol_index) {
    auto *entry = lookup_by_index(symbol_index);
    assert(entry != nullptr);
    auto *function_type = static_cast<SageFunctionType *>(entry->datatype);
    int bytesize = 0;
    for (auto *type: function_type->return_type) {
        bytesize += type->size;
    }
    return bytesize;
}

int SageSymbolTable::get_amount_of_elements_being_returned(SymbolIndex symbol_index) {
    auto *entry = lookup_by_index(symbol_index);
    assert(entry != nullptr);
    auto *function_type = static_cast<SageFunctionType *>(entry->datatype);
    return function_type->return_type.size();
}

bool SageSymbolTable::needs_return_stack_pointer(SymbolIndex index) {
    int bytesize = get_result_total_byte_size(index);
    return bytesize > 8;
}

SymbolEntry &SageSymbolTable::function_being_processed() {
    return entries.get(function_processing_context.top());
}

void SageSymbolTable::push_function_processing_context(SymbolIndex new_processing_context) {
    function_processing_context.push(new_processing_context);
}


void SageSymbolTable::pop_function_processing_context() {
    function_processing_context.pop();
}

int SageNamespace::get_field_offset(SageSymbolTable *table, const string &name) {
    SymbolIndex it = fields.find(name)->second;
    auto *entry = table->lookup_by_index(it);
    return entry->stack_offset;
}

void SageNamespace::add_method(SymbolEntry *entry) {
    methods[entry->name] = entry->symbol_index;
}

void SageNamespace::add_field_member(SymbolEntry *entry) {
    fields[entry->name] = entry->symbol_index;
    entry->stack_offset = next_offset;
    entry->is_struct_member = true;
    next_offset += entry->datatype->size;
}

bool SageNamespace::is_field_member(string &name) {
    return fields.find(name) != fields.end();
}

bool SageNamespace::is_method(string &name) {
    return methods.find(name) != methods.end();
}

int SageNamespace::lookup_struct_member(string &name) {
    return fields[name];
}

BuiltinNamespace *SageNamespace::as_builtin() {
    return static_cast<BuiltinNamespace *>(this);
}

void BuiltinNamespace::add_field_member(const string &name, SageType *member_type) {
    builtin_fields[name] = next_offset;
    builtin_field_types[name] = member_type;
    next_offset += member_type->size;
}

bool BuiltinNamespace::is_field_member(string &name) {
    return builtin_fields.find(name) != builtin_fields.end();
}

int BuiltinNamespace::get_field_offset(const string &name) {
    auto it = builtin_fields.find(name);
    if (it != builtin_fields.end()) return it->second;
    return -1;
}

SageType* BuiltinNamespace::get_field_type(const string &name) {
    auto it = builtin_field_types.find(name);
    if (it != builtin_field_types.end()) return it->second;
    return nullptr;
}


SymbolIndex SymbolArena::allocate_symbol() {
    assertm(size < CAPACITY && size + 1 < CAPACITY, "ATTEMPTED TO ALLOCATE WHEN ARENA IS FULL.");

    int new_id = size;
    size++;
    is_empty = false;
    return new_id;
}

SymbolEntry &SymbolArena::get(SymbolIndex index) {
    assertm((int)index < CAPACITY, "Out of bounds arena access.");
    return data[index];
}

SymbolEntry *SymbolArena::get_pointer(SymbolIndex index) {
    assertm((int)index < CAPACITY, "Out of bounds arena access.");
    return &data[index];
}



