#include <string>
#include <set>
#include <stack>
#include <numeric>
#include <algorithm>

#include "../include/codegen.h"
#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/sage_types.h"

using namespace std;

bool symbol_entry::type_is_resolved() {
    return type != nullptr;
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

    declare_builtin_symbol("__SAGE__NULL__SYMBOL__", nullptr);
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

    symbol_entry entry;
    entry.value = value;
    entry.type = value.valuetype;
    entry.identifier = name;
    entry.scope_id = current_scope;
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

    symbol_entry entry;
    entry.value = value;
    entry.type = value.valuetype;
    entry.identifier = name;
    entry.scope_id = current_scope;
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

void SageSymbolTable::declare_type_symbol(NodeIndex ast_id, SageType *type) {
    int current_scope = nm->get_scope_id(ast_id);
    string name = nm->get_identifier(ast_id);

    symbol_entry entry;
    entry.type = type;
    entry.identifier = name;
    entry.scope_id = current_scope;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    scope_symbol_map[{current_scope, name}] = capacity;
    scope_manager->register_symbol_in_current_scope(capacity);

    capacity++;
}

table_index SageSymbolTable::declare_symbol(NodeIndex ast_id, SageType *valuetype) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);

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

table_index SageSymbolTable::declare_parameter(NodeIndex ast_id, SageType *valuetype) {
    auto name = nm->get_identifier(ast_id);
    auto scope_id = nm->get_scope_id(ast_id);

    symbol_entry entry;
    entry.type = valuetype;
    entry.identifier = name;
    entry.scope_id = scope_id;
    entry.definition_ast_index = ast_id;
    entry.symbol_id = capacity;
    entries.push_back(entry);

    parameters.insert(capacity);
    scope_symbol_map[{scope_id, name}] = capacity;
    scope_manager->register_symbol_in_scope(scope_id, capacity);

    capacity++;
    return capacity - 1;
}

vector<table_index> SageSymbolTable::symbols_sorted_by_scope_id() {
    // note: we can cache the output of this until program source changes are detected
    vector<table_index> indices(entries.size());
    std::iota(indices.begin(), indices.end(), 0);

    // isolate just program symbols (ignore builtins)
    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        return (builtins.find(b) == builtins.end());
    });
    indices.erase(indices.begin(), indices.begin() + BUILTIN_COUNT);

    // then arange what is left according to scope_id descending
    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        return (entries[a].scope_id < entries[b].scope_id);
    });

    return indices;
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
    if (entry_index < (table_index)capacity) return &entries[entry_index];

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
    if (symbol_index >= (table_index)capacity || scope_manager == nullptr) return false;

    int symbol_scope = entries[symbol_index].scope_id;
    return scope_manager->is_ancestor_of(symbol_scope, from_scope_id);
}

void SageSymbolTable::initialize() {
    // Initialize root stack with sage built-in datatypes and methods
    auto chartype = TypeRegistery::get_builtin_type(CHAR);
    declare_builtin_type_symbol("bool", TypeRegistery::get_builtin_type(BOOL));
    declare_builtin_type_symbol("char", chartype);
    declare_builtin_type_symbol("int", TypeRegistery::get_builtin_type(I64));
    declare_builtin_type_symbol("i8", TypeRegistery::get_builtin_type(I8));
    declare_builtin_type_symbol("i32", TypeRegistery::get_builtin_type(I32));
    declare_builtin_type_symbol("i64", TypeRegistery::get_builtin_type(I64));
    declare_builtin_type_symbol("float", TypeRegistery::get_builtin_type(F64));
    declare_builtin_type_symbol("f32", TypeRegistery::get_builtin_type(F32));
    declare_builtin_type_symbol("f64", TypeRegistery::get_builtin_type(F64));
    declare_builtin_type_symbol("void", TypeRegistery::get_builtin_type(VOID));

    // TODO FIX: this interpretation of pointer syntax does not account for the fact that there could be double or triple pointers
    declare_builtin_type_symbol("char*", TypeRegistery::get_pointer_type(chartype));

    // setup builtin functions
    vector<SageType *> puti_params = {
        TypeRegistery::get_builtin_type(I64),
        TypeRegistery::get_builtin_type(I64)
    };
    vector<SageType *> puts_params = {
        TypeRegistery::get_pointer_type(TypeRegistery::get_builtin_type(CHAR)),
        TypeRegistery::get_builtin_type(I64)
    };
    declare_builtin_symbol("puti", TypeRegistery::get_function_type(puti_params, vector<SageType *>()));
    declare_builtin_symbol("puts", TypeRegistery::get_function_type(puts_params, vector<SageType *>()));
}

SageType *SageSymbolTable::resolve_sage_type(NodeManager *man, NodeIndex typenode) {
    return nullptr;
}

