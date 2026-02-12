#pragma once

#include <string>
#include <vector>
#include <stack>
#include <set>
#include <map>

#include "comptime_manager.h"
#include "scope_manager.h"
#include "node_manager.h"
#include "sage_types.h"

#define SAGE_NULL_SYMBOL 0

struct FunctionVisit {
    table_index symbol_index;
    int return_statement_count = 0;
    int stack_return_pointer_counter = 0;

    FunctionVisit(): symbol_index(-1) {};
    FunctionVisit(table_index index) : symbol_index(index) {};
    bool has_returned() const { return return_statement_count > 0; }
};

struct symbol_entry {
    FunctionVisit function_info;
    SageValue value;
    SageType *type;
    string identifier;
    int assigned_register;
    int spill_offset;
    int static_stack_pointer = -1;
    NodeIndex definition_ast_index = -1;
    int scope_id;
    table_index symbol_id;
    comptime_task_id task_id;
    bool spilled;
    bool is_comptime_constant = false;

    symbol_entry();
    symbol_entry(SageValue, string);
    bool type_is_resolved();
    void spill(int offset);
    bool needs_comptime_resolution();
};

class SymbolArena {
public:
    symbol_entry *data;
    int CAPACITY;
    int size = 0;
    bool is_empty = true;

    SymbolArena(): data(nullptr), CAPACITY(0) {}
    SymbolArena(int capacity): CAPACITY(capacity) {
        data = new symbol_entry[CAPACITY];
    }

    ~SymbolArena() {
        if (data != nullptr) {
            delete[] data;
        }
    }

    SymbolArena(SymbolArena &&other) noexcept
        : data(other.data), CAPACITY(other.CAPACITY),
          size(other.size), is_empty(other.is_empty) {
        other.data = nullptr;
        other.size = 0;
        other.is_empty = true;
    }

    SymbolArena &operator=(SymbolArena &&other) noexcept {
        if (this != &other) {
            delete[] data;
            data = other.data;
            CAPACITY = other.CAPACITY;
            size = other.size;
            is_empty = other.is_empty;
            other.data = nullptr;
            other.size = 0;
            other.is_empty = true;
        }
        return *this;
    }

    SymbolArena(const SymbolArena &) = delete;
    SymbolArena &operator=(const SymbolArena &) = delete;

    table_index allocate_symbol();
    symbol_entry &get(table_index index);
    symbol_entry *get_pointer(table_index index);
};

class SageSymbolTable {
public:
    SymbolArena entries;
    NodeManager *nm;
    ScopeManager *scope_manager;
    stack<FunctionVisit *> function_visitor_state;
    set<string> identifiers_that_must_be_spilled;
    set<table_index> builtins;
    set<table_index> variables;
    set<table_index> structs;
    set<table_index> functions;
    set<table_index> parameters;
    set<table_index> constants;
    set<table_index> literals; // used for strings and big literal values
    set<table_index> comptime_values;
    set<table_index> types;

    // (scope_id, name) -> entry index for fast lookup
    map<pair<int, string>, table_index> scope_symbol_map;
    map<comptime_task_id, table_index> comptime_task_id_to_symbol_id;
    map<string, SageType *> cached_type_identifiers;

    int temporary_counter_gen = 0;
    bool program_uses_main_function = false;

    SageSymbolTable();
    SageSymbolTable(ScopeManager* scopeman, NodeManager *nm, int size);

    SageType *resolve_type_identifier(string, int);
    SageType *resolve_variable_type(table_index);
    SageType *resolve_function_type(table_index);
    SageType *resolve_struct_type(table_index);

    bool is_variable(table_index idx) { return variables.find(idx) != variables.end(); }
    bool is_parameter(table_index idx) { return parameters.find(idx) != parameters.end(); }
    bool is_constant(table_index idx) { return constants.find(idx) != constants.end(); }
    bool is_literal(table_index idx) { return literals.find(idx) != literals.end(); }
    bool is_type(table_index idx) { return types.find(idx) != types.end(); }

    // Symbol declarations
    void declare_builtin_type_symbol(const string &name, SageType *type);
    table_index declare_builtin_symbol(const string &name, SageType *type);

    table_index declare_immediate(SageValue value, string lexeme);
    table_index declare_literal(NodeIndex ast_id, SageValue value);
    table_index declare_constant(NodeIndex ast_id, SageValue value);
    table_index declare_temporary(int register_alloc);
    table_index declare_symbol(NodeIndex ast_id, SageType *valuetype);
    table_index declare_variable(NodeIndex ast_id, SageType *valuetype);
    table_index declare_parameter(NodeIndex ast_id, SageType *valuetype, int parameter_register_assignment);
    table_index declare_type_symbol(NodeIndex ast_id, SageType *type);
    table_index declare_function(NodeIndex ast_id, SageType *type); // creates FunctionVisit value, lives here now

    // Lookups
    const symbol_entry *global_lookup(const string &name);
    symbol_entry *lookup_by_index(table_index entry_index);
    symbol_entry *lookup(const string &name, int scope_id);
    table_index lookup_table_index(const string &name, int scope_id);
    
    // Check if symbol is visible from a given scope
    bool is_visible(table_index symbol_index, int from_scope_id);
    void register_comptime_value(ComptimeManager &, NodeIndex, table_index);
    int get_result_total_byte_size(table_index);
    int get_amount_of_elements_being_returned(table_index);
    bool needs_return_stack_pointer(table_index);
    void initialize();
};
